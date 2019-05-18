/* $Id$ */
/** @file
 * VirtualBox Windows Guest Shared Folders - File System Driver file routines.
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
#include <iprt/fs.h>
#include <iprt/mem.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** How many pages we should try transfer in one I/O request (read/write). */
#define VBSF_MAX_IO_PAGES   RT_MIN(_16K / sizeof(RTGCPHYS64) /* => 8MB buffer */, VMMDEV_MAX_HGCM_DATA_SIZE >> PAGE_SHIFT)

/* How much data to transfer in one HGCM request. */
#define VBSF_MAX_READ_WRITE_PAGES 256


typedef int FNVBSFTRANSFERBUFFER(PVBGLSFCLIENT pClient, PVBGLSFMAP pMap, SHFLHANDLE hFile,
                                 uint64_t offset, uint32_t *pcbBuffer,
                                 uint8_t *pBuffer, bool fLocked);
typedef FNVBSFTRANSFERBUFFER *PFNVBSFTRANSFERBUFFER;

typedef int FNVBSFTRANSFERPAGES(PVBGLSFCLIENT pClient, PVBGLSFMAP pMap, SHFLHANDLE hFile,
                                uint64_t offset, uint32_t *pcbBuffer,
                                uint16_t offFirstPage, uint16_t cPages, RTGCPHYS64 *paPages);
typedef FNVBSFTRANSFERPAGES *PFNVBSFTRANSFERPAGES;


static int vbsfTransferBufferWrite(PVBGLSFCLIENT pClient, PVBGLSFMAP pMap, SHFLHANDLE hFile,
                                   uint64_t offset, uint32_t *pcbBuffer,
                                   uint8_t *pBuffer, bool fLocked)
{
    return VbglR0SfWrite(pClient, pMap, hFile, offset, pcbBuffer, pBuffer, fLocked);
}

static int vbsfTransferPagesWrite(PVBGLSFCLIENT pClient, PVBGLSFMAP pMap, SHFLHANDLE hFile,
                                  uint64_t offset, uint32_t *pcbBuffer,
                                  uint16_t offFirstPage, uint16_t cPages, RTGCPHYS64 *paPages)
{
    return VbglR0SfWritePageList(pClient, pMap, hFile, offset, pcbBuffer, offFirstPage, cPages, paPages);
}


typedef struct VBSFTRANSFERCTX
{
    PVBGLSFCLIENT pClient;
    PVBGLSFMAP pMap;
    SHFLHANDLE hFile;
    uint64_t offset;
    uint32_t cbData;

    PMDL pMdl;
    uint8_t *pBuffer;
    bool fLocked;

    PFNVBSFTRANSFERBUFFER pfnTransferBuffer;
    PFNVBSFTRANSFERPAGES pfnTransferPages;
} VBSFTRANSFERCTX;


static int vbsfTransferCommon(VBSFTRANSFERCTX *pCtx)
{
    int rc = VINF_SUCCESS;
    BOOLEAN fProcessed = FALSE;

    uint32_t cbTransferred = 0;

    uint32_t cbToTransfer;
    uint32_t cbIO;

    /** @todo Remove the test and the fall-back path.  VbglR0CanUsePhysPageList()
     *        returns true for any host version after 3.0, i.e. further back than
     *        we support. */
    if (VbglR0CanUsePhysPageList())
    {
        ULONG offFirstPage = MmGetMdlByteOffset(pCtx->pMdl);
        ULONG cPages = ADDRESS_AND_SIZE_TO_SPAN_PAGES(MmGetMdlVirtualAddress(pCtx->pMdl), pCtx->cbData);
        ULONG cPagesToTransfer = RT_MIN(cPages, VBSF_MAX_READ_WRITE_PAGES);
        RTGCPHYS64 *paPages = (RTGCPHYS64 *)RTMemTmpAlloc(cPagesToTransfer * sizeof(RTGCPHYS64));

        Log(("VBOXSF: vbsfTransferCommon: using page list: %d pages, offset 0x%03X\n", cPages, offFirstPage));

        if (paPages)
        {
            PPFN_NUMBER paPfns = MmGetMdlPfnArray(pCtx->pMdl);
            ULONG cPagesTransferred = 0;
            cbTransferred = 0;

            while (cPagesToTransfer != 0)
            {
                ULONG iPage;
                cbToTransfer = cPagesToTransfer * PAGE_SIZE - offFirstPage;

                if (cbToTransfer > pCtx->cbData - cbTransferred)
                    cbToTransfer = pCtx->cbData - cbTransferred;

                if (cbToTransfer == 0)
                {
                    /* Nothing to transfer. */
                    break;
                }

                cbIO = cbToTransfer;

                Log(("VBOXSF: vbsfTransferCommon: transferring %d pages at %d; %d bytes at %d\n",
                     cPagesToTransfer, cPagesTransferred, cbToTransfer, cbTransferred));

                for (iPage = 0; iPage < cPagesToTransfer; iPage++)
                    paPages[iPage] = (RTGCPHYS64)paPfns[iPage + cPagesTransferred] << PAGE_SHIFT;

                rc = pCtx->pfnTransferPages(pCtx->pClient, pCtx->pMap, pCtx->hFile,
                                            pCtx->offset + cbTransferred, &cbIO,
                                            (uint16_t)offFirstPage, (uint16_t)cPagesToTransfer, paPages);
                if (RT_FAILURE(rc))
                {
                    Log(("VBOXSF: vbsfTransferCommon: pfnTransferPages %Rrc, cbTransferred %d\n", rc, cbTransferred));

                    /* If some data was transferred, then it is no error. */
                    if (cbTransferred > 0)
                        rc = VINF_SUCCESS;

                    break;
                }

                cbTransferred += cbIO;

                if (cbToTransfer < cbIO)
                {
                    /* Transferred less than requested, do not continue with the possibly remaining data. */
                    break;
                }

                cPagesTransferred += cPagesToTransfer;
                offFirstPage = 0;

                cPagesToTransfer = cPages - cPagesTransferred;
                if (cPagesToTransfer > VBSF_MAX_READ_WRITE_PAGES)
                    cPagesToTransfer = VBSF_MAX_READ_WRITE_PAGES;
            }

            RTMemTmpFree(paPages);

            fProcessed = TRUE;
        }
    }

    if (fProcessed != TRUE)
    {
        /* Split large transfers. */
        cbTransferred = 0;
        cbToTransfer = RT_MIN(pCtx->cbData, VBSF_MAX_READ_WRITE_PAGES * PAGE_SIZE);

        /* Page list not supported or a fallback. */
        Log(("VBOXSF: vbsfTransferCommon: using linear address\n"));

        while (cbToTransfer != 0)
        {
            cbIO = cbToTransfer;

            Log(("VBOXSF: vbsfTransferCommon: transferring %d bytes at %d\n",
                 cbToTransfer, cbTransferred));

            rc = pCtx->pfnTransferBuffer(pCtx->pClient, pCtx->pMap, pCtx->hFile,
                                         pCtx->offset + cbTransferred, &cbIO,
                                         pCtx->pBuffer + cbTransferred, true /* locked */);

            if (RT_FAILURE(rc))
            {
                Log(("VBOXSF: vbsfTransferCommon: pfnTransferBuffer %Rrc, cbTransferred %d\n", rc, cbTransferred));

                /* If some data was transferred, then it is no error. */
                if (cbTransferred > 0)
                    rc = VINF_SUCCESS;

                break;
            }

            cbTransferred += cbIO;

            if (cbToTransfer < cbIO)
            {
                /* Transferred less than requested, do not continue with the possibly remaining data. */
                break;
            }

            cbToTransfer = pCtx->cbData - cbTransferred;
            if (cbToTransfer > VBSF_MAX_READ_WRITE_PAGES * PAGE_SIZE)
                cbToTransfer = VBSF_MAX_READ_WRITE_PAGES * PAGE_SIZE;
        }
    }

    pCtx->cbData = cbTransferred;

    return rc;
}

/**
 * Performs a read.
 */
static NTSTATUS vbsfNtReadWorker(PRX_CONTEXT RxContext)
{
    RxCaptureFcb;
    RxCaptureFobx;
    PMRX_VBOX_NETROOT_EXTENSION pNetRootX  = VBoxMRxGetNetRootExtension(capFcb->pNetRoot);
    PVBSFNTFCBEXT               pVBoxFcbX  = VBoxMRxGetFcbExtension(capFcb);
    PMRX_VBOX_FOBX              pVBoxFobX  = VBoxMRxGetFileObjectExtension(capFobx);
    PMDL                        pBufferMdl = RxContext->LowIoContext.ParamsFor.ReadWrite.Buffer;

    LogFlow(("vbsfNtReadWorker: hFile=%#RX64 offFile=%#RX64 cbToRead=%#x %s\n", pVBoxFobX->hFile,
             RxContext->LowIoContext.ParamsFor.ReadWrite.ByteOffset, RxContext->LowIoContext.ParamsFor.ReadWrite.ByteCount,
             RxContext->Flags, RX_CONTEXT_FLAG_ASYNC_OPERATION ? " async" : "sync"));

    AssertReturn(pBufferMdl,  STATUS_INTERNAL_ERROR);


    /*
     * We should never get a zero byte request (RDBSS checks), but in case we
     * do, it should succeed.
     */
    uint32_t cbRet  = 0;
    uint32_t cbLeft = RxContext->LowIoContext.ParamsFor.ReadWrite.ByteCount;
    AssertReturnStmt(cbLeft > 0, RxContext->InformationToReturn = 0, STATUS_SUCCESS);

    Assert(cbLeft <= MmGetMdlByteCount(pBufferMdl));

    /*
     * Allocate a request buffer.
     */
    uint32_t            cPagesLeft = ADDRESS_AND_SIZE_TO_SPAN_PAGES(MmGetMdlVirtualAddress(pBufferMdl), cbLeft);
    uint32_t            cMaxPages  = RT_MIN(cPagesLeft, VBSF_MAX_IO_PAGES);
    VBOXSFREADPGLSTREQ *pReq = (VBOXSFREADPGLSTREQ *)VbglR0PhysHeapAlloc(RT_UOFFSETOF_DYN(VBOXSFREADPGLSTREQ,
                                                                                          PgLst.aPages[cMaxPages]));
    while (!pReq && cMaxPages > 4)
    {
        cMaxPages /= 2;
        pReq = (VBOXSFREADPGLSTREQ *)VbglR0PhysHeapAlloc(RT_UOFFSETOF_DYN(VBOXSFREADPGLSTREQ, PgLst.aPages[cMaxPages]));
    }
    NTSTATUS rcNt = STATUS_SUCCESS;
    if (pReq)
    {
        /*
         * The read loop.
         */
        RTFOFF      offFile = RxContext->LowIoContext.ParamsFor.ReadWrite.ByteOffset;
        PPFN_NUMBER paPfns  = MmGetMdlPfnArray(pBufferMdl);
        uint32_t    offPage = MmGetMdlByteOffset(pBufferMdl);
        if (offPage < PAGE_SIZE)
        { /* likely */ }
        else
        {
            paPfns  += offPage >> PAGE_SHIFT;
            offPage &= PAGE_OFFSET_MASK;
        }

        for (;;)
        {
            /*
             * Figure out how much to process now and set up the page list for it.
             */
            uint32_t cPagesInChunk;
            uint32_t cbChunk;
            if (cPagesLeft <= cMaxPages)
            {
                cPagesInChunk = cPagesLeft;
                cbChunk       = cbLeft;
            }
            else
            {
                cPagesInChunk = cMaxPages;
                cbChunk       = (cMaxPages << PAGE_SHIFT) - offPage;
            }

            size_t iPage = cPagesInChunk;
            while (iPage-- > 0)
                pReq->PgLst.aPages[iPage] = (RTGCPHYS)paPfns[iPage] << PAGE_SHIFT;
            pReq->PgLst.offFirstPage = offPage;

            /*
             * Issue the request and unlock the pages.
             */
            int vrc = VbglR0SfHostReqReadPgLst(pNetRootX->map.root, pReq, pVBoxFobX->hFile, offFile, cbChunk, cPagesInChunk);
            if (RT_SUCCESS(vrc))
            {
                /*
                 * Success, advance position and buffer.
                 */
                uint32_t cbActual = pReq->Parms.cb32Read.u.value32;
                AssertStmt(cbActual <= cbChunk, cbActual = cbChunk);
                cbRet   += cbActual;
                offFile += cbActual;
                cbLeft  -= cbActual;

                /*
                 * Update timestamp state (FCB is shared).
                 */
                pVBoxFobX->fTimestampsImplicitlyUpdated |= VBOX_FOBX_F_INFO_LASTACCESS_TIME;
                if (pVBoxFcbX->pFobxLastAccessTime != pVBoxFobX)
                    pVBoxFcbX->pFobxLastAccessTime = NULL;

                /*
                 * Are we done already?
                 */
                if (!cbLeft || cbActual < cbChunk)
                {
                    /*
                     * Flag EOF.
                     */
                    if (cbActual != 0 || cbRet != 0)
                    { /* typical */ }
                    else
                        rcNt = STATUS_END_OF_FILE;

                    /*
                     * See if we've reached the EOF early or read beyond what we thought were the EOF.
                     *
                     * Note! We don't dare do this (yet) if we're in paging I/O as we then hold the
                     *       PagingIoResource in shared mode and would probably deadlock in the
                     *       updating code when taking the lock in exclusive mode.
                     */
                    if (RxContext->LowIoContext.Resource != capFcb->Header.PagingIoResource)
                    {
                        LONGLONG cbFileRdbss;
                        RxGetFileSizeWithLock((PFCB)capFcb, &cbFileRdbss);
                        if (   offFile < cbFileRdbss
                            && cbActual < cbChunk /* hit EOF */)
                            vbsfNtUpdateFcbSize(RxContext->pFobx->AssociatedFileObject, capFcb, pVBoxFobX, offFile, cbFileRdbss, -1);
                        else if (offFile > cbFileRdbss)
                            vbsfNtQueryAndUpdateFcbSize(pNetRootX, RxContext->pFobx->AssociatedFileObject,
                                                        pVBoxFobX, capFcb, pVBoxFcbX);
                    }
                    break;
                }

                /*
                 * More to read, advance page related variables and loop.
                 */
                paPfns     += cPagesInChunk;
                cPagesLeft -= cPagesInChunk;
                offPage     = 0;
            }
            else if (vrc == VERR_NO_MEMORY && cMaxPages > 4)
            {
                /*
                 * The host probably doesn't have enough heap to handle the
                 * request, reduce the page count and retry.
                 */
                cMaxPages /= 4;
                Assert(cMaxPages > 0);
            }
            else
            {
                /*
                 * If we've successfully read stuff, return it rather than
                 * the error.  (Not sure if this is such a great idea...)
                 */
                if (cbRet > 0)
                    Log(("vbsfNtReadWorker: read at %#RX64 -> %Rrc; got cbRet=%#zx already\n", offFile, vrc, cbRet));
                else
                {
                    rcNt = vbsfNtVBoxStatusToNt(vrc);
                    Log(("vbsfNtReadWorker: read at %#RX64 -> %Rrc (rcNt=%#x)\n", offFile, vrc, rcNt));
                }
                break;
            }

        }

        VbglR0PhysHeapFree(pReq);
    }
    else
        rcNt = STATUS_INSUFFICIENT_RESOURCES;
    RxContext->InformationToReturn = cbRet;
    LogFlow(("vbsfNtReadWorker: returns %#x cbRet=%#x @ %#RX64\n",
             rcNt, cbRet, RxContext->LowIoContext.ParamsFor.ReadWrite.ByteOffset));
    return rcNt;
}

/**
 * Wrapper for RxDispatchToWorkerThread().
 */
static VOID vbsfReadWorker(VOID *pv)
{
    PRX_CONTEXT RxContext = (PRX_CONTEXT)pv;

    Log(("VBOXSF: vbsfReadWorker: calling the worker\n"));

    RxContext->IoStatusBlock.Status = vbsfNtReadWorker(RxContext);

    Log(("VBOXSF: vbsfReadWorker: Status 0x%08X\n",
         RxContext->IoStatusBlock.Status));

    RxLowIoCompletion(RxContext);
}

/**
 * Read stuff from a file.
 *
 * Prior to calling us, RDBSS will have:
 *  - Called CcFlushCache() for uncached accesses.
 *  - For non-paging access the Fcb.Header.Resource lock in shared mode in one
 *    way or another (ExAcquireResourceSharedLite,
 *    ExAcquireSharedWaitForExclusive).
 *  - For paging the FCB isn't, but the Fcb.Header.PagingResource is taken
 *    in shared mode (ExAcquireResourceSharedLite).
 *
 * Upon completion, it will update the file pointer if applicable.  There are no
 * EOF checks and corresponding file size updating like in the write case, so
 * that's something we have to do ourselves it seems since the library relies on
 * the size information to be accurate in a few places (set EOF, cached reads).
 */
NTSTATUS VBoxMRxRead(IN PRX_CONTEXT RxContext)
{
    NTSTATUS Status;

    /* If synchronous operation, keep it on this thread (RDBSS already checked
       if we've got enough stack before calling us).   */
    if (!(RxContext->Flags & RX_CONTEXT_FLAG_ASYNC_OPERATION))
    {
        RxContext->IoStatusBlock.Status = Status = vbsfNtReadWorker(RxContext);
        Assert(Status != STATUS_PENDING);

        Log(("VBOXSF: MRxRead: vbsfReadInternal: Status %#08X\n", Status));
    }
    else
    {
        Status = RxDispatchToWorkerThread(VBoxMRxDeviceObject, DelayedWorkQueue, vbsfReadWorker, RxContext);

        Log(("VBOXSF: MRxRead: RxDispatchToWorkerThread: Status 0x%08X\n", Status));

        if (Status == STATUS_SUCCESS)
            Status = STATUS_PENDING;
    }

    return Status;
}

static NTSTATUS vbsfWriteInternal(IN PRX_CONTEXT RxContext)
{
    NTSTATUS Status = STATUS_SUCCESS;
    VBSFTRANSFERCTX ctx;

    RxCaptureFcb;
    RxCaptureFobx;

    PMRX_VBOX_NETROOT_EXTENSION pNetRootExtension = VBoxMRxGetNetRootExtension(capFcb->pNetRoot);
    PVBSFNTFCBEXT pVBoxFcbx = VBoxMRxGetFcbExtension(capFcb);
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(capFobx);

    PLOWIO_CONTEXT LowIoContext  = &RxContext->LowIoContext;

    PMDL BufferMdl = LowIoContext->ParamsFor.ReadWrite.Buffer;
    uint32_t ByteCount = LowIoContext->ParamsFor.ReadWrite.ByteCount;
    RXVBO ByteOffset = LowIoContext->ParamsFor.ReadWrite.ByteOffset;

    PVOID pbUserBuffer = RxLowIoGetBufferAddress(RxContext);

    int vrc;

#ifdef LOG_ENABLED
    BOOLEAN AsyncIo = BooleanFlagOn(RxContext->Flags, RX_CONTEXT_FLAG_ASYNC_OPERATION);
#endif
    LONGLONG FileSize;

    RxGetFileSizeWithLock((PFCB)capFcb, &FileSize);

    Log(("VBOXSF: vbsfWriteInternal: AsyncIo = %d, Fcb->FileSize = 0x%RX64\n",
         AsyncIo, capFcb->Header.FileSize.QuadPart));
    Log(("VBOXSF: vbsfWriteInternal: UserBuffer %p, BufferMdl %p\n",
         pbUserBuffer, BufferMdl));
    Log(("VBOXSF: vbsfWriteInternal: ByteCount is 0x%X, ByteOffset is 0x%RX64, FileSize 0x%RX64\n",
         ByteCount, ByteOffset, FileSize));

    /** @todo allow to write 0 bytes. */
    if (   !BufferMdl
        || ByteCount == 0)
    {
        AssertFailed();
        return STATUS_INVALID_PARAMETER;
    }

    ctx.pClient = &g_SfClient;
    ctx.pMap    = &pNetRootExtension->map;
    ctx.hFile   = pVBoxFobx->hFile;
    ctx.offset  = (uint64_t)ByteOffset;
    ctx.cbData  = ByteCount;
    ctx.pMdl    = BufferMdl;
    ctx.pBuffer = (uint8_t *)pbUserBuffer;
    ctx.fLocked = true;
    ctx.pfnTransferBuffer = vbsfTransferBufferWrite;
    ctx.pfnTransferPages = vbsfTransferPagesWrite;

    vrc = vbsfTransferCommon(&ctx);

    ByteCount = ctx.cbData;

    Status = vbsfNtVBoxStatusToNt(vrc);

    if (Status == STATUS_SUCCESS)
    {
        pVBoxFobx->fTimestampsImplicitlyUpdated |= VBOX_FOBX_F_INFO_LASTWRITE_TIME;
        if (pVBoxFcbx->pFobxLastWriteTime != pVBoxFobx)
            pVBoxFcbx->pFobxLastWriteTime = NULL;

        /* Make sure our cached file size value is up to date: */
        if (ctx.cbData > 0)
        {
            RTFOFF offEndOfWrite = LowIoContext->ParamsFor.ReadWrite.ByteOffset + ctx.cbData;
            if (pVBoxFobx->Info.cbObject < offEndOfWrite)
                pVBoxFobx->Info.cbObject = offEndOfWrite;

            if (pVBoxFobx->Info.cbAllocated < offEndOfWrite)
            {
                pVBoxFobx->Info.cbAllocated = offEndOfWrite;
                pVBoxFobx->nsUpToDate       = 0;
            }
        }
    }
    else
        ByteCount = 0; /* Nothing written. */

    RxContext->InformationToReturn = ByteCount;

    Log(("VBOXSF: vbsfWriteInternal: Status = 0x%08X, ByteCount = 0x%X\n",
         Status, ByteCount));

    return Status;
}

static VOID vbsfWriteWorker(VOID *pv)
{
    PRX_CONTEXT RxContext = (PRX_CONTEXT)pv;

    Log(("VBOXSF: vbsfWriteWorker: calling the worker\n"));

    RxContext->IoStatusBlock.Status = vbsfWriteInternal(RxContext);

    Log(("VBOXSF: vbsfWriteWorker: Status 0x%08X\n",
         RxContext->IoStatusBlock.Status));

    RxLowIoCompletion(RxContext);
}


NTSTATUS VBoxMRxWrite(IN PRX_CONTEXT RxContext)
{
    NTSTATUS Status = RxDispatchToWorkerThread(VBoxMRxDeviceObject, DelayedWorkQueue,
                                               vbsfWriteWorker,
                                               RxContext);

    Log(("VBOXSF: MRxWrite: RxDispatchToWorkerThread: Status 0x%08X\n",
         Status));

    if (Status == STATUS_SUCCESS)
        Status = STATUS_PENDING;

    return Status;
}


NTSTATUS VBoxMRxLocks(IN PRX_CONTEXT RxContext)
{
    NTSTATUS Status = STATUS_SUCCESS;

    RxCaptureFcb;
    RxCaptureFobx;

    PMRX_VBOX_NETROOT_EXTENSION pNetRootExtension = VBoxMRxGetNetRootExtension(capFcb->pNetRoot);
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(capFobx);

    PLOWIO_CONTEXT LowIoContext = &RxContext->LowIoContext;
    uint32_t fu32Lock = 0;
    int vrc;

    Log(("VBOXSF: MRxLocks: Operation %d\n",
         LowIoContext->Operation));

    switch (LowIoContext->Operation)
    {
        default:
            AssertMsgFailed(("VBOXSF: MRxLocks: Unsupported lock/unlock type %d detected!\n",
                             LowIoContext->Operation));
            return STATUS_NOT_IMPLEMENTED;

        case LOWIO_OP_UNLOCK_MULTIPLE:
            /** @todo Remove multiple locks listed in LowIoContext.ParamsFor.Locks.LockList. */
            Log(("VBOXSF: MRxLocks: Unsupported LOWIO_OP_UNLOCK_MULTIPLE!\n",
                 LowIoContext->Operation));
            return STATUS_NOT_IMPLEMENTED;

        case LOWIO_OP_SHAREDLOCK:
            fu32Lock = SHFL_LOCK_SHARED | SHFL_LOCK_PARTIAL;
            break;

        case LOWIO_OP_EXCLUSIVELOCK:
            fu32Lock = SHFL_LOCK_EXCLUSIVE | SHFL_LOCK_PARTIAL;
            break;

        case LOWIO_OP_UNLOCK:
            fu32Lock = SHFL_LOCK_CANCEL | SHFL_LOCK_PARTIAL;
            break;
    }

    if (LowIoContext->ParamsFor.Locks.Flags & LOWIO_LOCKSFLAG_FAIL_IMMEDIATELY)
        fu32Lock |= SHFL_LOCK_NOWAIT;
    else
        fu32Lock |= SHFL_LOCK_WAIT;

    vrc = VbglR0SfLock(&g_SfClient, &pNetRootExtension->map, pVBoxFobx->hFile,
                       LowIoContext->ParamsFor.Locks.ByteOffset, LowIoContext->ParamsFor.Locks.Length, fu32Lock);

    Status = vbsfNtVBoxStatusToNt(vrc);

    Log(("VBOXSF: MRxLocks: Returned 0x%08X\n", Status));
    return Status;
}

NTSTATUS VBoxMRxCompleteBufferingStateChangeRequest(IN OUT PRX_CONTEXT RxContext, IN OUT PMRX_SRV_OPEN SrvOpen,
                                                    IN PVOID pvContext)
{
    RT_NOREF(RxContext, SrvOpen, pvContext);
    Log(("VBOXSF: MRxCompleteBufferingStateChangeRequest: not implemented\n"));
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS VBoxMRxFlush (IN PRX_CONTEXT RxContext)
{
    NTSTATUS Status = STATUS_SUCCESS;

    RxCaptureFcb;
    RxCaptureFobx;

    PMRX_VBOX_NETROOT_EXTENSION pNetRootExtension = VBoxMRxGetNetRootExtension(capFcb->pNetRoot);
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(capFobx);

    int vrc;

    Log(("VBOXSF: MRxFlush\n"));

    /* Do the actual flushing of file buffers */
    vrc = VbglR0SfFlush(&g_SfClient, &pNetRootExtension->map, pVBoxFobx->hFile);

    Status = vbsfNtVBoxStatusToNt(vrc);

    Log(("VBOXSF: MRxFlush: Returned 0x%08X\n", Status));
    return Status;
}

/** See PMRX_EXTENDFILE_CALLDOWN in ddk/mrx.h
 *
 * Documentation says it returns STATUS_SUCCESS on success and an error
 * status on failure, so the ULONG return type is probably just a typo that
 * stuck.
 */
ULONG NTAPI VBoxMRxExtendStub(IN OUT struct _RX_CONTEXT * RxContext, IN OUT PLARGE_INTEGER pNewFileSize,
                              OUT PLARGE_INTEGER pNewAllocationSize)
{
    RT_NOREF(RxContext);

    /* Note: On Windows hosts vbsfNtSetEndOfFile returns ACCESS_DENIED if the file has been
     *       opened in APPEND mode. Writes to a file will extend it anyway, therefore it is
     *       better to not call the host at all and tell the caller that the file was extended.
     */
    Log(("VBOXSF: MRxExtendStub: new size = %RX64\n",
         pNewFileSize->QuadPart));

    pNewAllocationSize->QuadPart = pNewFileSize->QuadPart;

    return STATUS_SUCCESS;
}

