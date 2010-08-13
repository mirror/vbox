/*++

 Copyright (c) 1989 - 1999 Microsoft Corporation

 Module Name:

 read.c

 Abstract:

 This module implements the mini redirector call down routines pertaining to read
 of file system objects.

 --*/

#include "precomp.h"
#include <VBox/log.h>
#include "rdbss_vbox.h"

#pragma hdrstop

static NTSTATUS VBoxMRxReadInternal (IN PRX_CONTEXT RxContext)
/*++

 Routine Description:

 This routine handles network read requests.

 Arguments:

 RxContext - the RDBSS context

 Return Value:

 NTSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    RxCaptureFcb;
    RxCaptureFobx;

    PLOWIO_CONTEXT LowIoContext = &RxContext->LowIoContext;
    PMDL OriginalDataMdl = LowIoContext->ParamsFor.ReadWrite.Buffer;

    PVOID pbUserBuffer = VBoxRxLowIoGetBufferAddress(RxContext);
    uint32_t ByteCount = LowIoContext->ParamsFor.ReadWrite.ByteCount;
    RXVBO ByteOffset = LowIoContext->ParamsFor.ReadWrite.ByteOffset;
    LONGLONG FileSize = 0;
    PMRX_NET_ROOT pNetRoot = capFcb->pNetRoot;
    PMRX_VBOX_NETROOT_EXTENSION pNetRootExtension = (PMRX_VBOX_NETROOT_EXTENSION)pNetRoot->Context;
    BOOLEAN SynchronousIo = !BooleanFlagOn(RxContext->Flags, RX_CONTEXT_FLAG_ASYNC_OPERATION);
#ifndef VBOX
    PDEVICE_OBJECT deviceObject;
#endif

    PMRX_SRV_OPEN SrvOpen = capFobx->pSrvOpen;
    VBoxMRxGetDeviceExtension(RxContext, pDeviceExtension);
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(capFobx);

    Log(("VBOXSF: VBoxMRxRead: SynchronousIo = %d\n", SynchronousIo));
    Log(("VBOXSF: VBoxMRxRead: NetRoot is 0x%x, Fcb is 0x%x\n", pNetRoot, capFcb));

    VBoxRxGetFileSizeWithLock((PFCB)capFcb, &FileSize);

    //
    //  NB: This should be done by the wrapper ! It does this
    //  only if READCACHING is enabled on the FCB !!
    //
#if (NTDDI_VERSION >= NTDDI_VISTA)      /* Correct spelling for Vista 6001 SDK. */
    if (!FlagOn(capFcb->FcbState, FCB_STATE_READCACHING_ENABLED))
#else
    if (!FlagOn(capFcb->FcbState,FCB_STATE_READCACHEING_ENABLED))
#endif
    {
        //
        // If the read starts beyond End of File, return EOF.
        //

        if (ByteOffset >= FileSize)
        {
            Log(("VBOXSF: VBoxMRxRead: End of File\n", 0 ));
            Status = STATUS_END_OF_FILE;
            goto Exit;
        }

        //
        //  If the read extends beyond EOF, truncate the read
        //

        if (ByteCount > FileSize - ByteOffset)
        {
            ByteCount = (ULONG)(FileSize - ByteOffset);
        }
    }

    Log(("VBOXSF: VBoxMRxRead: UserBuffer is 0x%x\n", pbUserBuffer));
    Log(("VBOXSF: VBoxMRxRead: ByteCount is %d, ByteOffset is %x\n", ByteCount, ByteOffset));

    Assert((OriginalDataMdl != NULL && (RxMdlIsLocked(OriginalDataMdl) || RxMdlSourceIsNonPaged(OriginalDataMdl))) || (OriginalDataMdl == NULL && LowIoContext->ParamsFor.ReadWrite.ByteCount == 0));

    if (OriginalDataMdl == NULL || LowIoContext->ParamsFor.ReadWrite.ByteCount == 0)
    {
        AssertFailed();
        return STATUS_INVALID_PARAMETER;
    }

    /* @todo async io !!!! */
    SynchronousIo = TRUE;

    if (SynchronousIo)
    {
        int vboxRC;

        /* Do the actual reading */
#ifdef VBOX_FAKE_IO_READ_WRITE
        Status = STATUS_SUCCESS;
        memset(pbUserBuffer, 0, ByteCount);
#else
        vboxRC = vboxCallRead(&pDeviceExtension->hgcmClient, &pNetRootExtension->map, pVBoxFobx->hFile, ByteOffset, &ByteCount, (uint8_t *)pbUserBuffer, true /* locked */);
        AssertRC(vboxRC);

        Status = VBoxErrorToNTStatus(vboxRC);
#endif
        if (Status != STATUS_SUCCESS)
            ByteCount = 0; /* Nothing read */

        Log(("VBOXSF: VBoxMRxRead: This I/O is sync!\n"));

        RxContext->InformationToReturn = ByteCount;

        /* Never call RxLowIoCompletion here!! */
    }
    else
    {
        Log(("VBOXSF: VBoxMRxRead: This I/O is async!\n"));
    }

    Log(("VBOXSF: VBoxMRxRead: Status = %x Info = %x\n",RxContext->IoStatusBlock.Status,RxContext->IoStatusBlock.Information));

    Exit:

    return (Status);
}


static VOID VBoxMRxReadWorker (VOID *pv)
{
    PRX_CONTEXT RxContext = (PRX_CONTEXT)pv;

    Log(("VBOXSF: VBoxMRxReadWorker: calling the worker\n"));

    RxContext->IoStatusBlock.Status = VBoxMRxReadInternal (RxContext);

    Log(("VBOXSF: VBoxMRxReadWorker: Status %x\n", RxContext->IoStatusBlock.Status));

    RxLowIoCompletion (RxContext);
}


#ifdef VBOX_ASYNC_RW
NTSTATUS
VBoxMRxRead (
      IN PRX_CONTEXT RxContext)
{
    NTSTATUS Status = STATUS_SUCCESS;

    Status = RxDispatchToWorkerThread(VBoxMRxDeviceObject, DelayedWorkQueue,
                                      VBoxMRxReadWorker,
                                      RxContext);

    Log(("VBOXSF: VBoxMRxRead: RxDispatchToWorkerThread: Status %x\n", Status));

    if (Status == STATUS_SUCCESS)
    {
        Status = STATUS_PENDING;
    }

    return Status;
}
#else
NTSTATUS
VBoxMRxRead (
      IN PRX_CONTEXT RxContext)
{
    RxContext->IoStatusBlock.Status = VBoxMRxReadInternal (RxContext);
    return RxContext->IoStatusBlock.Status;
}
#endif /* !VBOX_ASYNC_RW */
