/* $Id$ */
/** @file
 * VirtualBox Windows Guest Shared Folders - Path related routines.
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
static UNICODE_STRING g_UnicodeBackslash = { 2, 4, L"\\" };


static NTSTATUS vbsfProcessCreate(PRX_CONTEXT RxContext,
                                  PUNICODE_STRING RemainingName,
                                  SHFLFSOBJINFO *pInfo,
                                  PVOID EaBuffer,
                                  ULONG EaLength,
                                  ULONG *pulCreateAction,
                                  SHFLHANDLE *pHandle)
{
    NTSTATUS Status = STATUS_SUCCESS;

    RxCaptureFcb;

    PMRX_VBOX_NETROOT_EXTENSION pNetRootExtension = VBoxMRxGetNetRootExtension(capFcb->pNetRoot);

    int vrc = VINF_SUCCESS;

    /* Various boolean flags. */
    struct
    {
        ULONG CreateDirectory :1;
        ULONG OpenDirectory :1;
        ULONG DirectoryFile :1;
        ULONG NonDirectoryFile :1;
        ULONG DeleteOnClose :1;
        ULONG TemporaryFile :1;
        ULONG SlashHack :1;
    } bf;

    ACCESS_MASK DesiredAccess;
    ULONG Options;
    UCHAR FileAttributes;
    ULONG ShareAccess;
    ULONG CreateDisposition;
    SHFLCREATEPARMS *pCreateParms = NULL;

    RT_NOREF(EaBuffer);

    if (EaLength)
    {
        Log(("VBOXSF: vbsfProcessCreate: Unsupported: extended attributes!\n"));
        Status = STATUS_NOT_SUPPORTED; /// @todo STATUS_EAS_NOT_SUPPORTED ?
        goto failure;
    }

    if (BooleanFlagOn(capFcb->FcbState, FCB_STATE_PAGING_FILE))
    {
        Log(("VBOXSF: vbsfProcessCreate: Unsupported: paging file!\n"));
        Status = STATUS_NOT_IMPLEMENTED;
        goto failure;
    }

    Log(("VBOXSF: vbsfProcessCreate: FileAttributes = 0x%08x\n",
         RxContext->Create.NtCreateParameters.FileAttributes));
    Log(("VBOXSF: vbsfProcessCreate: CreateOptions = 0x%08x\n",
         RxContext->Create.NtCreateParameters.CreateOptions));

    RtlZeroMemory (&bf, sizeof (bf));

    DesiredAccess = RxContext->Create.NtCreateParameters.DesiredAccess;
    Options = RxContext->Create.NtCreateParameters.CreateOptions & FILE_VALID_OPTION_FLAGS;
    FileAttributes = (UCHAR)(RxContext->Create.NtCreateParameters.FileAttributes & ~FILE_ATTRIBUTE_NORMAL);
    ShareAccess = RxContext->Create.NtCreateParameters.ShareAccess;

    /* We do not support opens by file ids. */
    if (FlagOn(Options, FILE_OPEN_BY_FILE_ID))
    {
        Log(("VBOXSF: vbsfProcessCreate: Unsupported: file open by id!\n"));
        Status = STATUS_NOT_IMPLEMENTED;
        goto failure;
    }

    /* Mask out unsupported attribute bits. */
    FileAttributes &= (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_ARCHIVE);

    bf.DirectoryFile = BooleanFlagOn(Options, FILE_DIRECTORY_FILE);
    bf.NonDirectoryFile = BooleanFlagOn(Options, FILE_NON_DIRECTORY_FILE);
    bf.DeleteOnClose = BooleanFlagOn(Options, FILE_DELETE_ON_CLOSE);
    if (bf.DeleteOnClose)
        Log(("VBOXSF: vbsfProcessCreate: Delete on close!\n"));

    CreateDisposition = RxContext->Create.NtCreateParameters.Disposition;

    bf.CreateDirectory = (bf.DirectoryFile && ((CreateDisposition == FILE_CREATE) || (CreateDisposition == FILE_OPEN_IF)));
    bf.OpenDirectory = (bf.DirectoryFile && ((CreateDisposition == FILE_OPEN) || (CreateDisposition == FILE_OPEN_IF)));
    bf.TemporaryFile = BooleanFlagOn(RxContext->Create.NtCreateParameters.FileAttributes, FILE_ATTRIBUTE_TEMPORARY);

    if (FlagOn(capFcb->FcbState, FCB_STATE_TEMPORARY))
        bf.TemporaryFile = TRUE;

    bf.SlashHack = RxContext->CurrentIrpSp
                && (RxContext->CurrentIrpSp->Parameters.Create.ShareAccess & VBOX_MJ_CREATE_SLASH_HACK);

    Log(("VBOXSF: vbsfProcessCreate: bf.TemporaryFile %d, bf.CreateDirectory %d, bf.DirectoryFile = %d, bf.SlashHack = %d\n",
         bf.TemporaryFile, bf.CreateDirectory, bf.DirectoryFile, bf.SlashHack));

    /* Check consistency in specified flags. */
    if (bf.TemporaryFile && bf.CreateDirectory) /* Directories with temporary flag set are not allowed! */
    {
        Log(("VBOXSF: vbsfProcessCreate: Not allowed: Temporary directories!\n"));
        Status = STATUS_INVALID_PARAMETER;
        goto failure;
    }

    if (bf.DirectoryFile && bf.NonDirectoryFile)
    {
        /** @todo r=bird: Check if FILE_DIRECTORY_FILE+FILE_NON_DIRECTORY_FILE really is illegal in all combinations... */
        Log(("VBOXSF: vbsfProcessCreate: Unsupported combination: dir && !dir\n"));
        Status = STATUS_INVALID_PARAMETER;
        goto failure;
    }

    /* Initialize create parameters. */
    pCreateParms = (SHFLCREATEPARMS *)vbsfNtAllocNonPagedMem(sizeof(SHFLCREATEPARMS));
    if (!pCreateParms)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto failure;
    }

    RtlZeroMemory(pCreateParms, sizeof (SHFLCREATEPARMS));

    pCreateParms->Handle = SHFL_HANDLE_NIL;
    pCreateParms->Result = SHFL_NO_RESULT;

    if (bf.DirectoryFile)
    {
        if (CreateDisposition != FILE_CREATE && CreateDisposition != FILE_OPEN && CreateDisposition != FILE_OPEN_IF)
        {
            Log(("VBOXSF: vbsfProcessCreate: Invalid disposition 0x%08X for directory!\n",
                 CreateDisposition));
            Status = STATUS_INVALID_PARAMETER;
            goto failure;
        }

        pCreateParms->CreateFlags |= SHFL_CF_DIRECTORY;
    }

    Log(("VBOXSF: vbsfProcessCreate: CreateDisposition = 0x%08X\n",
         CreateDisposition));

    switch (CreateDisposition)
    {
        case FILE_SUPERSEDE:
            pCreateParms->CreateFlags |= SHFL_CF_ACT_REPLACE_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW;
            Log(("VBOXSF: vbsfProcessCreate: CreateFlags |= SHFL_CF_ACT_REPLACE_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW\n"));
            break;

        case FILE_OPEN:
            pCreateParms->CreateFlags |= SHFL_CF_ACT_OPEN_IF_EXISTS | SHFL_CF_ACT_FAIL_IF_NEW;
            Log(("VBOXSF: vbsfProcessCreate: CreateFlags |= SHFL_CF_ACT_OPEN_IF_EXISTS | SHFL_CF_ACT_FAIL_IF_NEW\n"));
            break;

        case FILE_CREATE:
            pCreateParms->CreateFlags |= SHFL_CF_ACT_FAIL_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW;
            Log(("VBOXSF: vbsfProcessCreate: CreateFlags |= SHFL_CF_ACT_FAIL_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW\n"));
            break;

        case FILE_OPEN_IF:
            pCreateParms->CreateFlags |= SHFL_CF_ACT_OPEN_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW;
            Log(("VBOXSF: vbsfProcessCreate: CreateFlags |= SHFL_CF_ACT_OPEN_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW\n"));
            break;

        case FILE_OVERWRITE:
            pCreateParms->CreateFlags |= SHFL_CF_ACT_OVERWRITE_IF_EXISTS | SHFL_CF_ACT_FAIL_IF_NEW;
            Log(("VBOXSF: vbsfProcessCreate: CreateFlags |= SHFL_CF_ACT_OVERWRITE_IF_EXISTS | SHFL_CF_ACT_FAIL_IF_NEW\n"));
            break;

        case FILE_OVERWRITE_IF:
            pCreateParms->CreateFlags |= SHFL_CF_ACT_OVERWRITE_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW;
            Log(("VBOXSF: vbsfProcessCreate: CreateFlags |= SHFL_CF_ACT_OVERWRITE_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW\n"));
            break;

        default:
            Log(("VBOXSF: vbsfProcessCreate: Unexpected create disposition: 0x%08X\n",
                 CreateDisposition));
            Status = STATUS_INVALID_PARAMETER;
            goto failure;
    }

    Log(("VBOXSF: vbsfProcessCreate: DesiredAccess = 0x%08X\n",
         DesiredAccess));
    Log(("VBOXSF: vbsfProcessCreate: ShareAccess   = 0x%08X\n",
         ShareAccess));

    if (DesiredAccess & FILE_READ_DATA)
    {
        Log(("VBOXSF: vbsfProcessCreate: FILE_READ_DATA\n"));
        pCreateParms->CreateFlags |= SHFL_CF_ACCESS_READ;
    }

    if (DesiredAccess & FILE_WRITE_DATA)
    {
        Log(("VBOXSF: vbsfProcessCreate: FILE_WRITE_DATA\n"));
        /* FILE_WRITE_DATA means write access regardless of FILE_APPEND_DATA bit.
         */
        pCreateParms->CreateFlags |= SHFL_CF_ACCESS_WRITE;
    }
    else if (DesiredAccess & FILE_APPEND_DATA)
    {
        Log(("VBOXSF: vbsfProcessCreate: FILE_APPEND_DATA\n"));
        /* FILE_APPEND_DATA without FILE_WRITE_DATA means append only mode.
         *
         * Both write and append access flags are required for shared folders,
         * as on Windows FILE_APPEND_DATA implies write access.
         */
        pCreateParms->CreateFlags |= SHFL_CF_ACCESS_WRITE | SHFL_CF_ACCESS_APPEND;
    }

    if (DesiredAccess & FILE_READ_ATTRIBUTES)
        pCreateParms->CreateFlags |= SHFL_CF_ACCESS_ATTR_READ;
    if (DesiredAccess & FILE_WRITE_ATTRIBUTES)
        pCreateParms->CreateFlags |= SHFL_CF_ACCESS_ATTR_WRITE;

    if (ShareAccess & (FILE_SHARE_READ | FILE_SHARE_WRITE))
        pCreateParms->CreateFlags |= SHFL_CF_ACCESS_DENYNONE;
    else if (ShareAccess & FILE_SHARE_READ)
        pCreateParms->CreateFlags |= SHFL_CF_ACCESS_DENYWRITE;
    else if (ShareAccess & FILE_SHARE_WRITE)
        pCreateParms->CreateFlags |= SHFL_CF_ACCESS_DENYREAD;
    else pCreateParms->CreateFlags |= SHFL_CF_ACCESS_DENYALL;

    /* Set initial allocation size. */
    pCreateParms->Info.cbObject = RxContext->Create.NtCreateParameters.AllocationSize.QuadPart;

    if (FileAttributes == 0)
        FileAttributes = FILE_ATTRIBUTE_NORMAL;

    pCreateParms->Info.Attr.fMode = NTToVBoxFileAttributes(FileAttributes);

    {
        PSHFLSTRING ParsedPath;
        Log(("VBOXSF: vbsfProcessCreate: RemainingName->Length = %d\n", RemainingName->Length));


        if (!bf.SlashHack)
        {
            Status = vbsfNtShflStringFromUnicodeAlloc(&ParsedPath, RemainingName->Buffer, RemainingName->Length);
            if (Status != STATUS_SUCCESS)
                goto failure;
        }
        else
        {
            /* Add back the slash we had to hide from RDBSS. */
            Status = vbsfNtShflStringFromUnicodeAlloc(&ParsedPath, NULL, RemainingName->Length + sizeof(RTUTF16));
            if (Status != STATUS_SUCCESS)
                goto failure;
            memcpy(ParsedPath->String.utf16, RemainingName->Buffer, RemainingName->Length);
            ParsedPath->String.utf16[RemainingName->Length / sizeof(RTUTF16)] = '\\';
            ParsedPath->String.utf16[RemainingName->Length / sizeof(RTUTF16) + 1] = '\0';
            ParsedPath->u16Length = RemainingName->Length + sizeof(RTUTF16);
        }

        Log(("VBOXSF: ParsedPath: %.*ls\n",
             ParsedPath->u16Length / sizeof(WCHAR), ParsedPath->String.ucs2));

        /* Call host. */
        Log(("VBOXSF: vbsfProcessCreate: VbglR0SfCreate called.\n"));
        vrc = VbglR0SfCreate(&g_SfClient, &pNetRootExtension->map, ParsedPath, pCreateParms);

        vbsfNtFreeNonPagedMem(ParsedPath);
    }

    Log(("VBOXSF: vbsfProcessCreate: VbglR0SfCreate returns vrc = %Rrc, Result = 0x%x\n",
         vrc, pCreateParms->Result));

    if (RT_FAILURE(vrc))
    {
        /* Map some VBoxRC to STATUS codes expected by the system. */
        switch (vrc)
        {
            case VERR_ALREADY_EXISTS:
            {
                *pulCreateAction = FILE_EXISTS;
                Status = STATUS_OBJECT_NAME_COLLISION;
                goto failure;
            }

            /* On POSIX systems, the "mkdir" command returns VERR_FILE_NOT_FOUND when
               doing a recursive directory create. Handle this case.

               bird: We end up here on windows systems too if opening a dir that doesn't
                     exists.  Thus, I've changed the SHFL_PATH_NOT_FOUND to SHFL_FILE_NOT_FOUND
                     so that FsPerf is happy. */
            case VERR_FILE_NOT_FOUND: /** @todo r=bird: this is a host bug, isn't it? */
            {
                pCreateParms->Result = SHFL_FILE_NOT_FOUND;
                break;
            }

            default:
            {
                *pulCreateAction = FILE_DOES_NOT_EXIST;
                Status = vbsfNtVBoxStatusToNt(vrc);
                goto failure;
            }
        }
    }

    /*
     * The request succeeded. Analyze host response,
     */
    switch (pCreateParms->Result)
    {
        case SHFL_PATH_NOT_FOUND:
        {
            /* Path to the object does not exist. */
            Log(("VBOXSF: vbsfProcessCreate: Path not found\n"));
            *pulCreateAction = FILE_DOES_NOT_EXIST;
            Status = STATUS_OBJECT_PATH_NOT_FOUND;
            goto failure;
        }

        case SHFL_FILE_NOT_FOUND:
        {
            Log(("VBOXSF: vbsfProcessCreate: File not found\n"));
            *pulCreateAction = FILE_DOES_NOT_EXIST;
            if (pCreateParms->Handle == SHFL_HANDLE_NIL)
            {
                Status = STATUS_OBJECT_NAME_NOT_FOUND;
                goto failure;
            }

            Log(("VBOXSF: vbsfProcessCreate: File not found but have a handle!\n"));
            Status = STATUS_UNSUCCESSFUL;
            goto failure;
        }

        case SHFL_FILE_EXISTS:
        {
            Log(("VBOXSF: vbsfProcessCreate: File exists, Handle = 0x%RX64\n",
                 pCreateParms->Handle));
            if (pCreateParms->Handle == SHFL_HANDLE_NIL)
            {
                *pulCreateAction = FILE_EXISTS;
                if (CreateDisposition == FILE_CREATE)
                {
                    /* File was not opened because we requested a create. */
                    Status = STATUS_OBJECT_NAME_COLLISION;
                    goto failure;
                }

                /* Actually we should not go here, unless we have no rights to open the object. */
                Log(("VBOXSF: vbsfProcessCreate: Existing file was not opened!\n"));
                Status = STATUS_ACCESS_DENIED;
                goto failure;
            }

            *pulCreateAction = FILE_OPENED;

            /* Existing file was opened. Go check flags and create FCB. */
            break;
        }

        case SHFL_FILE_CREATED:
        {
            /* A new file was created. */
            Assert(pCreateParms->Handle != SHFL_HANDLE_NIL);

            *pulCreateAction = FILE_CREATED;

            /* Go check flags and create FCB. */
            break;
        }

        case SHFL_FILE_REPLACED:
        {
            /* Existing file was replaced or overwriting. */
            Assert(pCreateParms->Handle != SHFL_HANDLE_NIL);

            if (CreateDisposition == FILE_SUPERSEDE)
                *pulCreateAction = FILE_SUPERSEDED;
            else
                *pulCreateAction = FILE_OVERWRITTEN;
            /* Go check flags and create FCB. */
            break;
        }

        default:
        {
            Log(("VBOXSF: vbsfProcessCreate: Invalid CreateResult from host (0x%08X)\n",
                 pCreateParms->Result));
            *pulCreateAction = FILE_DOES_NOT_EXIST;
            Status = STATUS_OBJECT_PATH_NOT_FOUND;
            goto failure;
        }
    }

    /* Check flags. */
    if (bf.NonDirectoryFile && FlagOn(pCreateParms->Info.Attr.fMode, RTFS_DOS_DIRECTORY))
    {
        /* Caller wanted only a file, but the object is a directory. */
        Log(("VBOXSF: vbsfProcessCreate: File is a directory!\n"));
        Status = STATUS_FILE_IS_A_DIRECTORY;
        goto failure;
    }

    if (bf.DirectoryFile && !FlagOn(pCreateParms->Info.Attr.fMode, RTFS_DOS_DIRECTORY))
    {
        /* Caller wanted only a directory, but the object is not a directory. */
        Log(("VBOXSF: vbsfProcessCreate: File is not a directory!\n"));
        Status = STATUS_NOT_A_DIRECTORY;
        goto failure;
    }

    *pHandle = pCreateParms->Handle;
    *pInfo = pCreateParms->Info;

    vbsfNtFreeNonPagedMem(pCreateParms);

    return Status;

failure:

    Log(("VBOXSF: vbsfProcessCreate: Returned with status = 0x%08X\n",
          Status));

    if (pCreateParms && pCreateParms->Handle != SHFL_HANDLE_NIL)
    {
        VbglR0SfClose(&g_SfClient, &pNetRootExtension->map, pCreateParms->Handle);
        *pHandle = SHFL_HANDLE_NIL;
    }

    if (pCreateParms)
        vbsfNtFreeNonPagedMem(pCreateParms);

    return Status;
}

/**
 * Create/open a file, directory, ++.
 *
 * The RDBSS library will do a table lookup on the path passed in by the user
 * and therefore share FCBs for objects with the same path.
 *
 * The FCB needs to be locked exclusively upon successful return, however it
 * seems like it's not always locked when we get here (only older RDBSS library
 * versions?), so we have to check this before returning.
 *
 */
NTSTATUS VBoxMRxCreate(IN OUT PRX_CONTEXT RxContext)
{
    RxCaptureFcb;
    RxCaptureFobx;
    PMRX_NET_ROOT               pNetRoot          = capFcb->pNetRoot;
    PMRX_SRV_OPEN               SrvOpen           = RxContext->pRelevantSrvOpen;
    PUNICODE_STRING             RemainingName     = GET_ALREADY_PREFIXED_NAME_FROM_CONTEXT(RxContext);
    PMRX_VBOX_NETROOT_EXTENSION pNetRootExtension = VBoxMRxGetNetRootExtension(capFcb->pNetRoot);
    ULONG                       CreateAction      = FILE_CREATED;
    SHFLHANDLE                  Handle            = SHFL_HANDLE_NIL;
    SHFLFSOBJINFO               Info              = {0};
    NTSTATUS                    Status            = STATUS_SUCCESS;
    PMRX_VBOX_FOBX              pVBoxFobx;

    RT_NOREF(capFobx); /* RxCaptureFobx */

    Log(("VBOXSF: MRxCreate: name ptr %p length=%d, SrvOpen->Flags 0x%08X\n",
         RemainingName, RemainingName->Length, SrvOpen->Flags));

    /* Disable FastIO. It causes a verifier bugcheck. */
#ifdef SRVOPEN_FLAG_DONTUSE_READ_CACHING
    SetFlag(SrvOpen->Flags, SRVOPEN_FLAG_DONTUSE_READ_CACHING | SRVOPEN_FLAG_DONTUSE_WRITE_CACHING);
#else
    SetFlag(SrvOpen->Flags, SRVOPEN_FLAG_DONTUSE_READ_CACHEING | SRVOPEN_FLAG_DONTUSE_WRITE_CACHEING);
#endif

    if (RemainingName->Length)
        Log(("VBOXSF: MRxCreate: Attempt to open %.*ls\n",
             RemainingName->Length/sizeof(WCHAR), RemainingName->Buffer));
    else if (FlagOn(RxContext->Create.Flags, RX_CONTEXT_CREATE_FLAG_STRIPPED_TRAILING_BACKSLASH))
    {
        Log(("VBOXSF: MRxCreate: Empty name -> Only backslash used\n"));
        RemainingName = &g_UnicodeBackslash;
    }

    if (   pNetRoot->Type != NET_ROOT_WILD
        && pNetRoot->Type != NET_ROOT_DISK)
    {
        Log(("VBOXSF: MRxCreate: netroot type %d not supported\n",
             pNetRoot->Type));
        return STATUS_NOT_IMPLEMENTED;
    }

    Status = vbsfProcessCreate(RxContext,
                               RemainingName,
                               &Info,
                               RxContext->Create.EaBuffer,
                               RxContext->Create.EaLength,
                               &CreateAction,
                               &Handle);

    if (Status != STATUS_SUCCESS)
    {
        Log(("VBOXSF: MRxCreate: vbsfProcessCreate failed 0x%08X\n",
             Status));
        return Status;
    }

    Log(("VBOXSF: MRxCreate: EOF is 0x%RX64 AllocSize is 0x%RX64\n",
         Info.cbObject, Info.cbAllocated));

    RxContext->pFobx = RxCreateNetFobx(RxContext, SrvOpen);
    if (!RxContext->pFobx)
    {
        Log(("VBOXSF: MRxCreate: RxCreateNetFobx failed\n"));
        VbglR0SfHostReqCloseSimple(pNetRootExtension->map.root, Handle);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    pVBoxFobx = VBoxMRxGetFileObjectExtension(RxContext->pFobx);
    Log(("VBOXSF: MRxCreate: VBoxFobx = %p\n",
         pVBoxFobx));
    AssertReturnStmt(pVBoxFobx, VbglR0SfHostReqCloseSimple(pNetRootExtension->map.root, Handle), STATUS_INTERNAL_ERROR);


    Log(("VBOXSF: MRxCreate: CreateAction = 0x%08X\n",
         CreateAction));

    RxContext->Create.ReturnedCreateInformation = CreateAction;

    /*
     * Make sure we've got the FCB locked exclusivly before updating it and returning.
     * (bird: not entirely sure if this is needed for the W10 RDBSS, but cannot hurt.)
     */
    if (!RxIsFcbAcquiredExclusive(capFcb))
        RxAcquireExclusiveFcbResourceInMRx(capFcb);


    /* Initialize the FCB if this is the first open.
       Note! The RxFinishFcbInitialization call expects node types as the 2nd parameter, but is for
             some reason given enum RX_FILE_TYPE as type. */
    if (capFcb->OpenCount == 0)
    {
        FCB_INIT_PACKET               InitPacket;
        FILE_NETWORK_OPEN_INFORMATION Data;
        ULONG                         NumberOfLinks = 0; /** @todo ?? */
        Data.CreationTime.QuadPart   = RTTimeSpecGetNtTime(&Info.BirthTime);
        Data.LastAccessTime.QuadPart = RTTimeSpecGetNtTime(&Info.AccessTime);
        Data.LastWriteTime.QuadPart  = RTTimeSpecGetNtTime(&Info.ModificationTime);
        Data.ChangeTime.QuadPart     = RTTimeSpecGetNtTime(&Info.ChangeTime);
        Data.AllocationSize.QuadPart = Info.cbAllocated; /** @todo test sparse files.  CcSetFileSizes is documented to not want allocation size smaller than EOF offset. */
        Data.EndOfFile.QuadPart      = Info.cbObject;
        Data.FileAttributes          = VBoxToNTFileAttributes(Info.Attr.fMode);
        RxFormInitPacket(InitPacket,
                         &Data.FileAttributes,
                         &NumberOfLinks,
                         &Data.CreationTime,
                         &Data.LastAccessTime,
                         &Data.LastWriteTime,
                         &Data.ChangeTime,
                         &Data.AllocationSize,
                         &Data.EndOfFile,
                         &Data.EndOfFile);
        if (Info.Attr.fMode & RTFS_DOS_DIRECTORY)
            RxFinishFcbInitialization(capFcb, (RX_FILE_TYPE)RDBSS_NTC_STORAGE_TYPE_DIRECTORY, &InitPacket);
        else
            RxFinishFcbInitialization(capFcb, (RX_FILE_TYPE)RDBSS_NTC_STORAGE_TYPE_FILE, &InitPacket);
    }

    SrvOpen->BufferingFlags = 0;

    RxContext->pFobx->OffsetOfNextEaToReturn = 1;

    pVBoxFobx->hFile = Handle;
    pVBoxFobx->pSrvCall = RxContext->Create.pSrvCall;
    pVBoxFobx->Info = Info;

    /*
     * See if the size has changed and update the FCB if it has.
     */
    if (   capFcb->OpenCount > 0
        && capFcb->Header.FileSize.QuadPart != Info.cbObject)
    {
        PFILE_OBJECT pFileObj = RxContext->CurrentIrpSp->FileObject;
        Assert(pFileObj);
        if (pFileObj)
            vbsfNtUpdateFcbSize(pFileObj, capFcb, pVBoxFobx, Info.cbObject, capFcb->Header.FileSize.QuadPart, Info.cbAllocated);
    }

    /*
     * Do logging.
     */
    Log(("VBOXSF: MRxCreate: Info: BirthTime        %RI64\n", RTTimeSpecGetNano(&pVBoxFobx->Info.BirthTime)));
    Log(("VBOXSF: MRxCreate: Info: ChangeTime       %RI64\n", RTTimeSpecGetNano(&pVBoxFobx->Info.ChangeTime)));
    Log(("VBOXSF: MRxCreate: Info: ModificationTime %RI64\n", RTTimeSpecGetNano(&pVBoxFobx->Info.ModificationTime)));
    Log(("VBOXSF: MRxCreate: Info: AccessTime       %RI64\n", RTTimeSpecGetNano(&pVBoxFobx->Info.AccessTime)));
    Log(("VBOXSF: MRxCreate: Info: fMode            %#RX32\n", pVBoxFobx->Info.Attr.fMode));
    if (!(Info.Attr.fMode & RTFS_DOS_DIRECTORY))
    {
        Log(("VBOXSF: MRxCreate: Info: cbObject         %#RX64\n", pVBoxFobx->Info.cbObject));
        Log(("VBOXSF: MRxCreate: Info: cbAllocated      %#RX64\n", pVBoxFobx->Info.cbAllocated));
    }

    Log(("VBOXSF: MRxCreate: NetRoot is %p, Fcb is %p, SrvOpen is %p, Fobx is %p\n",
         pNetRoot, capFcb, SrvOpen, RxContext->pFobx));
    Log(("VBOXSF: MRxCreate: return 0x%08X\n",
         Status));

    return Status;
}

NTSTATUS VBoxMRxComputeNewBufferingState(IN OUT PMRX_SRV_OPEN pMRxSrvOpen, IN PVOID pMRxContext, OUT PULONG pNewBufferingState)
{
    RT_NOREF(pMRxSrvOpen, pMRxContext, pNewBufferingState);
    Log(("VBOXSF: MRxComputeNewBufferingState\n"));
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS VBoxMRxDeallocateForFcb(IN OUT PMRX_FCB pFcb)
{
    RT_NOREF(pFcb);
    Log(("VBOXSF: MRxDeallocateForFcb\n"));
    return STATUS_SUCCESS;
}

NTSTATUS VBoxMRxDeallocateForFobx(IN OUT PMRX_FOBX pFobx)
{
    RT_NOREF(pFobx);
    Log(("VBOXSF: MRxDeallocateForFobx\n"));
    return STATUS_SUCCESS;
}

NTSTATUS VBoxMRxTruncate(IN PRX_CONTEXT RxContext)
{
    RT_NOREF(RxContext);
    Log(("VBOXSF: MRxTruncate\n"));
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS VBoxMRxCleanupFobx(IN PRX_CONTEXT RxContext)
{
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(RxContext->pFobx);

    Log(("VBOXSF: MRxCleanupFobx: pVBoxFobx = %p, Handle = 0x%RX64\n", pVBoxFobx, pVBoxFobx? pVBoxFobx->hFile: 0));

    if (!pVBoxFobx)
        return STATUS_INVALID_PARAMETER;

    return STATUS_SUCCESS;
}

NTSTATUS VBoxMRxForceClosed(IN PMRX_SRV_OPEN pSrvOpen)
{
    RT_NOREF(pSrvOpen);
    Log(("VBOXSF: MRxForceClosed\n"));
    return STATUS_NOT_IMPLEMENTED;
}

/**
 * Ensures the FCBx doesn't have dangling pointers to @a pVBoxFobx.
 *
 * This isn't strictly speaking needed, as nobody currently dereference these
 * pointers, however better keeping things neath and tidy.
 */
DECLINLINE(void) vbsfNtCleanupFcbxTimestampRefsOnClose(PMRX_VBOX_FOBX pVBoxFobx, PVBSFNTFCBEXT pVBoxFcbx)
{
    pVBoxFobx->fTimestampsSetByUser          = 0;
    pVBoxFobx->fTimestampsUpdatingSuppressed = 0;
    pVBoxFobx->fTimestampsImplicitlyUpdated  = 0;
    if (pVBoxFcbx->pFobxLastAccessTime == pVBoxFobx)
        pVBoxFcbx->pFobxLastAccessTime = NULL;
    if (pVBoxFcbx->pFobxLastWriteTime  == pVBoxFobx)
        pVBoxFcbx->pFobxLastWriteTime  = NULL;
    if (pVBoxFcbx->pFobxChangeTime     == pVBoxFobx)
        pVBoxFcbx->pFobxChangeTime     = NULL;
}

/**
 * Closes an opened file handle of a MRX_VBOX_FOBX.
 *
 * Updates file attributes if necessary.
 *
 * Used by VBoxMRxCloseSrvOpen and vbsfNtRename.
 */
NTSTATUS vbsfNtCloseFileHandle(PMRX_VBOX_NETROOT_EXTENSION pNetRootExtension,
                               PMRX_VBOX_FOBX pVBoxFobx,
                               PVBSFNTFCBEXT pVBoxFcbx)
{
    if (pVBoxFobx->hFile == SHFL_HANDLE_NIL)
    {
        Log(("VBOXSF: vbsfCloseFileHandle: SHFL_HANDLE_NIL\n"));
        return STATUS_SUCCESS;
    }

    Log(("VBOXSF: vbsfCloseFileHandle: 0x%RX64, fTimestampsUpdatingSuppressed = %#x, fTimestampsImplicitlyUpdated = %#x\n",
         pVBoxFobx->hFile, pVBoxFobx->fTimestampsUpdatingSuppressed, pVBoxFobx->fTimestampsImplicitlyUpdated));

    /*
     * We allocate a single request buffer for the timestamp updating and the closing
     * to save time (at the risk of running out of heap, but whatever).
     */
    union MyCloseAndInfoReq
    {
        VBOXSFCLOSEREQ   Close;
        VBOXSFOBJINFOREQ Info;
    } *pReq = (union MyCloseAndInfoReq *)VbglR0PhysHeapAlloc(sizeof(*pReq));
    if (pReq)
        RT_ZERO(*pReq);
    else
        return STATUS_INSUFF_SERVER_RESOURCES;

    /*
     * Restore timestamp that we may implicitly been updated via this handle
     * after the user explicitly set them or turn off implict updating (the -1 value).
     *
     * Note! We ignore the status of this operation.
     */
    Assert(pVBoxFcbx);
    uint8_t fUpdateTs = pVBoxFobx->fTimestampsUpdatingSuppressed & pVBoxFobx->fTimestampsImplicitlyUpdated;
    if (fUpdateTs)
    {
        /** @todo skip this if the host is windows and fTimestampsUpdatingSuppressed == fTimestampsSetByUser */
        /** @todo pass -1 timestamps thru so we can always skip this on windows hosts! */
        if (   (fUpdateTs & VBOX_FOBX_F_INFO_LASTACCESS_TIME)
            && pVBoxFcbx->pFobxLastAccessTime == pVBoxFobx)
            pReq->Info.ObjInfo.AccessTime        = pVBoxFobx->Info.AccessTime;
        else
            fUpdateTs &= ~VBOX_FOBX_F_INFO_LASTACCESS_TIME;

        if (   (fUpdateTs & VBOX_FOBX_F_INFO_LASTWRITE_TIME)
            && pVBoxFcbx->pFobxLastWriteTime  == pVBoxFobx)
            pReq->Info.ObjInfo.ModificationTime  = pVBoxFobx->Info.ModificationTime;
        else
            fUpdateTs &= ~VBOX_FOBX_F_INFO_LASTWRITE_TIME;

        if (   (fUpdateTs & VBOX_FOBX_F_INFO_CHANGE_TIME)
            && pVBoxFcbx->pFobxChangeTime     == pVBoxFobx)
            pReq->Info.ObjInfo.ChangeTime        = pVBoxFobx->Info.ChangeTime;
        else
            fUpdateTs &= ~VBOX_FOBX_F_INFO_CHANGE_TIME;
        if (fUpdateTs)
        {
            Log(("VBOXSF: vbsfCloseFileHandle: Updating timestamp: %#x\n", fUpdateTs));
            int vrc = VbglR0SfHostReqSetObjInfo(pNetRootExtension->map.root, &pReq->Info, pVBoxFobx->hFile);
            if (RT_FAILURE(vrc))
                Log(("VBOXSF: vbsfCloseFileHandle: VbglR0SfHostReqSetObjInfo failed for fUpdateTs=%#x: %Rrc\n", fUpdateTs, vrc));
            RT_NOREF(vrc);
        }
        else
            Log(("VBOXSF: vbsfCloseFileHandle: no timestamp needing updating\n"));
    }

    vbsfNtCleanupFcbxTimestampRefsOnClose(pVBoxFobx, pVBoxFcbx);

    /*
     * Now close the handle.
     */
    int vrc = VbglR0SfHostReqClose(pNetRootExtension->map.root, &pReq->Close, pVBoxFobx->hFile);

    pVBoxFobx->hFile = SHFL_HANDLE_NIL;

    VbglR0PhysHeapFree(pReq);

    NTSTATUS const Status = RT_SUCCESS(vrc) ? STATUS_SUCCESS : vbsfNtVBoxStatusToNt(vrc);
    Log(("VBOXSF: vbsfCloseFileHandle: Returned 0x%08X (vrc=%Rrc)\n", Status, vrc));
    return Status;
}

/**
 * @note We don't collapse opens, this is called whenever a handle is closed.
 */
NTSTATUS VBoxMRxCloseSrvOpen(IN PRX_CONTEXT RxContext)
{
    RxCaptureFcb;
    RxCaptureFobx;

    PMRX_VBOX_NETROOT_EXTENSION pNetRootExtension = VBoxMRxGetNetRootExtension(capFcb->pNetRoot);
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(capFobx);
    PMRX_SRV_OPEN pSrvOpen = capFobx->pSrvOpen;


    Log(("VBOXSF: MRxCloseSrvOpen: capFcb = %p, capFobx = %p, pVBoxFobx = %p, pSrvOpen = %p\n",
          capFcb, capFobx, pVBoxFobx, pSrvOpen));

#ifdef LOG_ENABLED
    PUNICODE_STRING pRemainingName = pSrvOpen->pAlreadyPrefixedName;
    Log(("VBOXSF: MRxCloseSrvOpen: Remaining name = %.*ls, Len = %d\n",
         pRemainingName->Length / sizeof(WCHAR), pRemainingName->Buffer, pRemainingName->Length));
#endif

    if (!pVBoxFobx)
        return STATUS_INVALID_PARAMETER;

    if (FlagOn(pSrvOpen->Flags, (SRVOPEN_FLAG_FILE_RENAMED | SRVOPEN_FLAG_FILE_DELETED)))
    {
        /* If we renamed or delete the file/dir, then it's already closed */
        Assert(pVBoxFobx->hFile == SHFL_HANDLE_NIL);
        Log(("VBOXSF: MRxCloseSrvOpen: File was renamed, handle 0x%RX64 ignore close.\n",
             pVBoxFobx->hFile));
        return STATUS_SUCCESS;
    }

    /*
     * Remove file or directory if delete action is pending and the this is the last open handle.
     */
    NTSTATUS Status = STATUS_SUCCESS;
    if (capFcb->FcbState & FCB_STATE_DELETE_ON_CLOSE)
    {
        Log(("VBOXSF: MRxCloseSrvOpen: Delete on close. Open count = %d\n",
             capFcb->OpenCount));

        if (capFcb->OpenCount == 0)
            Status = vbsfNtRemove(RxContext);
    }

    /*
     * Close file if we still have a handle to it.
     */
    if (pVBoxFobx->hFile != SHFL_HANDLE_NIL)
        vbsfNtCloseFileHandle(pNetRootExtension, pVBoxFobx, VBoxMRxGetFcbExtension(capFcb));

    return Status;
}

/**
 * Worker for vbsfNtSetBasicInfo and VBoxMRxCloseSrvOpen.
 *
 * Only called by vbsfNtSetBasicInfo if there is exactly one open handle.  And
 * VBoxMRxCloseSrvOpen calls it when the last handle is being closed.
 */
NTSTATUS vbsfNtRemove(IN PRX_CONTEXT RxContext)
{
    RxCaptureFcb;
    RxCaptureFobx;
    PMRX_VBOX_NETROOT_EXTENSION pNetRootExtension = VBoxMRxGetNetRootExtension(capFcb->pNetRoot);
    PMRX_VBOX_FOBX              pVBoxFobx         = VBoxMRxGetFileObjectExtension(capFobx);
    PUNICODE_STRING             pRemainingName    = GET_ALREADY_PREFIXED_NAME_FROM_CONTEXT(RxContext);
    uint16_t const              cwcRemainingName  = pRemainingName->Length / sizeof(WCHAR);

    Log(("VBOXSF: vbsfNtRemove: Delete %.*ls. open count = %d\n",
         cwcRemainingName, pRemainingName->Buffer, capFcb->OpenCount));
    Assert(RxIsFcbAcquiredExclusive(capFcb));

    /*
     * We've got function that does both deletion and handle closing starting with 6.0.8,
     * this saves us a host call when just deleting the file/dir.
     */
    uint32_t const  fRemove = pVBoxFobx->Info.Attr.fMode & RTFS_DOS_DIRECTORY ? SHFL_REMOVE_DIR : SHFL_REMOVE_FILE;
    NTSTATUS        Status;
    int             vrc;
    if (g_uSfLastFunction >= SHFL_FN_CLOSE_AND_REMOVE)
    {
        size_t const cbReq = RT_UOFFSETOF(VBOXSFCLOSEANDREMOVEREQ, StrPath.String) + (cwcRemainingName + 1) * sizeof(RTUTF16);
        VBOXSFCLOSEANDREMOVEREQ *pReq = (VBOXSFCLOSEANDREMOVEREQ *)VbglR0PhysHeapAlloc((uint32_t)cbReq);
        if (pReq)
            RT_ZERO(*pReq);
        else
            return STATUS_INSUFFICIENT_RESOURCES;

        memcpy(&pReq->StrPath.String, pRemainingName->Buffer, cwcRemainingName * sizeof(RTUTF16));
        pReq->StrPath.String.utf16[cwcRemainingName] = '\0';
        pReq->StrPath.u16Length = cwcRemainingName * 2;
        pReq->StrPath.u16Size   = cwcRemainingName * 2 + (uint16_t)sizeof(RTUTF16);
        vrc = VbglR0SfHostReqCloseAndRemove(pNetRootExtension->map.root, pReq, fRemove, pVBoxFobx->hFile);
        pVBoxFobx->hFile = SHFL_HANDLE_NIL;

        VbglR0PhysHeapFree(pReq);
    }
    else
    {
        /*
         * We allocate a single request buffer for the closing and deletion to save time.
         */
        AssertCompile(sizeof(VBOXSFCLOSEREQ) <= sizeof(VBOXSFREMOVEREQ));
        AssertReturn((cwcRemainingName + 1) * sizeof(RTUTF16) < _64K, STATUS_NAME_TOO_LONG);
        size_t cbReq = RT_UOFFSETOF(VBOXSFREMOVEREQ, StrPath.String) + (cwcRemainingName + 1) * sizeof(RTUTF16);
        union MyCloseAndRemoveReq
        {
            VBOXSFCLOSEREQ  Close;
            VBOXSFREMOVEREQ Remove;
        } *pReq = (union MyCloseAndRemoveReq *)VbglR0PhysHeapAlloc((uint32_t)cbReq);
        if (pReq)
            RT_ZERO(*pReq);
        else
            return STATUS_INSUFFICIENT_RESOURCES;

        /*
         * Close file first if not already done.  We dont use vbsfNtCloseFileHandle here
         * as we got our own request buffer and have no need to update any file info.
         */
        if (pVBoxFobx->hFile != SHFL_HANDLE_NIL)
        {
            int vrcClose = VbglR0SfHostReqClose(pNetRootExtension->map.root, &pReq->Close, pVBoxFobx->hFile);
            pVBoxFobx->hFile = SHFL_HANDLE_NIL;
            if (RT_FAILURE(vrcClose))
                Log(("VBOXSF: vbsfNtRemove: Closing the handle failed! vrcClose %Rrc, hFile %#RX64 (probably)\n",
                     vrcClose, pReq->Close.Parms.u64Handle.u.value64));
        }

        /*
         * Try remove the file.
         */
        uint16_t const cwcToCopy = pRemainingName->Length / sizeof(WCHAR);
        AssertMsgReturnStmt(cwcToCopy == cwcRemainingName,
                            ("%#x, was %#x; FCB exclusivity: %d\n", cwcToCopy, cwcRemainingName, RxIsFcbAcquiredExclusive(capFcb)),
                            VbglR0PhysHeapFree(pReq), STATUS_INTERNAL_ERROR);
        memcpy(&pReq->Remove.StrPath.String, pRemainingName->Buffer, cwcToCopy * sizeof(RTUTF16));
        pReq->Remove.StrPath.String.utf16[cwcToCopy] = '\0';
        pReq->Remove.StrPath.u16Length = cwcToCopy * 2;
        pReq->Remove.StrPath.u16Size   = cwcToCopy * 2 + (uint16_t)sizeof(RTUTF16);
        vrc = VbglR0SfHostReqRemove(pNetRootExtension->map.root, &pReq->Remove, fRemove);

        VbglR0PhysHeapFree(pReq);
    }

    if (RT_SUCCESS(vrc))
    {
        SetFlag(capFobx->pSrvOpen->Flags, SRVOPEN_FLAG_FILE_DELETED);
        vbsfNtCleanupFcbxTimestampRefsOnClose(pVBoxFobx, VBoxMRxGetFcbExtension(capFcb));
        Status = STATUS_SUCCESS;
    }
    else
    {
        Log(("VBOXSF: vbsfNtRemove: VbglR0SfRemove failed with %Rrc\n", vrc));
        Status = vbsfNtVBoxStatusToNt(vrc);
    }

    Log(("VBOXSF: vbsfNtRemove: Returned %#010X (%Rrc)\n", Status, vrc));
    return Status;
}

NTSTATUS VBoxMRxShouldTryToCollapseThisOpen(IN PRX_CONTEXT RxContext)
{
    RT_NOREF(RxContext);
    Log(("VBOXSF: MRxShouldTryToCollapseThisOpen\n"));
    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS VBoxMRxCollapseOpen(IN OUT PRX_CONTEXT RxContext)
{
    RT_NOREF(RxContext);
    Log(("VBOXSF: MRxCollapseOpen\n"));
    return STATUS_MORE_PROCESSING_REQUIRED;
}
