/*++

 Copyright (c) 1989 - 1999 Microsoft Corporation

 Module Name:

 mrxglobs.h

 Abstract:

 The global include file for MRX_VBOX mini-redirector

 --*/

#ifndef _MRXGLOBS_H_
#define _MRXGLOBS_H_

#ifdef VBOX
#include "VBoxGuestR0LibSharedFolders.h"
#endif

extern PRDBSS_DEVICE_OBJECT VBoxMRxDeviceObject;
#define RxNetNameTable (*(*___MINIRDR_IMPORTS_NAME).pRxNetNameTable)

// The following enum type defines the various states associated with the null
// mini redirector. This is used during initialization

typedef enum _MRX_VBOX_STATE_
{
    MRX_VBOX_STARTABLE, MRX_VBOX_START_IN_PROGRESS, MRX_VBOX_STARTED
} MRX_VBOX_STATE, *PMRX_VBOX_STATE;

extern MRX_VBOX_STATE VBoxMRxState;
extern ULONG LogRate;
extern ULONG VBoxMRxVersion;

//
//  Reg keys
//
#define NULL_MINIRDR_PARAMETERS \
    L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\VBoxMRx\\Parameters"

/*
 * Maximum drive letters (A - Z).
 */
#define _MRX_MAX_DRIVE_LETTERS 26

//
// typedef our device extension - stores state global to the driver
//
typedef struct _MRX_VBOX_DEVICE_EXTENSION
{
    //
    //  Back-pointer to owning device object
    //
    PRDBSS_DEVICE_OBJECT pDeviceObject;

    //
    //  Count of active nodes
    //  Driver can be unloaded if ActiveNodes == 0
    //
    ULONG ulActiveNodes;

    /*
     * Keep a list of local connections used.
     * The size (_MRX_MAX_DRIVE_LETTERS = 26) of the array presents the available drive letters C: - Z: of Windows.
     */
    CHAR cLocalConnections[_MRX_MAX_DRIVE_LETTERS];
    PUNICODE_STRING wszLocalConnectionName[_MRX_MAX_DRIVE_LETTERS];
    FAST_MUTEX mtxLocalCon;

#ifdef VBOX
    /* The HGCM client information. */
    VBSFCLIENT hgcmClient;
    /* Poller thread flags */
    ULONG ulPollerThreadFlags;

    /* Poller thread handle */
    PKTHREAD pPollerThread;

    /* Redirector driver object pointer */
    PDRIVER_OBJECT pDriverObject;
#endif

    NTSTATUS (* pfnRDBSSDeviceControl) (PDEVICE_OBJECT pDevObj, PIRP pIrp);

} MRX_VBOX_DEVICE_EXTENSION, *PMRX_VBOX_DEVICE_EXTENSION;

//
// NET_ROOT extension - stores state global to a root
//
typedef struct _MRX_VBOX_NETROOT_EXTENSION
{
    /* The HGCM client information. */
    VBSFCLIENT *phgcmClient;
    VBSFMAP map;

} MRX_VBOX_NETROOT_EXTENSION, *PMRX_VBOX_NETROOT_EXTENSION;

// A pointer to an instance of MRX_VBOX_FOBX is stored in the context field
// of MRX_FOBXs handled by the SMB mini rdr. Depending upon the file type
// i.e., file or directory the appropriate context information is stored.

typedef struct _MRX_VBOX_FOBX_
{
    SHFLHANDLE hFile;
    PMRX_SRV_CALL pSrvCall;
    FILE_BASIC_INFORMATION FileBasicInfo;
    FILE_STANDARD_INFORMATION FileStandardInfo;
    BOOLEAN fKeepCreationTime;
    BOOLEAN fKeepLastAccessTime;
    BOOLEAN fKeepLastWriteTime;
    BOOLEAN fKeepChangeTime;

} MRX_VBOX_FOBX, *PMRX_VBOX_FOBX;

//
//  Macros to get & validate extensions
//

#define VBoxMRxGetDeviceExtension(RxContext,pExt)        \
        PMRX_VBOX_DEVICE_EXTENSION pExt = (PMRX_VBOX_DEVICE_EXTENSION)((PBYTE)(RxContext->RxDeviceObject) + sizeof(RDBSS_DEVICE_OBJECT))

#define VBoxMRxGetNetRootExtension(pNetRoot,pExt)        \
        PMRX_VBOX_NETROOT_EXTENSION pExt = (((pNetRoot) == NULL) ? NULL : (PMRX_VBOX_NETROOT_EXTENSION)((pNetRoot)->Context))

#define VBoxMRxGetFcbExtension(pFcb,pExt)                \
        PMRX_VBOX_FCB_EXTENSION pExt = (((pFcb) == NULL) ? NULL : (PMRX_VBOX_FCB_EXTENSION)((pFcb)->Context))

#define VBoxMRxGetSrvOpenExtension(pSrvOpen)  \
        (((pSrvOpen) == NULL) ? NULL : (PMRX_VBOX_SRV_OPEN)((pSrvOpen)->Context))

#define VBoxMRxGetFileObjectExtension(pFobx)  \
        (((pFobx) == NULL) ? NULL : (PMRX_VBOX_FOBX)((pFobx)->Context))

//
// forward declarations for all dispatch vector methods.
//

#ifdef __cplusplus
extern "C"
{
#endif

extern NTSTATUS
VBoxMRxStart (
        IN OUT struct _RX_CONTEXT * RxContext,
        IN OUT PRDBSS_DEVICE_OBJECT RxDeviceObject
);

extern NTSTATUS
VBoxMRxStop (
        IN OUT struct _RX_CONTEXT * RxContext,
        IN OUT PRDBSS_DEVICE_OBJECT RxDeviceObject
);

extern NTSTATUS
VBoxMRxMinirdrControl (
        IN OUT PRX_CONTEXT RxContext,
        IN OUT PVOID pContext,
        IN OUT PUCHAR SharedBuffer,
        IN ULONG InputBufferLength,
        IN ULONG OutputBufferLength,
        OUT PULONG CopyBackLength
);

extern NTSTATUS
VBoxMRxDevFcb (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxDevFcbXXXControlFile (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxCreate (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxCollapseOpen (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxShouldTryToCollapseThisOpen (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxRead (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxWrite (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxLocks(
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxFlush(
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxFsCtl(
        IN OUT PRX_CONTEXT RxContext
);

NTSTATUS
VBoxMRxIoCtl(
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxNotifyChangeDirectory(
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxComputeNewBufferingState(
        IN OUT PMRX_SRV_OPEN pSrvOpen,
        IN PVOID pMRxContext,
        OUT ULONG *pNewBufferingState);

extern NTSTATUS
VBoxMRxFlush (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxCloseWithDelete (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxZeroExtend (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxSetEndOfFile(
        IN OUT struct _RX_CONTEXT * RxContext,
        IN OUT PLARGE_INTEGER pNewFileSize,
        OUT PLARGE_INTEGER pNewAllocationSize
);

extern NTSTATUS
VBoxMRxTruncate (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxCleanupFobx (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxCloseSrvOpen (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxClosedSrvOpenTimeOut (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxQueryDirectory (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxQueryEaInformation (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxSetEaInformation (
        IN OUT struct _RX_CONTEXT * RxContext
);

extern NTSTATUS
VBoxMRxQuerySecurityInformation (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxSetSecurityInformation (
        IN OUT struct _RX_CONTEXT * RxContext
);

extern NTSTATUS
VBoxMRxQueryVolumeInformation (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxSetVolumeInformation (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxLowIOSubmit (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxCreateVNetRoot(
        IN OUT PMRX_CREATENETROOT_CONTEXT pContext
);

extern NTSTATUS
VBoxMRxFinalizeVNetRoot(
        IN OUT PMRX_V_NET_ROOT pVirtualNetRoot,
        IN PBOOLEAN ForceDisconnect);

extern NTSTATUS
VBoxMRxFinalizeNetRoot(
        IN OUT PMRX_NET_ROOT pNetRoot,
        IN PBOOLEAN ForceDisconnect);

extern NTSTATUS
VBoxMRxUpdateNetRootState(
        IN PMRX_NET_ROOT pNetRoot);

VOID
VBoxMRxExtractNetRootName(
        IN PUNICODE_STRING FilePathName,
        IN PMRX_SRV_CALL SrvCall,
        OUT PUNICODE_STRING NetRootName,
        OUT PUNICODE_STRING RestOfName OPTIONAL
);

extern NTSTATUS
VBoxMRxCreateSrvCall(
        PMRX_SRV_CALL pSrvCall,
        PMRX_SRVCALL_CALLBACK_CONTEXT pCallbackContext);

extern NTSTATUS
VBoxMRxFinalizeSrvCall(
        PMRX_SRV_CALL pSrvCall,
        BOOLEAN Force);

extern NTSTATUS
VBoxMRxSrvCallWinnerNotify(
        IN OUT PMRX_SRV_CALL pSrvCall,
        IN BOOLEAN ThisMinirdrIsTheWinner,
        IN OUT PVOID pSrvCallContext);

extern NTSTATUS
VBoxMRxQueryFileInformation (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxQueryNamedPipeInformation (
        IN OUT PRX_CONTEXT RxContext,
        IN FILE_INFORMATION_CLASS FileInformationClass,
        IN OUT PVOID Buffer,
        IN OUT PULONG pLengthRemaining
);

extern NTSTATUS
VBoxMRxSetFileInformation (
        IN OUT PRX_CONTEXT RxContext
);

extern NTSTATUS
VBoxMRxSetNamedPipeInformation (
        IN OUT PRX_CONTEXT RxContext,
        IN FILE_INFORMATION_CLASS FileInformationClass,
        IN PVOID pBuffer,
        IN ULONG BufferLength
);

NTSTATUS
VBoxMRxSetFileInformationAtCleanup(
        IN OUT PRX_CONTEXT RxContext
);

NTSTATUS
VBoxMRxDeallocateForFcb (
        IN OUT PMRX_FCB pFcb
);

NTSTATUS
VBoxMRxDeallocateForFobx (
        IN OUT PMRX_FOBX pFobx
);

extern NTSTATUS
VBoxMRxForcedClose (
        IN OUT PMRX_SRV_OPEN SrvOpen
);

extern ULONG
VBoxMRxExtendFile (
        IN OUT struct _RX_CONTEXT * RxContext,
        IN OUT PLARGE_INTEGER pNewFileSize,
        OUT PLARGE_INTEGER pNewAllocationSize
);

extern NTSTATUS
VBoxMRxExtendStub (
        IN OUT struct _RX_CONTEXT * RxContext,
        IN OUT PLARGE_INTEGER pNewFileSize,
        OUT PLARGE_INTEGER pNewAllocationSize
);

extern NTSTATUS
VBoxMRxTruncateFile(
        IN OUT struct _RX_CONTEXT * RxContext
);

extern NTSTATUS
VBoxMRxCompleteBufferingStateChangeRequest (
        IN OUT PRX_CONTEXT RxContext,
        IN OUT PMRX_SRV_OPEN SrvOpen,
        IN PVOID pContext
);

extern NTSTATUS
VBoxMRxExtendForCache (
        IN OUT PRX_CONTEXT RxContext,
        IN OUT PFCB Fcb,
        OUT PLONGLONG pNewFileSize
);

extern
NTSTATUS
VBoxMRxInitializeSecurity (VOID);

extern
NTSTATUS
VBoxMRxUninitializeSecurity (VOID);

extern
NTSTATUS
VBoxMRxInitializeTransport(VOID);

extern
NTSTATUS
VBoxMRxUninitializeTransport(VOID);

#ifdef __cplusplus
}
;
#endif

#define VBoxMRxMakeSrvOpenKey(Tid,Fid) \
        (PVOID)(((ULONG)(Tid) << 16) | (ULONG)(Fid))

#include "mrxprocs.h"   // crossreferenced routines
#endif _MRXGLOBS_H_

