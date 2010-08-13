/*++

Copyright (c) 1989 - 1999 Microsoft Corporation

Module Name:

    write.c

Abstract:

    This module implements the mini redirector call down routines pertaining
    to write of file system objects.

--*/

#include "precomp.h"
#include "rdbss_vbox.h"
#pragma hdrstop


static NTSTATUS
VBoxMRxWriteInternal (
      IN PRX_CONTEXT RxContext)

/*++

Routine Description:

   This routine opens a file across the network.

Arguments:

    RxContext - the RDBSS context

Return Value:

    RXSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    RxCaptureFcb;
    RxCaptureFobx;

    PLOWIO_CONTEXT  LowIoContext  = &RxContext->LowIoContext;
    PMDL            OriginalDataMdl = LowIoContext->ParamsFor.ReadWrite.Buffer;

    PVOID           pbUserBuffer = VBoxRxLowIoGetBufferAddress(RxContext);
    uint32_t        ByteCount = (LowIoContext->ParamsFor).ReadWrite.ByteCount;
    RXVBO           ByteOffset = (LowIoContext->ParamsFor).ReadWrite.ByteOffset;
    LONGLONG        FileSize = 0;
    PMRX_NET_ROOT   pNetRoot = capFcb->pNetRoot;
    VBoxMRxGetNetRootExtension(pNetRoot,pNetRootExtension);
    BOOLEAN         SynchronousIo = !BooleanFlagOn(RxContext->Flags,RX_CONTEXT_FLAG_ASYNC_OPERATION);
#ifndef VBOX
    PDEVICE_OBJECT  deviceObject;
#endif
    VBoxMRxGetDeviceExtension(RxContext, pDeviceExtension);
    PMRX_VBOX_FOBX  pVBoxFobx = VBoxMRxGetFileObjectExtension(capFobx);

    Log(("VBOXSF: VBoxMRxWrite: SynchronousIo = %d\n", SynchronousIo));
    Log(("VBOXSF: VBoxMRxWrite: NetRoot is 0x%x Fcb is 0x%x\n", pNetRoot, capFcb));

    Assert(  ( OriginalDataMdl != NULL
               &&
               (    RxMdlIsLocked(OriginalDataMdl)
                 || RxMdlSourceIsNonPaged(OriginalDataMdl)
               )
             )
           ||
             (
                OriginalDataMdl == NULL
                &&
                LowIoContext->ParamsFor.ReadWrite.ByteCount == 0
             )
          );

    if (    OriginalDataMdl == NULL
        ||  LowIoContext->ParamsFor.ReadWrite.ByteCount == 0)
    {
        AssertFailed();
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Lengths that are not sector aligned will be rounded up to
    //  the next sector boundary. The rounded up length should be
    //  < AllocationSize.
    //
    VBoxRxGetFileSizeWithLock((PFCB)capFcb,&FileSize);

    Log(("VBOXSF: VBoxMRxWrite: UserBuffer is 0x%x\n", pbUserBuffer ));
    Log(("VBOXSF: VBoxMRxWrite: ByteCount is %d, ByteOffset is %d\n", ByteCount, ByteOffset ));

    /* @todo async io !!!! */
    SynchronousIo = TRUE;

    if( SynchronousIo )
    {
        int vboxRC;
        PMDL pMdlBuf = NULL;

#ifdef VBOX_FAKE_IO_READ_WRITE
        Status =  STATUS_SUCCESS;
#else
        vboxRC = vboxCallWrite(&pDeviceExtension->hgcmClient, &pNetRootExtension->map, pVBoxFobx->hFile, ByteOffset, &ByteCount, (uint8_t *)pbUserBuffer, true /* locked */);
        AssertRC(vboxRC);

        Status = VBoxErrorToNTStatus(vboxRC);
#endif
        if (Status != STATUS_SUCCESS)
            ByteCount = 0;  /* Nothing written */

        Log(("VBOXSF: VBoxMRxWrite: This I/O is sync!\n"));

        RxContext->InformationToReturn = ByteCount;

        /* Never call RxLowIoCompletion here!! */
    }
    else {
        Log(("VBOXSF: VBoxMRxWrite: This I/O is async!\n"));
    }

    Log(("VBOXSF: VBoxMRxWrite: Status = %x, Info = %x\n",RxContext->IoStatusBlock.Status,RxContext->IoStatusBlock.Information));

#ifndef VBOX
Exit:
#endif
    return(Status);
}

static VOID VBoxMRxWriteWorker (VOID *pv)
{
    PRX_CONTEXT RxContext = (PRX_CONTEXT)pv;

    Log(("VBOXSF: VBoxMRxWriteWorker: calling the worker\n"));

    RxContext->IoStatusBlock.Status = VBoxMRxWriteInternal (RxContext);

    Log(("VBOXSF: VBoxMRxWriteWorker: Status %x\n", RxContext->IoStatusBlock.Status));

    RxLowIoCompletion (RxContext);
}


#ifdef VBOX_ASYNC_RW
NTSTATUS
VBoxMRxWrite (
      IN PRX_CONTEXT RxContext)
{
    NTSTATUS Status = STATUS_SUCCESS;

    Status = RxDispatchToWorkerThread(VBoxMRxDeviceObject, DelayedWorkQueue,
                                      VBoxMRxWriteWorker,
                                      RxContext);

    Log(("VBOXSF: VBoxMRxWrite: RxDispatchToWorkerThread: Status %x\n", Status));

    if (Status == STATUS_SUCCESS)
    {
        Status = STATUS_PENDING;
    }

    return Status;
}
#else
NTSTATUS
VBoxMRxWrite (
      IN PRX_CONTEXT RxContext)
{
    RxContext->IoStatusBlock.Status = VBoxMRxWriteInternal (RxContext);
    return RxContext->IoStatusBlock.Status;
}
#endif /* !VBOX_ASYNC_RW */
