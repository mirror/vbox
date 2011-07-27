/*++

 Copyright (c) 1989 - 1999 Microsoft Corporation

 Module Name:

 fileinfo.c

 Abstract:

 This module implements the mini redirector call down routines pertaining to retrieval/
 update of file/directory/volume information.

 --*/

#include "precomp.h"
#pragma hdrstop

/** Macro for copying a SHFLSTRING file name into a FILE_DIRECTORY_INFORMATION structure. */
#define INIT_FILE_NAME(obj, str) \
    do { \
        ULONG cbLength = (str).u16Length; \
        (obj)->FileNameLength = cbLength; \
        RtlCopyMemory((obj)->FileName, &(str).String.ucs2[0], cbLength + 2); \
    } while (0)

NTSTATUS VBoxMRxQueryDirectory (IN OUT PRX_CONTEXT RxContext)
/*++

 Routine Description:

 This routine does a directory query. Only the NT-->NT path is implemented.

 Arguments:

 RxContext - the RDBSS context

 Return Value:

 NTSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    FILE_INFORMATION_CLASS FileInformationClass;
    PCHAR Buffer;
    PLONG pLengthRemaining;
    LONG CopySize;
    RxCaptureFobx;
    RxCaptureFcb;
    VBoxMRxGetDeviceExtension(RxContext, pDeviceExtension);
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(capFobx);
    VBoxMRxGetNetRootExtension(capFcb->pNetRoot, pNetRootExtension);
    PUNICODE_STRING DirectoryName = GET_ALREADY_PREFIXED_NAME_FROM_CONTEXT(RxContext);
    PUNICODE_STRING Template = &capFobx->UnicodeQueryTemplate;
    int vboxRC;
    uint8_t *pHGCMBuffer;
    uint32_t index, fSFFlags, cFiles, u32BufSize;
    LONG cbHGCMBuffer, cbMaxSize;
    PSHFLDIRINFO pDirEntry;
    ULONG *pNextOffset = 0;
    PSHFLSTRING ParsedPath = 0;

    if (NULL == pVBoxFobx)
    {
        Log(("VBOXSF: QueryDirectory: pVBoxFobx is invalid!\n"));
        return STATUS_INVALID_PARAMETER;
    }

    FileInformationClass = RxContext->Info.FileInformationClass;
    Log(("VBOXSF: QueryDirectory: FileInformationClass %d\n", FileInformationClass));

    Buffer = (PCHAR)RxContext->Info.Buffer;
    cbMaxSize = RxContext->Info.Length;
    pLengthRemaining = (PULONG) & RxContext->Info.LengthRemaining;

    Log(("VBOXSF: QueryDirectory: vboxFobx = 0x%08x VBox file handle = 0x%08x\n", pVBoxFobx, pVBoxFobx->hFile));

    if (NULL == DirectoryName)
        return STATUS_INVALID_PARAMETER;

    if (DirectoryName->Length == 0)
    {
        Log(("VBOXSF: QueryDirectory: DirectoryName = \\ (null string)\n"));
    }
    else Log(("VBOXSF: QueryDirectory: DirectoryName = %.*ls\n", DirectoryName->Length / sizeof(WCHAR), DirectoryName->Buffer));

    if (NULL == Template)
        return STATUS_INVALID_PARAMETER;

    if (Template->Length == 0)
    {
        Log(("VBOXSF: QueryDirectory: Template = \\ (null string)\n"));
    }
    else Log(("VBOXSF: QueryDirectory: Template = %.*ls\n", Template->Length / sizeof(WCHAR), Template->Buffer));

    cbHGCMBuffer = RT_MAX(cbMaxSize, PAGE_SIZE);
    Log(("VBOXSF: QueryDirectory: Allocating HGCMBuffer = %d\n", cbHGCMBuffer));
    pHGCMBuffer = (uint8_t *)vbsfAllocNonPagedMem(cbHGCMBuffer);
    if (pHGCMBuffer == NULL)
    {
        AssertFailed();
        return STATUS_NO_MEMORY;
    }

    /* Assume start from the beginning. */
    index = 0;
    if (RxContext->QueryDirectory.IndexSpecified == TRUE)
    {
        Log(("VBOXSF: QueryDirectory: Index specified %d\n", index));
        index = RxContext->QueryDirectory.FileIndex;
    }
    fSFFlags = SHFL_LIST_NONE;
    if (RxContext->QueryDirectory.ReturnSingleEntry == TRUE)
    {
        Log(("VBOXSF: QueryDirectory: Query single entry\n"));
        fSFFlags |= SHFL_LIST_RETURN_ONE;
    }

    if (Template->Length)
    {
        ULONG ParsedPathSize, len;

        /* Calculate length required for parsed path. */
        ParsedPathSize = sizeof(*ParsedPath) + (DirectoryName->Length + Template->Length + 3 * sizeof(WCHAR));
        Log(("VBOXSF: QueryDirectory: ParsedPathSize = %d\n", ParsedPathSize));

        ParsedPath = (PSHFLSTRING)vbsfAllocNonPagedMem(ParsedPathSize);
        if (!ParsedPath)
        {
            Status = STATUS_NO_MEMORY;
            goto end;
        }
        RtlZeroMemory (ParsedPath, ParsedPathSize);
        ShflStringInitBuffer(ParsedPath, ParsedPathSize - sizeof(SHFLSTRING));

        ParsedPath->u16Size = DirectoryName->Length + Template->Length + sizeof(WCHAR);
        ParsedPath->u16Length = ParsedPath->u16Size - sizeof(WCHAR); /* Without terminating null. */

        len = 0;
        if (DirectoryName->Length)
        {
            /* Copy directory name into ParsedPath. */
            RtlCopyMemory (ParsedPath->String.ucs2, DirectoryName->Buffer, DirectoryName->Length);
            len = DirectoryName->Length / sizeof(WCHAR);

            /* Add terminating backslash. */
            ParsedPath->String.ucs2[len] = L'\\';
            len++;
            ParsedPath->u16Length += sizeof(WCHAR);
            ParsedPath->u16Size += sizeof(WCHAR);
        }
        RtlCopyMemory (&ParsedPath->String.ucs2[len], Template->Buffer, Template->Length);
        Log(("VBOXSF: QueryDirectory: ParsedPath = %.*ls\n", ParsedPath->u16Length / sizeof(WCHAR), ParsedPath->String.ucs2));
    }

    cFiles = 0;
    /* vboxCallDirInfo requires a pointer to uint32_t. */
    u32BufSize = cbHGCMBuffer;

    Log(("VBOXSF: QueryDirectory: CallDirInfo: File = 0x%08x, Flags = 0x%08x, Index = %d\n", pVBoxFobx->hFile, fSFFlags, index));
    Log(("VBOXSF: QueryDirectory: u32BufSize before CallDirInfo = %d\n", u32BufSize));
    vboxRC = vboxCallDirInfo(&pDeviceExtension->hgcmClient, &pNetRootExtension->map, pVBoxFobx->hFile, ParsedPath, fSFFlags, index, &u32BufSize, (PSHFLDIRINFO)pHGCMBuffer, &cFiles);
    Log(("VBOXSF: QueryDirectory: u32BufSize after CallDirInfo = %d\n", u32BufSize));
    Log(("VBOXSF: QueryDirectory: CallDirInfo returned %Rrc\n", vboxRC));

    switch (vboxRC)
    {
    case VINF_SUCCESS:
        /* Nothing to do here. */
        break;
    case VERR_NO_TRANSLATION:
        Log(("VBOXSF: QueryDirectory: Host could not translate entry!\n"));
        break;
    case VERR_NO_MORE_FILES:
        if (cFiles <= 0) /* VERR_NO_MORE_FILES appears at the first lookup when just returning the current dir ".".
                          * So we also have to check for the cFiles counter. */
        {
            /* Not an error, but we have to handle the return value. */
            Log(("VBOXSF: QueryDirectory: Host reported no more files!\n"));

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
        Log(("VBOXSF: QueryDirectory: no such file!\n"));
        break;
    default:
        Status = VBoxErrorToNTStatus(vboxRC);
        Log(("VBOXSF: QueryDirectory: Error %Rrc from CallDirInfo (cFiles=%d)!\n", vboxRC, cFiles));
        break;
    }

    if (Status != STATUS_SUCCESS)
        goto end;

    /* Verify that the returned buffer length is not greater than the original one. */
    if (u32BufSize > (uint32_t)cbHGCMBuffer)
    {
        Log(("VBOXSF: QueryDirectory: returned buffer size (%u) is invalid!!!\n", u32BufSize));
        Status = STATUS_INVALID_NETWORK_RESPONSE;
        goto end;
    }

    /* How many bytes remain in the buffer. */
    cbHGCMBuffer = u32BufSize;

    pDirEntry = (PSHFLDIRINFO)pHGCMBuffer;
    Status = STATUS_SUCCESS;

    Log(("VBOXSF: QueryDirectory: cFiles=%d, Length=%d\n", cFiles, cbHGCMBuffer));
    while ((*pLengthRemaining) && (cFiles > 0) && (pDirEntry != NULL))
    {
        int cbEntry = RT_OFFSETOF(SHFLDIRINFO, name.String) + pDirEntry->name.u16Size;

        if (cbEntry > cbHGCMBuffer)
        {
            Log(("VBOXSF: QueryDirectory: Entry size (%d) exceeds the buffer size (%d)!!!\n", cbEntry, cbHGCMBuffer));
            Status = STATUS_INVALID_NETWORK_RESPONSE;
            goto end;
        }

        switch (FileInformationClass)
        {
        case FileDirectoryInformation:
        {
            PFILE_DIRECTORY_INFORMATION pDirInfo = (PFILE_DIRECTORY_INFORMATION)Buffer;
            CopySize = sizeof(FILE_DIRECTORY_INFORMATION);

            /* Struct already contains one char for null terminator. */
            CopySize += pDirEntry->name.u16Size;

            Log(("VBOXSF: QueryDirectory: FileDirectoryInformation Buffer=%08x\n", Buffer));
            if (*pLengthRemaining >= CopySize)
            {
                RtlZeroMemory( pDirInfo, CopySize );

                pDirInfo->CreationTime.QuadPart = RTTimeSpecGetNtTime(&pDirEntry->Info.BirthTime); /* ridiculous name */
                pDirInfo->LastAccessTime.QuadPart = RTTimeSpecGetNtTime(&pDirEntry->Info.AccessTime);
                pDirInfo->LastWriteTime.QuadPart = RTTimeSpecGetNtTime(&pDirEntry->Info.ModificationTime);
                pDirInfo->ChangeTime.QuadPart = RTTimeSpecGetNtTime(&pDirEntry->Info.ChangeTime);
                pDirInfo->AllocationSize.QuadPart = pDirEntry->Info.cbAllocated;
                pDirInfo->EndOfFile.QuadPart = pDirEntry->Info.cbObject;

                pDirInfo->FileIndex = index;
                pDirInfo->FileAttributes = VBoxToNTFileAttributes(pDirEntry->Info.Attr.fMode);

                INIT_FILE_NAME(pDirInfo, pDirEntry->name);

                /* Align to 8 byte boundary */
                CopySize = RT_ALIGN(CopySize, sizeof(LONGLONG));
                pDirInfo->NextEntryOffset = CopySize;
                pNextOffset = &pDirInfo->NextEntryOffset;
            }
            else
            {
                pDirInfo->NextEntryOffset = 0; /* last item */
                Status = STATUS_BUFFER_OVERFLOW;
            }
            break;
        }

        case FileFullDirectoryInformation:
        {
            PFILE_FULL_DIR_INFORMATION pDirInfo = (PFILE_FULL_DIR_INFORMATION)Buffer;
            CopySize = sizeof(FILE_FULL_DIR_INFORMATION);

            /* Struct already contains one char for null terminator. */
            CopySize += pDirEntry->name.u16Size;

            Log(("VBOXSF: QueryDirectory: FileFullDirectoryInformation Buffer=%08x\n", Buffer));
            if (*pLengthRemaining >= CopySize)
            {
                RtlZeroMemory( pDirInfo, CopySize );

                pDirInfo->CreationTime.QuadPart = RTTimeSpecGetNtTime(&pDirEntry->Info.BirthTime); /* ridiculous name */
                pDirInfo->LastAccessTime.QuadPart = RTTimeSpecGetNtTime(&pDirEntry->Info.AccessTime);
                pDirInfo->LastWriteTime.QuadPart = RTTimeSpecGetNtTime(&pDirEntry->Info.ModificationTime);
                pDirInfo->ChangeTime.QuadPart = RTTimeSpecGetNtTime(&pDirEntry->Info.ChangeTime);
                pDirInfo->AllocationSize.QuadPart = pDirEntry->Info.cbAllocated;
                pDirInfo->EndOfFile.QuadPart = pDirEntry->Info.cbObject;

                pDirInfo->EaSize = 0;
                pDirInfo->FileIndex = index;
                pDirInfo->FileAttributes = VBoxToNTFileAttributes(pDirEntry->Info.Attr.fMode);

                INIT_FILE_NAME(pDirInfo, pDirEntry->name);

                /* Align to 8 byte boundary */
                CopySize = RT_ALIGN(CopySize, sizeof(LONGLONG));
                pDirInfo->NextEntryOffset = CopySize;
                pNextOffset = &pDirInfo->NextEntryOffset;
            }
            else
            {
                pDirInfo->NextEntryOffset = 0; /* last item */
                Status = STATUS_BUFFER_OVERFLOW;
            }
            break;
        }

        case FileBothDirectoryInformation:
        {
            PFILE_BOTH_DIR_INFORMATION pDirInfo = (PFILE_BOTH_DIR_INFORMATION)Buffer;
            CopySize = sizeof(FILE_BOTH_DIR_INFORMATION);
            /* struct already contains one char for null terminator */
            CopySize += pDirEntry->name.u16Size;

            Log(("VBOXSF: QueryDirectory: FileBothDirectoryInformation Buffer=%08x\n", Buffer));
            if (*pLengthRemaining >= CopySize)
            {
                RtlZeroMemory( pDirInfo, CopySize );

                pDirInfo->CreationTime.QuadPart = RTTimeSpecGetNtTime(&pDirEntry->Info.BirthTime); /* ridiculous name */
                pDirInfo->LastAccessTime.QuadPart = RTTimeSpecGetNtTime(&pDirEntry->Info.AccessTime);
                pDirInfo->LastWriteTime.QuadPart = RTTimeSpecGetNtTime(&pDirEntry->Info.ModificationTime);
                pDirInfo->ChangeTime.QuadPart = RTTimeSpecGetNtTime(&pDirEntry->Info.ChangeTime);
                pDirInfo->AllocationSize.QuadPart = pDirEntry->Info.cbAllocated;
                pDirInfo->EndOfFile.QuadPart = pDirEntry->Info.cbObject;

                pDirInfo->EaSize = 0;
                pDirInfo->ShortNameLength = 0; /* @todo ? */
                pDirInfo->FileIndex = index;
                pDirInfo->FileAttributes = VBoxToNTFileAttributes(pDirEntry->Info.Attr.fMode);

                INIT_FILE_NAME(pDirInfo, pDirEntry->name);

                Log(("VBOXSF: QueryDirectory: FileBothDirectoryInformation cbAlloc = %x cbObject = %x\n", pDirEntry->Info.cbAllocated, pDirEntry->Info.cbObject));
                Log(("VBOXSF: QueryDirectory: FileBothDirectoryInformation CopySize = %d, name size=%d name len=%d\n", CopySize, pDirEntry->name.u16Size, pDirEntry->name.u16Length));
                Log(("VBOXSF: QueryDirectory: FileBothDirectoryInformation File name %.*ls (DirInfo)\n", pDirInfo->FileNameLength / sizeof(WCHAR), pDirInfo->FileName));
                Log(("VBOXSF: QueryDirectory: FileBothDirectoryInformation File name %.*ls (DirEntry)\n", pDirEntry->name.u16Size / sizeof(WCHAR), pDirEntry->name.String.ucs2));

                /* Align to 8 byte boundary. */
                CopySize = RT_ALIGN(CopySize, sizeof(LONGLONG));
                pDirInfo->NextEntryOffset = CopySize;
                pNextOffset = &pDirInfo->NextEntryOffset;
            }
            else
            {
                pDirInfo->NextEntryOffset = 0; /* Last item. */
                Status = STATUS_BUFFER_OVERFLOW;
            }
            break;
        }

        case FileIdBothDirectoryInformation:
        {
            PFILE_ID_BOTH_DIR_INFORMATION pDirInfo = (PFILE_ID_BOTH_DIR_INFORMATION)Buffer;
            CopySize = sizeof(FILE_ID_BOTH_DIR_INFORMATION);
            /* struct already contains one char for null terminator */
            CopySize += pDirEntry->name.u16Size;

            Log(("VBOXSF: QueryDirectory: FileIdBothDirectoryInformation Buffer=%08x\n", Buffer));
            if (*pLengthRemaining >= CopySize)
            {
                RtlZeroMemory( pDirInfo, CopySize );

                pDirInfo->CreationTime.QuadPart = RTTimeSpecGetNtTime(&pDirEntry->Info.BirthTime); /* ridiculous name */
                pDirInfo->LastAccessTime.QuadPart = RTTimeSpecGetNtTime(&pDirEntry->Info.AccessTime);
                pDirInfo->LastWriteTime.QuadPart = RTTimeSpecGetNtTime(&pDirEntry->Info.ModificationTime);
                pDirInfo->ChangeTime.QuadPart = RTTimeSpecGetNtTime(&pDirEntry->Info.ChangeTime);
                pDirInfo->AllocationSize.QuadPart = pDirEntry->Info.cbAllocated;
                pDirInfo->EndOfFile.QuadPart = pDirEntry->Info.cbObject;

                pDirInfo->EaSize = 0;
                pDirInfo->ShortNameLength = 0; /* @todo ? */
                pDirInfo->EaSize = 0;
                pDirInfo->FileId.QuadPart = 0;
                pDirInfo->FileAttributes = VBoxToNTFileAttributes(pDirEntry->Info.Attr.fMode);

                INIT_FILE_NAME(pDirInfo, pDirEntry->name);

                Log(("VBOXSF: QueryDirectory: FileIdBothDirectoryInformation cbAlloc = %x cbObject = %x\n", pDirEntry->Info.cbAllocated, pDirEntry->Info.cbObject));
                Log(("VBOXSF: QueryDirectory: FileIdBothDirectoryInformation CopySize = %d, name size=%d name len=%d\n", CopySize, pDirEntry->name.u16Size, pDirEntry->name.u16Length));
                Log(("VBOXSF: QueryDirectory: FileIdBothDirectoryInformation File name %.*ls (DirInfo)\n", pDirInfo->FileNameLength / sizeof(WCHAR), pDirInfo->FileName));
                Log(("VBOXSF: QueryDirectory: FileIdBothDirectoryInformation File name %.*ls (DirEntry)\n", pDirEntry->name.u16Size / sizeof(WCHAR), pDirEntry->name.String.ucs2));

                /* Align to 8 byte boundary. */
                CopySize = RT_ALIGN(CopySize, sizeof(LONGLONG));
                pDirInfo->NextEntryOffset = CopySize;
                pNextOffset = &pDirInfo->NextEntryOffset;
            }
            else
            {
                pDirInfo->NextEntryOffset = 0; /* Last item. */
                Status = STATUS_BUFFER_OVERFLOW;
            }
            break;
        }

        case FileNamesInformation:
        {
            PFILE_NAMES_INFORMATION pDirInfo = (PFILE_NAMES_INFORMATION)Buffer;
            CopySize = sizeof(FILE_NAMES_INFORMATION);

            /* Struct already contains one char for null terminator. */
            CopySize += pDirEntry->name.u16Size;

            Log(("VBOXSF: QueryDirectory: FileNamesInformation Buffer=%08x\n", Buffer));
            if (*pLengthRemaining >= CopySize)
            {
                RtlZeroMemory( pDirInfo, CopySize );

                pDirInfo->FileIndex = index;

                INIT_FILE_NAME(pDirInfo, pDirEntry->name);

                Log(("VBOXSF: QueryDirectory: FileNamesInformation File name %.*ls\n", pDirInfo->FileNameLength / sizeof(WCHAR), pDirInfo->FileName));

                /* Align to 8 byte boundary. */
                CopySize = RT_ALIGN(CopySize, sizeof(LONGLONG));
                pDirInfo->NextEntryOffset = CopySize;
                pNextOffset = &pDirInfo->NextEntryOffset;
            }
            else
            {
                pDirInfo->NextEntryOffset = 0; /* Last item. */
                Status = STATUS_BUFFER_OVERFLOW;
            }
            break;
        }

        default:
            Log(("VBOXSF: QueryDirectory: Invalid FS information class %d!\n", FileInformationClass));
            Status = STATUS_INVALID_PARAMETER;
            goto end;
        }

        cbHGCMBuffer -= cbEntry;
        pDirEntry = (PSHFLDIRINFO)((uintptr_t)pDirEntry + cbEntry);
        Log(("VBOXSF: QueryDirectory: %d bytes left in HGCM buffer\n", cbHGCMBuffer));

        if (*pLengthRemaining >= CopySize)
        {
            Buffer += CopySize;
            *pLengthRemaining -= CopySize;
        }
        else break;

        if (RxContext->QueryDirectory.ReturnSingleEntry)
            break;

        /* More left? */
        if (cbHGCMBuffer <= 0)
            break;

        index++; /* File Index. */

        cFiles--;
    }
    if (pNextOffset)
        *pNextOffset = 0; /* Last pDirInfo->NextEntryOffset should be set to zero! */

    end: if (pHGCMBuffer)
        vbsfFreeNonPagedMem(pHGCMBuffer);

    if (ParsedPath)
        vbsfFreeNonPagedMem(ParsedPath);

    Log(("VBOXSF: QueryDirectory: Returned 0x%x\n", Status));
    return (Status);
}

NTSTATUS VBoxMRxQueryVolumeInformationWithFullBuffer (IN OUT PRX_CONTEXT RxContext)
/*++

 Routine Description:

 This routine queries the volume information

 Arguments:

 pRxContext         - the RDBSS context

 FsInformationClass - the kind of Fs information desired.

 pBuffer            - the buffer for copying the information

 pBufferLength      - the buffer length ( set to buffer length on input and set
 to the remaining length on output)

 Return Value:

 NTSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_INVALID_PARAMETER;
    RxCaptureFcb;
    RxCaptureFobx;
    VBoxMRxGetNetRootExtension(capFcb->pNetRoot, pNetRootExtension);
    VBoxMRxGetDeviceExtension(RxContext, pDeviceExtension);
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(capFobx);
    ULONG OriginalLength = RxContext->Info.LengthRemaining;
    FS_INFORMATION_CLASS FsInformationClass = RxContext->Info.FsInformationClass;
    PVOID OriginalBuffer = RxContext->Info.Buffer;
    ULONG BytesToCopy, cbMaxSize;

    if (NULL != pVBoxFobx)
        Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: vboxFobx = %p, Handle = 0x%08x\n", pVBoxFobx, pVBoxFobx->hFile));

    cbMaxSize = RxContext->Info.Length;

    switch (FsInformationClass)
    {
    case FileFsVolumeInformation:
    {
        PFILE_FS_VOLUME_INFORMATION pVolInfo = (PFILE_FS_VOLUME_INFORMATION)OriginalBuffer;
        PWCHAR pRootName;
        ULONG RootNameLength;
        PMRX_NET_ROOT pNetRoot = capFcb->pNetRoot;
        PMRX_SRV_CALL pSrvCall = pNetRoot->pSrvCall;
        PSHFLVOLINFO pSHFLVolInfo;
        uint32_t cbHGCMBuffer;
        uint8_t *pHGCMBuffer = NULL;
        int vboxRC;

        Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsVolumeInformation: OriginalBuffer = %p, LenRemaining = %d\n", OriginalBuffer, RxContext->Info.LengthRemaining));
        if (RxContext->Info.LengthRemaining < sizeof(FILE_FS_VOLUME_INFORMATION))
        {
            Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsVolumeInformation: Insufficient buffer size (%d)\n", RxContext->Info.LengthRemaining));
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        if (NULL == pVBoxFobx)
        {
            Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: pVBoxFobx is invalid!\n"));
            return STATUS_INVALID_PARAMETER;
        }

        RtlZeroMemory( pVolInfo, sizeof(FILE_FS_VOLUME_INFORMATION) );

        RootNameLength = pNetRoot->pNetRootName->Length - pSrvCall->pSrvCallName->Length;
        RootNameLength -= 2; /* remove leading backslash */

        pRootName = (PWCHAR)(pNetRoot->pNetRootName->Buffer + (pSrvCall->pSrvCallName->Length / sizeof(WCHAR)));
        pRootName++; /* remove leading backslash */

        Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsVolumeInformation: Root name = %.*ls\n", RootNameLength / sizeof(WCHAR), pRootName));
        Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsVolumeInformation: Prefix %ls has %d elements\n", VBOX_VOLNAME_PREFIX, RT_ELEMENTS(VBOX_VOLNAME_PREFIX)));

        /* Query serial number. */
        cbHGCMBuffer = sizeof(SHFLVOLINFO);
        pHGCMBuffer = (uint8_t *)vbsfAllocNonPagedMem(cbHGCMBuffer);
        if (pHGCMBuffer == 0)
        {
            AssertFailed();
            Status = STATUS_NO_MEMORY;
            break;
        }

        vboxRC = vboxCallFSInfo(&pDeviceExtension->hgcmClient, &pNetRootExtension->map, pVBoxFobx->hFile, SHFL_INFO_GET | SHFL_INFO_VOLUME, &cbHGCMBuffer, (PSHFLDIRINFO)pHGCMBuffer);
        AssertRC(vboxRC);

        if (vboxRC != VINF_SUCCESS)
        {
            Status = VBoxErrorToNTStatus(vboxRC);
            vbsfFreeNonPagedMem(pHGCMBuffer);
            break;
        }

        pSHFLVolInfo = (PSHFLVOLINFO)pHGCMBuffer;
        pVolInfo->VolumeSerialNumber = pSHFLVolInfo->ulSerial;
        vbsfFreeNonPagedMem(pHGCMBuffer);

        pVolInfo->VolumeCreationTime.QuadPart = 0;
        pVolInfo->VolumeLabelLength = RootNameLength + VBOX_VOLNAME_PREFIX_SIZE;
        pVolInfo->SupportsObjects = FALSE;

        RxContext->Info.LengthRemaining -= FIELD_OFFSET(FILE_FS_VOLUME_INFORMATION, VolumeLabel[0]);

        if (RxContext->Info.LengthRemaining >= (LONG)pVolInfo->VolumeLabelLength)
        {
            BytesToCopy = pVolInfo->VolumeLabelLength;

            /* Copy prefix */
            RtlCopyMemory( &pVolInfo->VolumeLabel[0], (PVOID)VBOX_VOLNAME_PREFIX, VBOX_VOLNAME_PREFIX_SIZE);

            /* Copy share name (- 1 for zero terminator) */
            RtlCopyMemory( &pVolInfo->VolumeLabel[RT_ELEMENTS(VBOX_VOLNAME_PREFIX) - 1], (PVOID)pRootName, RootNameLength );
        }
        else
        {
            BytesToCopy = RxContext->Info.LengthRemaining;

            /* Copy prefix */
            RtlCopyMemory( &pVolInfo->VolumeLabel[0], (PVOID)VBOX_VOLNAME_PREFIX, RT_MIN(BytesToCopy, VBOX_VOLNAME_PREFIX_SIZE) );

            if (BytesToCopy > VBOX_VOLNAME_PREFIX_SIZE)
            {
                /* Copy share name (- 1 for zero terminator) */
                RtlCopyMemory( &pVolInfo->VolumeLabel[RT_ELEMENTS(VBOX_VOLNAME_PREFIX) - 1], (PVOID)pRootName, BytesToCopy - VBOX_VOLNAME_PREFIX_SIZE );
            }
        }

        RxContext->Info.LengthRemaining -= BytesToCopy;
        pVolInfo->VolumeLabelLength = BytesToCopy;

        Status = STATUS_SUCCESS;
        break;
    }

    case FileFsLabelInformation:
    {
        PFILE_FS_LABEL_INFORMATION pLabelInfo = (PFILE_FS_LABEL_INFORMATION)OriginalBuffer;
        PWCHAR pRootName;
        ULONG RootNameLength;
        PMRX_NET_ROOT pNetRoot = capFcb->pNetRoot;
        PMRX_SRV_CALL pSrvCall = pNetRoot->pSrvCall;

        Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsLabelInformation: OriginalBuffer = %p, LenRemaining = %d\n", OriginalBuffer, RxContext->Info.LengthRemaining));
        if (RxContext->Info.LengthRemaining < sizeof(PFILE_FS_LABEL_INFORMATION))
        {
            Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsLabelInformation: Insufficient buffer size (%d)\n", RxContext->Info.LengthRemaining));
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        RtlZeroMemory( pLabelInfo, sizeof(FILE_FS_LABEL_INFORMATION) );

        RootNameLength = pNetRoot->pNetRootName->Length - pSrvCall->pSrvCallName->Length;
        RootNameLength -= 2; /* remove leading backslash */

        pRootName = (PWCHAR)(pNetRoot->pNetRootName->Buffer + (pSrvCall->pSrvCallName->Length / sizeof(WCHAR)));
        pRootName++; /* remove leading backslash */
        Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsLabelInformation: Root name = %.*ls\n", RootNameLength / sizeof(WCHAR), pRootName));
        Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsLabelInformation: Prefix %ls has %d elements\n", VBOX_VOLNAME_PREFIX, RT_ELEMENTS(VBOX_VOLNAME_PREFIX)));

        pLabelInfo->VolumeLabelLength = RootNameLength + VBOX_VOLNAME_PREFIX_SIZE;
        RxContext->Info.LengthRemaining -= FIELD_OFFSET(FILE_FS_LABEL_INFORMATION, VolumeLabel[0]);

        if (RxContext->Info.LengthRemaining >= (LONG)pLabelInfo->VolumeLabelLength)
        {
            BytesToCopy = pLabelInfo->VolumeLabelLength;

            /* Copy prefix */
            RtlCopyMemory( &pLabelInfo->VolumeLabel[0], (PVOID)VBOX_VOLNAME_PREFIX, VBOX_VOLNAME_PREFIX_SIZE);

            /* Copy share name (- 1 for zero terminator) */
            RtlCopyMemory( &pLabelInfo->VolumeLabel[RT_ELEMENTS(VBOX_VOLNAME_PREFIX) - 1], (PVOID)pRootName, RootNameLength );
        }
        else
        {
            BytesToCopy = RxContext->Info.LengthRemaining;

            /* Copy prefix */
            RtlCopyMemory( &pLabelInfo->VolumeLabel[0], (PVOID)VBOX_VOLNAME_PREFIX, RT_MIN(BytesToCopy, VBOX_VOLNAME_PREFIX_SIZE) );

            if (BytesToCopy > VBOX_VOLNAME_PREFIX_SIZE)
            {
                /* Copy share name (- 1 for zero terminator) */
                RtlCopyMemory( &pLabelInfo->VolumeLabel[RT_ELEMENTS(VBOX_VOLNAME_PREFIX) - 1], (PVOID)pRootName, BytesToCopy - VBOX_VOLNAME_PREFIX_SIZE );
            }
        }
        RxContext->Info.LengthRemaining -= BytesToCopy;
        pLabelInfo->VolumeLabelLength = BytesToCopy;

        Status = STATUS_SUCCESS;
        break;
    }

    case FileFsFullSizeInformation:
    case FileFsSizeInformation:
    {
        PFILE_FS_FULL_SIZE_INFORMATION pFullSizeInfo = (PFILE_FS_FULL_SIZE_INFORMATION)OriginalBuffer;
        PFILE_FS_SIZE_INFORMATION pSizeInfo = (PFILE_FS_SIZE_INFORMATION)OriginalBuffer;
        uint32_t cbHGCMBuffer;
        uint8_t *pHGCMBuffer = NULL;
        int vboxRC;
        PSHFLVOLINFO pVolInfo;

        if (FsInformationClass == FileFsFullSizeInformation)
        {
            Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsFullSizeInformation: OriginalBuffer = %p, LenRemaining = %d\n", OriginalBuffer, RxContext->Info.LengthRemaining));
            BytesToCopy = sizeof(FILE_FS_FULL_SIZE_INFORMATION);
        }
        else
        {
            Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsSizeInformation: OriginalBuffer = %p, LenRemaining = %d\n", OriginalBuffer, RxContext->Info.LengthRemaining));
            BytesToCopy = sizeof(FILE_FS_SIZE_INFORMATION);
        }

        if (RxContext->Info.LengthRemaining < (LONG)BytesToCopy)
        {
            Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsFullSizeInformation/FileFsSizeInformation: Insufficient buffer size (%d)!\n", RxContext->Info.LengthRemaining));
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        if (NULL == pVBoxFobx)
        {
            Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: pVBoxFobx is invalid!\n"));
            return STATUS_INVALID_PARAMETER;
        }

        RtlZeroMemory( pSizeInfo, BytesToCopy );

        cbHGCMBuffer = sizeof(SHFLVOLINFO);
        pHGCMBuffer = (uint8_t *)vbsfAllocNonPagedMem(cbHGCMBuffer);
        if (pHGCMBuffer == 0)
        {
            AssertFailed();
            Status = STATUS_NO_MEMORY;
            break;
        }

        vboxRC = vboxCallFSInfo(&pDeviceExtension->hgcmClient, &pNetRootExtension->map, pVBoxFobx->hFile, SHFL_INFO_GET | SHFL_INFO_VOLUME, &cbHGCMBuffer, (PSHFLDIRINFO)pHGCMBuffer);
        AssertRC(vboxRC);

        if (vboxRC != VINF_SUCCESS)
        {
            Status = VBoxErrorToNTStatus(vboxRC);
            vbsfFreeNonPagedMem(pHGCMBuffer);
            break;
        }

        pVolInfo = (PSHFLVOLINFO)pHGCMBuffer;

        if (FsInformationClass == FileFsFullSizeInformation)
        {
            pFullSizeInfo->SectorsPerAllocationUnit = pVolInfo->ulBytesPerAllocationUnit / pVolInfo->ulBytesPerSector;
            pFullSizeInfo->BytesPerSector = pVolInfo->ulBytesPerSector;
            pFullSizeInfo->ActualAvailableAllocationUnits.QuadPart = pVolInfo->ullAvailableAllocationBytes / pVolInfo->ulBytesPerAllocationUnit;
            pFullSizeInfo->CallerAvailableAllocationUnits.QuadPart = pFullSizeInfo->ActualAvailableAllocationUnits.QuadPart;
            pFullSizeInfo->TotalAllocationUnits.QuadPart = pVolInfo->ullTotalAllocationBytes / pVolInfo->ulBytesPerAllocationUnit;

            Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsFullSizeInformation: SectorsPerAllocationUnit       %08x\n", (ULONG)pFullSizeInfo->SectorsPerAllocationUnit));
            Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsFullSizeInformation: BytesPerSector                 %08x\n", (ULONG)pFullSizeInfo->BytesPerSector));
            Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsFullSizeInformation: ActualAvailableAllocationUnits %RX64\n", pFullSizeInfo->ActualAvailableAllocationUnits.QuadPart));
            Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsFullSizeInformation: TotalAllocationUnits           %RX64\n", pFullSizeInfo->TotalAllocationUnits.QuadPart));
        }
        else
        {
            pSizeInfo->SectorsPerAllocationUnit = pVolInfo->ulBytesPerAllocationUnit / pVolInfo->ulBytesPerSector;
            pSizeInfo->BytesPerSector = pVolInfo->ulBytesPerSector;
            pSizeInfo->AvailableAllocationUnits.QuadPart = pVolInfo->ullAvailableAllocationBytes / pVolInfo->ulBytesPerAllocationUnit;
            pSizeInfo->TotalAllocationUnits.QuadPart = pVolInfo->ullTotalAllocationBytes / pVolInfo->ulBytesPerAllocationUnit;

            Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsSizeInformation: SectorsPerAllocationUnit       %08x\n", (ULONG)pSizeInfo->SectorsPerAllocationUnit));
            Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsSizeInformation: BytesPerSector                 %08x\n", (ULONG)pSizeInfo->BytesPerSector));
            Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsSizeInformation: AvailableAllocationUnits       %RX64\n", pSizeInfo->AvailableAllocationUnits.QuadPart));
            Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsSizeInformation: TotalAllocationUnits           %RX64\n", pSizeInfo->TotalAllocationUnits.QuadPart));
        }

        if (pHGCMBuffer)
            vbsfFreeNonPagedMem(pHGCMBuffer);

        RxContext->Info.LengthRemaining -= BytesToCopy;
        Status = STATUS_SUCCESS;
        break;
    }

    case FileFsDeviceInformation:
    {
        PFILE_FS_DEVICE_INFORMATION pDevInfo = (PFILE_FS_DEVICE_INFORMATION)OriginalBuffer;
        PMRX_NET_ROOT NetRoot = capFcb->pNetRoot;

        Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsDeviceInformation: Type = 0x%x\n", NetRoot->DeviceType));
        Status = STATUS_SUCCESS;

        BytesToCopy = sizeof(FILE_FS_DEVICE_INFORMATION);
        if (RxContext->Info.LengthRemaining >= (LONG)BytesToCopy)
        {
            pDevInfo->DeviceType = NetRoot->DeviceType;
            pDevInfo->Characteristics = FILE_REMOTE_DEVICE;

            if (NetRoot->Type == NET_ROOT_PIPE)
            {
                NetRoot->DeviceType = RxDeviceType(NAMED_PIPE);
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            RxContext->Info.LengthRemaining -= BytesToCopy;
        }
        else
        {
            Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsDeviceInformation: Buffer too small (%d vs %d)!\n", RxContext->Info.LengthRemaining, BytesToCopy));
            Status = STATUS_BUFFER_TOO_SMALL;
        }
        break;
    }

    case FileFsAttributeInformation:
    {
        PFILE_FS_ATTRIBUTE_INFORMATION pAttribInfo = (PFILE_FS_ATTRIBUTE_INFORMATION)OriginalBuffer;

        BytesToCopy = sizeof(MRX_VBOX_FILESYS_NAME_U) + FIELD_OFFSET(FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName[0]);
        Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsAttributeInformation: BytesToCopy = %d\n", BytesToCopy));
        if (RxContext->Info.LengthRemaining < (LONG)BytesToCopy)
        {
            Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsAttributeInformation: Buffer too small (%d)!\n", RxContext->Info.LengthRemaining));
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        /** @todo set unicode, case sensitive etc? */
        pAttribInfo->FileSystemAttributes = 0;
        pAttribInfo->MaximumComponentNameLength = 255; /** @todo should query from the host */
        pAttribInfo->FileSystemNameLength = sizeof(MRX_VBOX_FILESYS_NAME_U);
        RtlCopyMemory( pAttribInfo->FileSystemName, MRX_VBOX_FILESYS_NAME_U, pAttribInfo->FileSystemNameLength);

        RxContext->Info.LengthRemaining -= BytesToCopy;
        Status = STATUS_SUCCESS;
        break;
    }

    case FileFsControlInformation:
        Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsControlInformation (skipping ...)\n"));
        break;

    case FileFsObjectIdInformation:
        Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsObjectIdInformation (skipping ...)\n"));
        break;

    case FileFsMaximumInformation:
        Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: FileFsMaximumInformation (skipping ...)\n"));
        break;

    default:
        Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: Invalid parameter!\n"));
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    Log(("VBOXSF: VBoxMRxQueryVolumeInformationWithFullBuffer: Returned 0x%x\n", Status));
    return (Status);
}

NTSTATUS VBoxMRxQueryVolumeInformation (IN OUT PRX_CONTEXT RxContext)
/*++

 Routine Description:

 This routine queries the volume information. Since the NT server does not
 handle bufferoverflow gracefully on query-fs-info, we allocate a buffer here
 that is big enough to hold anything passed back; then we call the "real"
 queryvolinfo routine.

 Arguments:

 pRxContext         - the RDBSS context

 Return Value:

 RXSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status;
    RxCaptureFcb;
    RxCaptureFobx;
    PVOID OriginalBuffer = RxContext->Info.Buffer;
    ULONG OriginalLength = RxContext->Info.LengthRemaining;
    ULONG ReturnedLength;

    BOOLEAN UsingSideBuffer = FALSE;
    uint32_t cbSideBuffer = 0;

    typedef struct
    {
        union
        {
            FILE_FS_LABEL_INFORMATION labelinfo;
            FILE_FS_VOLUME_INFORMATION volumeinfo;
            FILE_FS_SIZE_INFORMATION sizeinfo;
            FILE_FS_DEVICE_INFORMATION deviceinfo;
            FILE_FS_ATTRIBUTE_INFORMATION attributeinfo;
        } Info;
        WCHAR VolumeName[MAXIMUM_FILENAME_LENGTH];
    } SideBuffer;

    SideBuffer *pSideBuffer = NULL;

    PAGED_CODE();

    Log(
        ("VBOXSF: VBoxMRxQueryVolumeInformation: rxContextBufferAddr = 0x%x, rxContextBufferLengthRem = %d, (SideBufferSize = %d)\n", RxContext->Info.Buffer, RxContext->Info.LengthRemaining, sizeof(SideBuffer)));
    if (RxContext->Info.LengthRemaining < sizeof(SideBuffer))
    {
        /* I replace the buffer and length in the context with my stuff. */
        Log(("VBOXSF: VBoxMRxQueryVolumeInformation: Using Sidebuffer ...\n"));
        UsingSideBuffer = TRUE;
        RxContext->Info.Buffer = vbsfAllocNonPagedMem(sizeof(SideBuffer));
        pSideBuffer = (SideBuffer *)RxContext->Info.Buffer;
        RxContext->Info.LengthRemaining = sizeof(SideBuffer);
    }

    Status = VBoxMRxQueryVolumeInformationWithFullBuffer(RxContext);
    Assert(Status != STATUS_PENDING);

    Log(("VBOXSF: VBoxMRxQueryVolumeInformation: RxContext->Info.LengthRemaining = %d\n", RxContext->Info.LengthRemaining));

    if (Status != STATUS_SUCCESS)
    {
        RxContext->Info.Buffer = OriginalBuffer;
        RxContext->Info.LengthRemaining = OriginalLength;

        Log(("VBOXSF: VBoxMRxQueryVolumeInformation: VBoxMRxQueryVolumeInformationWithFullBuffer failed with 0x%x\n", Status));
        goto FINALLY;
    }

    if (UsingSideBuffer == TRUE)
    {
        ReturnedLength = sizeof(SideBuffer) - RxContext->Info.LengthRemaining;
    }
    else
    {
        ReturnedLength = OriginalLength - RxContext->Info.LengthRemaining;
    }

    Log(("VBOXSF: VBoxMRxQueryVolumeInformation: ReturnedLength = %d, OriginalLength = %d\n", ReturnedLength, OriginalLength));
    if (ReturnedLength > OriginalLength)
    {
        Status = STATUS_BUFFER_OVERFLOW;
        ReturnedLength = OriginalLength;
    }

    if (UsingSideBuffer == TRUE)
    {
        RtlCopyMemory(OriginalBuffer, pSideBuffer, ReturnedLength);
    }

    RxContext->Info.Buffer = OriginalBuffer;
    RxContext->Info.LengthRemaining = OriginalLength - ReturnedLength;
    Log(("VBOXSF: VBoxMRxQueryVolumeInformation: OriginalLength = %d, LengthRemaining = %d, Status = 0x%x\n", OriginalLength, RxContext->Info.LengthRemaining, Status));
    Log(("VBOXSF: VBoxMRxQueryVolumeInformation: Final rxContextBufferAddr = 0x%x, rxContextBufferLengthRem = %d\n", RxContext->Info.Buffer, RxContext->Info.LengthRemaining));

    FINALLY:

    if (NULL != pSideBuffer)
        vbsfFreeNonPagedMem(pSideBuffer);

    return Status;
}

NTSTATUS VBoxMRxSetVolumeInformation (IN OUT PRX_CONTEXT pRxContext)
/*++

 Routine Description:

 This routine sets the volume information

 Arguments:

 pRxContext - the RDBSS context

 FsInformationClass - the kind of Fs information desired.

 pBuffer            - the buffer for copying the information

 BufferLength       - the buffer length

 Return Value:

 NTSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_NOT_IMPLEMENTED;

    Log(("VBOXSF: VBoxMRxSetVolumeInformation called.\n"));
    return (Status);
}

NTSTATUS VBoxMRxQueryFileInformation (IN PRX_CONTEXT RxContext)
/*++

 Routine Description:

 This routine does a query file info. Only the NT-->NT path is implemented.

 The NT-->NT path works by just remoting the call basically without further ado.

 Arguments:

 RxContext - the RDBSS context

 Return Value:

 NTSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    ULONG CopySize = 0;
    RxCaptureFcb;
    RxCaptureFobx;
    FILE_INFORMATION_CLASS FunctionalityRequested = RxContext->Info.FileInformationClass;

    VBoxMRxGetNetRootExtension(capFcb->pNetRoot, pNetRootExtension);
    VBoxMRxGetDeviceExtension(RxContext, pDeviceExtension);
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(capFobx);
    PUNICODE_STRING FileName = GET_ALREADY_PREFIXED_NAME_FROM_CONTEXT(RxContext);

    uint8_t *pHGCMBuffer = 0;
    int vboxRC = 0;
    uint32_t cbHGCMBuffer, cbMaxSize = 0;
    PSHFLFSOBJINFO pFileEntry = NULL;
    PCHAR pInfoBuffer = NULL;
    ULONG *pLengthRemaining = NULL;

    pInfoBuffer = (PCHAR)RxContext->Info.Buffer;
    cbMaxSize = RxContext->Info.Length;
    pLengthRemaining = (PULONG) & RxContext->Info.LengthRemaining;

    if (NULL == pLengthRemaining)
    {
        Log(("VBOXSF: VBoxMRxQueryFileInformation: Invalid length pointer detected!\n"));
        return STATUS_INVALID_PARAMETER;
    }

    Log(("VBOXSF: VBoxMRxQueryFileInformation: InfoBuffer = 0x%08x, Size = %d bytes, LenRemain = %d bytes\n", pInfoBuffer, cbMaxSize, *pLengthRemaining));

    if (NULL == pVBoxFobx)
    {
        Log(("VBOXSF: VBoxMRxQueryFileInformation: pVBoxFobx is invalid!\n"));
        return STATUS_INVALID_PARAMETER;
    }

    if (NULL == pInfoBuffer)
    {
        Log(("VBOXSF: VBoxMRxQueryFileInformation: Invalid information buffer detected!\n"));
        return STATUS_INVALID_PARAMETER;
    }

    if (pVBoxFobx->FileStandardInfo.Directory == TRUE)
    {
        Log(("VBOXSF: VBoxMRxQueryFileInformation: Directory -> Copy info retrieved during the create call\n"));
        Status = STATUS_SUCCESS;

        switch (FunctionalityRequested)
        {

        case FileBasicInformation:
        {
            PFILE_BASIC_INFORMATION pFileInfo = (PFILE_BASIC_INFORMATION)pInfoBuffer;
            CopySize = sizeof(FILE_BASIC_INFORMATION);

            Log(("VBOXSF: VBoxMRxQueryFileInformation: FileBasicInformation: Buffer = 0x%08x\n", pFileInfo));
            if (*pLengthRemaining >= CopySize)
            {
                *pFileInfo = pVBoxFobx->FileBasicInfo;
                Log(("VBOXSF: VBoxMRxQueryFileInformation: FileBasicInformation: File attributes: 0x%x\n", pFileInfo->FileAttributes));
            }
            else
            {
                Log(("VBOXSF: VBoxMRxQueryFileInformation: FileBasicInformation: Buffer too small (%d vs %d)!\n", *pLengthRemaining, CopySize));
                Status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        case FileStandardInformation:
        {
            PFILE_STANDARD_INFORMATION pFileInfo = (PFILE_STANDARD_INFORMATION)pInfoBuffer;
            CopySize = sizeof(FILE_STANDARD_INFORMATION);

            Log(("VBOXSF: VBoxMRxQueryFileInformation: FileStandardInformation: Buffer = 0x%08x\n", pFileInfo));
            if (*pLengthRemaining >= CopySize)
            {
                *pFileInfo = pVBoxFobx->FileStandardInfo;
            }
            else
            {
                Log(("VBOXSF: VBoxMRxQueryFileInformation: FileStandardInformation: Buffer too small (%d vs %d)!\n", *pLengthRemaining, CopySize));
                Status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        case FileNamesInformation:
        {
            PFILE_NAMES_INFORMATION pFileInfo = (PFILE_NAMES_INFORMATION)pInfoBuffer;
            CopySize = sizeof(FILE_NAMES_INFORMATION);

            /* Struct already contains one char for null terminator. */
            CopySize += (FileName->Length + 1) * sizeof(WCHAR);

            Log(("VBOXSF: VBoxMRxQueryFileInformation: FileNamesInformation: Buffer = %08x\n", pFileInfo));
            if (*pLengthRemaining >= CopySize)
            {
                RtlZeroMemory (pFileInfo, CopySize);

                pFileInfo->FileIndex = 0;
                pFileInfo->FileNameLength = FileName->Length * sizeof(WCHAR);
                RtlCopyMemory(pFileInfo->FileName, FileName->Buffer, pFileInfo->FileNameLength);
            }
            else
            {
                Log(("VBOXSF: VBoxMRxQueryFileInformation: FileNamesInformation: Buffer too small (%d vs %d)!\n", *pLengthRemaining, CopySize));
                Status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        case FileInternalInformation:
        {
            PFILE_INTERNAL_INFORMATION UsersBuffer = (PFILE_INTERNAL_INFORMATION)pInfoBuffer;
            Log(("VBOXSF: VBoxMRxQueryFileInformation: FileInternalInformation: Buffer = 0x%08x\n", pInfoBuffer));

            /*
             *  Note: We use the address of the FCB to determine the
             *  index number of the file.  If we have to maintain persistence between
             *  file opens for this request, then we might have to do something
             *  like checksuming the reserved fields on a FUNIQUE SMB response.
             *

             *
             * NT64: the address of capFcb used to be stuffed into
             *       IndexNumber.LowPart, with HighPart being zeroed.
             *
             *       Whoever is asking for this pointer value should be
             *       prepared to deal with the returned 64-bit value.
             */

            CopySize = sizeof(FILE_INTERNAL_INFORMATION);

            if (*pLengthRemaining >= CopySize)
            {
                UsersBuffer->IndexNumber.QuadPart = (ULONG_PTR)capFcb;
            }
            else
            {
                Log(("VBOXSF: VBoxMRxQueryFileInformation: FileInternalInformation: Buffer overflow (%d vs %d)!\n", *pLengthRemaining, CopySize));
                Status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }
        case FileEaInformation:
        {
            PFILE_EA_INFORMATION EaBuffer = (PFILE_EA_INFORMATION)pInfoBuffer;
            Log(("VBOXSF: VBoxMRxQueryFileInformation: FileEaInformation: Buffer = 0x%08x\n", pInfoBuffer));

            CopySize = sizeof(FILE_EA_INFORMATION);
            if (*pLengthRemaining >= CopySize)
            {
                EaBuffer->EaSize = 0;
            }
            else
            {
                Log(("VBOXSF: VBoxMRxQueryFileInformation: FileEaInformation: Buffer overflow (%d vs %d)!\n", *pLengthRemaining, CopySize));
                Status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        case FileNetworkOpenInformation:
        {
            PFILE_NETWORK_OPEN_INFORMATION pFileInfo = (PFILE_NETWORK_OPEN_INFORMATION)pInfoBuffer;
            Log(("VBOXSF: VBoxMRxQueryFileInformation: FileNetworkOpenInformation: Buffer = 0x%08x\n", pInfoBuffer));

            CopySize = sizeof(FILE_NETWORK_OPEN_INFORMATION);
            if (*pLengthRemaining >= CopySize)
            {
                pFileInfo->CreationTime = pVBoxFobx->FileBasicInfo.CreationTime;
                pFileInfo->LastAccessTime = pVBoxFobx->FileBasicInfo.LastAccessTime;
                pFileInfo->LastWriteTime = pVBoxFobx->FileBasicInfo.LastWriteTime;
                pFileInfo->ChangeTime = pVBoxFobx->FileBasicInfo.ChangeTime;
                pFileInfo->AllocationSize.QuadPart = 0; /* This is a directory, attribute not needed. */
                pFileInfo->EndOfFile.QuadPart = 0; /* This is a directory, attribute not needed. */
                pFileInfo->FileAttributes = pVBoxFobx->FileBasicInfo.FileAttributes;
            }
            else
            {
                Log(("VBOXSF: VBoxMRxQueryFileInformation: FileNetworkOpenInformation: Buffer too small (%d vs %d)!\n", *pLengthRemaining, CopySize));
                Status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        default:

            Log(("VBOXSF: VBoxMRxQueryFileInformation: Invalid filesystem information class %d!\n", FunctionalityRequested));
            Status = STATUS_NOT_IMPLEMENTED;
            goto end;
        }
    }
    else /* Entry is a file. */
    {
        cbHGCMBuffer = RT_MAX(cbMaxSize, PAGE_SIZE);
        pHGCMBuffer = (uint8_t *)vbsfAllocNonPagedMem(cbHGCMBuffer);

        if (pHGCMBuffer == 0)
            return STATUS_NO_MEMORY;

        Assert(pVBoxFobx && pNetRootExtension && pDeviceExtension);
        vboxRC = vboxCallFSInfo(&pDeviceExtension->hgcmClient, &pNetRootExtension->map, pVBoxFobx->hFile, SHFL_INFO_GET | SHFL_INFO_FILE, &cbHGCMBuffer, (PSHFLDIRINFO)pHGCMBuffer);

        if (vboxRC != VINF_SUCCESS)
        {
            Status = VBoxErrorToNTStatus(vboxRC);
            goto end;
        }

        pFileEntry = (PSHFLFSOBJINFO)pHGCMBuffer;
        Status = STATUS_SUCCESS;

        switch (FunctionalityRequested)
        {

        case FileBasicInformation:
        {
            PFILE_BASIC_INFORMATION pFileInfo = (PFILE_BASIC_INFORMATION)pInfoBuffer;
            CopySize = sizeof(FILE_BASIC_INFORMATION);

            Log(("VBOXSF: VBoxMRxQueryFileInformation: FileBasicInformation: Buffer = 0x%08x\n", pInfoBuffer));
            if (*pLengthRemaining >= CopySize)
            {
                pFileInfo->CreationTime.QuadPart = RTTimeSpecGetNtTime(&pFileEntry->BirthTime); /* Ridiculous name. */
                pFileInfo->LastAccessTime.QuadPart = RTTimeSpecGetNtTime(&pFileEntry->AccessTime);
                pFileInfo->LastWriteTime.QuadPart = RTTimeSpecGetNtTime(&pFileEntry->ModificationTime);
                pFileInfo->ChangeTime.QuadPart = RTTimeSpecGetNtTime(&pFileEntry->ChangeTime);

                pFileInfo->FileAttributes = VBoxToNTFileAttributes(pFileEntry->Attr.fMode);
                Log(("VBOXSF: VBoxMRxQueryFileInformation: FileBasicInformation: File attributes = 0x%x\n", pFileInfo->FileAttributes));
            }
            else
            {
                Log(("VBOXSF: VBoxMRxQueryFileInformation: FileBasicInformation: Buffer too small (%d vs %d)!\n", *pLengthRemaining, CopySize));
                Status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        case FileStandardInformation:
        {
            PFILE_STANDARD_INFORMATION pFileInfo = (PFILE_STANDARD_INFORMATION)pInfoBuffer;
            CopySize = sizeof(FILE_STANDARD_INFORMATION);

            Log(("VBOXSF: VBoxMRxQueryFileInformation: FileStandardInformation: Buffer = 0x%08x\n", pInfoBuffer));
            if (*pLengthRemaining >= CopySize)
            {
                pFileInfo->AllocationSize.QuadPart = pFileEntry->cbAllocated;
                pFileInfo->EndOfFile.QuadPart = pFileEntry->cbObject;

                pFileInfo->NumberOfLinks = 1; /* @todo 0? */
                pFileInfo->DeletePending = FALSE;

                if (pFileEntry->Attr.fMode & RTFS_DOS_DIRECTORY)
                    pFileInfo->Directory = TRUE;
                else pFileInfo->Directory = FALSE;
            }
            else
            {
                Log(("VBOXSF: VBoxMRxQueryFileInformation: FileStandardInformation: Buffer too small (%d vs %d)!\n", *pLengthRemaining, CopySize));
                Status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        case FileNamesInformation:
        {
            PFILE_NAMES_INFORMATION pFileInfo = (PFILE_NAMES_INFORMATION)pInfoBuffer;

            CopySize = sizeof(FILE_NAMES_INFORMATION);

            /* Struct already contains one char for null terminator */
            CopySize += (FileName->Length + 1) * sizeof(WCHAR);

            Log(("VBOXSF: VBoxMRxQueryFileInformation: FileNamesInformation: Buffer = 0x%08x\n", pInfoBuffer));
            if (*pLengthRemaining >= CopySize)
            {
                RtlZeroMemory (pFileInfo, CopySize);

                pFileInfo->FileIndex = 0;
                pFileInfo->FileNameLength = FileName->Length * sizeof(WCHAR);
                RtlCopyMemory(pFileInfo->FileName, FileName->Buffer, pFileInfo->FileNameLength);
            }
            else
            {
                Log(("VBOXSF: VBoxMRxQueryFileInformation: FileNamesInformation: Buffer too small (%d vs %d)!\n", *pLengthRemaining, CopySize));
                Status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        case FileInternalInformation:
        {
            PFILE_INTERNAL_INFORMATION UsersBuffer = (PFILE_INTERNAL_INFORMATION)pInfoBuffer;
            Log(("VBOXSF: VBoxMRxQueryFileInformation: FileInternalInformation: Buffer = 0x%08x\n", pInfoBuffer));

            /* Note: We use the address of the FCB to determine the
             index number of the file.  If we have to maintain persistence between
             file opens for this request, then we might have to do something
             like checksuming the reserved fields on a FUNIQUE SMB response.

             NT64: the address of capFcb used to be stuffed into
             IndexNumber.LowPart, with HighPart being zeroed.

             Whoever is asking for this pointer value should be
             prepared to deal with the returned 64-bit value. */
            CopySize = sizeof(FILE_INTERNAL_INFORMATION);

            if (*pLengthRemaining >= CopySize)
            {
                UsersBuffer->IndexNumber.QuadPart = (ULONG_PTR)capFcb;
            }
            else
            {
                Log(("VBOXSF: VBoxMRxQueryFileInformation: FileInternalInformation: Buffer too small (%d vs %d)!\n", *pLengthRemaining, CopySize));
                Status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        case FileEaInformation:
        {
            PFILE_EA_INFORMATION EaBuffer = (PFILE_EA_INFORMATION)pInfoBuffer;

            Log(("VBOXSF: VBoxMRxQueryFileInformation: FileEaInformation: Buffer = 0x%08x\n", pInfoBuffer));
            CopySize = sizeof(FILE_EA_INFORMATION);

            if (*pLengthRemaining >= CopySize)
            {
                EaBuffer->EaSize = 0;
            }
            else
            {
                Log(("VBOXSF: VBoxMRxQueryFileInformation: FileEaInformation: Buffer too small (%d vs %d)!\n", *pLengthRemaining, CopySize));
                Status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        case FileAttributeTagInformation:
        {
            PFILE_ATTRIBUTE_TAG_INFORMATION pAttrTag = (PFILE_ATTRIBUTE_TAG_INFORMATION)pInfoBuffer;

            Log(("VBOXSF: VBoxMRxQueryFileInformation: FileAttributeTagInformation: Buffer = 0x%08x\n", pInfoBuffer));
            CopySize = sizeof(FILE_ATTRIBUTE_TAG_INFORMATION);
            if (*pLengthRemaining >= CopySize)
            {
                pAttrTag->FileAttributes = VBoxToNTFileAttributes(pFileEntry->Attr.fMode);
                pAttrTag->ReparseTag = 0;
            }
            else
            {
                Log(("VBOXSF: VBoxMRxQueryFileInformation: FileAttributeTagInformation: Buffer too small (%d vs %d)!\n", *pLengthRemaining, CopySize));
                Status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        case FileEndOfFileInformation:
        {
            PFILE_END_OF_FILE_INFORMATION pEndOfFileInfo = (PFILE_END_OF_FILE_INFORMATION)pInfoBuffer;

            Log(("VBOXSF: VBoxMRxQueryFileInformation: FileEndOfFileInformation: Buffer = %08x\n", pInfoBuffer));

            CopySize = sizeof(FILE_END_OF_FILE_INFORMATION);
            if (*pLengthRemaining >= CopySize)
            {
                pEndOfFileInfo->EndOfFile.QuadPart = pFileEntry->cbObject;
            }
            else
            {
                Log(("VBOXSF: VBoxMRxQueryFileInformation: FileEndOfFileInformation: Buffer too small (%d vs %d)!\n", *pLengthRemaining, CopySize));
                Status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        case FileAllocationInformation:
        {
            PFILE_ALLOCATION_INFORMATION pAllocInfo = (PFILE_ALLOCATION_INFORMATION)pInfoBuffer;

            Log(("VBOXSF: VBoxMRxQueryFileInformation: FileAllocationInformation: Buffer = 0x%08x\n", pInfoBuffer));

            CopySize = sizeof(FILE_ALLOCATION_INFORMATION);
            if (*pLengthRemaining >= CopySize)
            {
                pAllocInfo->AllocationSize.QuadPart = pFileEntry->cbAllocated;
            }
            else
            {
                Log(("VBOXSF: VBoxMRxQueryFileInformation: FileAllocationInformation: Buffer too small (%d vs %d)!\n", *pLengthRemaining, CopySize));
                Status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        case FileNetworkOpenInformation:
        {
            PFILE_NETWORK_OPEN_INFORMATION pFileInfo = (PFILE_NETWORK_OPEN_INFORMATION)pInfoBuffer;
            CopySize = sizeof(FILE_NETWORK_OPEN_INFORMATION);

            Log(("VBOXSF: VBoxMRxQueryFileInformation: FileNetworkOpenInformation: Buffer = 0x%08x\n", pInfoBuffer));
            if (*pLengthRemaining >= CopySize)
            {
                pFileInfo->CreationTime.QuadPart = RTTimeSpecGetNtTime(&pFileEntry->BirthTime); /* Ridiculous name. */
                pFileInfo->LastAccessTime.QuadPart = RTTimeSpecGetNtTime(&pFileEntry->AccessTime);
                pFileInfo->LastWriteTime.QuadPart = RTTimeSpecGetNtTime(&pFileEntry->ModificationTime);
                pFileInfo->ChangeTime.QuadPart = RTTimeSpecGetNtTime(&pFileEntry->ChangeTime);
                pFileInfo->AllocationSize.QuadPart = pFileEntry->cbAllocated;
                pFileInfo->EndOfFile.QuadPart = pFileEntry->cbObject;
                pFileInfo->FileAttributes = VBoxToNTFileAttributes(pFileEntry->Attr.fMode);
            }
            else
            {
                Log(("VBOXSF: VBoxMRxQueryFileInformation: FileNetworkOpenInformation: Buffer too small (%d vs %d)!\n", *pLengthRemaining, CopySize));
                Status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        default:
            Log(("VBOXSF: VBoxMRxQueryFileInformation: Unknown functionality requested (0x%x)!\n", FunctionalityRequested));
            Status = STATUS_NOT_IMPLEMENTED;
            goto end;
        }
    }

    if (Status == STATUS_SUCCESS)
    {
        if (*pLengthRemaining < CopySize)
        {
            /* This situation must be already taken into account by the above code. */
            AssertMsgFailed(("VBOXSF: VBoxMRxQueryFileInformation: Length remaining is below 0! (%d - %d)!\n", *pLengthRemaining, CopySize));
            Status = STATUS_BUFFER_TOO_SMALL;
        }
        else
        {
            pInfoBuffer += CopySize;
            *pLengthRemaining -= CopySize;
        }
    }

    end: if (Status == STATUS_BUFFER_TOO_SMALL)
        RxContext->InformationToReturn = CopySize;

    if (pHGCMBuffer)
        vbsfFreeNonPagedMem(pHGCMBuffer);

    if (Status == STATUS_SUCCESS)
        Log(("VBOXSF: VBoxMRxQueryFileInformation: Success! Remaining length = %d\n", *pLengthRemaining));

    Log(("VBOXSF: VBoxMRxQueryFileInformation: Returned 0x%x\n", Status));
    return Status;
}

NTSTATUS VBoxMRxSetFileInformation (IN PRX_CONTEXT RxContext)
/*++

 Routine Description:

 This routine does a set file info. Only the NT-->NT path is implemented.

 The NT-->NT path works by just remoting the call basically without further ado.

 Arguments:

 RxContext - the RDBSS context

 Return Value:

 NTSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    RxCaptureFcb;
    RxCaptureFobx;
    PMRX_NET_ROOT pNetRoot = capFcb->pNetRoot;
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(capFobx);
    VBoxMRxGetNetRootExtension(capFcb->pNetRoot, pNetRootExtension);
    VBoxMRxGetDeviceExtension(RxContext, pDeviceExtension);
    FILE_INFORMATION_CLASS FunctionalityRequested = RxContext->Info.FileInformationClass;
    PVOID pBuffer = (PVOID)RxContext->Info.Buffer;
    uint8_t *pHGCMBuffer = 0;
    uint32_t cbBuffer = 0;
    int vboxRC;

    Log(("VBOXSF: VBoxMRxSetFileInformation called.\n"));

    switch (FunctionalityRequested)
    {
    case FileBasicInformation:
    {
        PFILE_BASIC_INFORMATION pFileInfo = (PFILE_BASIC_INFORMATION)pBuffer;
        PSHFLFSOBJINFO pSHFLFileInfo;

        Log(("VBOXSF: VBoxMRxSetFileInformation: FileBasicInformation: CreationTime    %RX64\n", pFileInfo->CreationTime.QuadPart));
        Log(("VBOXSF: VBoxMRxSetFileInformation: FileBasicInformation: LastAccessTime  %RX64\n", pFileInfo->LastAccessTime.QuadPart));
        Log(("VBOXSF: VBoxMRxSetFileInformation: FileBasicInformation: LastWriteTime   %RX64\n", pFileInfo->LastWriteTime.QuadPart));
        Log(("VBOXSF: VBoxMRxSetFileInformation: FileBasicInformation: ChangeTime      %RX64\n", pFileInfo->ChangeTime.QuadPart));
        Log(("VBOXSF: VBoxMRxSetFileInformation: FileBasicInformation: FileAttributes  %RX32\n", pFileInfo->FileAttributes));

        /* When setting file attributes, a value of -1 indicates to the server that it MUST NOT change this attribute
         * for all subsequent operations on the same file handle.
         */
        if (pFileInfo->CreationTime.QuadPart == -1)
        {
            pVBoxFobx->fKeepCreationTime = TRUE;
        }
        if (pFileInfo->LastAccessTime.QuadPart == -1)
        {
            pVBoxFobx->fKeepLastAccessTime = TRUE;
        }
        if (pFileInfo->LastWriteTime.QuadPart == -1)
        {
            pVBoxFobx->fKeepLastWriteTime = TRUE;
        }
        if (pFileInfo->ChangeTime.QuadPart == -1)
        {
            pVBoxFobx->fKeepChangeTime = TRUE;
        }

        cbBuffer = sizeof(SHFLFSOBJINFO);
        pHGCMBuffer = (uint8_t *)vbsfAllocNonPagedMem(cbBuffer);
        if (pHGCMBuffer == 0)
        {
            AssertFailed();
            return STATUS_NO_MEMORY;
        }
        RtlZeroMemory(pHGCMBuffer, cbBuffer);
        pSHFLFileInfo = (PSHFLFSOBJINFO)pHGCMBuffer;

        Log(("VBOXSF: VBoxMRxSetFileInformation: FileBasicInformation: keeps %d %d %d %d\n",
             pVBoxFobx->fKeepCreationTime, pVBoxFobx->fKeepLastAccessTime, pVBoxFobx->fKeepLastWriteTime, pVBoxFobx->fKeepChangeTime));

        /* The properties, that need to be changed, are set to something other than zero */
        if (pFileInfo->CreationTime.QuadPart && !pVBoxFobx->fKeepCreationTime)
            RTTimeSpecSetNtTime(&pSHFLFileInfo->BirthTime, pFileInfo->CreationTime.QuadPart);
        if (pFileInfo->LastAccessTime.QuadPart && !pVBoxFobx->fKeepLastAccessTime)
            RTTimeSpecSetNtTime(&pSHFLFileInfo->AccessTime, pFileInfo->LastAccessTime.QuadPart);
        if (pFileInfo->LastWriteTime.QuadPart && !pVBoxFobx->fKeepLastWriteTime)
            RTTimeSpecSetNtTime(&pSHFLFileInfo->ModificationTime, pFileInfo->LastWriteTime.QuadPart);
        if (pFileInfo->ChangeTime.QuadPart && !pVBoxFobx->fKeepChangeTime)
            RTTimeSpecSetNtTime(&pSHFLFileInfo->ChangeTime, pFileInfo->ChangeTime.QuadPart);
        if (pFileInfo->FileAttributes)
            pSHFLFileInfo->Attr.fMode = NTToVBoxFileAttributes(pFileInfo->FileAttributes);

        Assert(pVBoxFobx && pNetRootExtension && pDeviceExtension);
        vboxRC = vboxCallFSInfo(&pDeviceExtension->hgcmClient, &pNetRootExtension->map, pVBoxFobx->hFile,
                                SHFL_INFO_SET | SHFL_INFO_FILE, &cbBuffer, (PSHFLDIRINFO)pSHFLFileInfo);
        AssertRC(vboxRC);

        if (vboxRC != VINF_SUCCESS)
        {
            Status = VBoxErrorToNTStatus(vboxRC);
            goto end;
        }
        else
        {
            /* Update our internal copy */
            pVBoxFobx->FileBasicInfo = *pFileInfo;
        }

        break;
    }

    case FileDispositionInformation:
    {
        PFILE_DISPOSITION_INFORMATION pDispInfo = (PFILE_DISPOSITION_INFORMATION)pBuffer;

        Log(("VBOXSF: VBoxMRxSetFileInformation: FileDispositionInformation: Delete = %d\n", pDispInfo->DeleteFile));
        if (pDispInfo->DeleteFile && capFcb->OpenCount == 1)
            Status = VBoxMRxRemove(RxContext);
        else Status = STATUS_SUCCESS;
        break;
    }

    case FilePositionInformation:
    {
        PFILE_POSITION_INFORMATION pPosInfo = (PFILE_POSITION_INFORMATION)pBuffer;

        Log(("VBOXSF: VBoxMRxSetFileInformation: FilePositionInformation: CurrentByteOffset = %RX64\n", pPosInfo->CurrentByteOffset.QuadPart));

        /* Unsupported */
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    case FileAllocationInformation:
    {
        PFILE_ALLOCATION_INFORMATION pAllocInfo = (PFILE_ALLOCATION_INFORMATION)pBuffer;
        LARGE_INTEGER NewAllocationSize;

        Log(("VBOXSF: VBoxMRxSetFileInformation: FileAllocationInformation: new AllocSize = %RX64, FileSize = %RX64\n",
             pAllocInfo->AllocationSize.QuadPart, capFcb->Header.FileSize.QuadPart));

        /* Check if the new allocation size changes the file size. */
        if (pAllocInfo->AllocationSize.QuadPart > capFcb->Header.FileSize.QuadPart)
        {
            /* Ignore this request and return success. Shared folders do not distinguish between
             * AllocationSize and FileSize.
             */
            Status = STATUS_SUCCESS;
        }
        else
        {
            /* Treat the request as a EndOfFile update. */
            Status = VBoxMRxSetEndOfFile(RxContext, &pAllocInfo->AllocationSize, &NewAllocationSize);
        }

        break;
    }

    case FileEndOfFileInformation:
    {
        PFILE_END_OF_FILE_INFORMATION pEndOfFileInfo = (PFILE_END_OF_FILE_INFORMATION)pBuffer;
        LARGE_INTEGER NewAllocationSize;

        Log(("VBOXSF: VBoxMRxSetFileInformation: FileEndOfFileInformation: new EndOfFile %RX64, FileSize = %RX64\n",
             pEndOfFileInfo->EndOfFile.QuadPart, capFcb->Header.FileSize.QuadPart));
        if (pEndOfFileInfo->EndOfFile.QuadPart > capFcb->Header.FileSize.QuadPart)
        {
            Status = VBoxMRxExtendFile(RxContext, &pEndOfFileInfo->EndOfFile, &NewAllocationSize);

            Log(("VBOXSF: VBoxMRxSetFileInformation: FileEndOfFileInformation: AllocSize = %d AllocSizeHigh = %d\n", NewAllocationSize.LowPart, NewAllocationSize.HighPart));

            //
            //  Change the file allocation
            //
            capFcb->Header.AllocationSize.QuadPart = NewAllocationSize.QuadPart;
        }
        else
        {
            Status = VBoxMRxSetEndOfFile(RxContext, &pEndOfFileInfo->EndOfFile, &NewAllocationSize);
        }
        /** @todo is this really necessary? */
        RxContext->Info.LengthRemaining -= sizeof(FILE_END_OF_FILE_INFORMATION);
        break;
    }

    case FileLinkInformation:
    {
        PFILE_RENAME_INFORMATION pRenameInfo = (PFILE_RENAME_INFORMATION)pBuffer;

        Log(
            ("VBOXSF: VBoxMRxSetFileInformation: FileLinkInformation: ReplaceIfExists = %d, RootDirectory = 0x%x = %.*ls\n", pRenameInfo->ReplaceIfExists, pRenameInfo->RootDirectory, pRenameInfo->FileNameLength
                    / sizeof(WCHAR), pRenameInfo->FileName));
        Status = STATUS_NOT_IMPLEMENTED;
        break;
    }

    case FileRenameInformation:
    {
        PFILE_RENAME_INFORMATION pRenameInfo = (PFILE_RENAME_INFORMATION)pBuffer;

        Log(
            ("VBOXSF: VBoxMRxSetFileInformation: FileRenameInformation: ReplaceIfExists = %d, RootDirectory = 0x%x = %.*ls\n", pRenameInfo->ReplaceIfExists, pRenameInfo->RootDirectory, pRenameInfo->FileNameLength
                    / sizeof(WCHAR), pRenameInfo->FileName));
        Status = VBoxMRxRename(RxContext, FileRenameInformation, pBuffer, RxContext->Info.Length);
        break;
    }

    default:
        Log(("VBOXSF: VBoxMRxSetFileInformation: Unknown functionality requested (0x%x)!\n", FunctionalityRequested));
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    end:

    if (pHGCMBuffer)
        vbsfFreeNonPagedMem(pHGCMBuffer);

    Log(("VBOXSF: VBoxMRxSetFileInformation: Returned 0x%x\n", Status));
    return Status;
}

NTSTATUS VBoxMRxSetFileInformationAtCleanup (IN PRX_CONTEXT RxContext)
/*++

 Routine Description:

 This routine sets the file information on cleanup. the old rdr just swallows this operation (i.e.
 it doesn't generate it). we are doing the same..........

 Arguments:

 pRxContext           - the RDBSS context

 Return Value:

 NTSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_NOT_IMPLEMENTED;

    Log(("VBOXSF: VBoxMRxSetFileInformationAtCleanup\n"));
    return (Status);
}

