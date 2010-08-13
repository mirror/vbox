/*++

 Copyright (c) 1989 - 1999 Microsoft Corporation

 Module Name:

 locks.c

 Abstract:

 This module implements the mini redirector call down routines pertaining to locks
 of file system objects.

 --*/

#include "precomp.h"
#pragma hdrstop

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, VBoxMRxLocks)
#pragma alloc_text(PAGE, VBoxMRxCompleteBufferingStateChangeRequest)
#pragma alloc_text(PAGE, VBoxMRxFlush)
#endif

NTSTATUS VBoxMRxLocks (IN PRX_CONTEXT RxContext)
/*++

 Routine Description:

 This routine handles network requests for filelocks

 Arguments:

 RxContext - the RDBSS context

 Return Value:

 RXSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    PLOWIO_CONTEXT LowIoContext = &RxContext->LowIoContext;
    RxCaptureFcb;
    RxCaptureFobx;
    PMRX_NET_ROOT pNetRoot = capFcb->pNetRoot;
#ifndef VBOX
    PDEVICE_OBJECT deviceObject;
#endif

    PMRX_SRV_OPEN SrvOpen = capFobx->pSrvOpen;
    VBoxMRxGetDeviceExtension(RxContext, pDeviceExtension);
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(capFobx);
    PMRX_VBOX_NETROOT_EXTENSION pNetRootExtension = (PMRX_VBOX_NETROOT_EXTENSION)pNetRoot->Context;
    uint32_t fLock = 0;

    int vboxRC;

    Log(("VBOXSF: VBoxMRxLocks: Called.\n"));

    switch (LowIoContext->Operation)
    {
    default:
        AssertMsgFailed(("VBOXSF: VBoxMRxLocks: Unsupported lock/unlock type %d detected!\n", LowIoContext->Operation));
        return STATUS_NOT_IMPLEMENTED;

    case LOWIO_OP_UNLOCK_MULTIPLE:
        /* @todo Remove multiple locks listed in LowIoContext.ParamsFor.Locks.LockList. */
        Log(("VBOXSF: VBoxMRxLocks: Unsupported lock/unlock type %d detected!\n", LowIoContext->Operation));
        return STATUS_NOT_IMPLEMENTED;

        /* Reading is allowed, writing isn't */
    case LOWIO_OP_SHAREDLOCK:
        fLock = SHFL_LOCK_SHARED | SHFL_LOCK_PARTIAL;
        break;

        /* Reading and writing not allowed */
    case LOWIO_OP_EXCLUSIVELOCK:
        fLock = SHFL_LOCK_EXCLUSIVE | SHFL_LOCK_PARTIAL;
        break;

    case LOWIO_OP_UNLOCK:
        fLock = SHFL_LOCK_CANCEL | SHFL_LOCK_PARTIAL;
        break;
    }

    if (LowIoContext->ParamsFor.Locks.Flags & LOWIO_LOCKSFLAG_FAIL_IMMEDIATELY)
    {
        fLock |= SHFL_LOCK_NOWAIT;
    }
    else
    {
        fLock |= SHFL_LOCK_WAIT;
    }

    /* Do the actual flushing of file buffers */
    vboxRC = vboxCallLock(&pDeviceExtension->hgcmClient, &pNetRootExtension->map, pVBoxFobx->hFile, LowIoContext->ParamsFor.Locks.ByteOffset, LowIoContext->ParamsFor.Locks.Length, fLock);
    AssertRC(vboxRC);
    Status = VBoxErrorToNTStatus(vboxRC);

    Log(("VBOXSF: VBoxMRxLocks: Returned 0x%x\n", Status));
    return (Status);
}

NTSTATUS VBoxMRxCompleteBufferingStateChangeRequest (IN OUT PRX_CONTEXT RxContext, IN OUT PMRX_SRV_OPEN SrvOpen, IN PVOID pContext)
/*++

 Routine Description:

 This routine is called to assert the locks that the wrapper has buffered. currently, it is synchronous!


 Arguments:

 RxContext - the open instance
 SrvOpen   - tells which fcb is to be used.
 LockEnumerator - the routine to call to get the locks

 Return Value:

 RXSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_NOT_IMPLEMENTED;

    Log(("VBOXSF: VBoxMRxCompleteBufferingStateChangeRequest: Called.\n"));
    return (Status);
}

NTSTATUS VBoxMRxFlush (IN PRX_CONTEXT RxContext)
/*++

 Routine Description:

 This routine handles network requests for file flush

 Arguments:

 RxContext - the RDBSS context

 Return Value:

 RXSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    RxCaptureFcb;
    RxCaptureFobx;
    PMRX_NET_ROOT pNetRoot = capFcb->pNetRoot;
#ifndef VBOX
    PDEVICE_OBJECT deviceObject;
#endif

    PMRX_SRV_OPEN SrvOpen = capFobx->pSrvOpen;
    VBoxMRxGetDeviceExtension(RxContext, pDeviceExtension);
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(capFobx);
    PMRX_VBOX_NETROOT_EXTENSION pNetRootExtension = (PMRX_VBOX_NETROOT_EXTENSION)pNetRoot->Context;

    int vboxRC;

    Log(("VBOXSF: VBoxMRxFlush: Called.\n"));

    /* Do the actual flushing of file buffers */
    vboxRC = vboxCallFlush(&pDeviceExtension->hgcmClient, &pNetRootExtension->map, pVBoxFobx->hFile);
    AssertRC(vboxRC);

    Status = VBoxErrorToNTStatus(vboxRC);

    return (Status);
}

