/* $Id$ */
/** @file
 * VirtualBox Windows Guest Shared Folders FSD - Information Querying & Setting Routines.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "vbsf.h"
#include <iprt/err.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Macro for copying a SHFLSTRING file name into a FILE_DIRECTORY_INFORMATION structure. */
#define INIT_FILE_NAME(obj, str) \
    do { \
        ULONG cbLength = (str).u16Length; \
        (obj)->FileNameLength = cbLength; \
        RtlCopyMemory((obj)->FileName, &(str).String.ucs2[0], cbLength + 2); \
    } while (0)


NTSTATUS VBoxMRxQueryDirectory(IN OUT PRX_CONTEXT RxContext)
{
    NTSTATUS Status = STATUS_SUCCESS;

    RxCaptureFobx;
    RxCaptureFcb;

    PMRX_VBOX_NETROOT_EXTENSION pNetRootExtension = VBoxMRxGetNetRootExtension(capFcb->pNetRoot);
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(capFobx);

    PUNICODE_STRING DirectoryName = GET_ALREADY_PREFIXED_NAME_FROM_CONTEXT(RxContext);
    PUNICODE_STRING Template = &capFobx->UnicodeQueryTemplate;
    FILE_INFORMATION_CLASS FileInformationClass = RxContext->Info.FileInformationClass;
    PCHAR pInfoBuffer = (PCHAR)RxContext->Info.Buffer;
    LONG cbMaxSize = RxContext->Info.Length;
    LONG *pLengthRemaining = (LONG *)&RxContext->Info.LengthRemaining;

    LONG cbToCopy;
    int vrc;
    uint8_t *pHGCMBuffer;
    uint32_t index, fSFFlags, cFiles, u32BufSize;
    LONG cbHGCMBuffer;
    PSHFLDIRINFO pDirEntry;

    ULONG *pNextOffset = 0;
    PSHFLSTRING ParsedPath = NULL;

    Log(("VBOXSF: MrxQueryDirectory: FileInformationClass %d, pVBoxFobx %p, hFile %RX64, pInfoBuffer %p\n",
         FileInformationClass, pVBoxFobx, pVBoxFobx->hFile, pInfoBuffer));

    if (!pVBoxFobx)
    {
        Log(("VBOXSF: MrxQueryDirectory: pVBoxFobx is invalid!\n"));
        return STATUS_INVALID_PARAMETER;
    }

    if (!DirectoryName)
        return STATUS_INVALID_PARAMETER;

    if (DirectoryName->Length == 0)
        Log(("VBOXSF: MrxQueryDirectory: DirectoryName = \\ (null string)\n"));
    else
        Log(("VBOXSF: MrxQueryDirectory: DirectoryName = %.*ls\n",
             DirectoryName->Length / sizeof(WCHAR), DirectoryName->Buffer));

    if (!Template)
        return STATUS_INVALID_PARAMETER;

    if (Template->Length == 0)
        Log(("VBOXSF: MrxQueryDirectory: Template = \\ (null string)\n"));
    else
        Log(("VBOXSF: MrxQueryDirectory: Template = %.*ls\n",
             Template->Length / sizeof(WCHAR), Template->Buffer));

    cbHGCMBuffer = RT_MAX(cbMaxSize, PAGE_SIZE);

    Log(("VBOXSF: MrxQueryDirectory: Allocating cbHGCMBuffer = %d\n",
         cbHGCMBuffer));

    pHGCMBuffer = (uint8_t *)vbsfNtAllocNonPagedMem(cbHGCMBuffer);
    if (!pHGCMBuffer)
    {
        AssertFailed();
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* Assume start from the beginning. */
    index = 0;
    if (RxContext->QueryDirectory.IndexSpecified == TRUE)
    {
        Log(("VBOXSF: MrxQueryDirectory: Index specified %d\n",
             index));
        index = RxContext->QueryDirectory.FileIndex;
    }

    fSFFlags = SHFL_LIST_NONE;
    if (RxContext->QueryDirectory.ReturnSingleEntry == TRUE)
    {
        Log(("VBOXSF: MrxQueryDirectory: Query single entry\n"));
        fSFFlags |= SHFL_LIST_RETURN_ONE;
    }
    if (   RxContext->QueryDirectory.RestartScan == TRUE
        && RxContext->QueryDirectory.InitialQuery == FALSE)
    {
        Log(("VBOXSF: MrxQueryDirectory: Restart scan\n"));
        fSFFlags |= SHFL_LIST_RESTART;
    }

    if (Template->Length)
    {
        ULONG ParsedPathSize, cch;

        /* Calculate size required for parsed path: dir + \ + template + 0. */
        ParsedPathSize = SHFLSTRING_HEADER_SIZE + Template->Length + sizeof(WCHAR);
        if (DirectoryName->Length)
            ParsedPathSize += DirectoryName->Length + sizeof(WCHAR);
        Log(("VBOXSF: MrxQueryDirectory: ParsedPathSize = %d\n", ParsedPathSize));

        ParsedPath = (PSHFLSTRING)vbsfNtAllocNonPagedMem(ParsedPathSize);
        if (!ParsedPath)
        {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto end;
        }

        if (!ShflStringInitBuffer(ParsedPath, ParsedPathSize))
        {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto end;
        }

        cch = 0;
        if (DirectoryName->Length)
        {
            /* Copy directory name into ParsedPath. */
            RtlCopyMemory(ParsedPath->String.ucs2, DirectoryName->Buffer, DirectoryName->Length);
            cch += DirectoryName->Length / sizeof(WCHAR);

            /* Add terminating backslash. */
            ParsedPath->String.ucs2[cch] = L'\\';
            cch++;
        }

        RtlCopyMemory (&ParsedPath->String.ucs2[cch], Template->Buffer, Template->Length);
        cch += Template->Length / sizeof(WCHAR);

        /* Add terminating nul. */
        ParsedPath->String.ucs2[cch] = 0;

        /* cch is the number of chars without trailing nul. */
        ParsedPath->u16Length = (uint16_t)(cch * sizeof(WCHAR));

        AssertMsg(ParsedPath->u16Length + sizeof(WCHAR) == ParsedPath->u16Size,
                  ("u16Length %d, u16Size %d\n", ParsedPath->u16Length, ParsedPath->u16Size));

        Log(("VBOXSF: MrxQueryDirectory: ParsedPath = %.*ls\n",
             ParsedPath->u16Length / sizeof(WCHAR), ParsedPath->String.ucs2));
    }

    cFiles = 0;

    /* VbglR0SfDirInfo requires a pointer to uint32_t. */
    u32BufSize = cbHGCMBuffer;

    Log(("VBOXSF: MrxQueryDirectory: CallDirInfo: File = 0x%08x, Flags = 0x%08x, Index = %d, u32BufSize = %d\n",
         pVBoxFobx->hFile, fSFFlags, index, u32BufSize));
    vrc = VbglR0SfDirInfo(&g_SfClient, &pNetRootExtension->map, pVBoxFobx->hFile,
                          ParsedPath, fSFFlags, index, &u32BufSize, (PSHFLDIRINFO)pHGCMBuffer, &cFiles);
    Log(("VBOXSF: MrxQueryDirectory: u32BufSize after CallDirInfo = %d, rc = %Rrc\n",
         u32BufSize, vrc));

    switch (vrc)
    {
        case VINF_SUCCESS:
            /* Nothing to do here. */
            break;

        case VERR_NO_TRANSLATION:
            Log(("VBOXSF: MrxQueryDirectory: Host could not translate entry!\n"));
            break;

        case VERR_NO_MORE_FILES:
            if (cFiles <= 0) /* VERR_NO_MORE_FILES appears at the first lookup when just returning the current dir ".".
                              * So we also have to check for the cFiles counter. */
            {
                /* Not an error, but we have to handle the return value. */
                Log(("VBOXSF: MrxQueryDirectory: Host reported no more files!\n"));

                if (RxContext->QueryDirectory.InitialQuery)
                {
                    /* First call. MSDN on FindFirstFile: "If the function fails because no matching files
                     * can be found, the GetLastError function returns ERROR_FILE_NOT_FOUND."
                     * So map this rc to file not found.
                     */
                    Status = STATUS_NO_SUCH_FILE;
                }
                else
                {
                    /* Search continued. */
                    Status = STATUS_NO_MORE_FILES;
                }
            }
            break;

        case VERR_FILE_NOT_FOUND:
            Status = STATUS_NO_SUCH_FILE;
            Log(("VBOXSF: MrxQueryDirectory: no such file!\n"));
            break;

        default:
            Status = vbsfNtVBoxStatusToNt(vrc);
            Log(("VBOXSF: MrxQueryDirectory: Error %Rrc from CallDirInfo (cFiles=%d)!\n",
                 vrc, cFiles));
            break;
    }

    if (Status != STATUS_SUCCESS)
        goto end;

    /* Verify that the returned buffer length is not greater than the original one. */
    if (u32BufSize > (uint32_t)cbHGCMBuffer)
    {
        Log(("VBOXSF: MrxQueryDirectory: returned buffer size (%u) is invalid!!!\n",
             u32BufSize));
        Status = STATUS_INVALID_NETWORK_RESPONSE;
        goto end;
    }

    /* How many bytes remain in the buffer. */
    cbHGCMBuffer = u32BufSize;

    pDirEntry = (PSHFLDIRINFO)pHGCMBuffer;
    Status = STATUS_SUCCESS;

    Log(("VBOXSF: MrxQueryDirectory: cFiles=%d, Length=%d\n",
         cFiles, cbHGCMBuffer));

    while ((*pLengthRemaining) && (cFiles > 0) && (pDirEntry != NULL))
    {
        int cbEntry = RT_UOFFSETOF(SHFLDIRINFO, name.String) + pDirEntry->name.u16Size;

        if (cbEntry > cbHGCMBuffer)
        {
            Log(("VBOXSF: MrxQueryDirectory: Entry size (%d) exceeds the buffer size (%d)!!!\n",
                 cbEntry, cbHGCMBuffer));
            Status = STATUS_INVALID_NETWORK_RESPONSE;
            goto end;
        }

        switch (FileInformationClass)
        {
            case FileDirectoryInformation:
            {
                PFILE_DIRECTORY_INFORMATION pInfo = (PFILE_DIRECTORY_INFORMATION)pInfoBuffer;
                Log(("VBOXSF: MrxQueryDirectory: FileDirectoryInformation\n"));

                cbToCopy = sizeof(FILE_DIRECTORY_INFORMATION);
                /* Struct already contains one char for null terminator. */
                cbToCopy += pDirEntry->name.u16Size;

                if (*pLengthRemaining >= cbToCopy)
                {
                    RtlZeroMemory(pInfo, cbToCopy);

                    pInfo->CreationTime.QuadPart   = RTTimeSpecGetNtTime(&pDirEntry->Info.BirthTime); /* ridiculous name */
                    pInfo->LastAccessTime.QuadPart = RTTimeSpecGetNtTime(&pDirEntry->Info.AccessTime);
                    pInfo->LastWriteTime.QuadPart  = RTTimeSpecGetNtTime(&pDirEntry->Info.ModificationTime);
                    pInfo->ChangeTime.QuadPart     = RTTimeSpecGetNtTime(&pDirEntry->Info.ChangeTime);
                    pInfo->AllocationSize.QuadPart = pDirEntry->Info.cbAllocated;
                    pInfo->EndOfFile.QuadPart      = pDirEntry->Info.cbObject;
                    pInfo->FileIndex               = index;
                    pInfo->FileAttributes          = VBoxToNTFileAttributes(pDirEntry->Info.Attr.fMode);

                    INIT_FILE_NAME(pInfo, pDirEntry->name);

                    /* Align to 8 byte boundary */
                    cbToCopy = RT_ALIGN(cbToCopy, sizeof(LONGLONG));
                    pInfo->NextEntryOffset = cbToCopy;
                    pNextOffset = &pInfo->NextEntryOffset;
                }
                else
                {
                    pInfo->NextEntryOffset = 0; /* last item */
                    Status = STATUS_BUFFER_OVERFLOW;
                }
                break;
            }

            case FileFullDirectoryInformation:
            {
                PFILE_FULL_DIR_INFORMATION pInfo = (PFILE_FULL_DIR_INFORMATION)pInfoBuffer;
                Log(("VBOXSF: MrxQueryDirectory: FileFullDirectoryInformation\n"));

                cbToCopy = sizeof(FILE_FULL_DIR_INFORMATION);
                /* Struct already contains one char for null terminator. */
                cbToCopy += pDirEntry->name.u16Size;

                if (*pLengthRemaining >= cbToCopy)
                {
                    RtlZeroMemory(pInfo, cbToCopy);

                    pInfo->CreationTime.QuadPart   = RTTimeSpecGetNtTime(&pDirEntry->Info.BirthTime); /* ridiculous name */
                    pInfo->LastAccessTime.QuadPart = RTTimeSpecGetNtTime(&pDirEntry->Info.AccessTime);
                    pInfo->LastWriteTime.QuadPart  = RTTimeSpecGetNtTime(&pDirEntry->Info.ModificationTime);
                    pInfo->ChangeTime.QuadPart     = RTTimeSpecGetNtTime(&pDirEntry->Info.ChangeTime);
                    pInfo->AllocationSize.QuadPart = pDirEntry->Info.cbAllocated;
                    pInfo->EndOfFile.QuadPart      = pDirEntry->Info.cbObject;
                    pInfo->EaSize                  = 0;
                    pInfo->FileIndex               = index;
                    pInfo->FileAttributes          = VBoxToNTFileAttributes(pDirEntry->Info.Attr.fMode);

                    INIT_FILE_NAME(pInfo, pDirEntry->name);

                    /* Align to 8 byte boundary */
                    cbToCopy = RT_ALIGN(cbToCopy, sizeof(LONGLONG));
                    pInfo->NextEntryOffset = cbToCopy;
                    pNextOffset = &pInfo->NextEntryOffset;
                }
                else
                {
                    pInfo->NextEntryOffset = 0; /* last item */
                    Status = STATUS_BUFFER_OVERFLOW;
                }
                break;
            }

            case FileBothDirectoryInformation:
            {
                PFILE_BOTH_DIR_INFORMATION pInfo = (PFILE_BOTH_DIR_INFORMATION)pInfoBuffer;
                Log(("VBOXSF: MrxQueryDirectory: FileBothDirectoryInformation\n"));

                cbToCopy = sizeof(FILE_BOTH_DIR_INFORMATION);
                /* struct already contains one char for null terminator */
                cbToCopy += pDirEntry->name.u16Size;

                if (*pLengthRemaining >= cbToCopy)
                {
                    RtlZeroMemory(pInfo, cbToCopy);

                    pInfo->CreationTime.QuadPart   = RTTimeSpecGetNtTime(&pDirEntry->Info.BirthTime); /* ridiculous name */
                    pInfo->LastAccessTime.QuadPart = RTTimeSpecGetNtTime(&pDirEntry->Info.AccessTime);
                    pInfo->LastWriteTime.QuadPart  = RTTimeSpecGetNtTime(&pDirEntry->Info.ModificationTime);
                    pInfo->ChangeTime.QuadPart     = RTTimeSpecGetNtTime(&pDirEntry->Info.ChangeTime);
                    pInfo->AllocationSize.QuadPart = pDirEntry->Info.cbAllocated;
                    pInfo->EndOfFile.QuadPart      = pDirEntry->Info.cbObject;
                    pInfo->EaSize                  = 0;
                    pInfo->ShortNameLength         = 0; /** @todo ? */
                    pInfo->FileIndex               = index;
                    pInfo->FileAttributes          = VBoxToNTFileAttributes(pDirEntry->Info.Attr.fMode);

                    INIT_FILE_NAME(pInfo, pDirEntry->name);

                    Log(("VBOXSF: MrxQueryDirectory: FileBothDirectoryInformation cbAlloc = %x cbObject = %x\n",
                         pDirEntry->Info.cbAllocated, pDirEntry->Info.cbObject));
                    Log(("VBOXSF: MrxQueryDirectory: FileBothDirectoryInformation cbToCopy = %d, name size=%d name len=%d\n",
                         cbToCopy, pDirEntry->name.u16Size, pDirEntry->name.u16Length));
                    Log(("VBOXSF: MrxQueryDirectory: FileBothDirectoryInformation File name %.*ls (DirInfo)\n",
                         pInfo->FileNameLength / sizeof(WCHAR), pInfo->FileName));
                    Log(("VBOXSF: MrxQueryDirectory: FileBothDirectoryInformation File name %.*ls (DirEntry)\n",
                         pDirEntry->name.u16Size / sizeof(WCHAR), pDirEntry->name.String.ucs2));

                    /* Align to 8 byte boundary. */
                    cbToCopy = RT_ALIGN(cbToCopy, sizeof(LONGLONG));
                    pInfo->NextEntryOffset = cbToCopy;
                    pNextOffset = &pInfo->NextEntryOffset;
                }
                else
                {
                    pInfo->NextEntryOffset = 0; /* Last item. */
                    Status = STATUS_BUFFER_OVERFLOW;
                }
                break;
            }

            case FileIdBothDirectoryInformation:
            {
                PFILE_ID_BOTH_DIR_INFORMATION pInfo = (PFILE_ID_BOTH_DIR_INFORMATION)pInfoBuffer;
                Log(("VBOXSF: MrxQueryDirectory: FileIdBothDirectoryInformation\n"));

                cbToCopy = sizeof(FILE_ID_BOTH_DIR_INFORMATION);
                /* struct already contains one char for null terminator */
                cbToCopy += pDirEntry->name.u16Size;

                if (*pLengthRemaining >= cbToCopy)
                {
                    RtlZeroMemory(pInfo, cbToCopy);

                    pInfo->CreationTime.QuadPart   = RTTimeSpecGetNtTime(&pDirEntry->Info.BirthTime); /* ridiculous name */
                    pInfo->LastAccessTime.QuadPart = RTTimeSpecGetNtTime(&pDirEntry->Info.AccessTime);
                    pInfo->LastWriteTime.QuadPart  = RTTimeSpecGetNtTime(&pDirEntry->Info.ModificationTime);
                    pInfo->ChangeTime.QuadPart     = RTTimeSpecGetNtTime(&pDirEntry->Info.ChangeTime);
                    pInfo->AllocationSize.QuadPart = pDirEntry->Info.cbAllocated;
                    pInfo->EndOfFile.QuadPart      = pDirEntry->Info.cbObject;
                    pInfo->EaSize                  = 0;
                    pInfo->ShortNameLength         = 0; /** @todo ? */
                    pInfo->EaSize                  = 0;
                    pInfo->FileId.QuadPart         = 0;
                    pInfo->FileAttributes          = VBoxToNTFileAttributes(pDirEntry->Info.Attr.fMode);

                    INIT_FILE_NAME(pInfo, pDirEntry->name);

                    Log(("VBOXSF: MrxQueryDirectory: FileIdBothDirectoryInformation cbAlloc = 0x%RX64 cbObject = 0x%RX64\n",
                         pDirEntry->Info.cbAllocated, pDirEntry->Info.cbObject));
                    Log(("VBOXSF: MrxQueryDirectory: FileIdBothDirectoryInformation cbToCopy = %d, name size=%d name len=%d\n",
                         cbToCopy, pDirEntry->name.u16Size, pDirEntry->name.u16Length));
                    Log(("VBOXSF: MrxQueryDirectory: FileIdBothDirectoryInformation File name %.*ls (DirInfo)\n",
                         pInfo->FileNameLength / sizeof(WCHAR), pInfo->FileName));
                    Log(("VBOXSF: MrxQueryDirectory: FileIdBothDirectoryInformation File name %.*ls (DirEntry)\n",
                         pDirEntry->name.u16Size / sizeof(WCHAR), pDirEntry->name.String.ucs2));

                    /* Align to 8 byte boundary. */
                    cbToCopy = RT_ALIGN(cbToCopy, sizeof(LONGLONG));
                    pInfo->NextEntryOffset = cbToCopy;
                    pNextOffset = &pInfo->NextEntryOffset;
                }
                else
                {
                    pInfo->NextEntryOffset = 0; /* Last item. */
                    Status = STATUS_BUFFER_OVERFLOW;
                }
                break;
            }

            case FileNamesInformation:
            {
                PFILE_NAMES_INFORMATION pInfo = (PFILE_NAMES_INFORMATION)pInfoBuffer;
                Log(("VBOXSF: MrxQueryDirectory: FileNamesInformation\n"));

                cbToCopy = sizeof(FILE_NAMES_INFORMATION);
                /* Struct already contains one char for null terminator. */
                cbToCopy += pDirEntry->name.u16Size;

                if (*pLengthRemaining >= cbToCopy)
                {
                    RtlZeroMemory(pInfo, cbToCopy);

                    pInfo->FileIndex = index;

                    INIT_FILE_NAME(pInfo, pDirEntry->name);

                    Log(("VBOXSF: MrxQueryDirectory: FileNamesInformation: File name [%.*ls]\n",
                         pInfo->FileNameLength / sizeof(WCHAR), pInfo->FileName));

                    /* Align to 8 byte boundary. */
                    cbToCopy = RT_ALIGN(cbToCopy, sizeof(LONGLONG));
                    pInfo->NextEntryOffset = cbToCopy;
                    pNextOffset = &pInfo->NextEntryOffset;
                }
                else
                {
                    pInfo->NextEntryOffset = 0; /* Last item. */
                    Status = STATUS_BUFFER_OVERFLOW;
                }
                break;
            }

            default:
                Log(("VBOXSF: MrxQueryDirectory: Not supported FileInformationClass %d!\n",
                     FileInformationClass));
                Status = STATUS_INVALID_PARAMETER;
                goto end;
        }

        cbHGCMBuffer -= cbEntry;
        pDirEntry = (PSHFLDIRINFO)((uintptr_t)pDirEntry + cbEntry);

        Log(("VBOXSF: MrxQueryDirectory: %d bytes left in HGCM buffer\n",
             cbHGCMBuffer));

        if (*pLengthRemaining >= cbToCopy)
        {
            pInfoBuffer += cbToCopy;
            *pLengthRemaining -= cbToCopy;
        }
        else
            break;

        if (RxContext->QueryDirectory.ReturnSingleEntry)
            break;

        /* More left? */
        if (cbHGCMBuffer <= 0)
            break;

        index++; /* File Index. */

        cFiles--;
    }

    if (pNextOffset)
        *pNextOffset = 0; /* Last pInfo->NextEntryOffset should be set to zero! */

end:
    if (pHGCMBuffer)
        vbsfNtFreeNonPagedMem(pHGCMBuffer);

    if (ParsedPath)
        vbsfNtFreeNonPagedMem(ParsedPath);

    Log(("VBOXSF: MrxQueryDirectory: Returned 0x%08X\n",
         Status));
    return Status;
}

NTSTATUS VBoxMRxQueryVolumeInfo(IN OUT PRX_CONTEXT RxContext)
{
    NTSTATUS Status;

    RxCaptureFcb;
    RxCaptureFobx;

    PMRX_VBOX_NETROOT_EXTENSION pNetRootExtension = VBoxMRxGetNetRootExtension(capFcb->pNetRoot);
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(capFobx);

    FS_INFORMATION_CLASS FsInformationClass = RxContext->Info.FsInformationClass;
    PVOID pInfoBuffer = RxContext->Info.Buffer;
    ULONG cbInfoBuffer = RxContext->Info.LengthRemaining;
    ULONG cbToCopy = 0;
    ULONG cbString = 0;

    Log(("VBOXSF: MrxQueryVolumeInfo: pInfoBuffer = %p, cbInfoBuffer = %d\n",
         RxContext->Info.Buffer, RxContext->Info.LengthRemaining));
    Log(("VBOXSF: MrxQueryVolumeInfo: vboxFobx = %p, Handle = 0x%RX64\n",
         pVBoxFobx, pVBoxFobx? pVBoxFobx->hFile: 0));

    Status = STATUS_INVALID_PARAMETER;

    switch (FsInformationClass)
    {
        case FileFsVolumeInformation:
        {
            PFILE_FS_VOLUME_INFORMATION pInfo = (PFILE_FS_VOLUME_INFORMATION)pInfoBuffer;

            PMRX_NET_ROOT pNetRoot = capFcb->pNetRoot;
            PMRX_SRV_CALL pSrvCall = pNetRoot->pSrvCall;

            PWCHAR pRootName;
            ULONG cbRootName;

            PSHFLVOLINFO pShflVolInfo;
            uint32_t cbHGCMBuffer;
            uint8_t *pHGCMBuffer;
            int vrc;

            Log(("VBOXSF: MrxQueryVolumeInfo: FileFsVolumeInformation\n"));

            if (!pVBoxFobx)
            {
                Log(("VBOXSF: MrxQueryVolumeInfo: pVBoxFobx is NULL!\n"));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            cbRootName = pNetRoot->pNetRootName->Length - pSrvCall->pSrvCallName->Length;
            cbRootName -= sizeof(WCHAR); /* Remove the leading backslash. */
            pRootName = pNetRoot->pNetRootName->Buffer + (pSrvCall->pSrvCallName->Length / sizeof(WCHAR));
            pRootName++; /* Remove the leading backslash. */

            Log(("VBOXSF: MrxQueryVolumeInfo: FileFsVolumeInformation: Root name = %.*ls, %d bytes\n",
                 cbRootName / sizeof(WCHAR), pRootName, cbRootName));

            cbToCopy = FIELD_OFFSET(FILE_FS_VOLUME_INFORMATION, VolumeLabel);

            cbString  = VBOX_VOLNAME_PREFIX_SIZE;
            cbString += cbRootName;
            cbString += sizeof(WCHAR);

            Log(("VBOXSF: MrxQueryVolumeInfo: FileFsVolumeInformation: cbToCopy %d, cbString %d\n",
                 cbToCopy, cbString));

            if (cbInfoBuffer < cbToCopy)
            {
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            RtlZeroMemory(pInfo, cbToCopy);

            /* Query serial number. */
            cbHGCMBuffer = sizeof(SHFLVOLINFO);
            pHGCMBuffer = (uint8_t *)vbsfNtAllocNonPagedMem(cbHGCMBuffer);
            if (!pHGCMBuffer)
            {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            vrc = VbglR0SfFsInfo(&g_SfClient, &pNetRootExtension->map, pVBoxFobx->hFile,
                                 SHFL_INFO_GET | SHFL_INFO_VOLUME, &cbHGCMBuffer, (PSHFLDIRINFO)pHGCMBuffer);

            if (vrc != VINF_SUCCESS)
            {
                Status = vbsfNtVBoxStatusToNt(vrc);
                vbsfNtFreeNonPagedMem(pHGCMBuffer);
                break;
            }

            pShflVolInfo = (PSHFLVOLINFO)pHGCMBuffer;
            pInfo->VolumeSerialNumber = pShflVolInfo->ulSerial;
            vbsfNtFreeNonPagedMem(pHGCMBuffer);

            pInfo->VolumeCreationTime.QuadPart = 0;
            pInfo->SupportsObjects = FALSE;

            if (cbInfoBuffer >= cbToCopy + cbString)
            {
                RtlCopyMemory(&pInfo->VolumeLabel[0],
                              VBOX_VOLNAME_PREFIX,
                              VBOX_VOLNAME_PREFIX_SIZE);
                RtlCopyMemory(&pInfo->VolumeLabel[VBOX_VOLNAME_PREFIX_SIZE / sizeof(WCHAR)],
                              pRootName,
                              cbRootName);
                pInfo->VolumeLabel[cbString / sizeof(WCHAR) -  1] = 0;
            }
            else
            {
                cbString = cbInfoBuffer - cbToCopy;

                RtlCopyMemory(&pInfo->VolumeLabel[0],
                              VBOX_VOLNAME_PREFIX,
                              RT_MIN(cbString, VBOX_VOLNAME_PREFIX_SIZE));
                if (cbString > VBOX_VOLNAME_PREFIX_SIZE)
                {
                    RtlCopyMemory(&pInfo->VolumeLabel[VBOX_VOLNAME_PREFIX_SIZE / sizeof(WCHAR)],
                                  pRootName,
                                  cbString - VBOX_VOLNAME_PREFIX_SIZE);
                }
            }

            pInfo->VolumeLabelLength = cbString;

            cbToCopy += cbString;

            Log(("VBOXSF: MrxQueryVolumeInfo: FileFsVolumeInformation: VolumeLabelLength %d\n",
                 pInfo->VolumeLabelLength));

            Status = STATUS_SUCCESS;
            break;
        }

        case FileFsLabelInformation:
        {
            PFILE_FS_LABEL_INFORMATION pInfo = (PFILE_FS_LABEL_INFORMATION)pInfoBuffer;

            PMRX_NET_ROOT pNetRoot = capFcb->pNetRoot;
            PMRX_SRV_CALL pSrvCall = pNetRoot->pSrvCall;

            PWCHAR pRootName;
            ULONG cbRootName;

            Log(("VBOXSF: MrxQueryVolumeInfo: FileFsLabelInformation\n"));

            cbRootName = pNetRoot->pNetRootName->Length - pSrvCall->pSrvCallName->Length;
            cbRootName -= sizeof(WCHAR); /* Remove the leading backslash. */
            pRootName = pNetRoot->pNetRootName->Buffer + (pSrvCall->pSrvCallName->Length / sizeof(WCHAR));
            pRootName++; /* Remove the leading backslash. */

            Log(("VBOXSF: MrxQueryVolumeInfo: FileFsLabelInformation: Root name = %.*ls, %d bytes\n",
                 cbRootName / sizeof(WCHAR), pRootName, cbRootName));

            cbToCopy = FIELD_OFFSET(FILE_FS_LABEL_INFORMATION, VolumeLabel);

            cbString  = VBOX_VOLNAME_PREFIX_SIZE;
            cbString += cbRootName;
            cbString += sizeof(WCHAR);

            if (cbInfoBuffer < cbToCopy)
            {
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            RtlZeroMemory(pInfo, cbToCopy);

            if (cbInfoBuffer >= cbToCopy + cbString)
            {
                RtlCopyMemory(&pInfo->VolumeLabel[0],
                              VBOX_VOLNAME_PREFIX,
                              VBOX_VOLNAME_PREFIX_SIZE);
                RtlCopyMemory(&pInfo->VolumeLabel[VBOX_VOLNAME_PREFIX_SIZE / sizeof(WCHAR)],
                              pRootName,
                              cbRootName);
                pInfo->VolumeLabel[cbString / sizeof(WCHAR) -  1] = 0;
            }
            else
            {
                cbString = cbInfoBuffer - cbToCopy;

                RtlCopyMemory(&pInfo->VolumeLabel[0],
                              VBOX_VOLNAME_PREFIX,
                              RT_MIN(cbString, VBOX_VOLNAME_PREFIX_SIZE));
                if (cbString > VBOX_VOLNAME_PREFIX_SIZE)
                {
                    RtlCopyMemory(&pInfo->VolumeLabel[VBOX_VOLNAME_PREFIX_SIZE / sizeof(WCHAR)],
                                  pRootName,
                                  cbString - VBOX_VOLNAME_PREFIX_SIZE);
                }
            }

            pInfo->VolumeLabelLength = cbString;

            cbToCopy += cbString;

            Log(("VBOXSF: MrxQueryVolumeInfo: FileFsLabelInformation: VolumeLabelLength %d\n",
                 pInfo->VolumeLabelLength));

            Status = STATUS_SUCCESS;
            break;
        }

        case FileFsFullSizeInformation:
        case FileFsSizeInformation:
        {
            PFILE_FS_FULL_SIZE_INFORMATION pFullSizeInfo = (PFILE_FS_FULL_SIZE_INFORMATION)pInfoBuffer;
            PFILE_FS_SIZE_INFORMATION pSizeInfo = (PFILE_FS_SIZE_INFORMATION)pInfoBuffer;

            uint32_t cbHGCMBuffer;
            uint8_t *pHGCMBuffer;
            int vrc;
            PSHFLVOLINFO pShflVolInfo;

            LARGE_INTEGER TotalAllocationUnits;
            LARGE_INTEGER AvailableAllocationUnits;
            ULONG         SectorsPerAllocationUnit;
            ULONG         BytesPerSector;

            if (FsInformationClass == FileFsFullSizeInformation)
            {
                Log(("VBOXSF: MrxQueryVolumeInfo: FileFsFullSizeInformation\n"));
                cbToCopy = sizeof(FILE_FS_FULL_SIZE_INFORMATION);
            }
            else
            {
                Log(("VBOXSF: MrxQueryVolumeInfo: FileFsSizeInformation\n"));
                cbToCopy = sizeof(FILE_FS_SIZE_INFORMATION);
            }

            if (!pVBoxFobx)
            {
                Log(("VBOXSF: MrxQueryVolumeInfo: pVBoxFobx is NULL!\n"));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            if (cbInfoBuffer < cbToCopy)
            {
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            RtlZeroMemory(pInfoBuffer, cbToCopy);

            cbHGCMBuffer = sizeof(SHFLVOLINFO);
            pHGCMBuffer = (uint8_t *)vbsfNtAllocNonPagedMem(cbHGCMBuffer);
            if (!pHGCMBuffer)
            {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            vrc = VbglR0SfFsInfo(&g_SfClient, &pNetRootExtension->map, pVBoxFobx->hFile,
                                 SHFL_INFO_GET | SHFL_INFO_VOLUME, &cbHGCMBuffer, (PSHFLDIRINFO)pHGCMBuffer);

            if (vrc != VINF_SUCCESS)
            {
                Status = vbsfNtVBoxStatusToNt(vrc);
                vbsfNtFreeNonPagedMem(pHGCMBuffer);
                break;
            }

            pShflVolInfo = (PSHFLVOLINFO)pHGCMBuffer;

            TotalAllocationUnits.QuadPart     = pShflVolInfo->ullTotalAllocationBytes / pShflVolInfo->ulBytesPerAllocationUnit;
            AvailableAllocationUnits.QuadPart = pShflVolInfo->ullAvailableAllocationBytes / pShflVolInfo->ulBytesPerAllocationUnit;
            SectorsPerAllocationUnit          = pShflVolInfo->ulBytesPerAllocationUnit / pShflVolInfo->ulBytesPerSector;
            BytesPerSector                    = pShflVolInfo->ulBytesPerSector;

            Log(("VBOXSF: MrxQueryVolumeInfo: TotalAllocationUnits     0x%RX64\n", TotalAllocationUnits.QuadPart));
            Log(("VBOXSF: MrxQueryVolumeInfo: AvailableAllocationUnits 0x%RX64\n", AvailableAllocationUnits.QuadPart));
            Log(("VBOXSF: MrxQueryVolumeInfo: SectorsPerAllocationUnit 0x%X\n", SectorsPerAllocationUnit));
            Log(("VBOXSF: MrxQueryVolumeInfo: BytesPerSector           0x%X\n", BytesPerSector));

            if (FsInformationClass == FileFsFullSizeInformation)
            {
                pFullSizeInfo->TotalAllocationUnits           = TotalAllocationUnits;
                pFullSizeInfo->CallerAvailableAllocationUnits = AvailableAllocationUnits;
                pFullSizeInfo->ActualAvailableAllocationUnits = AvailableAllocationUnits;
                pFullSizeInfo->SectorsPerAllocationUnit       = SectorsPerAllocationUnit;
                pFullSizeInfo->BytesPerSector                 = BytesPerSector;
            }
            else
            {
                pSizeInfo->TotalAllocationUnits     = TotalAllocationUnits;
                pSizeInfo->AvailableAllocationUnits = AvailableAllocationUnits;
                pSizeInfo->SectorsPerAllocationUnit = SectorsPerAllocationUnit;
                pSizeInfo->BytesPerSector           = BytesPerSector;
            }

            vbsfNtFreeNonPagedMem(pHGCMBuffer);

            Status = STATUS_SUCCESS;
            break;
        }

        case FileFsDeviceInformation:
        {
            PFILE_FS_DEVICE_INFORMATION pInfo = (PFILE_FS_DEVICE_INFORMATION)pInfoBuffer;
            PMRX_NET_ROOT NetRoot = capFcb->pNetRoot;

            Log(("VBOXSF: MrxQueryVolumeInfo: FileFsDeviceInformation: Type = %d\n",
                 NetRoot->DeviceType));

            cbToCopy = sizeof(FILE_FS_DEVICE_INFORMATION);

            if (cbInfoBuffer < cbToCopy)
            {
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            pInfo->DeviceType = NetRoot->DeviceType;
            pInfo->Characteristics = FILE_REMOTE_DEVICE;

            Status = STATUS_SUCCESS;
            break;
        }

        case FileFsAttributeInformation:
        {
            PFILE_FS_ATTRIBUTE_INFORMATION pInfo = (PFILE_FS_ATTRIBUTE_INFORMATION)pInfoBuffer;

            Log(("VBOXSF: MrxQueryVolumeInfo: FileFsAttributeInformation\n"));

            cbToCopy = FIELD_OFFSET(FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName);

            cbString = sizeof(MRX_VBOX_FILESYS_NAME_U);

            if (cbInfoBuffer < cbToCopy)
            {
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            pInfo->FileSystemAttributes = 0; /** @todo set unicode, case sensitive etc? */
            pInfo->MaximumComponentNameLength = 255; /** @todo should query from the host */

            if (cbInfoBuffer >= cbToCopy + cbString)
            {
                RtlCopyMemory(pInfo->FileSystemName,
                              MRX_VBOX_FILESYS_NAME_U,
                              sizeof(MRX_VBOX_FILESYS_NAME_U));
            }
            else
            {
                cbString = cbInfoBuffer - cbToCopy;

                RtlCopyMemory(pInfo->FileSystemName,
                              MRX_VBOX_FILESYS_NAME_U,
                              RT_MIN(cbString, sizeof(MRX_VBOX_FILESYS_NAME_U)));
            }

            pInfo->FileSystemNameLength = cbString;

            cbToCopy += cbString;

            Log(("VBOXSF: MrxQueryVolumeInfo: FileFsAttributeInformation: FileSystemNameLength %d\n",
                 pInfo->FileSystemNameLength));

            Status = STATUS_SUCCESS;
            break;
        }

        case FileFsControlInformation:
            Log(("VBOXSF: MrxQueryVolumeInfo: FileFsControlInformation: not supported\n"));
            Status = STATUS_INVALID_PARAMETER;
            break;

        case FileFsObjectIdInformation:
            Log(("VBOXSF: MrxQueryVolumeInfo: FileFsObjectIdInformation: not supported\n"));
            Status = STATUS_INVALID_PARAMETER;
            break;

        case FileFsMaximumInformation:
            Log(("VBOXSF: MrxQueryVolumeInfo: FileFsMaximumInformation: not supported\n"));
            Status = STATUS_INVALID_PARAMETER;
            break;

        default:
            Log(("VBOXSF: MrxQueryVolumeInfo: Not supported FsInformationClass %d!\n",
                 FsInformationClass));
            Status = STATUS_INVALID_PARAMETER;
            break;
    }

    if (Status == STATUS_SUCCESS)
        RxContext->Info.LengthRemaining = cbInfoBuffer - cbToCopy;
    else if (Status == STATUS_BUFFER_TOO_SMALL)
    {
        Log(("VBOXSF: MrxQueryVolumeInfo: Insufficient buffer size %d, required %d\n",
             cbInfoBuffer, cbToCopy));
        RxContext->InformationToReturn = cbToCopy;
    }

    Log(("VBOXSF: MrxQueryVolumeInfo: cbToCopy = %d, LengthRemaining = %d, Status = 0x%08X\n",
         cbToCopy, RxContext->Info.LengthRemaining, Status));

    return Status;
}

void vbsfNtCopyInfoToLegacy(PMRX_VBOX_FOBX pVBoxFobx, PCSHFLFSOBJINFO pInfo)
{
    pVBoxFobx->FileBasicInfo.CreationTime.QuadPart   = RTTimeSpecGetNtTime(&pInfo->BirthTime);
    pVBoxFobx->FileBasicInfo.LastAccessTime.QuadPart = RTTimeSpecGetNtTime(&pInfo->AccessTime);
    pVBoxFobx->FileBasicInfo.LastWriteTime.QuadPart  = RTTimeSpecGetNtTime(&pInfo->ModificationTime);
    pVBoxFobx->FileBasicInfo.ChangeTime.QuadPart     = RTTimeSpecGetNtTime(&pInfo->ChangeTime);
    pVBoxFobx->FileBasicInfo.FileAttributes          = VBoxToNTFileAttributes(pInfo->Attr.fMode);
}

static void vbsfNtCopyInfo(PMRX_VBOX_FOBX pVBoxFobx, PSHFLFSOBJINFO pObjInfo, uint32_t fOverrides)
{
    if (pObjInfo->cbObject != pVBoxFobx->Info.cbObject)
    {
        /** @todo tell RDBSS about this? */
    }

    /* To simplify stuff, copy user timestamps to the input structure before copying. */
    if (   pVBoxFobx->fKeepCreationTime
        || pVBoxFobx->fKeepLastAccessTime
        || pVBoxFobx->fKeepLastWriteTime
        || pVBoxFobx->fKeepChangeTime)
    {
        if (pVBoxFobx->fKeepCreationTime   && !(fOverrides & VBOX_FOBX_F_INFO_CREATION_TIME))
            pObjInfo->BirthTime         = pVBoxFobx->Info.BirthTime;
        if (pVBoxFobx->fKeepLastAccessTime && !(fOverrides & VBOX_FOBX_F_INFO_LASTACCESS_TIME))
            pObjInfo->AccessTime        = pVBoxFobx->Info.AccessTime;
        if (pVBoxFobx->fKeepLastWriteTime  && !(fOverrides & VBOX_FOBX_F_INFO_LASTWRITE_TIME))
            pObjInfo->ModificationTime  = pVBoxFobx->Info.ModificationTime;
        if (pVBoxFobx->fKeepChangeTime     && !(fOverrides & VBOX_FOBX_F_INFO_CHANGE_TIME))
            pObjInfo->ChangeTime        = pVBoxFobx->Info.ChangeTime;
    }
    pVBoxFobx->Info = *pObjInfo;

    vbsfNtCopyInfoToLegacy(pVBoxFobx, pObjInfo);
}


/**
 * Handle NtQueryInformationFile and similar requests.
 */
NTSTATUS VBoxMRxQueryFileInfo(IN PRX_CONTEXT RxContext)
{
    RxCaptureFcb;
    RxCaptureFobx;
    NTSTATUS                    Status            = STATUS_SUCCESS;
    PMRX_VBOX_NETROOT_EXTENSION pNetRootExtension = VBoxMRxGetNetRootExtension(capFcb->pNetRoot);
    PMRX_VBOX_FOBX              pVBoxFobx         = VBoxMRxGetFileObjectExtension(capFobx);
    ULONG                       cbToCopy          = 0;

    Log(("VBOXSF: MrxQueryFileInfo: InfoBuffer = %p, Size = %d bytes, FileInformationClass = %d\n",
         RxContext->Info.Buffer, RxContext->Info.Length, RxContext->Info.FileInformationClass));

    AssertReturn(pVBoxFobx, STATUS_INVALID_PARAMETER);
    AssertReturn(RxContext->Info.Buffer, STATUS_INVALID_PARAMETER);

#define CHECK_SIZE_BREAK(a_RxContext, a_cbNeeded) \
        /* IO_STACK_LOCATION::Parameters::SetFile::Length is signed, the RxContext bugger is LONG. See end of function for why. */ \
        if ((ULONG)(a_RxContext)->Info.Length >= (a_cbNeeded)) \
        { /*likely */ } \
        else if (1) { Status = STATUS_BUFFER_TOO_SMALL; break; } else do { } while (0)

    switch (RxContext->Info.FileInformationClass)
    {
        /*
         * Queries we can satisfy without calling the host:
         */

        case FileNamesInformation:
        {
            PFILE_NAMES_INFORMATION pInfo    = (PFILE_NAMES_INFORMATION)RxContext->Info.Buffer;
            PUNICODE_STRING         FileName = GET_ALREADY_PREFIXED_NAME_FROM_CONTEXT(RxContext);
            Log(("VBOXSF: MrxQueryFileInfo: FileNamesInformation\n"));

            cbToCopy = RT_UOFFSETOF_DYN(FILE_NAMES_INFORMATION, FileName[FileName->Length / 2 + 1]);
            CHECK_SIZE_BREAK(RxContext, cbToCopy);

            pInfo->NextEntryOffset = 0;
            pInfo->FileIndex       = 0;
            pInfo->FileNameLength  = FileName->Length;

            RtlCopyMemory(pInfo->FileName, FileName->Buffer, FileName->Length);
            pInfo->FileName[FileName->Length] = 0;
            break;
        }

        case FileInternalInformation:
        {
            PFILE_INTERNAL_INFORMATION pInfo = (PFILE_INTERNAL_INFORMATION)RxContext->Info.Buffer;
            Log(("VBOXSF: MrxQueryFileInfo: FileInternalInformation\n"));

            cbToCopy = sizeof(FILE_INTERNAL_INFORMATION);
            CHECK_SIZE_BREAK(RxContext, cbToCopy);

            /* A 8-byte file reference number for the file. */
            pInfo->IndexNumber.QuadPart = (ULONG_PTR)capFcb;
            break;
        }

        case FileEaInformation:
        {
            PFILE_EA_INFORMATION pInfo = (PFILE_EA_INFORMATION)RxContext->Info.Buffer;
            Log(("VBOXSF: MrxQueryFileInfo: FileEaInformation\n"));

            cbToCopy = sizeof(FILE_EA_INFORMATION);
            CHECK_SIZE_BREAK(RxContext, cbToCopy);

            pInfo->EaSize = 0;
            break;
        }

        case FileStreamInformation:
            Log(("VBOXSF: MrxQueryFileInfo: FileStreamInformation: not supported\n"));
            Status = STATUS_INVALID_PARAMETER;
            break;

        case FileAlternateNameInformation:
            Log(("VBOXSF: MrxQueryFileInfo: FileStreamInformation: not implemented\n"));
            Status = STATUS_OBJECT_NAME_NOT_FOUND;
            break;

        case FileNumaNodeInformation:
            Log(("VBOXSF: MrxQueryFileInfo: FileNumaNodeInformation: not supported\n"));
            Status = STATUS_NO_SUCH_DEVICE; /* what's returned on a samba share */
            break;

        case FileStandardLinkInformation:
            Log(("VBOXSF: MrxQueryFileInfo: FileStandardLinkInformation: not supported\n"));
            Status = STATUS_NOT_SUPPORTED; /* what's returned on a samba share */
            break;

        /*
         * Queries where we need info from the host.
         *
         * For directories we don't necessarily go to the host but use info from when we
         * opened the them, why we do this is a little unclear as all the clues that r9630
         * give is "fixes".
         *
         * Note! We verify the buffer size after talking to the host, assuming that there
         *       won't be a problem and saving an extra switch statement.  IIRC the
         *       NtQueryInformationFile code verfies the sizes too.
         */
        /** @todo r=bird: install a hack so we get FileAllInformation directly up here
         *        rather than 5 individual queries.  We may end up going 3 times to
         *        the host (depending on the TTL hack) to fetch the same info over
         *        and over again. */
        case FileEndOfFileInformation:
        case FileAllocationInformation:
        case FileBasicInformation:
        case FileStandardInformation:
        case FileNetworkOpenInformation:
        case FileAttributeTagInformation:
        case FileCompressionInformation:
        {
            /* Query the information if necessary. */
            if (   !(pVBoxFobx->Info.Attr.fMode & RTFS_DOS_DIRECTORY) /** @todo figure out why we don't return up-to-date info for directories! */
                && (   !pVBoxFobx->nsUpToDate
                    || pVBoxFobx->nsUpToDate - RTTimeSystemNanoTS() < RT_NS_100US /** @todo implement proper TTL */ ) )
            {
                int               vrc;
                VBOXSFOBJINFOREQ *pReq = (VBOXSFOBJINFOREQ *)VbglR0PhysHeapAlloc(sizeof(*pReq));
                AssertBreakStmt(pReq, Status = STATUS_NO_MEMORY);

                vrc = VbglR0SfHostReqQueryObjInfo(pNetRootExtension->map.root, pReq, pVBoxFobx->hFile);
                if (RT_SUCCESS(vrc))
                    vbsfNtCopyInfo(pVBoxFobx, &pReq->ObjInfo, 0);
                else
                {
                    Status = vbsfNtVBoxStatusToNt(vrc);
                    VbglR0PhysHeapFree(pReq);
                    break;
                }
                VbglR0PhysHeapFree(pReq);
            }

            /* Copy it into the return buffer. */
            switch (RxContext->Info.FileInformationClass)
            {
                case FileBasicInformation:
                {
                    PFILE_BASIC_INFORMATION pInfo = (PFILE_BASIC_INFORMATION)RxContext->Info.Buffer;
                    Log(("VBOXSF: MrxQueryFileInfo: FileBasicInformation\n"));

                    cbToCopy = sizeof(FILE_BASIC_INFORMATION);
                    CHECK_SIZE_BREAK(RxContext, cbToCopy);

                    pInfo->CreationTime.QuadPart   = RTTimeSpecGetNtTime(&pVBoxFobx->Info.BirthTime);
                    pInfo->LastAccessTime.QuadPart = RTTimeSpecGetNtTime(&pVBoxFobx->Info.AccessTime);
                    pInfo->LastWriteTime.QuadPart  = RTTimeSpecGetNtTime(&pVBoxFobx->Info.ModificationTime);
                    pInfo->ChangeTime.QuadPart     = RTTimeSpecGetNtTime(&pVBoxFobx->Info.ChangeTime);
                    pInfo->FileAttributes          = VBoxToNTFileAttributes(pVBoxFobx->Info.Attr.fMode);
                    Log(("VBOXSF: MrxQueryFileInfo: FileBasicInformation: File attributes: 0x%x\n",
                         pInfo->FileAttributes));
                    break;
                }

                case FileStandardInformation:
                {
                    PFILE_STANDARD_INFORMATION pInfo = (PFILE_STANDARD_INFORMATION)RxContext->Info.Buffer;
                    Log(("VBOXSF: MrxQueryFileInfo: FileStandardInformation\n"));

                    cbToCopy = sizeof(FILE_STANDARD_INFORMATION);
                    CHECK_SIZE_BREAK(RxContext, cbToCopy);

                    /* Note! We didn't used to set allocation size and end-of-file for directories.
                             NTFS reports these, though, so why shouldn't we. */
                    pInfo->AllocationSize.QuadPart = pVBoxFobx->Info.cbAllocated;
                    pInfo->EndOfFile.QuadPart      = pVBoxFobx->Info.cbObject;
                    pInfo->NumberOfLinks           = 1; /** @todo 0? */
                    pInfo->DeletePending           = FALSE;
                    pInfo->Directory               = pVBoxFobx->Info.Attr.fMode & RTFS_DOS_DIRECTORY ? TRUE : FALSE;
                    break;
                }

                case FileNetworkOpenInformation:
                {
                    PFILE_NETWORK_OPEN_INFORMATION pInfo = (PFILE_NETWORK_OPEN_INFORMATION)RxContext->Info.Buffer;
                    Log(("VBOXSF: MrxQueryFileInfo: FileNetworkOpenInformation\n"));

                    cbToCopy = sizeof(FILE_NETWORK_OPEN_INFORMATION);
                    CHECK_SIZE_BREAK(RxContext, cbToCopy);

                    pInfo->CreationTime.QuadPart   = RTTimeSpecGetNtTime(&pVBoxFobx->Info.BirthTime);
                    pInfo->LastAccessTime.QuadPart = RTTimeSpecGetNtTime(&pVBoxFobx->Info.AccessTime);
                    pInfo->LastWriteTime.QuadPart  = RTTimeSpecGetNtTime(&pVBoxFobx->Info.ModificationTime);
                    pInfo->ChangeTime.QuadPart     = RTTimeSpecGetNtTime(&pVBoxFobx->Info.ChangeTime);
                    /* Note! We didn't used to set allocation size and end-of-file for directories.
                             NTFS reports these, though, so why shouldn't we. */
                    pInfo->AllocationSize.QuadPart = pVBoxFobx->Info.cbAllocated;
                    pInfo->EndOfFile.QuadPart      = pVBoxFobx->Info.cbObject;
                    pInfo->FileAttributes          = VBoxToNTFileAttributes(pVBoxFobx->Info.Attr.fMode);
                    break;
                }

                case FileEndOfFileInformation:
                {
                    PFILE_END_OF_FILE_INFORMATION pInfo = (PFILE_END_OF_FILE_INFORMATION)RxContext->Info.Buffer;
                    Log(("VBOXSF: MrxQueryFileInfo: FileEndOfFileInformation\n"));

                    cbToCopy = sizeof(FILE_END_OF_FILE_INFORMATION);
                    CHECK_SIZE_BREAK(RxContext, cbToCopy);

                    /* Note! We didn't used to set allocation size and end-of-file for directories.
                             NTFS reports these, though, so why shouldn't we. */
                    pInfo->EndOfFile.QuadPart      = pVBoxFobx->Info.cbObject;
                    break;
                }

                case FileAllocationInformation:
                {
                    PFILE_ALLOCATION_INFORMATION pInfo = (PFILE_ALLOCATION_INFORMATION)RxContext->Info.Buffer;
                    Log(("VBOXSF: MrxQueryFileInfo: FileAllocationInformation\n"));

                    cbToCopy = sizeof(FILE_ALLOCATION_INFORMATION);
                    CHECK_SIZE_BREAK(RxContext, cbToCopy);

                    /* Note! We didn't used to set allocation size and end-of-file for directories.
                             NTFS reports these, though, so why shouldn't we. */
                    pInfo->AllocationSize.QuadPart = pVBoxFobx->Info.cbAllocated;
                    break;
                }

                case FileAttributeTagInformation:
                {
                    PFILE_ATTRIBUTE_TAG_INFORMATION pInfo = (PFILE_ATTRIBUTE_TAG_INFORMATION)RxContext->Info.Buffer;
                    Log(("VBOXSF: MrxQueryFileInfo: FileAttributeTagInformation\n"));

                    cbToCopy = sizeof(FILE_ATTRIBUTE_TAG_INFORMATION);
                    CHECK_SIZE_BREAK(RxContext, cbToCopy);

                    pInfo->FileAttributes = VBoxToNTFileAttributes(pVBoxFobx->Info.Attr.fMode);
                    pInfo->ReparseTag     = 0;
                    break;
                }

                case FileCompressionInformation:
                {
                    //PFILE_COMPRESSION_INFO pInfo = (PFILE_COMPRESSION_INFO)RxContext->Info.Buffer;
                    struct MY_FILE_COMPRESSION_INFO
                    {
                        LARGE_INTEGER   CompressedFileSize;
                        WORD            CompressionFormat;
                        UCHAR           CompressionUnitShift;
                        UCHAR           ChunkShift;
                        UCHAR           ClusterShift;
                        UCHAR           Reserved[3];
                    } *pInfo = (struct MY_FILE_COMPRESSION_INFO *)RxContext->Info.Buffer;
                    Log(("VBOXSF: MrxQueryFileInfo: FileCompressionInformation\n"));

                    cbToCopy = sizeof(*pInfo);
                    CHECK_SIZE_BREAK(RxContext, cbToCopy);

                    pInfo->CompressedFileSize.QuadPart = pVBoxFobx->Info.cbObject;
                    pInfo->CompressionFormat           = 0;
                    pInfo->CompressionUnitShift        = 0;
                    pInfo->ChunkShift                  = 0;
                    pInfo->ClusterShift                = 0;
                    pInfo->Reserved[0]                 = 0;
                    pInfo->Reserved[1]                 = 0;
                    pInfo->Reserved[2]                 = 0;
                    AssertCompile(sizeof(pInfo->Reserved) == 3);
                    break;
                }

                default:
                    AssertLogRelMsgFailed(("FileInformationClass=%d\n",
                                           RxContext->Info.FileInformationClass));
                    Status = STATUS_INTERNAL_ERROR;
                    break;
            }
            break;
        }


/** @todo Implement:
 * FileHardLinkInformation: rcNt=0 (STATUS_SUCCESS) Ios.Status=0 (STATUS_SUCCESS) Ios.Information=0000000000000048
 * FileProcessIdsUsingFileInformation: rcNt=0 (STATUS_SUCCESS) Ios.Status=0 (STATUS_SUCCESS) Ios.Information=0000000000000010
 * FileNormalizedNameInformation: rcNt=0 (STATUS_SUCCESS) Ios.Status=0 (STATUS_SUCCESS) Ios.Information=00000000000000AA
 * FileNetworkPhysicalNameInformation: rcNt=0xc000000d (STATUS_INVALID_PARAMETER) Ios={not modified}
 * FileShortNameInformation?
 * FileNetworkPhysicalNameInformation
 */

        /*
         * Unsupported ones (STATUS_INVALID_PARAMETER is correct here if you
         * go by what fat + ntfs return, however samba mounts generally returns
         * STATUS_INVALID_INFO_CLASS except for pipe info - see queryfileinfo-1).
         */
        default:
            Log(("VBOXSF: MrxQueryFileInfo: Not supported FileInformationClass: %d!\n",
                 RxContext->Info.FileInformationClass));
            Status = STATUS_INVALID_PARAMETER;
            break;

    }
#undef CHECK_SIZE_BREAK

    /* Note! InformationToReturn doesn't seem to be used, instead Info.LengthRemaining should underflow
             so it can be used together with RxContext->CurrentIrpSp->Parameters.QueryFile.Length
             to calc the Ios.Information value.  This explain the weird LONG type choice.  */
    RxContext->InformationToReturn   = cbToCopy;
    RxContext->Info.LengthRemaining -= cbToCopy;
    AssertStmt(RxContext->Info.LengthRemaining >= 0 || Status != STATUS_SUCCESS, Status = STATUS_BUFFER_TOO_SMALL);

    Log(("VBOXSF: MrxQueryFileInfo: Returns %#x, Remaining length = %d, cbToCopy = %u (%#x)\n",
         Status, RxContext->Info.Length, cbToCopy));
    return Status;
}

/**
 * Worker for vbsfNtSetBasicInfo.
 */
static NTSTATUS vbsfNtSetBasicInfo(PMRX_VBOX_FOBX pVBoxFobx, PMRX_VBOX_NETROOT_EXTENSION pNetRootExtension,
                                   PFILE_BASIC_INFORMATION pBasicInfo)
{
    PSHFLFSOBJINFO          pSHFLFileInfo;
    uint8_t                *pHGCMBuffer = NULL;
    uint32_t                cbBuffer = 0;
    uint32_t                fModified;
    int                     vrc;

    Log(("VBOXSF: MRxSetFileInfo: FileBasicInformation: CreationTime   %RX64\n", pBasicInfo->CreationTime.QuadPart));
    Log(("VBOXSF: MRxSetFileInfo: FileBasicInformation: LastAccessTime %RX64\n", pBasicInfo->LastAccessTime.QuadPart));
    Log(("VBOXSF: MRxSetFileInfo: FileBasicInformation: LastWriteTime  %RX64\n", pBasicInfo->LastWriteTime.QuadPart));
    Log(("VBOXSF: MRxSetFileInfo: FileBasicInformation: ChangeTime     %RX64\n", pBasicInfo->ChangeTime.QuadPart));
    Log(("VBOXSF: MRxSetFileInfo: FileBasicInformation: FileAttributes %RX32\n", pBasicInfo->FileAttributes));

    /*
     * Note! If a timestamp value is non-zero, the client disables implicit updating of
     *       that timestamp via this handle when reading, writing and changing attributes.
     *       The special -1 value is used to just disable implicit updating without
     *       modifying the timestamp.  While the value is allowed for the CreationTime
     *       field, it will be treated as zero.
     *
     *       More clues can be found here:
     * https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/16023025-8a78-492f-8b96-c873b042ac50
     * https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/d4bc551b-7aaf-4b4f-ba0e-3a75e7c528f0#Appendix_A_86
     */

    /** @todo r=bird: The attempt at implementing the disable-timestamp-update
     *        behaviour here needs a little adjusting.  I'll get to that later.
     *
     * Reminders:
     *
     *  1. Drop VBOX_FOBX_F_INFO_CREATION_TIME.
     *
     *  2. Drop unused VBOX_FOBX_F_INFO_ATTRIBUTES.
     *
     *  3. Only act on VBOX_FOBX_F_INFO_CHANGE_TIME if modified attributes or grown
     *     the file (?) so we don't cancel out updates by other parties (like the
     *     host).
     *
     *  4. Only act on VBOX_FOBX_F_INFO_LASTWRITE_TIME if we've written to the file.
     *
     *  5. Only act on VBOX_FOBX_F_INFO_LASTACCESS_TIME if we've read from the file
     *     or done whatever else might modify the access time.
     *
     *  6. Don't bother calling the host if there are only zeros and -1 values.
     *
     *  7. Client application should probably be allowed to modify the timestamps
     *     explicitly using this API after disabling updating, given the wording of
     *     the footnote referenced above.
     *
     *  8. Extend the host interface to let the host handle this crap instead as it
     *     can do a better job, like on windows it's done implicitly if we let -1
     *     pass thru IPRT.
     *
     * One worry here is that we hide timestamp updates made by the host or other
     * guest side processes.  This could account for some of the issues we've been
     * having with the guest not noticing host side changes.
     */

    if (pBasicInfo->CreationTime.QuadPart == -1)
    {
        pVBoxFobx->fKeepCreationTime = TRUE;
        pVBoxFobx->SetFileInfoOnCloseFlags |= VBOX_FOBX_F_INFO_CREATION_TIME;
    }
    if (pBasicInfo->LastAccessTime.QuadPart == -1)
    {
        pVBoxFobx->fKeepLastAccessTime = TRUE;
        pVBoxFobx->SetFileInfoOnCloseFlags |= VBOX_FOBX_F_INFO_LASTACCESS_TIME;
    }
    if (pBasicInfo->LastWriteTime.QuadPart == -1)
    {
        pVBoxFobx->fKeepLastWriteTime = TRUE;
        pVBoxFobx->SetFileInfoOnCloseFlags |= VBOX_FOBX_F_INFO_LASTWRITE_TIME;
    }
    if (pBasicInfo->ChangeTime.QuadPart == -1)
    {
        pVBoxFobx->fKeepChangeTime = TRUE;
        pVBoxFobx->SetFileInfoOnCloseFlags |= VBOX_FOBX_F_INFO_CHANGE_TIME;
    }

    cbBuffer = sizeof(SHFLFSOBJINFO);
    pHGCMBuffer = (uint8_t *)vbsfNtAllocNonPagedMem(cbBuffer);
    AssertReturn(pHGCMBuffer, STATUS_INSUFFICIENT_RESOURCES);
    RtlZeroMemory(pHGCMBuffer, cbBuffer);
    pSHFLFileInfo = (PSHFLFSOBJINFO)pHGCMBuffer;

    Log(("VBOXSF: MrxSetFileInfo: FileBasicInformation: keeps %d %d %d %d\n",
         pVBoxFobx->fKeepCreationTime, pVBoxFobx->fKeepLastAccessTime, pVBoxFobx->fKeepLastWriteTime, pVBoxFobx->fKeepChangeTime));

    /* The properties, that need to be changed, are set to something other than zero */
    fModified = 0;
    if (pBasicInfo->CreationTime.QuadPart && !pVBoxFobx->fKeepCreationTime)
    {
        RTTimeSpecSetNtTime(&pSHFLFileInfo->BirthTime, pBasicInfo->CreationTime.QuadPart);
        fModified |= VBOX_FOBX_F_INFO_CREATION_TIME;
    }
    /** @todo FsPerf need to check what is supposed to happen if modified
     *        against after -1 is specified.  */
    if (pBasicInfo->LastAccessTime.QuadPart && !pVBoxFobx->fKeepLastAccessTime)
    {
        RTTimeSpecSetNtTime(&pSHFLFileInfo->AccessTime, pBasicInfo->LastAccessTime.QuadPart);
        fModified |= VBOX_FOBX_F_INFO_LASTACCESS_TIME;
    }
    if (pBasicInfo->LastWriteTime.QuadPart && !pVBoxFobx->fKeepLastWriteTime)
    {
        RTTimeSpecSetNtTime(&pSHFLFileInfo->ModificationTime, pBasicInfo->LastWriteTime.QuadPart);
        fModified |= VBOX_FOBX_F_INFO_LASTWRITE_TIME;
    }
    if (pBasicInfo->ChangeTime.QuadPart && !pVBoxFobx->fKeepChangeTime)
    {
        RTTimeSpecSetNtTime(&pSHFLFileInfo->ChangeTime, pBasicInfo->ChangeTime.QuadPart);
        fModified |= VBOX_FOBX_F_INFO_CHANGE_TIME;
    }
    if (pBasicInfo->FileAttributes)
        pSHFLFileInfo->Attr.fMode = NTToVBoxFileAttributes(pBasicInfo->FileAttributes);

    Assert(pVBoxFobx && pNetRootExtension);
    vrc = VbglR0SfFsInfo(&g_SfClient, &pNetRootExtension->map, pVBoxFobx->hFile,
                         SHFL_INFO_SET | SHFL_INFO_FILE, &cbBuffer, (PSHFLDIRINFO)pSHFLFileInfo);

    NTSTATUS Status;
    if (RT_SUCCESS(vrc))
    {
        vbsfNtCopyInfo(pVBoxFobx, pSHFLFileInfo, fModified);
        pVBoxFobx->SetFileInfoOnCloseFlags |= fModified;
        Status = STATUS_SUCCESS;
    }
    else
        Status = vbsfNtVBoxStatusToNt(vrc);
    vbsfNtFreeNonPagedMem(pHGCMBuffer);
    return Status;
}

/**
 * Worker for vbsfNtSetBasicInfo.
 */
static NTSTATUS vbsfNtSetEndOfFile(IN OUT struct _RX_CONTEXT * RxContext, IN uint64_t cbNewFileSize)
{
    NTSTATUS Status = STATUS_SUCCESS;

    RxCaptureFcb;
    RxCaptureFobx;

    PMRX_VBOX_NETROOT_EXTENSION pNetRootExtension = VBoxMRxGetNetRootExtension(capFcb->pNetRoot);
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(capFobx);

    PSHFLFSOBJINFO pObjInfo;
    uint32_t cbBuffer;
    int vrc;

    Log(("VBOXSF: vbsfNtSetEndOfFile: New size = %RX64\n",
         cbNewFileSize));

    Assert(pVBoxFobx && pNetRootExtension);

    cbBuffer = sizeof(SHFLFSOBJINFO);
    pObjInfo = (SHFLFSOBJINFO *)vbsfNtAllocNonPagedMem(cbBuffer);
    if (!pObjInfo)
    {
        AssertFailed();
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(pObjInfo, cbBuffer);
    pObjInfo->cbObject = cbNewFileSize;

    vrc = VbglR0SfFsInfo(&g_SfClient, &pNetRootExtension->map, pVBoxFobx->hFile,
                         SHFL_INFO_SET | SHFL_INFO_SIZE, &cbBuffer, (PSHFLDIRINFO)pObjInfo);

    Log(("VBOXSF: vbsfNtSetEndOfFile: VbglR0SfFsInfo returned %Rrc\n", vrc));

    Status = vbsfNtVBoxStatusToNt(vrc);
    if (Status == STATUS_SUCCESS)
    {
        Log(("VBOXSF: vbsfNtSetEndOfFile: VbglR0SfFsInfo new allocation size = %RX64\n",
             pObjInfo->cbAllocated));

        /** @todo update the file stats! */
    }

    if (pObjInfo)
        vbsfNtFreeNonPagedMem(pObjInfo);

    Log(("VBOXSF: vbsfNtSetEndOfFile: Returned 0x%08X\n", Status));
    return Status;
}

/**
 * Worker for vbsfNtSetBasicInfo.
 */
static NTSTATUS vbsfNtRename(IN PRX_CONTEXT RxContext,
                             IN FILE_INFORMATION_CLASS FileInformationClass,
                             IN PVOID pBuffer,
                             IN ULONG BufferLength)
{
    NTSTATUS Status = STATUS_SUCCESS;

    RxCaptureFcb;
    RxCaptureFobx;

    PMRX_VBOX_NETROOT_EXTENSION pNetRootExtension = VBoxMRxGetNetRootExtension(capFcb->pNetRoot);
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(capFobx);
    PMRX_SRV_OPEN pSrvOpen = capFobx->pSrvOpen;

    PFILE_RENAME_INFORMATION RenameInformation = (PFILE_RENAME_INFORMATION)RxContext->Info.Buffer;
    PUNICODE_STRING RemainingName = GET_ALREADY_PREFIXED_NAME(pSrvOpen, capFcb);

    int vrc;
    PSHFLSTRING SrcPath = 0, DestPath = 0;
    ULONG flags;

    RT_NOREF(FileInformationClass, pBuffer, BufferLength);

    Assert(FileInformationClass == FileRenameInformation);

    Log(("VBOXSF: vbsfNtRename: FileName = %.*ls\n",
         RenameInformation->FileNameLength / sizeof(WCHAR), &RenameInformation->FileName[0]));

    /* Must close the file before renaming it! */
    if (pVBoxFobx->hFile != SHFL_HANDLE_NIL)
        vbsfNtCloseFileHandle(pNetRootExtension, pVBoxFobx);

    /* Mark it as renamed, so we do nothing during close */
    SetFlag(pSrvOpen->Flags, SRVOPEN_FLAG_FILE_RENAMED);

    Log(("VBOXSF: vbsfNtRename: RenameInformation->FileNameLength = %d\n", RenameInformation->FileNameLength));
    Status = vbsfNtShflStringFromUnicodeAlloc(&DestPath, RenameInformation->FileName, (uint16_t)RenameInformation->FileNameLength);
    if (Status != STATUS_SUCCESS)
        return Status;

    Log(("VBOXSF: vbsfNtRename: Destination path = %.*ls\n",
         DestPath->u16Length / sizeof(WCHAR), &DestPath->String.ucs2[0]));

    Log(("VBOXSF: vbsfNtRename: RemainingName->Length = %d\n", RemainingName->Length));
    Status = vbsfNtShflStringFromUnicodeAlloc(&SrcPath, RemainingName->Buffer, RemainingName->Length);
    if (Status != STATUS_SUCCESS)
    {
        vbsfNtFreeNonPagedMem(DestPath);
        return Status;
    }

    Log(("VBOXSF: vbsfNtRename: Source path = %.*ls\n",
         SrcPath->u16Length / sizeof(WCHAR), &SrcPath->String.ucs2[0]));

    /* Call host. */
    flags = pVBoxFobx->Info.Attr.fMode & RTFS_DOS_DIRECTORY ? SHFL_RENAME_DIR : SHFL_RENAME_FILE;
    if (RenameInformation->ReplaceIfExists)
        flags |= SHFL_RENAME_REPLACE_IF_EXISTS;

    Log(("VBOXSF: vbsfNtRename: Calling VbglR0SfRename\n"));
    vrc = VbglR0SfRename(&g_SfClient, &pNetRootExtension->map, SrcPath, DestPath, flags);

    vbsfNtFreeNonPagedMem(SrcPath);
    vbsfNtFreeNonPagedMem(DestPath);

    Status = vbsfNtVBoxStatusToNt(vrc);
    if (vrc != VINF_SUCCESS)
        Log(("VBOXSF: vbsfNtRename: VbglR0SfRename failed with %Rrc\n", vrc));

    Log(("VBOXSF: vbsfNtRename: Returned 0x%08X\n", Status));
    return Status;
}


NTSTATUS VBoxMRxSetFileInfo(IN PRX_CONTEXT RxContext)
{
    RxCaptureFcb;
    RxCaptureFobx;
    PMRX_VBOX_NETROOT_EXTENSION pNetRootExtension = VBoxMRxGetNetRootExtension(capFcb->pNetRoot);
    PMRX_VBOX_FOBX              pVBoxFobx         = VBoxMRxGetFileObjectExtension(capFobx);
    NTSTATUS                    Status            = STATUS_SUCCESS;

    Log(("VBOXSF: MrxSetFileInfo: Buffer=%p Length=%#x\n",
         RxContext->Info.Buffer, RxContext->Info.Length));

    switch (RxContext->Info.FileInformationClass)
    {
        case FileBasicInformation:
        {
            Assert(RxContext->Info.Length >= sizeof(FILE_BASIC_INFORMATION));
            Status = vbsfNtSetBasicInfo(pVBoxFobx, pNetRootExtension, (PFILE_BASIC_INFORMATION)RxContext->Info.Buffer);
            break;
        }

        case FileDispositionInformation:
        {
            PFILE_DISPOSITION_INFORMATION pInfo = (PFILE_DISPOSITION_INFORMATION)RxContext->Info.Buffer;
            Log(("VBOXSF: MrxSetFileInfo: FileDispositionInformation: Delete = %d\n",
                 pInfo->DeleteFile));

            if (pInfo->DeleteFile && capFcb->OpenCount == 1)
                Status = vbsfNtRemove(RxContext);
            else
                Status = STATUS_SUCCESS;
            break;
        }

        /*
         * Change the allocation size, leaving the EOF alone unless the file shrinks.
         *
         * There is no shared folder operation for this, so we only need to care
         * about adjusting EOF if the file shrinks.
         *
         * https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/d4bc551b-7aaf-4b4f-ba0e-3a75e7c528f0#Appendix_A_83
         */
        case FileAllocationInformation:
        {
            PFILE_ALLOCATION_INFORMATION pInfo = (PFILE_ALLOCATION_INFORMATION)RxContext->Info.Buffer;
            Log(("VBOXSF: MrxSetFileInfo: FileAllocationInformation: new AllocSize = 0x%RX64, FileSize = 0x%RX64\n",
                 pInfo->AllocationSize.QuadPart, capFcb->Header.FileSize.QuadPart));

            if (pInfo->AllocationSize.QuadPart >= capFcb->Header.FileSize.QuadPart)
                Status = STATUS_SUCCESS;
            else
            {
                /** @todo get up to date EOF from host?  We may risk accidentally growing the
                 *        file here if the host (or someone else) truncated it. */
                Status = vbsfNtSetEndOfFile(RxContext, pInfo->AllocationSize.QuadPart);
            }
            break;
        }

        /*
         * Prior to calling us, RxSetEndOfFileInfo will have updated the FCB fields space.FileSize,
         * Header.AllocationSize and (if old value was larger) Header.ValidDataLength.  On success
         * it will inform the cache manager, while on failure the old values will be restored.
         *
         * Note! RxSetEndOfFileInfo assumes that the old Header.FileSize value is up to date and
         *       will hide calls which does not change the size from us.  This is of course not
         *       the case for non-local file systems, as the server is the only which up-to-date
         *       information.  Don't see an easy way of working around it, so ignoring it for now.
         */
        case FileEndOfFileInformation:
        {
            PFILE_END_OF_FILE_INFORMATION pInfo = (PFILE_END_OF_FILE_INFORMATION)RxContext->Info.Buffer;
            Log(("VBOXSF: MrxSetFileInfo: FileEndOfFileInformation: new EndOfFile 0x%RX64, FileSize = 0x%RX64\n",
                 pInfo->EndOfFile.QuadPart, capFcb->Header.FileSize.QuadPart));

            Status = vbsfNtSetEndOfFile(RxContext, pInfo->EndOfFile.QuadPart);

            Log(("VBOXSF: MrxSetFileInfo: FileEndOfFileInformation: Status 0x%08X\n",
                 Status));
            break;
        }

        case FileLinkInformation:
        {
#ifdef LOG_ENABLED
            PFILE_LINK_INFORMATION pInfo = (PFILE_LINK_INFORMATION )RxContext->Info.Buffer;
            Log(("VBOXSF: MrxSetFileInfo: FileLinkInformation: ReplaceIfExists = %d, RootDirectory = 0x%x = [%.*ls]. Not implemented!\n",
                 pInfo->ReplaceIfExists, pInfo->RootDirectory, pInfo->FileNameLength / sizeof(WCHAR), pInfo->FileName));
#endif

            Status = STATUS_NOT_IMPLEMENTED;
            break;
        }

        case FileRenameInformation:
        {
#ifdef LOG_ENABLED
            PFILE_RENAME_INFORMATION pInfo = (PFILE_RENAME_INFORMATION)RxContext->Info.Buffer;
            Log(("VBOXSF: MrxSetFileInfo: FileRenameInformation: ReplaceIfExists = %d, RootDirectory = 0x%x = [%.*ls]\n",
                 pInfo->ReplaceIfExists, pInfo->RootDirectory, pInfo->FileNameLength / sizeof(WCHAR), pInfo->FileName));
#endif

            Status = vbsfNtRename(RxContext, FileRenameInformation, RxContext->Info.Buffer, RxContext->Info.Length);
            break;
        }

        /* The file position is handled by the RDBSS library (RxSetPositionInfo)
           and we should never see this request. */
        case FilePositionInformation:
            AssertMsgFailed(("VBOXSF: MrxSetFileInfo: FilePositionInformation: CurrentByteOffset = 0x%RX64. Unsupported!\n",
                             ((PFILE_POSITION_INFORMATION)RxContext->Info.Buffer)->CurrentByteOffset.QuadPart));
            Status = STATUS_INVALID_PARAMETER;
            break;

        default:
            Log(("VBOXSF: MrxSetFileInfo: Not supported FileInformationClass: %d!\n",
                 RxContext->Info.FileInformationClass));
            Status = STATUS_INVALID_PARAMETER;
            break;
    }

    Log(("VBOXSF: MrxSetFileInfo: Returned 0x%08X\n", Status));
    return Status;
}

NTSTATUS VBoxMRxSetFileInfoAtCleanup(IN PRX_CONTEXT RxContext)
{
    RT_NOREF(RxContext);
    Log(("VBOXSF: MRxSetFileInfoAtCleanup\n"));
    return STATUS_SUCCESS;
}
