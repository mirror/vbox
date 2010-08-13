/*++

 Copyright (c) 1989 - 1999 Microsoft Corporation

 Module Name:

 openclos.c

 Abstract:

 This module implements the mini redirector call down routines pertaining to opening/
 closing of file/directories.

 --*/

#include "precomp.h"
#include "rdbss_vbox.h"
#pragma hdrstop

static UNICODE_STRING UnicodeBackslash = { 2, 4,L"\\" };

//
//  forwards & pragmas
//

#ifdef __cplusplus
extern "C"
{
#endif

NTSTATUS VBoxMRxProcessCreate (IN PRX_CONTEXT RxContext, IN PUNICODE_STRING RemainingName, IN FILE_BASIC_INFORMATION *pFileBasicInfo, IN FILE_STANDARD_INFORMATION *pFileStandardInfo,
                               IN PVOID EaBuffer, IN ULONG EaLength, OUT ULONG *pulCreateAction, OUT SHFLHANDLE *pHandle);

NTSTATUS VBoxMRxCreateFileSuccessTail (PRX_CONTEXT RxContext, PBOOLEAN MustRegainExclusiveResource, RX_FILE_TYPE StorageType, ULONG CreateAction, FILE_BASIC_INFORMATION* pFileBasicInfo,
                                       FILE_STANDARD_INFORMATION* pFileStandardInfo, SHFLHANDLE Handle);

VOID VBoxMRxSetSrvOpenFlags (PRX_CONTEXT RxContext, RX_FILE_TYPE StorageType, PMRX_SRV_OPEN SrvOpen);

#ifdef __cplusplus
}
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, VBoxMRxCreate)
#pragma alloc_text(PAGE, VBoxMRxShouldTryToCollapseThisOpen)
#pragma alloc_text(PAGE, VBoxMRxProcessCreate)
#pragma alloc_text(PAGE, VBoxMRxCreateFileSuccessTail)
#pragma alloc_text(PAGE, VBoxMRxSetSrvOpenFlags)
#endif

NTSTATUS VBoxMRxShouldTryToCollapseThisOpen (IN PRX_CONTEXT RxContext)
/*++

 Routine Description:

 This routine determines if the mini knows of a good reason not
 to try collapsing on this open. Presently, the only reason would
 be if this were a copychunk open.

 Arguments:

 RxContext - the RDBSS context

 Return Value:

 NTSTATUS - The return status for the operation
 SUCCESS --> okay to try collapse
 other (MORE_PROCESSING_REQUIRED) --> dont collapse

 --*/
{
#ifdef VBOX_SHFL_WITH_COLLAPSE
    NTSTATUS Status = STATUS_SUCCESS;
#else
    NTSTATUS Status = STATUS_NOT_IMPLEMENTED;
#endif

    Log(("VBoxMRxShouldTryToCollapseThisOpen returned %x\n", Status));

    PAGED_CODE();

    return Status;
}

BOOLEAN VBoxMRxIsStreamFile (PUNICODE_STRING FileName, PUNICODE_STRING AdjustFileName)
/*++

 Routine Description:

 This routine checks if it is a stream file and return the root file name if true.

 Arguments:

 FileName - the file name needs to be parsed
 AdjustFileName - the file name contains only root name of the stream

 Return Value:

 BOOLEAN - stream file

 --*/
{
    USHORT i;
    BOOLEAN IsStream = FALSE;
    NTSTATUS Status = STATUS_SUCCESS;

    Log(("VBoxMRxIsStreamFile %.*ls\n", FileName->Length / sizeof(WCHAR), FileName->Buffer));

    for (i = 0; i < FileName->Length / sizeof(WCHAR); i++)
    {
        if (FileName->Buffer[i] == L':')
        {
            IsStream = TRUE;
            break;
        }
    }

    if (AdjustFileName != NULL)
    {
        if (IsStream)
        {
            AdjustFileName->Length = AdjustFileName->MaximumLength = i * sizeof(WCHAR);
            AdjustFileName->Buffer = FileName->Buffer;
        }
        else
        {
            AdjustFileName->Length = AdjustFileName->MaximumLength = 0;
            AdjustFileName->Buffer = NULL;
        }
    }

    return IsStream;
}

#define NulMRxGetFcbExtension(pFcb,pExt)                \
        PNULMRX_FCB_EXTENSION pExt = (((pFcb) == NULL) ? NULL : (PNULMRX_FCB_EXTENSION)((pFcb)->Context))

#define NulMRxGetNetRootExtension(pNetRoot,pExt)        \
        PNULMRX_NETROOT_EXTENSION pExt = (((pNetRoot) == NULL) ? NULL : (PNULMRX_NETROOT_EXTENSION)((pNetRoot)->Context))

typedef struct _NULMRX_FCB_EXTENSION_
{
    //
    //  Node type code and size
    //
    NODE_TYPE_CODE NodeTypeCode;
    NODE_BYTE_SIZE NodeByteSize;

} NULMRX_FCB_EXTENSION, *PNULMRX_FCB_EXTENSION;

//
// NET_ROOT extension - stores state global to a root
//
typedef struct _NULMRX_NETROOT_EXTENSION
{
    //
    //  Node type code and size
    //
    NODE_TYPE_CODE NodeTypeCode;
    NODE_BYTE_SIZE NodeByteSize;

} NULMRX_NETROOT_EXTENSION, *PNULMRX_NETROOT_EXTENSION;

NTSTATUS NulMRxProcessCreate (IN PNULMRX_FCB_EXTENSION pFcbExtension, IN PVOID EaBuffer, IN ULONG EaLength, OUT PLONGLONG pEndOfFile, OUT PLONGLONG pAllocationSize)
/*++

 Routine Description:

 This routine processes a create calldown.

 Arguments:

 pFcbExtension   -   ptr to the FCB extension
 EaBuffer        -   ptr to the EA param buffer
 EaLength        -   len of EaBuffer
 pEndOfFile      -   return end of file value
 pAllocationSize -   return allocation size (which maybe > EOF)

 Notes:

 It is possible to create a file with no EAs

 Return Value:

 None

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    RxDbgTrace(0, Dbg, ("NulMRxInitializeFcbExtension\n"));

    *pAllocationSize = *pEndOfFile = 0;
    return Status;
}

VOID NulMRxSetSrvOpenFlags (PRX_CONTEXT RxContext, RX_FILE_TYPE StorageType, PMRX_SRV_OPEN SrvOpen)
{
    PMRX_SRV_CALL SrvCall = (PMRX_SRV_CALL)RxContext->Create.pSrvCall;

    //
    //  set this only if cache manager will be used for mini-rdr handles !
    //
    SrvOpen->BufferingFlags |= (FCB_STATE_FILESIZECACHEING_ENABLED | FCB_STATE_FILETIMECACHEING_ENABLED | FCB_STATE_WRITEBUFFERING_ENABLED | FCB_STATE_LOCK_BUFFERING_ENABLED
            | FCB_STATE_READBUFFERING_ENABLED |
#if (NTDDI_VERSION >= NTDDI_VISTA)      /* Correct spelling for Vista 6001 SDK. */
            FCB_STATE_WRITECACHING_ENABLED | FCB_STATE_READCACHING_ENABLED);
#else
    FCB_STATE_WRITECACHEING_ENABLED |
    FCB_STATE_READCACHEING_ENABLED);
#endif
}

NTSTATUS NulMRxCreateFileSuccessTail (PRX_CONTEXT RxContext, PBOOLEAN MustRegainExclusiveResource, RX_FILE_TYPE StorageType, ULONG CreateAction, FILE_BASIC_INFORMATION* pFileBasicInfo,
                                      FILE_STANDARD_INFORMATION* pFileStandardInfo)
/*++

 Routine Description:

 This routine finishes the initialization of the fcb and srvopen for a
 successful open.

 Arguments:


 Return Value:

 RXSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    RxCaptureFcb;
    PMRX_SRV_OPEN SrvOpen = RxContext->pRelevantSrvOpen;

    FCB_INIT_PACKET InitPacket;

    RxDbgTrace(0, Dbg, ("MRxExCreateFileSuccessTail\n"));
    PAGED_CODE();

    ASSERT(NodeType(SrvOpen) == RDBSS_NTC_SRVOPEN);
    ASSERT(NodeType(RxContext) == RDBSS_NTC_RX_CONTEXT);

    if (*MustRegainExclusiveResource)
    { //this is required because of oplock breaks
        RxAcquireExclusiveFcb(RxContext, capFcb);
        *MustRegainExclusiveResource = FALSE;
    }

    // This Fobx should be cleaned up by the wrapper
    RxContext->pFobx = RxCreateNetFobx(RxContext, SrvOpen);
    if (RxContext->pFobx == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    ASSERT(RxIsFcbAcquiredExclusive(capFcb));
    RxDbgTrace(0, Dbg, ("Storagetype %08lx/Action %08lx\n", StorageType, CreateAction));

    RxContext->Create.ReturnedCreateInformation = CreateAction;

    RxFormInitPacket(InitPacket, &pFileBasicInfo->FileAttributes, &pFileStandardInfo->NumberOfLinks, &pFileBasicInfo->CreationTime, &pFileBasicInfo->LastAccessTime, &pFileBasicInfo->LastWriteTime,
                     &pFileBasicInfo->ChangeTime, &pFileStandardInfo->AllocationSize, &pFileStandardInfo->EndOfFile, &pFileStandardInfo->EndOfFile);

    if (capFcb->OpenCount == 0)
    {
        RxFinishFcbInitialization(capFcb, RDBSS_STORAGE_NTC(StorageType), &InitPacket);
    }
    else
    {

        ASSERT(StorageType == 0 || NodeType(capFcb) == RDBSS_STORAGE_NTC(StorageType));

    }

    NulMRxSetSrvOpenFlags(RxContext, StorageType, SrvOpen);

    RxContext->pFobx->OffsetOfNextEaToReturn = 1;
    //transition happens later

    return Status;
}

NTSTATUS VBoxMRxCreate (IN OUT PRX_CONTEXT RxContext)
/*++

 Routine Description:

 This routine opens a file across the network

 Arguments:

 RxContext - the RDBSS context

 Return Value:

 RXSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    BOOLEAN fMustRegainExclusiveResource = FALSE;
    RX_FILE_TYPE StorageType = FileTypeFile;
    ULONG CreateAction = FILE_CREATED;
    FILE_BASIC_INFORMATION FileBasicInfo;
    FILE_STANDARD_INFORMATION FileStandardInfo;
    RxCaptureFcb;
    RxCaptureFobx;
    PMRX_SRV_OPEN SrvOpen = RxContext->pRelevantSrvOpen;
    PMRX_SRV_CALL SrvCall = RxContext->Create.pSrvCall;
    PMRX_NET_ROOT NetRoot = capFcb->pNetRoot;
    PUNICODE_STRING RemainingName = GET_ALREADY_PREFIXED_NAME_FROM_CONTEXT(RxContext);
    PVOID EaBuffer = RxContext->Create.EaBuffer;
    ULONG EaLength = RxContext->Create.EaLength;
    VBoxMRxGetNetRootExtension(NetRoot,pNetRootExtension);
    SHFLHANDLE Handle = SHFL_HANDLE_NIL;

    PAGED_CODE();

    Log(("VBOXSF: VBoxMRxCreate: %x length=%d\n", RemainingName, RemainingName->Length));

    if (RemainingName->Length)
    {
#ifdef VBOX_GUEST
        Log(("VBOXSF: VBoxMRxCreate: Attempt to open %.*ls Len is %d\n", RemainingName->Length/sizeof(WCHAR), RemainingName->Buffer, RemainingName->Length ));
#else
        Log(("VBOXSF: VBoxMRxCreate: Attempt to open %wZ Len is %d\n", RemainingName, RemainingName->Length));
#endif
    }
    else
    {
        if (FlagOn(RxContext->Create.Flags, RX_CONTEXT_CREATE_FLAG_STRIPPED_TRAILING_BACKSLASH))
        {
            Log(("VBOXSF: VBoxMRxCreate: Empty name -> Only backslash used\n"));
            RemainingName = &UnicodeBackslash;
        }
    }

    switch (NetRoot->Type)
    {
    case NET_ROOT_MAILSLOT:
    case NET_ROOT_PIPE:
    case NET_ROOT_PRINT:
    default:
        Log(("VBOXSF: VBoxMRxCreate: Type %d not supported or invalid open\n", NetRoot->Type));
        Status = STATUS_NOT_SUPPORTED;
        goto Exit;

    case NET_ROOT_WILD: /** @todo what's this?? */
    case NET_ROOT_DISK:
        break;
    }

    FileBasicInfo.FileAttributes = FILE_ATTRIBUTE_NORMAL;

    //
    //  Squirrel away the scatter list in the FCB extension.
    //  This is done only for data files.
    //
    Status = VBoxMRxProcessCreate(RxContext, RemainingName, &FileBasicInfo, &FileStandardInfo, EaBuffer, EaLength, &CreateAction, &Handle);
    if (Status != STATUS_SUCCESS)
    {
        //
        //  error..
        //
        Log(("VBOXSF: VBoxMRxCreate: Failed to initialize scatter list!\n"));
        goto Exit;
    }

    //
    //  Complete CreateFile contract
    //
    Log(("VBOXSF: VBoxMRxCreate: EOF is %d AllocSize is %d\n", (ULONG)FileStandardInfo.EndOfFile.QuadPart, (ULONG)FileStandardInfo.AllocationSize.QuadPart));

    Status = VBoxMRxCreateFileSuccessTail(RxContext, &fMustRegainExclusiveResource, StorageType, CreateAction, &FileBasicInfo, &FileStandardInfo, Handle);

    if (Status != STATUS_SUCCESS)
    {
        //
        //  alloc error..
        //
        Log(("VBOXSF: VBoxMRxCreate: Failed to allocate Fobx!\n"));
        goto Exit;
    }

    if (!RxIsFcbAcquiredExclusive(capFcb))
    {
        Assert(!RxIsFcbAcquiredShared(capFcb));
        VboxRxAcquireExclusiveFcbResourceInMRx(capFcb);
    }

    Assert(Status != (STATUS_PENDING));
    Assert(RxIsFcbAcquiredExclusive(capFcb));

    Log(("VBOXSF: VBoxMRxCreate: NetRoot is 0x%x, Fcb is 0x%x, SrvOpen is 0x%x, Fobx is 0x%x\n", NetRoot, capFcb, SrvOpen, RxContext->pFobx));
    Log(("VBOXSF: VBoxMRxCreate: Exit with status=%08lx\n", Status));

    Exit: return (Status);
}

NTSTATUS VBoxMRxProcessCreate (IN PRX_CONTEXT RxContext, IN PUNICODE_STRING RemainingName, IN FILE_BASIC_INFORMATION *pFileBasicInfo, IN FILE_STANDARD_INFORMATION *pFileStandardInfo,
                               IN PVOID EaBuffer, IN ULONG EaLength, OUT ULONG *pulCreateAction, OUT SHFLHANDLE *pHandle)
/*++

 Routine Description:

 This routine processes a create calldown.

 Arguments:

 pFcbExtension   -   ptr to the FCB extension
 EaBuffer        -   ptr to the EA param buffer
 EaLength        -   len of EaBuffer
 pEndOfFile      -   return end of file value
 pAllocationSize -   return allocation size (which maybe > EOF)

 Notes:

 It is possible to create a file with no EAs

 Return Value:

 None

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    RxCaptureFcb;
    VBoxMRxGetDeviceExtension(RxContext, pDeviceExtension);
    VBoxMRxGetNetRootExtension(capFcb->pNetRoot,pNetRootExtension);

    int vboxRC = VINF_SUCCESS;

    /* Various boolean flags. */
    struct
    {
        ULONG CreateDirectory :1;
        ULONG SequentialOnly :1;
        ULONG NoIntermediateBuffering :1;
        ULONG OpenDirectory :1;
        ULONG IsPagingFile :1;
        ULONG OpenTargetDirectory :1;
        ULONG DirectoryFile :1;
        ULONG NonDirectoryFile :1;
        ULONG NoEaKnowledge :1;
        ULONG DeleteOnClose :1;
        ULONG TemporaryFile :1;
        ULONG FileNameOpenedDos :1;
        ULONG PostIrp :1;
        ULONG OplockPostIrp :1;
        ULONG TrailingBackslash :1;
        ULONG FirstLoop :1;
        ULONG SeparatorEncountered :1;
        ULONG DriveAcquired :1;
    } bf;

    ACCESS_MASK DesiredAccess;
    ULONG Options;
    UCHAR FileAttributes;
    ULONG ShareAccess;
    ULONG CreateDisposition;
    SHFLCREATEPARMS *pCreateParms = 0;

    Log(("VBOXSF: VBoxMRxProcessCreate\n"));
    Log(("VBOXSF: VBoxMRxProcessCreate: FileAttributes = %08x\n", RxContext->Create.NtCreateParameters.FileAttributes));
    Log(("VBOXSF: VBoxMRxProcessCreate: CreateOptions = %08x\n", RxContext->Create.NtCreateParameters.CreateOptions));

    RtlZeroMemory (&bf, sizeof (bf));

    DesiredAccess = RxContext->Create.NtCreateParameters.DesiredAccess;
    Options = RxContext->Create.NtCreateParameters.CreateOptions & FILE_VALID_OPTION_FLAGS;
    FileAttributes = (UCHAR)(RxContext->Create.NtCreateParameters.FileAttributes & ~FILE_ATTRIBUTE_NORMAL);
    ShareAccess = RxContext->Create.NtCreateParameters.ShareAccess;

    /* Extended attributes are not supported! */
    if (EaLength)
    {
        Log(("VBOXSF: VBoxMRxProcessCreate: Unsupported: extended attributes!\n"));
        Status = STATUS_NOT_SUPPORTED;
        goto failure;
    }

    /* We do not support opens by file ids yet. */
    if (FlagOn(Options, FILE_OPEN_BY_FILE_ID))
    {
        Log(("VBOXSF: VBoxMRxProcessCreate: Unsupported: file open by id!\n"));
        Status = STATUS_NOT_IMPLEMENTED;
        goto failure;
    }

    /* Mask out unsupported attribute bits. */
    FileAttributes &= (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_ARCHIVE);

    bf.DirectoryFile = BooleanFlagOn(Options, FILE_DIRECTORY_FILE);
    bf.NonDirectoryFile = BooleanFlagOn(Options, FILE_NON_DIRECTORY_FILE);
    bf.SequentialOnly = BooleanFlagOn(Options, FILE_SEQUENTIAL_ONLY);
    bf.NoIntermediateBuffering = BooleanFlagOn(Options, FILE_NO_INTERMEDIATE_BUFFERING);
    bf.NoEaKnowledge = BooleanFlagOn(Options, FILE_NO_EA_KNOWLEDGE);
    bf.DeleteOnClose = BooleanFlagOn(Options, FILE_DELETE_ON_CLOSE);
    if (bf.DeleteOnClose)
        Log(("VBOXSF: VBoxMRxProcessCreate: Delete on close!\n"));

    CreateDisposition = RxContext->Create.NtCreateParameters.Disposition;

    bf.IsPagingFile = BooleanFlagOn(capFcb->FcbState, FCB_STATE_PAGING_FILE);
    if (bf.IsPagingFile)
    {
        Log(("VBOXSF: VBoxMRxProcessCreate: Unsupported: paging file!\n"));
        Status = STATUS_NOT_IMPLEMENTED;
        goto failure;
    }

    bf.CreateDirectory = (BOOLEAN)(bf.DirectoryFile && ((CreateDisposition == FILE_CREATE) || (CreateDisposition == FILE_OPEN_IF)));

    bf.OpenDirectory = (BOOLEAN)(bf.DirectoryFile && ((CreateDisposition == FILE_OPEN) || (CreateDisposition == FILE_OPEN_IF)));

    bf.TemporaryFile = BooleanFlagOn(RxContext->Create.NtCreateParameters.FileAttributes, FILE_ATTRIBUTE_TEMPORARY );

    if (FlagOn(capFcb->FcbState, FCB_STATE_TEMPORARY))
        bf.TemporaryFile = TRUE;

    Log(("VBOXSF: VBoxMRxProcessCreate: bf.TemporaryFile %d, bf.CreateDirectory %d, bf.DirectoryFile = %d\n", (ULONG)bf.TemporaryFile, (ULONG)bf.CreateDirectory, (ULONG)bf.DirectoryFile));

    /* Check consistency in specified flags. */
    if (bf.TemporaryFile && bf.CreateDirectory) /* Directories with temporary flag set are not allowed! */
    {
        Log(("VBOXSF: VBoxMRxProcessCreate: Not allowed: Temporary directories!\n"));
        Status = STATUS_INVALID_PARAMETER;
        goto failure;
    }

    if (bf.DirectoryFile && bf.NonDirectoryFile)
    {
        AssertMsgFailed(("VBOXSF: VBoxMRxProcessCreate: Unsupported combination: dir && !dir\n"));
        Status = STATUS_INVALID_PARAMETER;
        goto failure;
    }

    /* Initialize create parameters. */
    pCreateParms = (SHFLCREATEPARMS *)vbsfAllocNonPagedMem(sizeof(SHFLCREATEPARMS));
    if (!pCreateParms)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto failure;
    }

    RtlZeroMemory (pCreateParms, sizeof (SHFLCREATEPARMS));

    pCreateParms->Handle = SHFL_HANDLE_NIL;
    pCreateParms->Result = SHFL_NO_RESULT;

    if (bf.OpenTargetDirectory)
    {
        pCreateParms->CreateFlags |= SHFL_CF_OPEN_TARGET_DIRECTORY;
    }

    if (bf.DirectoryFile)
    {
        if (CreateDisposition != FILE_CREATE && CreateDisposition != FILE_OPEN && CreateDisposition != FILE_OPEN_IF)
        {
            AssertMsgFailed(("VBOXSF: VBoxMRxProcessCreate: Invalid disposition %x for directory!\n", CreateDisposition));
            Status = STATUS_INVALID_PARAMETER;
            goto failure;
        }

        pCreateParms->CreateFlags |= SHFL_CF_DIRECTORY;
    }

    Log(("VBOXSF: VBoxMRxProcessCreate: CreateDisposition = %08x\n", CreateDisposition));

    switch (CreateDisposition)
    {
    case FILE_SUPERSEDE:
        pCreateParms->CreateFlags |= SHFL_CF_ACT_REPLACE_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW;
        Log(("VBOXSF: VBoxMRxProcessCreate: CreateFlags |= SHFL_CF_ACT_REPLACE_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW\n"));
        break;

    case FILE_OPEN:
        pCreateParms->CreateFlags |= SHFL_CF_ACT_OPEN_IF_EXISTS | SHFL_CF_ACT_FAIL_IF_NEW;
        Log(("VBOXSF: VBoxMRxProcessCreate: CreateFlags |= SHFL_CF_ACT_OPEN_IF_EXISTS | SHFL_CF_ACT_FAIL_IF_NEW\n"));
        break;

    case FILE_CREATE:
        pCreateParms->CreateFlags |= SHFL_CF_ACT_FAIL_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW;
        Log(("VBOXSF: VBoxMRxProcessCreate: CreateFlags |= SHFL_CF_ACT_FAIL_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW\n"));
        break;

    case FILE_OPEN_IF:
        pCreateParms->CreateFlags |= SHFL_CF_ACT_OPEN_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW;
        Log(("VBOXSF: VBoxMRxProcessCreate: CreateFlags |= SHFL_CF_ACT_OPEN_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW\n"));
        break;

    case FILE_OVERWRITE:
        pCreateParms->CreateFlags |= SHFL_CF_ACT_OVERWRITE_IF_EXISTS | SHFL_CF_ACT_FAIL_IF_NEW;
        Log(("VBOXSF: VBoxMRxProcessCreate: CreateFlags |= SHFL_CF_ACT_OVERWRITE_IF_EXISTS | SHFL_CF_ACT_FAIL_IF_NEW\n"));
        break;

    case FILE_OVERWRITE_IF:
        pCreateParms->CreateFlags |= SHFL_CF_ACT_OVERWRITE_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW;
        Log(("VBOXSF: VBoxMRxProcessCreate: CreateFlags |= SHFL_CF_ACT_OVERWRITE_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW\n"));
        break;

    default:
        AssertMsgFailed(("VBOXSF: VBoxMRxProcessCreate: Unexpected create disposition: 0x%08X\n", CreateDisposition));
        Status = STATUS_INVALID_PARAMETER;
        goto failure;
    }

    Log(("VBOXSF: VBoxMRxProcessCreate: DesiredAccess = 0x%08x\n", DesiredAccess));
    Log(("VBOXSF: VBoxMRxProcessCreate: ShareAccess   = 0x%08x\n", ShareAccess));

    if (DesiredAccess & FILE_READ_DATA)
    {
        Log(("VBOXSF: VBoxMRxProcessCreate: FILE_READ_DATA\n"));
        pCreateParms->CreateFlags |= SHFL_CF_ACCESS_READ;
    }

    if (DesiredAccess & FILE_WRITE_DATA)
    {
        Log(("VBOXSF: VBoxMRxProcessCreate: FILE_WRITE_DATA\n"));
        /* FILE_WRITE_DATA means write access regardless of FILE_APPEND_DATA bit.
         */
        pCreateParms->CreateFlags |= SHFL_CF_ACCESS_WRITE;
    }
    else if (DesiredAccess & FILE_APPEND_DATA)
    {
        Log(("VBOXSF: VBoxMRxProcessCreate: FILE_APPEND_DATA\n"));
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

#ifdef VBOX_FAKE_IO_CREATE
    {
        LARGE_INTEGER liSystemTime;
        pFileBasicInfo->FileAttributes = FILE_ATTRIBUTE_NORMAL;
        KeQuerySystemTime(&liSystemTime);
        pFileBasicInfo->CreationTime = liSystemTime;
        pFileBasicInfo->LastAccessTime = liSystemTime;
        pFileBasicInfo->LastWriteTime = liSystemTime;
        pFileBasicInfo->ChangeTime = liSystemTime;
        pFileStandardInfo->AllocationSize.QuadPart = 0;
        pFileStandardInfo->EndOfFile.QuadPart = 0;
        pFileStandardInfo->NumberOfLinks = 0;

        *pulCreateAction = FILE_CREATED;

        *pHandle = (SHFLHANDLE)0xABCDEF00;
    }
#else
    {
        PSHFLSTRING ParsedPath = 0;
        ULONG ParsedPathSize;

        /* Calculate length required for parsed path.
         */
        ParsedPathSize = sizeof(*ParsedPath) + (RemainingName->Length + sizeof(WCHAR));

        Log(("VBOXSF: VBoxMRxProcessCreate: ParsedPathSize = %d\n", ParsedPathSize));

        ParsedPath = (PSHFLSTRING)vbsfAllocNonPagedMem(ParsedPathSize);
        if (NULL == ParsedPath)
        {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto failure;
        }

        ShflStringInitBuffer(ParsedPath, ParsedPathSize - sizeof(SHFLSTRING));

        ParsedPath->u16Size = RemainingName->Length + sizeof(WCHAR);
        ParsedPath->u16Length = ParsedPath->u16Size - sizeof(WCHAR); /* without terminating null */
        RtlCopyMemory (ParsedPath->String.ucs2, RemainingName->Buffer, ParsedPath->u16Length);
        Log(("ParsedPath: %.*ls\n", ParsedPath->u16Length / sizeof(WCHAR), ParsedPath->String.ucs2));

        /* Call host. */
        Log(("VBOXSF: VBoxMRxProcessCreate: vboxCallCreate called.\n"));
        vboxRC = vboxCallCreate(&pDeviceExtension->hgcmClient, &pNetRootExtension->map, ParsedPath, pCreateParms);
        vbsfFreeNonPagedMem(ParsedPath);
    }

    Log(("VBOXSF: VBoxMRxProcessCreate: vboxCallCreate returns vboxRC = %Rrc, Result = %x\n", vboxRC, pCreateParms->Result));
    if (RT_FAILURE(vboxRC))
    {
        Log(("VBOXSF: VBoxMRxProcessCreate: vboxCallCreate failed with %Rrc\n", vboxRC));
        /* Map some VBoxRC to STATUS codes expected by the system. */
        switch (vboxRC)
        {

        case VERR_ALREADY_EXISTS:
            {
                *pulCreateAction = FILE_EXISTS;
                Status = STATUS_OBJECT_NAME_COLLISION;
            }
            goto failure;
            break;

        /* On POSIX systems, the "mkdir" command returns VERR_FILE_NOT_FOUND when
           doing a recursive directory create. Handle this case in the next switch below. */
        case VERR_FILE_NOT_FOUND:

            pCreateParms->Result = SHFL_PATH_NOT_FOUND;
            break;

        default:
            {
                *pulCreateAction = FILE_DOES_NOT_EXIST;
                Status = VBoxErrorToNTStatus(vboxRC);
            }
            goto failure;
            break;
        }
    }

    /*
     * The request succeeded. Analyze host response,
     */
    switch (pCreateParms->Result)
    {
    case SHFL_PATH_NOT_FOUND:
    {
        /* Path to object does not exist. */
        Log(("VBOXSF: VBoxMRxProcessCreate: Path not found\n"));
        *pulCreateAction = FILE_DOES_NOT_EXIST;
        Status = STATUS_OBJECT_PATH_NOT_FOUND;
        goto failure;
    }

    case SHFL_FILE_NOT_FOUND:
    {
        Log(("VBOXSF: VBoxMRxProcessCreate: File not found\n"));
        *pulCreateAction = FILE_DOES_NOT_EXIST;
        if (pCreateParms->Handle == SHFL_HANDLE_NIL)
        {
            Status = STATUS_OBJECT_NAME_NOT_FOUND;
            goto failure;
        }

        /* Here we can only have the OpenTargetDirectory mode. */
        if (!bf.OpenTargetDirectory)
        {
            Log(("VBOXSF: VBoxMRxProcessCreate: File not found but have a handle!\n"));
            Status = STATUS_UNSUCCESSFUL;
            goto failure;
        }
        /* File components does not exist but target directory was opened. */
        break;
    }

    case SHFL_FILE_EXISTS:
    {
        Log(("VBOXSF: VBoxMRxProcessCreate: File exists, Handle = %08x\n", pCreateParms->Handle));
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
            Log(("VBOXSF: VBoxMRxProcessCreate: Existing file was not opened!\n"));
            Status = STATUS_ACCESS_DENIED;
            goto failure;
        }

        if (bf.OpenTargetDirectory)
        {
            *pulCreateAction = FILE_EXISTS;
        }
        else
        {
            *pulCreateAction = FILE_OPENED;
        }

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
        {
            *pulCreateAction = FILE_SUPERSEDED;
        }
        else
        {
            *pulCreateAction = FILE_OVERWRITTEN;
        }
        /* Go check flags and create FCB. */
        break;
    }

    default:
    {
        Log(("VBOXSF: VBoxMRxProcessCreate: Invalid CreateResult from host (0x%08X)\n", pCreateParms->Result));
        *pulCreateAction = FILE_DOES_NOT_EXIST;
        Status = STATUS_OBJECT_PATH_NOT_FOUND;
        goto failure;
    }
    }

    /* Check flags. */
    if (bf.NonDirectoryFile && FlagOn(pCreateParms->Info.Attr.fMode, RTFS_DOS_DIRECTORY))
    {
        /* Caller wanted only a file, but the object is a directory. */
        Log(("VBOXSF: VBoxMRxProcessCreate: File is a directory!\n"));
        Status = STATUS_FILE_IS_A_DIRECTORY;
        goto failure;
    }

    if (bf.DirectoryFile && !FlagOn(pCreateParms->Info.Attr.fMode, RTFS_DOS_DIRECTORY))
    {
        /* Caller wanted only a directory, but the object is not a directory. */
        Log(("VBOXSF: VBoxMRxProcessCreate: File is not a directory!\n"));
        Status = STATUS_NOT_A_DIRECTORY;
        goto failure;
    }

    *pHandle = pCreateParms->Handle;

    /* Translate attributes */
    pFileBasicInfo->FileAttributes = VBoxToNTFileAttributes(pCreateParms->Info.Attr.fMode);

    /* Translate file times */
    pFileBasicInfo->CreationTime.QuadPart = RTTimeSpecGetNtTime(&pCreateParms->Info.BirthTime); /* ridiculous name */
    pFileBasicInfo->LastAccessTime.QuadPart = RTTimeSpecGetNtTime(&pCreateParms->Info.AccessTime);
    pFileBasicInfo->LastWriteTime.QuadPart = RTTimeSpecGetNtTime(&pCreateParms->Info.ModificationTime);
    pFileBasicInfo->ChangeTime.QuadPart = RTTimeSpecGetNtTime(&pCreateParms->Info.ChangeTime);

    if (!FlagOn(pCreateParms->Info.Attr.fMode, RTFS_DOS_DIRECTORY))
    {
        Assert(!FlagOn(pCreateParms->Info.Attr.fMode, RTFS_DOS_DIRECTORY));
        pFileStandardInfo->AllocationSize.QuadPart = pCreateParms->Info.cbAllocated;
        pFileStandardInfo->EndOfFile.QuadPart = pCreateParms->Info.cbObject;
        pFileStandardInfo->Directory = FALSE;

        Log(("VBOXSF: VBoxMRxProcessCreate: AllocationSize = %RX64, EndOfFile = %RX64\n", pCreateParms->Info.cbAllocated, pCreateParms->Info.cbObject));
    }
    else
    {
        Assert(FlagOn(pCreateParms->Info.Attr.fMode, RTFS_DOS_DIRECTORY));
        pFileStandardInfo->AllocationSize.QuadPart = 0;
        pFileStandardInfo->EndOfFile.QuadPart = 0;
        pFileStandardInfo->Directory = TRUE;
    }
    pFileStandardInfo->NumberOfLinks = 0;
    pFileStandardInfo->DeletePending = FALSE;
#endif

    vbsfFreeNonPagedMem(pCreateParms);
    return Status;

    failure:

    Log(("VBOXSF: VBoxMRxProcessCreate: Returned with status = 0x%08lx\n", Status));
    if (pCreateParms && pCreateParms->Handle != SHFL_HANDLE_NIL)
    {
#ifndef VBOX_FAKE_IO_CREATE
        vboxCallClose(&pDeviceExtension->hgcmClient, &pNetRootExtension->map, pCreateParms->Handle);
#endif
        *pHandle = SHFL_HANDLE_NIL;
    }

    if (pCreateParms)
        vbsfFreeNonPagedMem(pCreateParms);

    return Status;
}

VOID VBoxMRxSetSrvOpenFlags (PRX_CONTEXT RxContext, RX_FILE_TYPE StorageType, PMRX_SRV_OPEN SrvOpen)
{
    PMRX_SRV_CALL SrvCall = (PMRX_SRV_CALL)RxContext->Create.pSrvCall;

    //
    //  set this only if cache manager will be used for mini-rdr handles !
    //
    Log(("VBoxMRxSetSrvOpenFlags %x\n", SrvOpen->BufferingFlags));
#if 0
    /* This seems to be the root cause of the strange bug checks when enabling
     * the driver verifier.
     */
    SrvOpen->BufferingFlags |= (FCB_STATE_WRITECACHEING_ENABLED |
            FCB_STATE_FILESIZECACHEING_ENABLED |
            FCB_STATE_FILETIMECACHEING_ENABLED |
            FCB_STATE_WRITEBUFFERING_ENABLED |
            FCB_STATE_LOCK_BUFFERING_ENABLED |
            FCB_STATE_READBUFFERING_ENABLED |
            FCB_STATE_READCACHEING_ENABLED);
#endif
}

NTSTATUS VBoxMRxCreateFileSuccessTail (PRX_CONTEXT RxContext, PBOOLEAN MustRegainExclusiveResource, RX_FILE_TYPE StorageType, ULONG CreateAction, FILE_BASIC_INFORMATION* pFileBasicInfo,
                                       FILE_STANDARD_INFORMATION* pFileStandardInfo, SHFLHANDLE Handle)
/*++

 Routine Description:

 This routine finishes the initialization of the fcb and srvopen for a
 successful open.

 Arguments:


 Return Value:

 RXSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    RxCaptureFcb;
    PMRX_SRV_OPEN SrvOpen = RxContext->pRelevantSrvOpen;
    PMRX_VBOX_FOBX pVBoxFobx;

    FCB_INIT_PACKET InitPacket;

    Log(("VBOXSF: VBoxMRxCreateFileSuccessTail\n"));
    PAGED_CODE();

    Assert(NodeType(SrvOpen) == RDBSS_NTC_SRVOPEN);
    Assert(NodeType(RxContext) == RDBSS_NTC_RX_CONTEXT);

    if (*MustRegainExclusiveResource)
    { //this is required because of oplock breaks
        VboxRxAcquireExclusiveFcbResourceInMRx(capFcb);
        *MustRegainExclusiveResource = FALSE;
    }

    // This Fobx should be cleaned up by the wrapper
    RxContext->pFobx = VboxRxCreateNetFobx(RxContext, SrvOpen);
    if (RxContext->pFobx == NULL)
    {
        AssertFailed();
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Assert(VBoxRxIsFcbAcquiredExclusive(capFcb));
    Log(("VBOXSF: VBoxMRxCreateFileSuccessTail: StorageType = 0x%08lx, Action = 0x%08lx\n", StorageType, CreateAction));

    RxContext->Create.ReturnedCreateInformation = CreateAction;

    RxFormInitPacket(InitPacket, &pFileBasicInfo->FileAttributes, &pFileStandardInfo->NumberOfLinks, &pFileBasicInfo->CreationTime, &pFileBasicInfo->LastAccessTime, &pFileBasicInfo->LastWriteTime,
                     &pFileBasicInfo->ChangeTime, &pFileStandardInfo->AllocationSize, &pFileStandardInfo->EndOfFile, &pFileStandardInfo->EndOfFile);

    if (capFcb->OpenCount == 0)
    {
        VboxRxFinishFcbInitialization(capFcb, RDBSS_STORAGE_NTC(StorageType), &InitPacket);
    }
    else
    {
        Assert(StorageType == 0 || NodeType(capFcb) == RDBSS_STORAGE_NTC(StorageType));
    }

    VBoxMRxSetSrvOpenFlags(RxContext, StorageType, SrvOpen);

    RxContext->pFobx->OffsetOfNextEaToReturn = 1;
    //transition happens later

    pVBoxFobx = VBoxMRxGetFileObjectExtension(RxContext->pFobx);
    Assert(pVBoxFobx);
    if (pVBoxFobx)
    {
        Log(("VBOXSF: VBoxMRxCreateFileSuccessTail: VBoxFobx = %p\n", pVBoxFobx));
        pVBoxFobx->hFile = Handle;
        pVBoxFobx->pSrvCall = RxContext->Create.pSrvCall;
        pVBoxFobx->FileStandardInfo = *pFileStandardInfo;
        pVBoxFobx->FileBasicInfo = *pFileBasicInfo;
        pVBoxFobx->fKeepCreationTime = FALSE;
        pVBoxFobx->fKeepLastAccessTime = FALSE;
        pVBoxFobx->fKeepLastWriteTime = FALSE;
        pVBoxFobx->fKeepChangeTime = FALSE;
    }
    else return STATUS_INSUFFICIENT_RESOURCES;

    return Status;
}

NTSTATUS VBoxMRxCollapseOpen (IN OUT PRX_CONTEXT RxContext)
/*++

 Routine Description:

 This routine collapses a open locally

 Arguments:

 RxContext - the RDBSS context

 Return Value:

 RXSTATUS - The return status for the operation

 --*/
{
#ifdef VBOX_SHFL_WITH_COLLAPSE
    NTSTATUS Status;

    RxCaptureFcb;
    RxCaptureRequestPacket;

    PMRX_SRV_OPEN SrvOpen = RxContext->pRelevantSrvOpen;
    PMRX_SRV_CALL SrvCall = RxContext->Create.pSrvCall;
    PMRX_NET_ROOT NetRoot = capFcb->pNetRoot;
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(RxContext->pFobx);

    Log(("VBOXSF: VBoxMRxCollapseOpen: Old Fobx = %p, old VBoxFobx = %p\n", RxContext->pFobx, pVBoxFobx));

    RxContext->pFobx = (PMRX_FOBX)RxCreateNetFobx( RxContext, SrvOpen);
    pVBoxFobx = VBoxMRxGetFileObjectExtension(RxContext->pFobx);

    Log(("VBOXSF: VBoxMRxCollapseOpen: New Fobx = %p, new VBoxFobx = %p\n", RxContext->pFobx, pVBoxFobx));

    if (RxContext->pFobx != NULL)
    {
        SHFLCREATEPARMS CreateParms;
        PUNICODE_STRING RemainingName = GET_ALREADY_PREFIXED_NAME_FROM_CONTEXT(RxContext);
        int vboxRC;
        VBoxMRxGetDeviceExtension(RxContext, pDeviceExtension);
        VBoxMRxGetNetRootExtension(capFcb->pNetRoot,pNetRootExtension);
        FILE_BASIC_INFORMATION * pFileBasicInfo;
        FILE_STANDARD_INFORMATION* pFileStandardInfo;

        Assert(RxIsFcbAcquiredExclusive(capFcb));

        RxContext->pFobx->OffsetOfNextEaToReturn = 1;
        capReqPacket->IoStatus.Information = FILE_OPENED;
        Status = STATUS_SUCCESS;

        /* Initialize create parameters. */
        RtlZeroMemory (&CreateParms, sizeof (SHFLCREATEPARMS));

        CreateParms.Handle = SHFL_HANDLE_NIL;
        CreateParms.Result = SHFL_NO_RESULT;

        /* Assume it's a directory; open existing, read-only access, deny-none sharing */
        --> incorrect
        CreateParms.CreateFlags = SHFL_CF_OPEN_TARGET_DIRECTORY
        | SHFL_CF_DIRECTORY
        | SHFL_CF_ACT_OPEN_IF_EXISTS | SHFL_CF_ACT_FAIL_IF_NEW
        | SHFL_CF_ACCESS_READ
        | SHFL_CF_ACCESS_DENYNONE;
        {
            PSHFLSTRING ParsedPath = 0;
            ULONG ParsedPathSize;

            /* Calculate length required for parsed path.
             */
            ParsedPathSize = sizeof(*ParsedPath) + (RemainingName->Length + sizeof(WCHAR));

            Log(("ParsedPathSize %d\n", ParsedPathSize));

            ParsedPath = (PSHFLSTRING)vbsfAllocNonPagedMem(ParsedPathSize);
            if (NULL == ParsedPath)
            return STATUS_INSUFFICIENT_RESOURCES;

            ShflStringInitBuffer(ParsedPath, ParsedPathSize - sizeof(SHFLSTRING));

            ParsedPath->u16Size = RemainingName->Length + sizeof(WCHAR);
            ParsedPath->u16Length = ParsedPath->u16Size - sizeof(WCHAR); /* without terminating null */
            RtlCopyMemory (ParsedPath->String.ucs2, RemainingName->Buffer, ParsedPath->u16Length);

            /* Call host. */
            vboxRC = vboxCallCreate (&pDeviceExtension->hgcmClient, &pNetRootExtension->map, ParsedPath, &CreateParms);

            if (ParsedPath)
            vbsfFreeNonPagedMem (ParsedPath);
        }

        Log(("VBoxMRxCollapseOpen result %Rrc Result=%x\n", vboxRC, CreateParms.Result));
        if (RT_FAILURE(vboxRC))
        {
            /* A host/guest communication error occurred while processing the request. */
            AssertMsgFailed(("VBOXSF: vboxCallCreate failed with %Rrc\n", vboxRC));
            return STATUS_UNSUCCESSFUL;
        }

        /*
         * The request succeeded. Analyze host response,
         */
        switch (CreateParms.Result)
        {
            case SHFL_PATH_NOT_FOUND:
            {
                /* Path to object does not exist. */
                Log(("VBOXSF: Path not found\n"));
                return STATUS_OBJECT_PATH_NOT_FOUND;
            }

            case SHFL_FILE_NOT_FOUND:
            {
                Log(("VBOXSF: File not found\n"));
                return STATUS_OBJECT_NAME_NOT_FOUND;
            }
            case SHFL_FILE_EXISTS:
            {
                Log(("VBOXSF: Object exists.\n"));
                break; /* existing dir opened */
            }

            default:
            {
                Log(("VBOXSF: VBoxSF: invalid CreateResult from host %08X\n", CreateParms.Result));
                return STATUS_OBJECT_PATH_NOT_FOUND;
            }
        }

        pFileBasicInfo = &pVBoxFobx->FileBasicInfo;
        pFileStandardInfo = &pVBoxFobx->FileStandardInfo;

        /* Translate attributes */
        pFileBasicInfo->FileAttributes = VBoxToNTFileAttributes(CreateParms.Info.Attr.fMode);

        /* Translate file times */
        pFileBasicInfo->CreationTime.QuadPart = RTTimeSpecGetNtTime(&CreateParms.Info.BirthTime); /* ridiculous name */
        pFileBasicInfo->LastAccessTime.QuadPart = RTTimeSpecGetNtTime(&CreateParms.Info.AccessTime);
        pFileBasicInfo->LastWriteTime.QuadPart = RTTimeSpecGetNtTime(&CreateParms.Info.ModificationTime);
        pFileBasicInfo->ChangeTime.QuadPart = RTTimeSpecGetNtTime(&CreateParms.Info.ChangeTime);

        Assert(FlagOn(CreateParms.Info.Attr.fMode, RTFS_DOS_DIRECTORY));
        pFileStandardInfo->AllocationSize.QuadPart = 0;
        pFileStandardInfo->EndOfFile.QuadPart = 0;
        pFileStandardInfo->Directory = TRUE;
        pFileStandardInfo->NumberOfLinks = 0;
        pFileStandardInfo->DeletePending = FALSE;

        Log(("VBOXSF: VBoxMRxCollapseOpen VBoxFobx=%p\n", pVBoxFobx));
        pVBoxFobx->hFile = CreateParms.Handle;
        pVBoxFobx->pSrvCall = RxContext->Create.pSrvCall;
        pVBoxFobx->FileStandardInfo = *pFileStandardInfo;
        pVBoxFobx->FileBasicInfo = *pFileBasicInfo;
    }
    else
    {
        Status = (STATUS_INSUFFICIENT_RESOURCES);
        AssertFailed();
    }

    return Status;
#else

#ifdef DEBUG
    RxCaptureFcb;
    RxCaptureRequestPacket;

    PMRX_SRV_OPEN SrvOpen = RxContext->pRelevantSrvOpen;
    PMRX_SRV_CALL SrvCall = RxContext->Create.pSrvCall;
    PMRX_NET_ROOT NetRoot = capFcb->pNetRoot;
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(RxContext->pFobx);

    Log(("VBOXSF: VBoxMRxCollapseOpen: Old Fobx = %p, old VBoxFobx = %p -> Not implemented!\n", RxContext->pFobx, pVBoxFobx));
#endif
    return STATUS_NOT_IMPLEMENTED;
#endif
}

NTSTATUS VBoxMRxComputeNewBufferingState (IN OUT PMRX_SRV_OPEN pMRxSrvOpen, IN PVOID pMRxContext, OUT PULONG pNewBufferingState)
/*++

 Routine Description:

 This routine maps specific oplock levels into the appropriate RDBSS
 buffering state flags

 Arguments:

 pMRxSrvOpen - the MRX SRV_OPEN extension

 pMRxContext - the context passed to RDBSS at Oplock indication time

 pNewBufferingState - the place holder for the new buffering state

 Return Value:


 Notes:

 --*/
{
    NTSTATUS Status = STATUS_NOT_IMPLEMENTED;

    Log(("VBOXSF: VBoxMRxComputeNewBufferingState \n"));
    return (Status);
}

NTSTATUS VBoxMRxDeallocateForFcb (IN OUT PMRX_FCB pFcb)
{
    NTSTATUS Status = STATUS_SUCCESS;

    Log(("VBOXSF: VBoxMRxDeallocateForFcb\n"));

    return (Status);
}

NTSTATUS VBoxMRxTruncate (IN PRX_CONTEXT pRxContext)
/*++

 Routine Description:

 This routine truncates the contents of a file system object

 Arguments:

 pRxContext - the RDBSS context

 Return Value:

 RXSTATUS - The return status for the operation

 --*/
{
    Log(("VBoxMRxTruncate\n"));
    AssertMsgFailed(("Found a truncate"));
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS VBoxMRxCleanupFobx (IN PRX_CONTEXT RxContext)
/*++

 Routine Description:

 This routine cleansup a file system object...normally a noop. unless it's a pipe in which case
 we do the close at cleanup time and mark the file as being not open.

 Arguments:

 pRxContext - the RDBSS context

 Return Value:

 RXSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;

    RxCaptureFcb;
    RxCaptureFobx;

    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(RxContext->pFobx);
    NODE_TYPE_CODE TypeOfOpen = NodeType(capFcb);
    PMRX_SRV_OPEN pSrvOpen = NULL;
    BOOLEAN SearchHandleOpen = FALSE;

    PAGED_CODE();

    Log(("VBOXSF: VBoxMRxCleanupFobx: capFcb = %p, capFobx = %p\n", capFcb, capFobx));
    Assert(capFcb);
    Assert(capFobx);

    pSrvOpen = capFobx->pSrvOpen;
    Assert(pSrvOpen);

    Assert(NodeType(pSrvOpen) == RDBSS_NTC_SRVOPEN);
    Assert(NodeTypeIsFcb(capFcb));

    if (NULL == pVBoxFobx)
        return STATUS_INVALID_PARAMETER;

    Log(("VBOXSF: VBoxMRxCleanupFobx: vboxFobx = %p, Handle = 0x%08x\n", pVBoxFobx, pVBoxFobx->hFile));

    /* Close file */
    if (!((capFcb->pNetRoot->Type != NET_ROOT_PIPE) && !SearchHandleOpen))
    {
        if (pVBoxFobx->hFile != SHFL_HANDLE_NIL)
        {
            PMRX_NET_ROOT pNetRoot = capFcb->pNetRoot;
            VBoxMRxGetNetRootExtension(pNetRoot,pNetRootExtension);
            VBoxMRxGetDeviceExtension(RxContext, pDeviceExtension);

#ifndef VBOX_FAKE_IO_CREATE
            int vboxRC = vboxCallClose(&pDeviceExtension->hgcmClient, &pNetRootExtension->map, pVBoxFobx->hFile);
            AssertRC(vboxRC);
#endif

            pVBoxFobx->hFile = SHFL_HANDLE_NIL;
        }
    }

    if (FlagOn(capFcb->FcbState, FCB_STATE_ORPHANED))
    {
        Log(("VBOXSF: VBoxMRxCleanupFobx: File orphaned\n"));
        return (STATUS_SUCCESS);
    }

    if ((capFcb->pNetRoot->Type != NET_ROOT_PIPE) && !SearchHandleOpen)
    {
        Log(("VBOXSF: VBoxMRxCleanupFobx: File not for closing at cleanup\n"));
        return (STATUS_SUCCESS);
    }

    Log(("VBOXSF: VBoxMRxCleanupFobx: Returned with status = 0x%08lx\n", Status));
    return Status;
}

NTSTATUS VBoxMRxForcedClose (IN PMRX_SRV_OPEN pSrvOpen)
/*++

 Routine Description:

 This routine closes a file system object

 Arguments:

 pSrvOpen - the instance to be closed

 Return Value:

 RXSTATUS - The return status for the operation

 Notes:



 --*/
{
    Log(("VBOXSF: VBoxMRxForcedClose\n"));
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS VBoxMRxCloseSrvOpen (IN PRX_CONTEXT RxContext)
/*++

 Routine Description:

 This routine closes a file across the network

 Arguments:

 RxContext - the RDBSS context

 Return Value:

 RXSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;

    RxCaptureFcb;
    RxCaptureFobx;

    int vboxRC = 0;
    PMRX_SRV_OPEN pSrvOpen = NULL;
    PUNICODE_STRING RemainingName = NULL;
    NODE_TYPE_CODE TypeOfOpen = NodeType(capFcb);
    PMRX_NET_ROOT pNetRoot = capFcb->pNetRoot;

    VBoxMRxGetNetRootExtension(pNetRoot,pNetRootExtension);
    VBoxMRxGetDeviceExtension(RxContext, pDeviceExtension);

    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(capFobx);

    PAGED_CODE();

    Log(("VBOXSF: VBoxMRxCloseSrvOpen: capFcb = %p, capFobx = %p\n", capFcb, capFobx));
    Log(("VBOXSF: VBoxMRxCloseSrvOpen: pVBoxFobx = 0x%08x\n", pVBoxFobx));

    Assert(capFcb);
    Assert(capFobx);

    pSrvOpen = capFobx->pSrvOpen;
    Assert(pSrvOpen);

    RemainingName = pSrvOpen->pAlreadyPrefixedName;
    Log(("VBOXSF: VBoxMRxCloseSrvOpen: Remaining name = %.*ls, Len = %d\n", RemainingName->Length / sizeof(WCHAR), RemainingName->Buffer, RemainingName->Length));

    if (NULL == pVBoxFobx)
        return STATUS_INVALID_PARAMETER;

    /* If we renamed or delete the file/dir, then it's already closed */
    if (FlagOn(pSrvOpen->Flags, (SRVOPEN_FLAG_FILE_RENAMED | SRVOPEN_FLAG_FILE_DELETED)))
    {
        Assert(pVBoxFobx->hFile == SHFL_HANDLE_NIL);

        Log(("VBOXSF: VBoxMRxCloseSrvOpen: File was renamed; ignore close!\n"));
        return STATUS_SUCCESS;
    }

    /* Close file */
    if (pVBoxFobx->hFile != SHFL_HANDLE_NIL)
    {
#ifndef VBOX_FAKE_IO_CREATE
        vboxRC = vboxCallClose(&pDeviceExtension->hgcmClient, &pNetRootExtension->map, pVBoxFobx->hFile);
        AssertRC(vboxRC);
#endif
        pVBoxFobx->hFile = SHFL_HANDLE_NIL;
    }

    if (capFcb->FcbState & FCB_STATE_DELETE_ON_CLOSE)
        Log(("VBOXSF: VBoxMRxCloseSrvOpen: Delete on close. Open count = %d\n", capFcb->OpenCount));

    /* Remove file or directory if delete action is pending. */
    if ((capFcb->FcbState & FCB_STATE_DELETE_ON_CLOSE) && capFcb->OpenCount == 0)
    {
        Status = VBoxMRxRemove(RxContext);
    }

    return (Status);
}

NTSTATUS VBoxMRxDeallocateForFobx (IN OUT PMRX_FOBX pFobx)
{
    Log(("VBOXSF: VBoxMRxDeallocateForFobx\n"));

    return (STATUS_SUCCESS);
}

NTSTATUS VBoxMRxRemove (IN PRX_CONTEXT RxContext)
/*++

 Routine Description:

 This routine removes a file or directory

 Arguments:

 RxContext - the RDBSS context

 Return Value:

 NTSTATUS - The return status for the operation

 --*/
{
    RxCaptureFcb;
    RxCaptureFobx;
    PMRX_NET_ROOT pNetRoot = capFcb->pNetRoot;
    VBoxMRxGetNetRootExtension(pNetRoot,pNetRootExtension);
    VBoxMRxGetDeviceExtension(RxContext, pDeviceExtension);
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(capFobx);
    PUNICODE_STRING RemainingName = GET_ALREADY_PREFIXED_NAME_FROM_CONTEXT(RxContext);
    int vboxRC;
    PSHFLSTRING ParsedPath = 0;
    ULONG ParsedPathSize;
    PMRX_SRV_OPEN pSrvOpen = capFobx->pSrvOpen;
    NTSTATUS Status = STATUS_SUCCESS;

    Log(("VBOXSF: VBoxMRxRemove Delete %.*ls. open count = %d!!\n", RemainingName->Length / sizeof(WCHAR), RemainingName->Buffer, capFcb->OpenCount));

    /* Close file first if not already done. */
    if (pVBoxFobx->hFile != SHFL_HANDLE_NIL)
    {
#ifndef VBOX_FAKE_IO_CREATE
        vboxRC = vboxCallClose(&pDeviceExtension->hgcmClient, &pNetRootExtension->map, pVBoxFobx->hFile);
        AssertRC(vboxRC);
#endif
        pVBoxFobx->hFile = SHFL_HANDLE_NIL;
    }

    /* Calculate length required for parsed path.
     */
    ParsedPathSize = sizeof(*ParsedPath) + (RemainingName->Length + sizeof(WCHAR));

    Log(("ParsedPathSize %d\n", ParsedPathSize));

    ParsedPath = (PSHFLSTRING)vbsfAllocNonPagedMem(ParsedPathSize);
    if (!ParsedPath)
        return STATUS_INSUFFICIENT_RESOURCES;

    ShflStringInitBuffer(ParsedPath, ParsedPathSize - sizeof(SHFLSTRING));

    Log(("Setup ParsedPath\n"));
    ParsedPath->u16Size = RemainingName->Length + sizeof(WCHAR);
    ParsedPath->u16Length = ParsedPath->u16Size - sizeof(WCHAR); /* without terminating null */
    RtlCopyMemory (ParsedPath->String.ucs2, RemainingName->Buffer, ParsedPath->u16Length);

    /* Call host. */
#ifdef VBOX_FAKE_IO_DELETE
    vboxRC = VINF_SUCCESS;
#else
    vboxRC = vboxCallRemove(&pDeviceExtension->hgcmClient, &pNetRootExtension->map, ParsedPath, (pVBoxFobx->FileStandardInfo.Directory) ? SHFL_REMOVE_DIR : SHFL_REMOVE_FILE);
#endif

    if (ParsedPath)
        vbsfFreeNonPagedMem(ParsedPath);

    Status = VBoxErrorToNTStatus(vboxRC);
    if (vboxRC != VINF_SUCCESS)
        Log(("vboxCallRemove failed with %d (status=%d)\n", vboxRC, Status));

    if (vboxRC == VINF_SUCCESS)
        SetFlag(pSrvOpen->Flags, SRVOPEN_FLAG_FILE_DELETED);

    return Status;
}
