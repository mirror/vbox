/*++

 Copyright (c) 1989 - 1999 Microsoft Corporation

 Module Name:

 DownLvlI.c

 Abstract:

 This module implements downlevel fileinfo, volinfo, and dirctrl.

 --*/

#include "precomp.h"
#include <iprt/fs.h>
#pragma hdrstop

//
//  The local debug trace level
//

RXDT_DefineCategory (DOWNLVLI);
#define Dbg                 (DEBUG_TRACE_DOWNLVLI)

NTSTATUS VBoxMRxSetEndOfFile (IN OUT struct _RX_CONTEXT * RxContext, IN OUT PLARGE_INTEGER pNewFileSize, OUT PLARGE_INTEGER pNewAllocationSize)
/*++

 Routine Description:

 This routine handles requests to truncate or extend the file

 Arguments:

 RxContext - the RDBSS context

 Return Value:

 NTSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    RxCaptureFcb;
    RxCaptureFobx;
    PMRX_NET_ROOT pNetRoot = capFcb->pNetRoot;
    PMRX_VBOX_FOBX pVBoxFobx = VBoxMRxGetFileObjectExtension(capFobx);
    VBoxMRxGetNetRootExtension(capFcb->pNetRoot, pNetRootExtension);
    VBoxMRxGetDeviceExtension(RxContext, pDeviceExtension);
    PRTFSOBJINFO pObjInfo = 0;
    uint32_t cbBuffer;
    int vboxRC;

    Log(("VBOXSF: VBoxMRxSetEndOfFile: New size = %RX64 (0x%x), pNewAllocationSize = 0x%x\n", pNewFileSize->QuadPart, pNewFileSize, pNewAllocationSize));

    cbBuffer = sizeof(RTFSOBJINFO);
    pObjInfo = (PRTFSOBJINFO)vbsfAllocNonPagedMem(cbBuffer);
    if (pObjInfo == 0)
    {
        AssertFailed();
        return STATUS_NO_MEMORY;
    }
    memset(pObjInfo, 0, cbBuffer);

    pObjInfo->cbObject = pNewFileSize->QuadPart;
    Assert(pVBoxFobx && pNetRootExtension && pDeviceExtension);
    vboxRC = vboxCallFSInfo(&pDeviceExtension->hgcmClient, &pNetRootExtension->map, pVBoxFobx->hFile, SHFL_INFO_SET | SHFL_INFO_SIZE, &cbBuffer, (PSHFLDIRINFO)pObjInfo);
    AssertRC(vboxRC);

    Log(("VBOXSF: VBoxMRxSetEndOfFile: vboxCallFSInfo returned %d\n", vboxRC));

    Status = VBoxErrorToNTStatus(vboxRC);
    if (Status == STATUS_SUCCESS)
    {
        Log(("VBOXSF: VBoxMRxSetEndOfFile: vboxCallFSInfo new allocation size = %RX64\n", pObjInfo->cbAllocated));

        /* Return new allocation size */
        pNewAllocationSize->QuadPart = pObjInfo->cbAllocated;
    }

    if (pObjInfo)
        vbsfFreeNonPagedMem((PVOID)pObjInfo);

    Log(("VBOXSF: VBoxMRxSetEndOfFile: Returned %d\n", Status));
    return Status;
}

NTSTATUS VBoxMRxTruncateFile (IN OUT struct _RX_CONTEXT * RxContext)
/*++

 Routine Description:

 This routine handles requests to truncate the file

 Arguments:

 RxContext - the RDBSS context

 Return Value:

 NTSTATUS - The return status for the operation

 --*/
{
    Log(("VBOXSF: VBoxMRxTruncateFile not implemented!\n"));
    return STATUS_NOT_IMPLEMENTED;
}

ULONG VBoxMRxExtendFile (IN OUT struct _RX_CONTEXT * RxContext, IN OUT PLARGE_INTEGER pNewFileSize, OUT PLARGE_INTEGER pNewAllocationSize)
/*++

 Routine Description:

 This routine handles requests to extend the file for cached IO.

 Arguments:

 RxContext - the RDBSS context

 Return Value:

 NTSTATUS - The return status for the operation

 --*/
{
    Log(("VBOXSF: VBoxMRxExtendFile new size = %RX64\n", pNewFileSize->QuadPart));
    return VBoxMRxSetEndOfFile(RxContext, pNewFileSize, pNewAllocationSize);
}

NTSTATUS VBoxMRxExtendStub (IN OUT struct _RX_CONTEXT * RxContext, IN OUT PLARGE_INTEGER pNewFileSize, OUT PLARGE_INTEGER pNewAllocationSize)
/*++

Routine Description:

   This routine handles MRxExtendForCache and MRxExtendForNonCache requests. Since the write
   itself will extend the file, we can pretty much just get out quickly.

Arguments:

    RxContext - the RDBSS context

Return Value:

    RXSTATUS - The return status for the operation

--*/
{
    /* Note: On Windows hosts VBoxMRxSetEndOfFile returns ACCESS_DENIED if the file has been
     *       opened in APPEND mode. Therefore it is better to not call host at all and implement
     *       this MrxExtend* request just like in mrxsmb DDK sample.
     */
    Log(("VBOXSF: VBoxMRxExtendStub new size = %RX64\n", pNewFileSize->QuadPart));

    pNewAllocationSize->QuadPart = pNewFileSize->QuadPart;

    return(STATUS_SUCCESS);
}
