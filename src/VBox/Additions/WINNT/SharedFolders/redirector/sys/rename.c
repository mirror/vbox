/*++

 Copyright (c) 1989 - 1999 Microsoft Corporation

 Module Name:

 rename.c

 Abstract:

 This module implements rename in the null minirdr.

 --*/

#include "precomp.h"
#pragma hdrstop

#ifdef VBOX
NTSTATUS
#endif
VBoxMRxRename (IN PRX_CONTEXT RxContext, IN FILE_INFORMATION_CLASS FileInformationClass, IN PVOID pBuffer, IN ULONG BufferLength)
/*++

 Routine Description:

 This routine does a rename. since the real NT-->NT path is not implemented at the server end,
 we implement just the downlevel path.

 Arguments:

 RxContext - the RDBSS context
 FILE_INFO_CLASS - must be rename....shouldn't really pass this
 pBuffer - pointer to the new name
 bufferlength - and the size

 Return Value:

 NTSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_NOT_IMPLEMENTED;

    RxCaptureFcb;
    RxCaptureFobx;
    PMRX_SRV_OPEN SrvOpen = capFobx->pSrvOpen;
    PUNICODE_STRING RemainingName = GET_ALREADY_PREFIXED_NAME(SrvOpen, capFcb);
    PFILE_RENAME_INFORMATION RenameInformation = (PFILE_RENAME_INFORMATION)RxContext->Info.Buffer;
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(capFobx);
    PMRX_NET_ROOT pNetRoot = capFcb->pNetRoot;
    VBoxMRxGetNetRootExtension(pNetRoot,pNetRootExtension);
    VBoxMRxGetDeviceExtension(RxContext, pDeviceExtension);
    int vboxRC;
    PSHFLSTRING SrcPath = 0, DestPath = 0;
    ULONG ParsedPathSize, flags;

    Assert(FileInformationClass == FileRenameInformation);

    Log(("VBOXSF: VBoxMRxRename: FileName = %.*ls\n", RenameInformation->FileNameLength / sizeof(WCHAR), &RenameInformation->FileName[0]));

    /* Must close the file before renaming it! */
    if (pVBoxFobx->hFile != SHFL_HANDLE_NIL)
    {
        vboxRC = vboxCallClose(&pDeviceExtension->hgcmClient, &pNetRootExtension->map, pVBoxFobx->hFile);
        AssertRC(vboxRC);
        pVBoxFobx->hFile = SHFL_HANDLE_NIL;
    }

    /* Mark it as renamed, so we do nothing during close */
    SetFlag(SrvOpen->Flags, SRVOPEN_FLAG_FILE_RENAMED);

    /* Calculate length required for destination path.
     */
    ParsedPathSize = sizeof(*DestPath) + (RenameInformation->FileNameLength + sizeof(WCHAR));

    Log(("VBOXSF: VBoxMRxRename: ParsedPathSize = %d\n", ParsedPathSize));

    DestPath = (PSHFLSTRING)vbsfAllocNonPagedMem(ParsedPathSize);

    if (NULL == DestPath)
        return STATUS_INSUFFICIENT_RESOURCES;

    RtlZeroMemory(DestPath, ParsedPathSize);
    ShflStringInitBuffer(DestPath, ParsedPathSize - sizeof(SHFLSTRING));

    Log(("VBOXSF: VBoxMRxRename: Setting up destination path ...\n"));
    DestPath->u16Size = (USHORT)(RenameInformation->FileNameLength + sizeof(WCHAR));
    DestPath->u16Length = DestPath->u16Size - sizeof(WCHAR); /* without terminating null */
    RtlCopyMemory(DestPath->String.ucs2, RenameInformation->FileName, DestPath->u16Length);
    Log(("VBOXSF: VBoxMRxRename: Destination path = %.*ls\n", DestPath->u16Length / sizeof(WCHAR), &DestPath->String.ucs2[0]));

    /* Calculate length required for source path
     */
    ParsedPathSize = sizeof(*DestPath) + (RemainingName->Length + sizeof(WCHAR));
    Log(("VBOXSF: VBoxMRxRename: ParsedPathSize = %d\n", ParsedPathSize));

    SrcPath = (PSHFLSTRING)vbsfAllocNonPagedMem(ParsedPathSize);

    if (NULL == SrcPath)
        return STATUS_INSUFFICIENT_RESOURCES;

    RtlZeroMemory(SrcPath, ParsedPathSize);
    ShflStringInitBuffer(SrcPath, ParsedPathSize - sizeof(SHFLSTRING));

    Log(("VBOXSF: VBoxMRxRename: Setting up source path ...\n"));
    SrcPath->u16Size = RemainingName->Length + sizeof(WCHAR);
    SrcPath->u16Length = SrcPath->u16Size - sizeof(WCHAR); /* without terminating null */
    RtlCopyMemory(SrcPath->String.ucs2, RemainingName->Buffer, SrcPath->u16Length);
    Log(("VBOXSF: VBoxMRxRename: Source path = %.*ls\n", SrcPath->u16Length / sizeof(WCHAR), &SrcPath->String.ucs2[0]));

    /* Call host. */
    flags = (pVBoxFobx->FileStandardInfo.Directory) ? SHFL_RENAME_DIR : SHFL_RENAME_FILE;
    if (RenameInformation->ReplaceIfExists)
        flags |= SHFL_RENAME_REPLACE_IF_EXISTS;

    Log(("VBOXSF: VBoxMRxRename: Calling vboxCallRename ...\n", SrcPath->u16Length / sizeof(WCHAR), &SrcPath->String.ucs2[0]));
    vboxRC = vboxCallRename(&pDeviceExtension->hgcmClient, &pNetRootExtension->map, SrcPath, DestPath, flags);

    vbsfFreeNonPagedMem(SrcPath);
    vbsfFreeNonPagedMem(DestPath);

    Status = VBoxErrorToNTStatus(vboxRC);
    if (vboxRC != VINF_SUCCESS)
        Log(("VBOXSF: VBoxMRxRename: vboxCallRename failed with %d (Status = %x)!\n", vboxRC, Status));

    Log(("VBOXSF: VBoxMRxRename: Returned 0x%lx\n", Status));
    return (Status);
}

