/*++

 Copyright (c) 1989 - 1999 Microsoft Corporation

 Module Name:

 netroot.c

 Abstract:

 This module implements the routines for creating the net root.

 --*/

#include "precomp.h"
#pragma hdrstop

//
// Forward declarations ...
//

NTSTATUS VBoxMRxUpdateNetRootState (IN OUT PMRX_NET_ROOT pNetRoot)
/*++

 Routine Description:

 This routine updates the mini redirector state associated with a net root.

 Arguments:

 pNetRoot - the net root instance.

 Return Value:

 NTSTATUS - The return status for the operation

 Notes:

 By differentiating the mini redirector state from the net root condition it is possible
 to permit a variety of reconnect strategies. It is conceivable that the RDBSS considers
 a net root to be good while the underlying mini redirector might mark it as invalid
 and reconnect on the fly.

 --*/
{
    NTSTATUS Status = STATUS_NOT_IMPLEMENTED;

    Log(("VBOXSF: VBoxMRxUpdateNetRootState: Called.\n"));
    return (Status);
}

NTSTATUS VBoxMRxInitializeNetRootEntry (IN PMRX_NET_ROOT pNetRoot)
/*++

 Routine Description:

 This routine initializes a new net root.
 It also validates rootnames. Eg: attempts to create a
 file in a root that has not been created will fail.

 Arguments:

 pNetRoot - the net root

 Return Value:

 NTSTATUS - The return status for the operation

 Notes:

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    PMRX_SRV_CALL pSrvCall = pNetRoot->pSrvCall;
    PMRX_VBOX_NETROOT_EXTENSION pNetRootExtension = (PMRX_VBOX_NETROOT_EXTENSION)pNetRoot->Context;

    Log(("VBOXSF: VBoxMRxInitializeNetRootEntry: Called.\n"));
    return Status;
}

NTSTATUS VBoxUpdateNetRoot (PMRX_NET_ROOT pNetRoot)
/*++

 Routine Description:

 This routine initializes the wrapper data structure corresponding to a
 given net root entry.

 Arguments:

 pNetRootEntry - the server entry

 Return Value:

 STATUS_SUCCESS if successful

 --*/
{
    PAGED_CODE();

    Log(("VBOXSF: VBoxUpdateNetRoot: NetRoot = 0x%x Type = 0x%x\n", pNetRoot, pNetRoot->Type));

    switch (pNetRoot->Type)
    {
    case NET_ROOT_DISK:
        pNetRoot->DeviceType = RxDeviceType(DISK);
        break;

    case NET_ROOT_PIPE:
        pNetRoot->DeviceType = RxDeviceType(NAMED_PIPE);
        break;
    case NET_ROOT_COMM:
        pNetRoot->DeviceType = RxDeviceType(SERIAL_PORT);
        break;
    case NET_ROOT_PRINT:
        pNetRoot->DeviceType = RxDeviceType(PRINTER);
        break;
    case NET_ROOT_MAILSLOT:
        pNetRoot->DeviceType = RxDeviceType(MAILSLOT);
        break;
    case NET_ROOT_WILD:
        /* We get this type when for example Windows Media player opens an MP3 file.
         * This NetRoot has the same remote path (\\vboxsrv\dir) as other NetRoots,
         * which were created earlier and which were NET_ROOT_DISK.
         *
         * In the beginning of the function (UpdateNetRoot) the DDK sample sets
         * pNetRoot->Type of newly created NetRoots using a value previously
         * pstored in a NetRootExtension. One NetRootExtensions is used for a single
         * remote path and reused by a few NetRoots, if they point to the same path.
         *
         * To simplify things we just set the type to DISK here (we do not support
         * anything else anyway), and update the DeviceType correspondingly.
         */
        pNetRoot->Type = NET_ROOT_DISK;
        pNetRoot->DeviceType = RxDeviceType(DISK);
        break;
    default:
        AssertMsgFailed(("VBOXSF: VBoxUpdateNetRoot: Invalid net root type! Type = 0x%x\n", pNetRoot->Type));
        break;
    }

    Log(("VBOXSF: VBoxUpdateNetRoot: leaving pNetRoot->DeviceType = 0x%x\n", pNetRoot->DeviceType));
    return STATUS_SUCCESS;
}

NTSTATUS VBoxMRxCreateVNetRoot (IN PMRX_CREATENETROOT_CONTEXT pCreateNetRootContext)
/*++

 Routine Description:

 This routine patches the RDBSS created net root instance with the information required
 by the mini redirector.

 Arguments:

 pVNetRoot - the virtual net root instance.

 pCreateNetRootContext - the net root context for calling back

 Return Value:

 NTSTATUS - The return status for the operation

 Notes:

 --*/
{
    NTSTATUS Status;
    PRX_CONTEXT pRxContext = pCreateNetRootContext->RxContext;
    PMRX_V_NET_ROOT pVNetRoot = (PMRX_V_NET_ROOT)pCreateNetRootContext->pVNetRoot;
    VBoxMRxGetDeviceExtension(pRxContext, pDeviceExtension);
    VBoxMRxGetNetRootExtension(pVNetRoot->pNetRoot, pNetRootExtension);

    PMRX_NET_ROOT pNetRoot = pVNetRoot->pNetRoot;
    PMRX_SRV_CALL pSrvCall = pNetRoot->pSrvCall;

    BOOLEAN fTreeConnectOpen = TRUE; // RxContext->Create.ThisIsATreeConnectOpen;
    BOOLEAN fInitializeNetRoot = FALSE;

    Log(("VBOXSF: VBoxMRxCreateVNetRoot: pNetRoot = %p, fTreeConnectOpen = %d\n", pNetRoot, pRxContext->Create.ThisIsATreeConnectOpen));

    /* IMPORTANT:
     *
     * This function must always call 'pCreateNetRootContext->Callback(pCreateNetRootContext)' before
     * returning and then return STATUS_PENDING. Otherwise Win64 will hang.
     */

    if (pNetRoot->Type == NET_ROOT_MAILSLOT || pNetRoot->Type == NET_ROOT_PIPE)
    {
        /* DDK sample returns this status code. And our driver does get such NetRoots. */
        Log(("VBOXSF: VBoxMRxCreateVNetRoot: Mailslot or Pipe open (%d) not supported!\n", pNetRoot->Type));
        pVNetRoot->Context = NULL;

        Status = STATUS_NOT_SUPPORTED;
        goto l_Exit;
    }

    // The V_NET_ROOT is associated with a NET_ROOT. The two cases of interest are as
    // follows
    // 1) the V_NET_ROOT and the associated NET_ROOT are being newly created.
    // 2) a new V_NET_ROOT associated with an existing NET_ROOT is being created.
    //
    // These two cases can be distinguished by checking if the context associated with
    // NET_ROOT is NULL. Since the construction of NET_ROOT's/V_NET_ROOT's are serialized
    // by the wrapper this is a safe check.
    // ( The wrapper cannot have more then one thread trying to initialize the same
    // NET_ROOT).
    //
    // The above is not really true in our case. Since we have asked the wrapper,
    // to manage our netroot extension, the netroot context will always be non-NULL.
    // We will distinguish the cases by checking our root state in the context...
    //

    if (pNetRoot->Context == NULL)
    {
        fInitializeNetRoot = TRUE;
    }
    else
    {
        VBoxMRxGetNetRootExtension(pNetRoot,pNetRootExtension);
        /* Detect an already initialized NetRoot.
         * pNetRootExtension is actually the pNetRoot->Context and it is not null.
         */
        fInitializeNetRoot = pNetRootExtension->phgcmClient == NULL;
    }

    Assert((NodeType(pNetRoot) == RDBSS_NTC_NETROOT) && (NodeType(pNetRoot->pSrvCall) == RDBSS_NTC_SRVCALL));

    Status = STATUS_SUCCESS;

    // update the net root state to be good.

    if (fInitializeNetRoot)
    {
        PWCHAR pRootName;
        ULONG RootNameLength;
        int vboxRC;
        PSHFLSTRING ParsedPath = 0;
        ULONG ParsedPathSize;

        pNetRoot->MRxNetRootState = MRX_NET_ROOT_STATE_GOOD;

        // validate the fixed netroot name

        RootNameLength = pNetRoot->pNetRootName->Length - pSrvCall->pSrvCallName->Length;
        if (!RootNameLength)
        {
            /* Refuse a netroot path with an empty shared folder name */
            Log(("VBOXSF: VBoxMRxCreateVNetRoot: Invalid shared folder name! (length=0)\n"));
            pNetRoot->MRxNetRootState = MRX_NET_ROOT_STATE_ERROR;

            Status = STATUS_BAD_NETWORK_NAME;
            goto l_Exit;
        }

        RootNameLength -= 2; /* remove leading backslash */

        pRootName = (PWCHAR)(pNetRoot->pNetRootName->Buffer + (pSrvCall->pSrvCallName->Length / sizeof(WCHAR)));
        pRootName++; /* remove leading backslash */

        if (pNetRootExtension->phgcmClient == NULL)
        {
            Log(("VBOXSF: VBoxMRxCreateVNetRoot: Initialize netroot length = %d, name = %.*ls\n", RootNameLength, RootNameLength / sizeof(WCHAR), pRootName));

            /* Calculate length required for parsed path.
             */
            ParsedPathSize = sizeof(*ParsedPath) + RootNameLength + sizeof(WCHAR);

            ParsedPath = (PSHFLSTRING)vbsfAllocNonPagedMem(ParsedPathSize);
            if (!ParsedPath)
            {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto l_Exit;
            }
            memset(ParsedPath, 0, ParsedPathSize);

            ShflStringInitBuffer(ParsedPath, ParsedPathSize - sizeof(SHFLSTRING));

            ParsedPath->u16Size = (uint16_t)RootNameLength + sizeof(WCHAR);
            ParsedPath->u16Length = ParsedPath->u16Size - sizeof(WCHAR); /* without terminating null */
            RtlCopyMemory(ParsedPath->String.ucs2, pRootName, ParsedPath->u16Length);

            vboxRC = vboxCallMapFolder(&pDeviceExtension->hgcmClient, ParsedPath, &pNetRootExtension->map);
            vbsfFreeNonPagedMem(ParsedPath);
            if (vboxRC != VINF_SUCCESS)
            {
                Log(("VBOXSF: VBoxMRxCreateVNetRoot: vboxCallMapFolder failed with %d\n", vboxRC));
                Status = STATUS_BAD_NETWORK_NAME;
            }
            else
            {
                Status = STATUS_SUCCESS;
                pNetRootExtension->phgcmClient = &pDeviceExtension->hgcmClient;
            }
        }
    }
    else Log(("VBOXSF: VBoxMRxCreateVNetRoot: Creating V_NET_ROOT on existing NET_ROOT!\n"));

    if ((Status == STATUS_SUCCESS) && fInitializeNetRoot)
    {
        //
        //  A new NET_ROOT and associated V_NET_ROOT are being created !
        //
        Status = VBoxMRxInitializeNetRootEntry(pNetRoot);
        Log(("VBOXSF: VBoxMRxCreateVNetRoot: NulMRXInitializeNetRootEntry called, Status = 0x%lx\n", Status));
    }

    VBoxUpdateNetRoot(pNetRoot);

l_Exit:
    if (Status != STATUS_PENDING)
    {
        Log(("VBOXSF: VBoxMRxCreateVNetRoot: Returning 0x%lx\n", Status));
        pCreateNetRootContext->VirtualNetRootStatus = Status;
        if (fInitializeNetRoot)
            pCreateNetRootContext->NetRootStatus = Status;
        else pCreateNetRootContext->NetRootStatus = Status;

        // Callback the RDBSS for resumption.
        pCreateNetRootContext->Callback(pCreateNetRootContext);

        // Map the error code to STATUS_PENDING since this triggers
        // the synchronization mechanism in the RDBSS.
        Status = STATUS_PENDING;
    }

    Log(("VBOXSF: VBoxMRxCreateVNetRoot: Returned STATUS_PENDING\n"));
    return Status;
}

NTSTATUS VBoxMRxFinalizeVNetRoot (IN PMRX_V_NET_ROOT pVNetRoot, IN PBOOLEAN ForceDisconnect)
/*++

 Routine Description:


 Arguments:

 pVNetRoot - the virtual net root

 ForceDisconnect - disconnect is forced

 Return Value:

 NTSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    PMRX_NET_ROOT pNetRoot = pVNetRoot->pNetRoot;
    VBoxMRxGetNetRootExtension(pVNetRoot->pNetRoot, pNetRootExtension);
    int vboxRC;

    Log(("VBOXSF: VBoxMRxFinalizeVNetRoot: Address = 0x%lx\n", pVNetRoot));

    if (pNetRootExtension->phgcmClient)
    {
        vboxRC = vboxCallUnmapFolder(pNetRootExtension->phgcmClient, &pNetRootExtension->map);
        if (vboxRC != VINF_SUCCESS)
        {
            Log(("VBOXSF: VBoxMRxFinalizeVNetRoot: vboxCallMapFolder failed with %d\n", vboxRC));
        }
        pNetRootExtension->phgcmClient = NULL;
    }

    //
    //  This is called when all outstanding handles on this
    //  root have been cleaned up ! We can now zap the netroot
    //  extension...
    //

    return Status;
}

NTSTATUS VBoxMRxFinalizeNetRoot (IN PMRX_NET_ROOT pNetRoot, IN PBOOLEAN ForceDisconnect)
/*++

 Routine Description:


 Arguments:

 pVirtualNetRoot - the virtual net root

 ForceDisconnect - disconnect is forced

 Return Value:

 NTSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;

    Log(("VBOXSF: VBoxMRxFinalizeNetRoot: Called.\n"));
    //
    //  This is called when all outstanding handles on this
    //  root have been cleaned up ! We can now zap the netroot
    //  extension...
    //
    return (Status);
}

VOID VBoxMRxExtractNetRootName (IN PUNICODE_STRING FilePathName, IN PMRX_SRV_CALL SrvCall, OUT PUNICODE_STRING NetRootName, OUT PUNICODE_STRING RestOfName OPTIONAL
    )
/*++

 Routine Description:

 This routine parses the input name into srv, netroot, and the
 rest.

 Arguments:


 --*/
{
    UNICODE_STRING xRestOfName;

    ULONG length = FilePathName->Length;
    PWCH w = FilePathName->Buffer;
    PWCH wlimit = (PWCH)(((PCHAR)w) + length);
    PWCH wlow;

    Log(("VBOXSF: VBoxMRxExtractNetRootName: Called.\n"));

    w += (SrvCall->pSrvCallName->Length / sizeof(WCHAR));
    NetRootName->Buffer = wlow = w;
    for (;;)
    {
        if (w >= wlimit)
            break;
        if ((*w == OBJ_NAME_PATH_SEPARATOR) && (w != wlow))
        {
            break;
        }
        w++;
    }

    NetRootName->Length = NetRootName->MaximumLength = (USHORT)((PCHAR)w - (PCHAR)wlow);

    //w = FilePathName->Buffer;
    //NetRootName->Buffer = w++;

    if (!RestOfName)
        RestOfName = &xRestOfName;
    RestOfName->Buffer = w;
    RestOfName->Length = (USHORT)RestOfName->MaximumLength = (USHORT)((PCHAR)wlimit - (PCHAR)w);

    Log(("VBOXSF: VBoxMRxExtractNetRootName: FilePath = %.*ls\n", FilePathName->Length / sizeof(WCHAR), FilePathName->Buffer));
    Log(("VBOXSF: VBoxMRxExtractNetRootName: Srv = %.*ls, Root = %.*ls, Rest = %.*ls\n", SrvCall->pSrvCallName->Length / sizeof(WCHAR), SrvCall->pSrvCallName->Buffer, NetRootName->Length
            / sizeof(WCHAR), NetRootName->Buffer, RestOfName->Length / sizeof(WCHAR), RestOfName->Buffer));

    return;
}

