/** @file
 *
 * VBoxGuestLib - A support library for VirtualBox guest additions:
 * Central calls
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

/* Entire file is ifdef'ed with !VBGL_VBOXGUEST */
#ifndef VBGL_VBOXGUEST

#ifdef RT_OS_LINUX
# include "VBoxCalls.h"
# define DbgPrint AssertMsg2
#else
# include "VBoxCalls.h"
#endif
#include <iprt/time.h>
#include <iprt/path.h>
#include <iprt/string.h>

#define SHFL_CPARMS_SET_UTF8 0

#define VBOX_INIT_CALL(a, b, c)          \
    (a)->result      = VINF_SUCCESS;     \
    (a)->u32ClientID = (c)->ulClientID;  \
    (a)->u32Function = SHFL_FN_##b;      \
    (a)->cParms      = SHFL_CPARMS_##b

#ifndef RT_OS_WINDOWS
# define RtlZeroMemory(a, b) memset (a, 0, b)
#endif


DECLVBGL(int) vboxInit (void)
{
    int rc = VINF_SUCCESS;

    rc = VbglInit ();
    return rc;
}

DECLVBGL(void) vboxUninit (void)
{
    VbglTerminate ();
}

DECLVBGL(int) vboxConnect (PVBSFCLIENT pClient)
{
    int rc = VINF_SUCCESS;

    VBoxGuestHGCMConnectInfo data;

    RtlZeroMemory (&data, sizeof (VBoxGuestHGCMConnectInfo));

    pClient->handle = NULL;

    data.result   = VINF_SUCCESS;
    data.Loc.type = VMMDevHGCMLoc_LocalHost_Existing;
    strcpy (data.Loc.u.host.achName, "VBoxSharedFolders");

    rc = VbglHGCMConnect (&pClient->handle, &data);

/*    Log(("VBOXSF: VBoxSF::vboxConnect: VbglHGCMConnect rc = %#x, result = %#x\n",
         rc, data.result));
*/
    if (VBOX_SUCCESS (rc))
    {
        rc = data.result;
    }

    if (VBOX_SUCCESS (rc))
    {
        pClient->ulClientID = data.u32ClientID;
    }
    return rc;
}

DECLVBGL(void) vboxDisconnect (PVBSFCLIENT pClient)
{
    int rc;

    VBoxGuestHGCMDisconnectInfo data;

    if (pClient->handle == NULL)
        return;                 /* not connected */

    RtlZeroMemory (&data, sizeof (VBoxGuestHGCMDisconnectInfo));

    data.result      = VINF_SUCCESS;
    data.u32ClientID = pClient->ulClientID;

    rc = VbglHGCMDisconnect (pClient->handle, &data);
/*    Log(("VBOXSF: VBoxSF::vboxDisconnect: "
         "VbglHGCMDisconnect rc = %#x, result = %#x\n", rc, data.result));
*/
    return;
}

DECLVBGL(int) vboxCallQueryMappings (PVBSFCLIENT pClient, SHFLMAPPING paMappings[],
                           uint32_t *pcMappings)
{
    int rc = VINF_SUCCESS;

    VBoxSFQueryMappings data;

    VBOX_INIT_CALL(&data.callInfo, QUERY_MAPPINGS, pClient);

    data.flags.type                      = VMMDevHGCMParmType_32bit;
    data.flags.u.value32                 = SHFL_MF_UCS2;

    data.numberOfMappings.type           = VMMDevHGCMParmType_32bit;
    data.numberOfMappings.u.value32      = *pcMappings;

    data.mappings.type                   = VMMDevHGCMParmType_LinAddr;
    data.mappings.u.Pointer.size         = sizeof (SHFLMAPPING) * *pcMappings;
    data.mappings.u.Pointer.u.linearAddr = (VBOXGCPTR)&paMappings[0];

/*    Log(("VBOXSF: in ifs difference %d\n",
         (char *)&data.flags.type - (char *)&data.callInfo.cParms));
*/
    rc = VbglHGCMCall (pClient->handle, &data.callInfo, sizeof (data));

/*
    Log(("VBOXSF: VBoxSF::vboxCallQueryMappings: "
         "VbglHGCMCall rc = %#x, result = %#x\n", rc, data.callInfo.result));
*/
    if (VBOX_SUCCESS (rc))
    {
        rc = data.callInfo.result;
    }

    if (VBOX_SUCCESS (rc))
    {
        *pcMappings = data.numberOfMappings.u.value32;
    }

    return rc;
}

DECLVBGL(int) vboxCallQueryMapName (PVBSFCLIENT pClient, SHFLROOT root, SHFLSTRING *pString, uint32_t size)
{
    int rc = VINF_SUCCESS;

    VBoxSFQueryMapName data;

    VBOX_INIT_CALL(&data.callInfo, QUERY_MAP_NAME, pClient);

    data.root.type                   = VMMDevHGCMParmType_32bit;
    data.root.u.value32              = root;

    data.name.type                   = VMMDevHGCMParmType_LinAddr;
    data.name.u.Pointer.size         = size;
    data.name.u.Pointer.u.linearAddr = (VBOXGCPTR)pString;

    rc = VbglHGCMCall (pClient->handle, &data.callInfo, sizeof (data));

/*    Log(("VBOXSF: VBoxSF::vboxCallQueryMapName: "
         "VbglHGCMCall rc = %#x, result = %#x\n", rc, data.callInfo.result));
*/
    if (VBOX_SUCCESS (rc))
    {
        rc = data.callInfo.result;
    }

    return rc;
}

DECLVBGL(int) vboxCallMapFolder(PVBSFCLIENT pClient, PSHFLSTRING szFolderName,
                                PVBSFMAP pMap)
{
    int rc = VINF_SUCCESS;

    VBoxSFMapFolder data;

    VBOX_INIT_CALL(&data.callInfo, MAP_FOLDER, pClient);

    data.path.type                    = VMMDevHGCMParmType_LinAddr;
    data.path.u.Pointer.size          = ShflStringSizeOfBuffer (szFolderName);
    data.path.u.Pointer.u.linearAddr  = (VBOXGCPTR)szFolderName;

    data.root.type                    = VMMDevHGCMParmType_32bit;
    data.root.u.value32               = 0;

    data.delimiter.type               = VMMDevHGCMParmType_32bit;
    data.delimiter.u.value32          = RTPATH_DELIMITER;

    data.fCaseSensitive.type          = VMMDevHGCMParmType_32bit;
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    data.fCaseSensitive.u.value32     = 0;
#else
    data.fCaseSensitive.u.value32     = 1;
#endif

    rc = VbglHGCMCall (pClient->handle, &data.callInfo, sizeof (data));

/*    Log(("VBOXSF: VBoxSF::vboxCallMapFolder: "
         "VbglHGCMCall rc = %#x, result = %#x\n", rc, data.callInfo.result));
*/
    if (VBOX_SUCCESS (rc))
    {
        pMap->root = data.root.u.value32;
        rc         = data.callInfo.result;
    }
    else
    if (rc == VERR_NOT_IMPLEMENTED)
    {
        /* try the legacy interface too; temporary to assure backwards compatibility */
        VBoxSFMapFolder_Old data;

        VBOX_INIT_CALL(&data.callInfo, MAP_FOLDER_OLD, pClient);

        data.path.type                    = VMMDevHGCMParmType_LinAddr;
        data.path.u.Pointer.size          = ShflStringSizeOfBuffer (szFolderName);
        data.path.u.Pointer.u.linearAddr  = (VBOXGCPTR)szFolderName;

        data.root.type                    = VMMDevHGCMParmType_32bit;
        data.root.u.value32               = 0;

        data.delimiter.type               = VMMDevHGCMParmType_32bit;
        data.delimiter.u.value32          = RTPATH_DELIMITER;

        rc = VbglHGCMCall (pClient->handle, &data.callInfo, sizeof (data));

        if (VBOX_SUCCESS (rc))
        {
            pMap->root = data.root.u.value32;
            rc         = data.callInfo.result;
        }
    }
    return rc;
}

DECLVBGL(int) vboxCallUnmapFolder(PVBSFCLIENT pClient, PVBSFMAP pMap)
{
    int rc = VINF_SUCCESS;

    VBoxSFUnmapFolder data;

    VBOX_INIT_CALL(&data.callInfo, UNMAP_FOLDER, pClient);

    data.root.type                      = VMMDevHGCMParmType_32bit;
    data.root.u.value32                 = pMap->root;

    rc = VbglHGCMCall (pClient->handle, &data.callInfo, sizeof (data));

/*    Log(("VBOXSF: VBoxSF::vboxCallUnmapFolder: "
         "VbglHGCMCall rc = %#x, result = %#x\n", rc, data.callInfo.result));
*/
    if (VBOX_SUCCESS (rc))
    {
        rc         = data.callInfo.result;
    }
    return rc;
}

DECLVBGL(int) vboxCallCreate (PVBSFCLIENT pClient, PVBSFMAP pMap,
                              PSHFLSTRING pParsedPath, PSHFLCREATEPARMS pCreateParms)
{
    /** @todo copy buffers to physical or mapped memory. */
    int rc = VINF_SUCCESS;

    VBoxSFCreate data;

    VBOX_INIT_CALL(&data.callInfo, CREATE, pClient);

    data.root.type                    = VMMDevHGCMParmType_32bit;
    data.root.u.value32               = pMap->root;

    data.path.type                    = VMMDevHGCMParmType_LinAddr;
    data.path.u.Pointer.size          = ShflStringSizeOfBuffer (pParsedPath);
    data.path.u.Pointer.u.linearAddr  = (VBOXGCPTR)pParsedPath;

    data.parms.type                   = VMMDevHGCMParmType_LinAddr;
    data.parms.u.Pointer.size         = sizeof (SHFLCREATEPARMS);
    data.parms.u.Pointer.u.linearAddr = (VBOXGCPTR)pCreateParms;

    rc = VbglHGCMCall (pClient->handle, &data.callInfo, sizeof (data));

/*    Log(("VBOXSF: VBoxSF::vboxCallCreate: "
         "VbglHGCMCall rc = %#x, result = %#x\n", rc, data.callInfo.result));
*/
    if (VBOX_SUCCESS (rc))
    {
        rc = data.callInfo.result;
    }
    return rc;
}

DECLVBGL(int) vboxCallClose (PVBSFCLIENT pClient, PVBSFMAP pMap, SHFLHANDLE Handle)
{
    int rc = VINF_SUCCESS;

    VBoxSFClose data;

    VBOX_INIT_CALL(&data.callInfo, CLOSE, pClient);

    data.root.type                      = VMMDevHGCMParmType_32bit;
    data.root.u.value32                 = pMap->root;

    data.handle.type                    = VMMDevHGCMParmType_64bit;
    data.handle.u.value64               = Handle;

    rc = VbglHGCMCall (pClient->handle, &data.callInfo, sizeof (data));

/*    Log(("VBOXSF: VBoxSF::vboxCallClose: "
         "VbglHGCMCall rc = %#x, result = %#x\n", rc, data.callInfo.result));
*/
    if (VBOX_SUCCESS (rc))
    {
        rc = data.callInfo.result;
    }

    return rc;
}

DECLVBGL(int) vboxCallRemove (PVBSFCLIENT pClient, PVBSFMAP pMap,
                              PSHFLSTRING pParsedPath, uint32_t flags)
{
    int rc = VINF_SUCCESS;

    VBoxSFRemove data;

    VBOX_INIT_CALL(&data.callInfo, REMOVE, pClient);

    data.root.type                      = VMMDevHGCMParmType_32bit;
    data.root.u.value32                 = pMap->root;

    data.path.type                      = VMMDevHGCMParmType_LinAddr_In;
    data.path.u.Pointer.size            = ShflStringSizeOfBuffer (pParsedPath);
    data.path.u.Pointer.u.linearAddr    = (VBOXGCPTR)pParsedPath;

    data.flags.type                     = VMMDevHGCMParmType_32bit;
    data.flags.u.value32                = flags;

    rc = VbglHGCMCall (pClient->handle, &data.callInfo, sizeof (data));

/*    Log(("VBOXSF: VBoxSF::vboxCallRemove: "
         "VbglHGCMCall rc = %#x, result = %#x\n", rc, data.callInfo.result));
*/
    if (VBOX_SUCCESS (rc))
    {
        rc = data.callInfo.result;
    }

    return rc;
}

DECLVBGL(int) vboxCallRename (PVBSFCLIENT pClient, PVBSFMAP pMap,
                              PSHFLSTRING pSrcPath, PSHFLSTRING pDestPath, uint32_t flags)
{
    int rc = VINF_SUCCESS;

    VBoxSFRename data;

    VBOX_INIT_CALL(&data.callInfo, RENAME, pClient);

    data.root.type                      = VMMDevHGCMParmType_32bit;
    data.root.u.value32                 = pMap->root;

    data.src.type                       = VMMDevHGCMParmType_LinAddr_In;
    data.src.u.Pointer.size             = ShflStringSizeOfBuffer (pSrcPath);
    data.src.u.Pointer.u.linearAddr     = (VBOXGCPTR)pSrcPath;

    data.dest.type                      = VMMDevHGCMParmType_LinAddr_In;
    data.dest.u.Pointer.size            = ShflStringSizeOfBuffer (pDestPath);
    data.dest.u.Pointer.u.linearAddr    = (VBOXGCPTR)pDestPath;

    data.flags.type                     = VMMDevHGCMParmType_32bit;
    data.flags.u.value32                = flags;

    rc = VbglHGCMCall (pClient->handle, &data.callInfo, sizeof (data));

/*    Log(("VBOXSF: VBoxSF::vboxCallRename: "
         "VbglHGCMCall rc = %#x, result = %#x\n", rc, data.callInfo.result));
*/
    if (VBOX_SUCCESS (rc))
    {
        rc = data.callInfo.result;
    }
    return rc;
}

DECLVBGL(int) vboxCallRead(PVBSFCLIENT pClient, PVBSFMAP pMap, SHFLHANDLE hFile,
                           uint64_t offset, uint32_t *pcbBuffer, uint8_t *pBuffer, bool fLocked)
{
    int rc = VINF_SUCCESS;

    VBoxSFRead data;

    VBOX_INIT_CALL(&data.callInfo, READ, pClient);

    data.root.type                      = VMMDevHGCMParmType_32bit;
    data.root.u.value32                 = pMap->root;

    data.handle.type                    = VMMDevHGCMParmType_64bit;
    data.handle.u.value64               = hFile;
    data.offset.type                    = VMMDevHGCMParmType_64bit;
    data.offset.u.value64               = offset;
    data.cb.type                        = VMMDevHGCMParmType_32bit;
    data.cb.u.value32                   = *pcbBuffer;
    data.buffer.type                    = (fLocked) ? VMMDevHGCMParmType_LinAddr_Locked_Out : VMMDevHGCMParmType_LinAddr_Out;
    data.buffer.u.Pointer.size          = *pcbBuffer;
    data.buffer.u.Pointer.u.linearAddr  = (VBOXGCPTR)pBuffer;

    rc = VbglHGCMCall (pClient->handle, &data.callInfo, sizeof (data));

/*    Log(("VBOXSF: VBoxSF::vboxCallRead: "
         "VbglHGCMCall rc = %#x, result = %#x\n", rc, data.callInfo.result));
*/
    if (VBOX_SUCCESS (rc))
    {
        rc = data.callInfo.result;
        *pcbBuffer = data.cb.u.value32;
    }
    return rc;
}

DECLVBGL(int) vboxCallWrite(PVBSFCLIENT pClient, PVBSFMAP pMap, SHFLHANDLE hFile,
                            uint64_t offset, uint32_t *pcbBuffer, uint8_t *pBuffer, bool fLocked)
{
    int rc = VINF_SUCCESS;

    VBoxSFWrite data;

    VBOX_INIT_CALL(&data.callInfo, WRITE, pClient);

    data.root.type                      = VMMDevHGCMParmType_32bit;
    data.root.u.value32                 = pMap->root;

    data.handle.type                    = VMMDevHGCMParmType_64bit;
    data.handle.u.value64               = hFile;
    data.offset.type                    = VMMDevHGCMParmType_64bit;
    data.offset.u.value64               = offset;
    data.cb.type                        = VMMDevHGCMParmType_32bit;
    data.cb.u.value32                   = *pcbBuffer;
    data.buffer.type                    = (fLocked) ? VMMDevHGCMParmType_LinAddr_Locked_In : VMMDevHGCMParmType_LinAddr_In;
    data.buffer.u.Pointer.size          = *pcbBuffer;
    data.buffer.u.Pointer.u.linearAddr  = (VBOXGCPTR)pBuffer;

    rc = VbglHGCMCall (pClient->handle, &data.callInfo, sizeof (data));

/*    Log(("VBOXSF: VBoxSF::vboxCallWrite: "
         "VbglHGCMCall rc = %#x, result = %#x\n", rc, data.callInfo.result));
*/
    if (VBOX_SUCCESS (rc))
    {
        rc = data.callInfo.result;
        *pcbBuffer = data.cb.u.value32;
    }
    return rc;
}

DECLVBGL(int) vboxCallFlush(PVBSFCLIENT pClient, PVBSFMAP pMap, SHFLHANDLE hFile)
{
    int rc = VINF_SUCCESS;

    VBoxSFFlush data;

    VBOX_INIT_CALL(&data.callInfo, FLUSH, pClient);

    data.root.type                      = VMMDevHGCMParmType_32bit;
    data.root.u.value32                 = pMap->root;

    data.handle.type                    = VMMDevHGCMParmType_64bit;
    data.handle.u.value64               = hFile;

    rc = VbglHGCMCall (pClient->handle, &data.callInfo, sizeof (data));

/*    Log(("VBOXSF: VBoxSF::vboxCallFlush: "
         "VbglHGCMCall rc = %#x, result = %#x\n", rc, data.callInfo.result));
*/
    if (VBOX_SUCCESS (rc))
    {
        rc = data.callInfo.result;
    }
    return rc;
}

DECLVBGL(int) vboxCallDirInfo (
    PVBSFCLIENT pClient,
    PVBSFMAP pMap,
    SHFLHANDLE hFile,
    PSHFLSTRING ParsedPath,
    uint32_t flags,
    uint32_t index,
    uint32_t *pcbBuffer,
    PSHFLDIRINFO pBuffer,
    uint32_t *pcFiles)
{
    int rc = VINF_SUCCESS;

    VBoxSFList data;

    VBOX_INIT_CALL(&data.callInfo, LIST, pClient);

    data.root.type                      = VMMDevHGCMParmType_32bit;
    data.root.u.value32                 = pMap->root;

    data.handle.type                    = VMMDevHGCMParmType_64bit;
    data.handle.u.value64               = hFile;
    data.flags.type                     = VMMDevHGCMParmType_32bit;
    data.flags.u.value32                = flags;
    data.cb.type                        = VMMDevHGCMParmType_32bit;
    data.cb.u.value32                   = *pcbBuffer;
    data.path.type                      = VMMDevHGCMParmType_LinAddr_In;
    data.path.u.Pointer.size            =
        (ParsedPath) ? ShflStringSizeOfBuffer(ParsedPath) : 0;
    data.path.u.Pointer.u.linearAddr    = (VBOXGCPTR) ParsedPath;

    data.buffer.type                    = VMMDevHGCMParmType_LinAddr_Out;
    data.buffer.u.Pointer.size          = *pcbBuffer;
    data.buffer.u.Pointer.u.linearAddr  = (VBOXGCPTR)pBuffer;

    data.resumePoint.type               = VMMDevHGCMParmType_32bit;
    data.resumePoint.u.value32          = index;
    data.cFiles.type                    = VMMDevHGCMParmType_32bit;
    data.cFiles.u.value32               = 0; /* out parameters only */

    rc = VbglHGCMCall (pClient->handle, &data.callInfo, sizeof (data));

/*    Log(("VBOXSF: VBoxSF::vboxCallDirInfo: "
         "VbglHGCMCall rc = %#x, result = %#x\n", rc, data.callInfo.result));
*/
    if (VBOX_SUCCESS (rc))
    {
        rc = data.callInfo.result;
    }
    *pcbBuffer = data.cb.u.value32;
    *pcFiles   = data.cFiles.u.value32;
    return rc;
}

DECLVBGL(int) vboxCallFSInfo(PVBSFCLIENT pClient, PVBSFMAP pMap, SHFLHANDLE hFile,
                             uint32_t flags, uint32_t *pcbBuffer, PSHFLDIRINFO pBuffer)
{
    int rc = VINF_SUCCESS;

    VBoxSFInformation data;

    VBOX_INIT_CALL(&data.callInfo, INFORMATION, pClient);

    data.root.type                      = VMMDevHGCMParmType_32bit;
    data.root.u.value32                 = pMap->root;

    data.handle.type                    = VMMDevHGCMParmType_64bit;
    data.handle.u.value64               = hFile;
    data.flags.type                     = VMMDevHGCMParmType_32bit;
    data.flags.u.value32                = flags;
    data.cb.type                        = VMMDevHGCMParmType_32bit;
    data.cb.u.value32                   = *pcbBuffer;
    data.info.type                      = VMMDevHGCMParmType_LinAddr;
    data.info.u.Pointer.size            = *pcbBuffer;
    data.info.u.Pointer.u.linearAddr    = (VBOXGCPTR)pBuffer;

    rc = VbglHGCMCall (pClient->handle, &data.callInfo, sizeof (data));

/*    Log(("VBOXSF: VBoxSF::vboxCallFileInfo: "
         "VbglHGCMCall rc = %#x, result = %#x\n", rc, data.callInfo.result));
*/
    if (VBOX_SUCCESS (rc))
    {
        rc = data.callInfo.result;
        *pcbBuffer = data.cb.u.value32;
    }
    return rc;
}

DECLVBGL(int) vboxCallLock(PVBSFCLIENT pClient, PVBSFMAP pMap, SHFLHANDLE hFile,
                           uint64_t offset, uint64_t cbSize, uint32_t fLock)
{
    int rc = VINF_SUCCESS;

    VBoxSFLock data;

    VBOX_INIT_CALL(&data.callInfo, LOCK, pClient);

    data.root.type                      = VMMDevHGCMParmType_32bit;
    data.root.u.value32                 = pMap->root;

    data.handle.type                    = VMMDevHGCMParmType_64bit;
    data.handle.u.value64               = hFile;
    data.offset.type                    = VMMDevHGCMParmType_64bit;
    data.offset.u.value64               = offset;
    data.length.type                    = VMMDevHGCMParmType_64bit;
    data.length.u.value64               = cbSize;

    data.flags.type                     = VMMDevHGCMParmType_32bit;
    data.flags.u.value32                = fLock;

    rc = VbglHGCMCall (pClient->handle, &data.callInfo, sizeof (data));

/*    Log(("VBOXSF: VBoxSF::vboxCallLock: "
         "VbglHGCMCall rc = %#x, result = %#x\n", rc, data.callInfo.result));
*/
    if (VBOX_SUCCESS (rc))
    {
        rc = data.callInfo.result;
    }
    return rc;
}

DECLVBGL(int) vboxCallSetUtf8 (PVBSFCLIENT pClient)
{
    int rc = VINF_SUCCESS;

    VBoxGuestHGCMCallInfo callInfo;

    VBOX_INIT_CALL (&callInfo, SET_UTF8, pClient);
    rc = VbglHGCMCall (pClient->handle, &callInfo, sizeof (callInfo));
    if (VBOX_SUCCESS (rc))
    {
        rc = callInfo.result;
    }
    return rc;
}

#endif /* !VBGL_VBOXGUEST */
