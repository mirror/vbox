/*++

 Copyright (c) 1989 - 1999 Microsoft Corporation

 Module Name:

 devfcb.c

 Abstract:

 This module implements the mechanism for deleting an established connection

 --*/

#include "precomp.h"
#pragma hdrstop
#include "rdbss_vbox.h"

//
//  The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_DEVFCB)

//#define FIXED_CONNECT_NAME L"\\;0:\\vboxsvr\\share"
#define FIXED_CONNECT_NAME L"\\;0:"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, VBoxMRxDevFcbXXXControlFile)
#endif

NTSYSAPI
NTSTATUS NTAPI
ZwSetSecurityObject (IN HANDLE Handle, IN SECURITY_INFORMATION SecurityInformation, IN PSECURITY_DESCRIPTOR SecurityDescriptor);

NTSTATUS VBoxMRxDevFcbXXXControlFile (IN OUT PRX_CONTEXT RxContext)
/*++

 Routine Description:

 This routine handles all the device FCB related FSCTL's in the mini rdr

 Arguments:

 RxContext - Describes the Fsctl and Context.

 Return Value:

 a valid NTSTATUS code.

 Notes:

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    RxCaptureFobx;
    UCHAR MajorFunctionCode = RxContext->MajorFunction;
    VBoxMRxGetDeviceExtension(RxContext,pDeviceExtension);
    PLOWIO_CONTEXT LowIoContext = &RxContext->LowIoContext;
    ULONG ControlCode = 0;

    Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: MajorFunctionCode = %x\n", MajorFunctionCode));

    switch (MajorFunctionCode)
    {
    case IRP_MJ_FILE_SYSTEM_CONTROL:
    {
        switch (LowIoContext->ParamsFor.FsCtl.MinorFunction)
        {
        case IRP_MN_USER_FS_REQUEST:
            Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IRP_MN_USER_FS_REQUEST: ControlCode = %x\n", ControlCode));
            Status = STATUS_INVALID_DEVICE_REQUEST;
            break;

        default: //minor function != IRP_MN_USER_FS_REQUEST
            Status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }
        break;
    }

    case IRP_MJ_DEVICE_CONTROL:
    {

        ControlCode = LowIoContext->ParamsFor.IoCtl.IoControlCode;

        switch (ControlCode)
        {
        case IOCTL_MRX_VBOX_ADDCONN:
            Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_ADDCONN\n"));
            Status = VBoxMRxCreateConnection(RxContext, &RxContext->PostRequest);
            break;

        case IOCTL_MRX_VBOX_DELCONN:
            Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_DELCONN\n"));
            Status = VBoxMRxDeleteConnection(RxContext, &RxContext->PostRequest);
            break;

        case IOCTL_MRX_VBOX_GETLIST:
        {
            ULONG cbOut = LowIoContext->ParamsFor.IoCtl.OutputBufferLength;
            PVOID pbOut = LowIoContext->ParamsFor.IoCtl.pOutputBuffer;

            Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_GETLIST\n"));

            if ((cbOut >= _MRX_MAX_DRIVE_LETTERS) && (NULL != pbOut))
            {
                BOOLEAN GotMutex = FALSE;

                Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_GETLIST: Copying local connections ...\n"));

                if (NULL == pDeviceExtension)
                    break;

                GotMutex = ExTryToAcquireFastMutex(&pDeviceExtension->mtxLocalCon);

                RtlCopyMemory (pbOut, pDeviceExtension->cLocalConnections, _MRX_MAX_DRIVE_LETTERS);

                if (GotMutex)
                    ExReleaseFastMutex(&pDeviceExtension->mtxLocalCon);

                RxContext->InformationToReturn = _MRX_MAX_DRIVE_LETTERS;
            }
            else
            {
                Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_GETLIST: Nothing to copy, cbOut is too small (%d bytes)\n", cbOut));
                RxContext->InformationToReturn = 0;
            }

            Status = STATUS_SUCCESS;
            break;
        }

        /*
         * Returns the root IDs of shared folder mappings.
         */
        case IOCTL_MRX_VBOX_GETGLOBALLIST:
        {
            ULONG cbOut = LowIoContext->ParamsFor.IoCtl.OutputBufferLength;
            uint8_t *pbOut = (uint8_t *)LowIoContext->ParamsFor.IoCtl.pOutputBuffer;

            Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_GETGLOBALLIST\n"));

            if (NULL == pDeviceExtension)
                break;

            RxContext->InformationToReturn = 0;
            if ((cbOut >= _MRX_MAX_DRIVE_LETTERS) && (NULL != pbOut))
            {
                SHFLMAPPING mappings[_MRX_MAX_DRIVE_LETTERS];
                uint32_t cMappings = RT_ELEMENTS(mappings);
                int vboxRC;

                vboxRC = vboxCallQueryMappings(&pDeviceExtension->hgcmClient, mappings, &cMappings);
                if (vboxRC == VINF_SUCCESS)
                {
                    uint32_t i;

                    RtlZeroMemory(pbOut, _MRX_MAX_DRIVE_LETTERS);
                    for (i = 0; i < RT_MIN(cMappings, cbOut); i++)
                    {
                        pbOut[i] = mappings[i].root;
                        pbOut[i] |= 0x80; /* mark active *//** @todo fix properly */
                    }

                    RxContext->InformationToReturn = _MRX_MAX_DRIVE_LETTERS;
                }
                else
                {
                    Status = VBoxErrorToNTStatus(vboxRC);
                    Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_GETGLOBALLIST Error: 0x%08x\n", Status));
                }
            }

            Status = STATUS_SUCCESS;
            break;
        }

        /*
         * Translates a local connection name (e.g. drive "S:") to the
         * corresponding remote name (e.g. \\vboxsrv\share).
         */
        case IOCTL_MRX_VBOX_GETCONN:
        {
            ULONG ulConnectNameLen = LowIoContext->ParamsFor.IoCtl.InputBufferLength;
            PWCHAR pwcConnectName = (PWCHAR)LowIoContext->ParamsFor.IoCtl.pInputBuffer;

            ULONG ulRemoteNameLen = LowIoContext->ParamsFor.IoCtl.OutputBufferLength;
            PULONG pulRemoteName = (PULONG)LowIoContext->ParamsFor.IoCtl.pOutputBuffer;

            Log(
                ("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_GETCONN: ConnectName = %.*ls, Len = %d, RemoteName = 0x%x, Len = %d\n", ulConnectNameLen / sizeof(WCHAR), pwcConnectName, ulConnectNameLen, pulRemoteName, ulRemoteNameLen));

            if (NULL == pDeviceExtension)
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            /* Insert the local connection name. */
            Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_GETCONN: Looking up connection name and connections ...\n"));
            if ((ulConnectNameLen > sizeof(WCHAR)) && (NULL != pwcConnectName))
            {
                ULONG ulLocalConnectionNameLen;

                uint32_t idx = *pwcConnectName - L'A';

                ExAcquireFastMutex(&pDeviceExtension->mtxLocalCon);
                Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_GETCONN: Index = %d\n", idx));

                if ((idx < 0) || (idx >= RTL_NUMBER_OF(pDeviceExtension->wszLocalConnectionName)))
                {
                    Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_GETCONN: Index is invalid!\n"));
                    ExReleaseFastMutex(&pDeviceExtension->mtxLocalCon);
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                if (NULL == pDeviceExtension->wszLocalConnectionName[idx])
                {
                    Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_GETCONN: LocalConnectionName is NULL!\n"));
                    ExReleaseFastMutex(&pDeviceExtension->mtxLocalCon);
                    Status = STATUS_BAD_NETWORK_NAME;
                    break;
                }

                ulLocalConnectionNameLen = pDeviceExtension->wszLocalConnectionName[idx]->Length;

                Log(
                    ("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_GETCONN: LocalConnectionName = %.*ls\n", ulLocalConnectionNameLen / sizeof(WCHAR), pDeviceExtension->wszLocalConnectionName[idx]->Buffer));

                if ((pDeviceExtension->cLocalConnections[idx]) && (ulLocalConnectionNameLen <= ulRemoteNameLen))
                {
                    RtlZeroMemory (pulRemoteName, ulRemoteNameLen);
                    RtlCopyMemory (pulRemoteName, pDeviceExtension->wszLocalConnectionName[idx]->Buffer, ulLocalConnectionNameLen);

                    Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_GETCONN: Remote name = %.*ls, Size = %d\n", ulLocalConnectionNameLen / sizeof(WCHAR), pulRemoteName, ulLocalConnectionNameLen));
                    RxContext->InformationToReturn = ulLocalConnectionNameLen;
                }
                else
                {
                    Status = STATUS_BUFFER_TOO_SMALL;
                    RxContext->InformationToReturn = ulLocalConnectionNameLen;
                }

                ExReleaseFastMutex(&pDeviceExtension->mtxLocalCon);
            }
            else
            {
                Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_GETCONN: No connection name found!\n"));
                Status = STATUS_BAD_NETWORK_NAME;
            }

            break;
        }

        case IOCTL_MRX_VBOX_GETGLOBALCONN:
        {
            ULONG ReturnedSize = 0;
            uint8_t *pConnectId = (uint8_t *)LowIoContext->ParamsFor.IoCtl.pInputBuffer;
            ULONG RemoteNameLen = LowIoContext->ParamsFor.IoCtl.OutputBufferLength;
            PULONG RemoteName = (PULONG)LowIoContext->ParamsFor.IoCtl.pOutputBuffer;
            PSHFLSTRING pString;
            int vboxRC;
            uint32_t cbString = sizeof(SHFLSTRING) + RemoteNameLen;

            Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_GETGLOBALCONN: Connection ID = %d, RemoteName = 0x%x, Len = %d\n", *pConnectId, RemoteName, RemoteNameLen));

            pString = (PSHFLSTRING)vbsfAllocNonPagedMem(cbString);
            if (!pString)
            {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }
            memset(pString, 0, cbString);
            ShflStringInitBuffer(pString, RemoteNameLen); /** @todo r=andy This function does not have an effect ...? */

            vboxRC = vboxCallQueryMapName(&pDeviceExtension->hgcmClient, (*pConnectId) & ~0x80 /** @todo fix properly */, pString, cbString);
            if (vboxRC == VINF_SUCCESS)
            {
                if (pString->u16Length < RemoteNameLen)
                {
                    ReturnedSize = pString->u16Length;
                    RtlCopyMemory( RemoteName, pString->String.ucs2, pString->u16Length);
                    Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_GETGLOBALCONN: Returned name = %.*ls, Size = %d\n", ReturnedSize / sizeof(WCHAR), RemoteName, ReturnedSize));
                }
            }
            else
                Status = VBoxErrorToNTStatus(vboxRC);

            vbsfFreeNonPagedMem(pString);

            if (ReturnedSize)
            {
                Status = STATUS_SUCCESS;
            }
            else
            {
                Status = STATUS_BAD_NETWORK_NAME;
            }
            RxContext->InformationToReturn = ReturnedSize;
            break;
        }

        case IOCTL_MRX_VBOX_START:
        {
            Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_START: Called.\n"));
            switch (VBoxMRxState)
            {
            case MRX_VBOX_STARTABLE:

                /* The correct sequence of start events issued by the workstation
                 service would have avoided this. We can recover from this
                 by actually invoking RxStartMiniRdr. */

                if (capFobx)
                {
                    Status = STATUS_INVALID_DEVICE_REQUEST;
                    break;;
                }

                /* Set global VBoxMRxState variable to MRX_VBOX_STARTABLE and fall through to the next case. */
                InterlockedCompareExchange((PLONG) & VBoxMRxState, MRX_VBOX_START_IN_PROGRESS, MRX_VBOX_STARTABLE);

                /* Lack of break is intentional. */

            case MRX_VBOX_START_IN_PROGRESS:
                Status = VBoxRxStartMinirdr(RxContext, &RxContext->PostRequest);

                if (Status == STATUS_REDIRECTOR_STARTED)
                    Status = STATUS_SUCCESS;
                else if (Status == STATUS_PENDING && RxContext->PostRequest == TRUE)
                    Status = STATUS_MORE_PROCESSING_REQUIRED;

                /* Allow restricted users to use shared folders; works only in XP and Vista. (@@todo hack) */
                if (Status == STATUS_SUCCESS)
                {
                    SECURITY_DESCRIPTOR SecurityDescriptor;
                    OBJECT_ATTRIBUTES InitializedAttributes;
                    HANDLE hDevice;
                    IO_STATUS_BLOCK IoStatusBlock;
                    UNICODE_STRING UserModeDeviceName;

                    RtlInitUnicodeString(&UserModeDeviceName, DD_MRX_VBOX_USERMODE_SHADOW_DEV_NAME_U);

                    /* Create empty security descriptor */
                    RtlZeroMemory (&SecurityDescriptor, sizeof (SecurityDescriptor));
                    Status = RtlCreateSecurityDescriptor(&SecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
                    if (Status != STATUS_SUCCESS)
                    {
                        Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_START: MRX_VBOX_START_IN_PROGRESS: RtlCreateSecurityDescriptor failed with 0x%x!\n", Status));
                        return Status;
                    }
                    RtlZeroMemory (&InitializedAttributes, sizeof (InitializedAttributes));
                    InitializeObjectAttributes(&InitializedAttributes, &UserModeDeviceName, OBJ_KERNEL_HANDLE, 0, 0);

                    /* Open our symbolic link device name */
                    Status = ZwOpenFile(&hDevice, WRITE_DAC, &InitializedAttributes, &IoStatusBlock, 0, 0);
                    if (Status != STATUS_SUCCESS)
                    {
                        Log(
                            ("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_START: MRX_VBOX_START_IN_PROGRESS: ZwOpenFile %ls failed with 0x%x!\n", DD_MRX_VBOX_USERMODE_SHADOW_DEV_NAME_U, Status));
                        return Status;
                    }

                    /* Override the discretionary access control list (DACL) settings */
                    Status = ZwSetSecurityObject(hDevice, DACL_SECURITY_INFORMATION, &SecurityDescriptor);
                    if (Status != STATUS_SUCCESS)
                    {
                        Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_START: MRX_VBOX_START_IN_PROGRESS: ZwSetSecurityObject failed with 0x%x!\n", Status));
                        return Status;
                    }

                    Status = ZwClose(hDevice);
                    if (Status != STATUS_SUCCESS)
                    {
                        Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_START: MRX_VBOX_START_IN_PROGRESS: ZwClose failed with 0x%x\n", Status));
                        return Status;
                    }
                }
                break;

            case MRX_VBOX_STARTED:
                Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_START: MRX_VBOX_STARTED: Already started\n"));
                Status = STATUS_SUCCESS;
                break;

            default:
                Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_START: Invalid state (%d)!\n", VBoxMRxState));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }
            Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_START: Returned 0x%x\n", Status));
            break;
        }

        case IOCTL_MRX_VBOX_STOP:
        {
            MRX_VBOX_STATE CurrentState;

            Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_STOP: Called.\n"));
            if (capFobx)
            {
                Status = STATUS_INVALID_DEVICE_REQUEST;
                break;
            }

            if (RxContext->RxDeviceObject->NumberOfActiveFcbs > 0)
            {
                Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_STOP: Open handles = %d\n", RxContext->RxDeviceObject->NumberOfActiveFcbs));
                Status = STATUS_REDIRECTOR_HAS_OPEN_HANDLES;
                break;
            }

            CurrentState = (MRX_VBOX_STATE)InterlockedCompareExchange((PLONG) & VBoxMRxState, MRX_VBOX_STARTABLE, MRX_VBOX_STARTED);

            Status = VBoxRxStopMinirdr(RxContext, &RxContext->PostRequest);
            Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_STOP: Returned 0x%x\n", Status));

            if (Status == STATUS_PENDING && RxContext->PostRequest == TRUE)
                Status = STATUS_MORE_PROCESSING_REQUIRED;

            break;
        }

        default:
            Status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }
        break;
    }

    case IRP_MJ_INTERNAL_DEVICE_CONTROL:
    {
        Status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    default:
        AssertMsgFailed(("VBOXSF: VBoxMRxDevFcbXXXControlFile: Unimplemented major function!"));
        Status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: Status = %08lx, Info = %08lx\n", Status, RxContext->InformationToReturn));
    return Status;
}

HANDLE GetConnectionHandle (IN PUNICODE_STRING ConnectionName)
{

    NTSTATUS Status;
    HANDLE Handle;
    IO_STATUS_BLOCK IoStatusBlock;
    OBJECT_ATTRIBUTES ObjectAttributes;
#ifndef VBOX
    UNICODE_STRING FileName;
#endif

    // Connection name should get checked to be certain our device is in the path

    Log(("VBOXSF: GetConnectionHandle: Called.\n"));

    InitializeObjectAttributes(&ObjectAttributes, ConnectionName, OBJ_CASE_INSENSITIVE, NULL, NULL);

    Status = ZwCreateFile(&Handle, SYNCHRONIZE, &ObjectAttributes, &IoStatusBlock, NULL, // Allocation size
                          FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN_IF, FILE_CREATE_TREE_CONNECTION | FILE_SYNCHRONOUS_IO_NONALERT, NULL, // Ptr to EA Buffer
                          0); // Length of EA buffer

    Log(("VBOXSF: GetConnectionHandle: ZwCreateFile returned 0x%x\n", Status));

    if ((STATUS_SUCCESS == Status) && (INVALID_HANDLE_VALUE != Handle))
    {
        Log(("VBOXSF: GetConnectionHandle: ZwCreateFile returned success\n"));
    }
    else
    {
        Handle = INVALID_HANDLE_VALUE;
        Log(("VBOXSF: GetConnectionHandle: ZwCreateFile returned invalid handle!\n"));
    }

    return Handle;
}

NTSTATUS DoCreateConnection (IN PRX_CONTEXT RxContext, ULONG CreateDisposition)
{

    NTSTATUS Status = STATUS_SUCCESS;
    HANDLE Handle;
    PLOWIO_CONTEXT LowIoContext = &RxContext->LowIoContext;
    ULONG ConnectNameLen = LowIoContext->ParamsFor.IoCtl.InputBufferLength;
    PWCHAR ConnectName = (PWCHAR)LowIoContext->ParamsFor.IoCtl.pInputBuffer;
    UNICODE_STRING FileName;

    VBoxMRxGetDeviceExtension(RxContext,pDeviceExtension);

    if (NULL == pDeviceExtension)
        return STATUS_INVALID_PARAMETER;

    if ((0 == ConnectNameLen) || (NULL == ConnectName))
    {
        Log(("VBOXSF: DoCreateConnection: Connection name / length is invalid!\n"));
        return STATUS_INVALID_PARAMETER;
    }

    // The sample code passes in only the filename in the Ioctl data buffer.
    //  An actual implementation could pass in stuff like EAs, security
    // credentials, etc

    Log(("VBOXSF: DoCreateConnection: Name = %.*ls, Length = %d\n", ConnectNameLen / sizeof(WCHAR), ConnectName, ConnectNameLen));

    FileName.Buffer = ConnectName;
    FileName.Length = (USHORT)ConnectNameLen;
    FileName.MaximumLength = (USHORT)ConnectNameLen;

    Handle = GetConnectionHandle(&FileName);

    if (INVALID_HANDLE_VALUE != Handle)
    {
        PWCHAR pwcLC;
        ULONG i;

        Log(("VBOXSF: DoCreateConnection: GetConnectionHandle returned success!\n"));
        ZwClose(Handle);

        /* Skip the "\Device\VBoxMiniRdr\;X:" of the string "\Device\VBoxMiniRdr\;X:\vboxsrv\sf" */
        pwcLC = (PWCHAR)ConnectName;
        for (i = 0; i < ConnectNameLen && *pwcLC != L':'; i += sizeof(WCHAR), pwcLC++)
            ;

        if (i >= sizeof(WCHAR) && i < ConnectNameLen)
        {
            pwcLC--; /* Go back to the drive letter, "X" for example. */
            if (*pwcLC >= L'A' && *pwcLC <= L'Z') /* Are we in range? */
            {
                uint32_t idx = *pwcLC - L'A'; /* Get the index based on the driver letter numbers (26). */
                PUNICODE_STRING pRemoteName;

                ExAcquireFastMutex(&pDeviceExtension->mtxLocalCon);

                if ((idx < 0) || (idx >= RTL_NUMBER_OF(pDeviceExtension->cLocalConnections)))
                {
                    Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_MRX_VBOX_GETCONN: Index is invalid!\n"));
                    ExReleaseFastMutex(&pDeviceExtension->mtxLocalCon);
                    return STATUS_BAD_NETWORK_NAME;
                }

                pDeviceExtension->cLocalConnections[idx] = TRUE;

                if (NULL != pDeviceExtension->wszLocalConnectionName[idx])
                    Log(("VBOXSF: DoCreateConnection: LocalConnectionName at index %d is NOT empty!\n", idx));

                /*pDeviceExtension->LocalConnectionName[idx] = (PUNICODE_STRING)ExAllocatePool(PagedPool, sizeof(UNICODE_STRING) + ConnectNameLen /* more than enough );*/
                pDeviceExtension->wszLocalConnectionName[idx] = (PUNICODE_STRING)vbsfAllocNonPagedMem(sizeof(UNICODE_STRING) + ConnectNameLen);

                if (NULL == pDeviceExtension->wszLocalConnectionName[idx])
                    Log(("VBOXSF: DoCreateConnection: LocalConnectionName at index %d NOT allocated!\n", idx));

                pRemoteName = pDeviceExtension->wszLocalConnectionName[idx];

                pRemoteName->Buffer = (PWSTR)(pRemoteName + 1);
                pRemoteName->Length = (USHORT)(ConnectNameLen - i - sizeof(WCHAR));
                pRemoteName->MaximumLength = pRemoteName->Length;
                RtlCopyMemory(&pRemoteName->Buffer[0], pwcLC+2, pRemoteName->Length);

                Log(("VBOXSF: DoCreateConnection: RemoteName %.*ls, Len = %d\n", pRemoteName->Length / sizeof(WCHAR), pRemoteName->Buffer, pRemoteName->Length));
                ExReleaseFastMutex(&pDeviceExtension->mtxLocalCon);
            }
        }
    }
    else
    {
        Log(("VBOXSF: DoCreateConnection: GetConnectionHandle returned failure!\n"));
        Status = STATUS_BAD_NETWORK_NAME;
    }

    return (Status);
}

NTSTATUS VBoxMRxCreateConnection (IN PRX_CONTEXT RxContext, OUT PBOOLEAN PostToFsp)
/*++

 Routine Description:


 Arguments:

 IN PRX_CONTEXT RxContext - Describes the Fsctl and Context....for later when i need the buffers

 Return Value:

 RXSTATUS

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;

    BOOLEAN Wait = BooleanFlagOn(RxContext->Flags, RX_CONTEXT_FLAG_WAIT);
    BOOLEAN InFSD = !BooleanFlagOn(RxContext->Flags, RX_CONTEXT_FLAG_IN_FSP);

    PAGED_CODE();

    Log(("VBOXSF: VBoxMRxCreateConnection: Called.\n"));

    if (!Wait)
    {
        //just post right now!
        *PostToFsp = TRUE;
        return STATUS_PENDING;
    }

    Status = DoCreateConnection(RxContext, (ULONG)FILE_OPEN_IF);
    return Status;
}

NTSTATUS DoDeleteConnection (PUNICODE_STRING FileName)
{
    HANDLE Handle;
    NTSTATUS Status;
    PFILE_OBJECT pFileObject;
    PNET_ROOT NetRoot;
    PV_NET_ROOT VNetRoot;
    PFOBX Fobx;

    Log(("VBOXSF: DoDeleteConnection: Called.\n"));

    Handle = GetConnectionHandle(FileName);

    if (INVALID_HANDLE_VALUE != Handle)
    {
        Log(("VBOXSF: DoDeleteConnection: GetConnectionHandle returned success\n"));

        Status = ObReferenceObjectByHandle(Handle, 0L, NULL, KernelMode, (PVOID *)&pFileObject, NULL);

        Log(("VBOXSF: DoDeleteConnection: ObReferenceObjectByHandle Status 0x%08X\n", Status));
        if (NT_SUCCESS(Status))
        {

            // Got the FileObject. Now get an Fobx
            Fobx = (PFOBX)pFileObject->FsContext2;
            if (NodeType(Fobx) == RDBSS_NTC_V_NETROOT)
            {
                VNetRoot = (PV_NET_ROOT)(Fobx);
                NetRoot = (PNET_ROOT)VNetRoot->NetRoot;
                Log(("Calling RxFinalizeConnection\n"));
                Status = VBoxRxFinalizeConnection(NetRoot, VNetRoot, TRUE);
            }
            else
            {
                ASSERT(FALSE);
                Status = STATUS_INVALID_DEVICE_REQUEST;
            }

            ObDereferenceObject(pFileObject);
        }

        ZwClose(Handle);
    }

    return Status;
}

NTSTATUS VBoxMRxDeleteConnection (IN PRX_CONTEXT RxContext, OUT PBOOLEAN PostToFsp)
/*++

 Routine Description:

 This routine deletes a single vnetroot.

 Arguments:

 IN PRX_CONTEXT RxContext - Describes the Fsctl and Context....for later when i need the buffers

 Return Value:

 RXSTATUS

 --*/
{
    NTSTATUS Status;
    UNICODE_STRING FileName;
    BOOLEAN Wait = BooleanFlagOn(RxContext->Flags, RX_CONTEXT_FLAG_WAIT);
    PLOWIO_CONTEXT LowIoContext = &RxContext->LowIoContext;
    PWCHAR ConnectName = (PWCHAR)LowIoContext->ParamsFor.IoCtl.pInputBuffer;
    ULONG ConnectNameLen = LowIoContext->ParamsFor.IoCtl.InputBufferLength;
    VBoxMRxGetDeviceExtension(RxContext,pDeviceExtension);

    PAGED_CODE();

    Log(("VBOXSF: VBoxMRxDeleteConnection: ConnectName = %.*ls\n", ConnectNameLen / sizeof(WCHAR), ConnectName));

    if (!Wait)
    {
        //just post right now!
        *PostToFsp = TRUE;
        return (STATUS_PENDING);
    }

    FileName.Buffer = ConnectName;
    FileName.Length = (USHORT)ConnectNameLen;
    FileName.MaximumLength = (USHORT)ConnectNameLen;

    Status = DoDeleteConnection(&FileName);

    if (NT_SUCCESS(Status))
    {
        PWCHAR pwcLC;
        ULONG i;

        for (i = 0, pwcLC = ConnectName; i < ConnectNameLen && *pwcLC != L':'; i += sizeof(WCHAR), pwcLC++)
            ;

        if (i >= sizeof(WCHAR) && i < ConnectNameLen)
        {
            pwcLC--;
            if (*pwcLC >= L'A' && *pwcLC <= L'Z')
            {
                uint32_t idx = *pwcLC - L'A';

                ExAcquireFastMutex(&pDeviceExtension->mtxLocalCon);

                pDeviceExtension->cLocalConnections[idx] = FALSE;

                /* Free saved name */
                /*Assert(pDeviceExtension->LocalConnectionName[idx]);
                 ExFreePool(pDeviceExtension->LocalConnectionName[idx]);
                 pDeviceExtension->LocalConnectionName[idx] = NULL;*/
                if (pDeviceExtension->wszLocalConnectionName[idx])
                {
                    vbsfFreeNonPagedMem(pDeviceExtension->wszLocalConnectionName[idx]);
                    pDeviceExtension->wszLocalConnectionName[idx] = NULL;
                }

                ExReleaseFastMutex(&pDeviceExtension->mtxLocalCon);
            }
        }
    }

    return Status;
}

