/*

 Copyright (c) 1989 - 1999 Microsoft Corporation

 Module Name:

 srvcall.c

 Abstract:

 This module implements the routines for handling the creation/manipulation of
 server entries in the connection engine database. It also contains the routines
 for parsing the negotiate response from  the server.

 --*/

#include "precomp.h"
#pragma hdrstop

VOID ExecuteCreateSrvCall (PMRX_SRVCALL_CALLBACK_CONTEXT pCallbackContext)
/*++

 Routine Description:

 This routine patches the RDBSS created srv call instance with the
 information required by the mini redirector.

 Arguments:

 CallBackContext  - the call back context in RDBSS for continuation.

 Return Value:

 RXSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status;
    PWCHAR pSrvName = 0;
    BOOLEAN Verifier;

    PMRX_SRVCALL_CALLBACK_CONTEXT SCCBC = pCallbackContext;
    PMRX_SRVCALLDOWN_STRUCTURE SrvCalldownStructure = (PMRX_SRVCALLDOWN_STRUCTURE)(SCCBC->SrvCalldownStructure);
    PMRX_SRV_CALL pSrvCall = SrvCalldownStructure->SrvCall;

    Assert(pSrvCall);
    Assert(NodeType(pSrvCall) == RDBSS_NTC_SRVCALL);

    /* Validate the server name with the test name of 'vboxsvr'. */
    Log(("VBOXSF: ExecuteCreateSrvCall: Connection Name %.*ls Length: %d, pSrvCall = %p\n", pSrvCall->pSrvCallName->Length / sizeof(WCHAR), pSrvCall->pSrvCallName->Buffer, pSrvCall->pSrvCallName->Length, pSrvCall));

    if (pSrvCall->pPrincipalName && pSrvCall->pPrincipalName->Length)
        Log(("VBOXSF: ExecuteCreateSrvCall: Principal name = %.*ls\n", pSrvCall->pPrincipalName->Length / sizeof(WCHAR), pSrvCall->pPrincipalName->Buffer));

    if (pSrvCall->pDomainName && pSrvCall->pDomainName->Length)
        Log(("VBOXSF: ExecuteCreateSrvCall: Domain name = %.*ls\n", pSrvCall->pDomainName->Length / sizeof(WCHAR), pSrvCall->pDomainName->Buffer));

    if (pSrvCall->pSrvCallName->Length >= 14)
    {
        pSrvName = pSrvCall->pSrvCallName->Buffer;

        Verifier = (pSrvName[0] == L'\\');
        Verifier &= (pSrvName[1] == L'V') || (pSrvName[1] == L'v');
        Verifier &= (pSrvName[2] == L'B') || (pSrvName[2] == L'b');
        Verifier &= (pSrvName[3] == L'O') || (pSrvName[3] == L'o');
        Verifier &= (pSrvName[4] == L'X') || (pSrvName[4] == L'x');
        Verifier &= (pSrvName[5] == L'S') || (pSrvName[5] == L's');
        /* Both vboxsvr & vboxsrv are now accepted */
        if ((pSrvName[6] == L'V') || (pSrvName[6] == L'v'))
        {
            Verifier &= (pSrvName[6] == L'V') || (pSrvName[6] == L'v');
            Verifier &= (pSrvName[7] == L'R') || (pSrvName[7] == L'r');
        }
        else
        {
            Verifier &= (pSrvName[6] == L'R') || (pSrvName[6] == L'r');
            Verifier &= (pSrvName[7] == L'V') || (pSrvName[7] == L'v');
        }
        Verifier &= (pSrvName[8] == L'\\') || (pSrvName[8] == 0);
    }
    else
    {
        Verifier = FALSE;
    }

    if (Verifier)
    {
        Log(("VBOXSF: ExecuteCreateSrvCall: Verifier succeeded!\n"));
        Status = STATUS_SUCCESS;
    }
    else
    {
        Log(("VBOXSF: ExecuteCreateSrvCall: Verifier failed!\n"));
        Status = STATUS_BAD_NETWORK_PATH;
    }

    SCCBC->Status = Status;
    SrvCalldownStructure->CallBack(SCCBC);
}

NTSTATUS VBoxMRxCreateSrvCall (PMRX_SRV_CALL pSrvCall, PMRX_SRVCALL_CALLBACK_CONTEXT pCallbackContext)
/*++

 Routine Description:

 This routine patches the RDBSS created srv call instance with the information required
 by the mini redirector.

 Arguments:

 RxContext        - Supplies the context of the original create/ioctl

 CallBackContext  - the call back context in RDBSS for continuation.

 Return Value:

 RXSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status;

    PMRX_SRVCALLDOWN_STRUCTURE SrvCalldownStructure = (PMRX_SRVCALLDOWN_STRUCTURE)(pCallbackContext->SrvCalldownStructure);

    Assert(pSrvCall);
    Assert(NodeType(pSrvCall) == RDBSS_NTC_SRVCALL);

    Log(("VBOXSF: VBoxMRxCreateSrvCall: Called %p.\n", pSrvCall));

    //
    // If this request was made on behalf of the RDBSS, do ExecuteCreatSrvCall
    // immediately. If the request was made from somewhere else, create a work item
    // and place it on a queue for a worker thread to process later. This distinction
    // is made to simplify transport handle management.
    //
    if (IoGetCurrentProcess() == RxGetRDBSSProcess())
    {
        Log(("VBOXSF: VBoxMRxCreateSrvCall: Called in context of RDBSS process\n"));

        //
        // Peform the processing immediately because RDBSS is the initiator of this
        // request
        //

        ExecuteCreateSrvCall(pCallbackContext);
    }
    else
    {
        Log(("VBOXSF: VBoxMRxCreateSrvCall: Dispatching to worker thread\n"));

        //
        // Dispatch the request to a worker thread because the redirected drive
        // buffering sub-system (RDBSS) was not the initiator
        //

        Status = RxDispatchToWorkerThread(VBoxMRxDeviceObject, DelayedWorkQueue, (PWORKER_THREAD_ROUTINE)ExecuteCreateSrvCall, /* explicit cast for first parameter type */
        pCallbackContext);

        if (Status == STATUS_SUCCESS)
        {
            Log(("VBOXSF: VBoxMRxCreateSrvCall: queued!\n"));
        }
        else
        {
            pCallbackContext->Status = Status;
            SrvCalldownStructure->CallBack(pCallbackContext);
        }
    }

    //
    // The wrapper expects PENDING.
    //
    return STATUS_PENDING;
}

NTSTATUS VBoxMRxFinalizeSrvCall (PMRX_SRV_CALL pSrvCall, BOOLEAN Force)
/*++

 Routine Description:

 This routine destroys a given server call instance

 Arguments:

 pSrvCall  - the server call instance to be disconnected.

 Force     - TRUE if a disconnection is to be enforced immediately.

 Return Value:

 RXSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;

    Log(("VBOXSF: VBoxMRxFinalizeSrvCall called %p, ctx = %p.\n", pSrvCall, pSrvCall->Context));
    pSrvCall->Context = NULL;

    return (Status);
}

NTSTATUS VBoxMRxSrvCallWinnerNotify (IN PMRX_SRV_CALL pSrvCall, IN BOOLEAN ThisMinirdrIsTheWinner, IN OUT PVOID pSrvCallContext)
/*++

 Routine Description:

 This routine finalizes the mini rdr context associated with an RDBSS Server call instance

 Arguments:

 pSrvCall               - the Server Call

 ThisMinirdrIsTheWinner - TRUE if this mini rdr is the chosen one.

 pSrvCallContext  - the server call context created by the mini redirector.

 Return Value:

 RXSTATUS - The return status for the operation

 Notes:

 The two phase construction protocol for Server calls is required because of parallel
 initiation of a number of mini redirectors. The RDBSS finalizes the particular mini
 redirector to be used in communicating with a given server based on quality of
 service criterion.

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;

    Log(("VBOXSF: VBoxMRxSrvCallWinnerNotify called %p.\n", pSrvCall));
    if (ThisMinirdrIsTheWinner)
    {
        Log(("VBOXSF: VBoxMRxSrvCallWinnerNotify: VboxSF.sys is the winner!\n"));
    }

    pSrvCall->Context = (PVOID)0xDEADBEEF;

    return (Status);
}

