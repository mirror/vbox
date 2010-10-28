/*++

 Copyright (c) 1989 - 1999 Microsoft Corporation

 Module Name:

 Init.c

 Abstract:

 This module implements the DRIVER_INITIALIZATION routine for
 the null mini rdr.

 --*/

#include "precomp.h"
#include "rdbss_vbox.h"
#pragma  hdrstop

#include "ntverp.h"
#include "nulmrx.h"

#ifdef VBOX
#include "VBoxGuestR0LibSharedFolders.h"

#if defined(DEBUG) || defined (LOG_ENABLED)
static PCHAR MajorFunctionString(UCHAR MajorFunction, LONG MinorFunction);
#endif
#endif

//
// Global data declarations.
//

MRX_VBOX_STATE VBoxMRxState = MRX_VBOX_STARTABLE;

//
//  Mini Redirector global variables.
//

//
//  LogRate
//
ULONG LogRate = 0;

//
//  This is the minirdr dispatch table. It is initialized by
//  VBoxMRxInitializeTables. This table will be used by the wrapper to call
//  into this minirdr
//

struct _MINIRDR_DISPATCH VBoxMRxDispatch;

//
// Pointer to the device Object for this minirdr. Since the device object is
// created by the wrapper when this minirdr registers, this pointer is
// initialized in the DriverEntry routine below (see RxRegisterMinirdr)
//

PRDBSS_DEVICE_OBJECT VBoxMRxDeviceObject;

#define VBOX_SF_STATIC_LIB

//
// declare the shadow debugtrace controlpoints
//

RXDT_DefineCategory (CREATE);
RXDT_DefineCategory (CLEANUP);
RXDT_DefineCategory (CLOSE);
RXDT_DefineCategory (READ);
RXDT_DefineCategory (WRITE);
RXDT_DefineCategory (LOCKCTRL);
RXDT_DefineCategory (FLUSH);
RXDT_DefineCategory (PREFIX);
RXDT_DefineCategory (FCBSTRUCTS);
RXDT_DefineCategory (DISPATCH);
RXDT_DefineCategory (EA);
RXDT_DefineCategory (DEVFCB);
RXDT_DefineCategory (INIT);

//
// The following enumerated values signify the current state of the minirdr
// initialization. With the aid of this state information, it is possible
// to determine which resources to deallocate, whether deallocation comes
// as a result of a normal stop/unload, or as the result of an exception
//

typedef enum _MRX_VBOX_INIT_STATES
{
    MRX_VBOXINIT_UNINT, MRX_VBOXINIT_ALL_INITIALIZATION_COMPLETED, MRX_VBOXINIT_MINIRDR_REGISTERED, MRX_VBOXINIT_START
} MRX_VBOX_INIT_STATES;

//
// function prototypes
//

NTSTATUS VBoxMRxInitializeTables (void);

VOID VBoxMRxUnload (IN PDRIVER_OBJECT DriverObject);

VOID VBoxMRxInitUnwind (IN PDRIVER_OBJECT DriverObject, IN MRX_VBOX_INIT_STATES VBoxMRxInitState);

NTSTATUS VBoxMRxFsdDispatch (IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

VOID VBoxMRxReadRegistryParameters ();

#ifdef VBOX
static int
vboxCtlGuestFilterMask (uint32_t u32OrMask, uint32_t u32NotMask)
{
    int rc;
    VMMDevCtlGuestFilterMask *req;

    rc = VbglGRAlloc ((VMMDevRequestHeader**) &req, sizeof (*req),
            VMMDevReq_CtlGuestFilterMask);

    if (RT_FAILURE (rc))
    {
        Log (("VBOXSF: vboxCtlGuestFilterMask: "
                        "VbglGRAlloc (CtlGuestFilterMask) failed rc=%Rrc\n", rc));
        return -1;
    }

    req->u32OrMask = u32OrMask;
    req->u32NotMask = u32NotMask;

    rc = VbglGRPerform (&req->header);
    VbglGRFree (&req->header);
    if (RT_FAILURE (rc))
    {
        Log (("VBOXSF: vboxCtlGuestFilterMask: "
                        "VbglGRPerform (CtlGuestFilterMask) failed rc=%Rrc\n", rc));
        return -1;
    }
    return 0;
}
#endif

static PFAST_IO_DISPATCH pfnRDBSSTable = NULL;

BOOLEAN VBoxMrxFastIoRead (IN struct _FILE_OBJECT *FileObject, IN PLARGE_INTEGER FileOffset, IN ULONG Length, IN BOOLEAN Wait, IN ULONG LockKey, OUT PVOID Buffer, OUT PIO_STATUS_BLOCK IoStatus,
                           IN struct _DEVICE_OBJECT *DeviceObject)
{
#ifdef VBOX_FAIL_FASTIO
    Log(("VBOXSF: VBoxMrxFastIoRead\n"));
    IoStatus->Information = 0;
    IoStatus->Status = STATUS_NOT_IMPLEMENTED;
    return FALSE;
#else
    BOOLEAN ret = FALSE;

    Log(("VBOXSF: VBoxMrxFastIoRead: FileObject = 0x%x, QuadPart = %RX64, Len = %d, Buffer = 0x%x\n", FileObject, FileOffset->QuadPart, Length, Buffer));
    if (pfnRDBSSTable && pfnRDBSSTable->FastIoRead)
    {
        /* Necessary to satisfy the driver verifier. APCs must be turned off when ExAcquireResourceExclusiveLite is called */
        FsRtlEnterFileSystem();
        ret = pfnRDBSSTable->FastIoRead(FileObject, FileOffset, Length, Wait, LockKey, Buffer, IoStatus, DeviceObject);
        FsRtlExitFileSystem();
    }

    Log(("VBOXSF: VBoxMrxFastIoRead: Returned 0x%x\n", ret));
    return ret;
#endif /* !VBOX_FAIL_FASTIO */
}

BOOLEAN VBoxMrxFastIoWrite (IN struct _FILE_OBJECT *FileObject, IN PLARGE_INTEGER FileOffset, IN ULONG Length, IN BOOLEAN Wait, IN ULONG LockKey, IN PVOID Buffer, OUT PIO_STATUS_BLOCK IoStatus,
                            IN struct _DEVICE_OBJECT *DeviceObject)
{
#ifdef VBOX_FAIL_FASTIO
    Log(("VBOXSF: VBoxMrxFastIoWrite\n"));
    IoStatus->Information = 0;
    IoStatus->Status = STATUS_NOT_IMPLEMENTED;
    return FALSE;
#else
    BOOLEAN ret = FALSE;

    Log(("VBOXSF: VBoxMrxFastIoWrite: FileObject = 0x%x, QuadPart = %RX64, Len = %d, Buffer = 0x%x\n", FileObject, FileOffset->QuadPart, Length, Buffer));
    if (pfnRDBSSTable && pfnRDBSSTable->FastIoWrite)
    {
        /* Necessary to satisfy the driver verifier. APCs must be turned off when ExAcquireResourceExclusiveLite is called */
        FsRtlEnterFileSystem();
        ret = pfnRDBSSTable->FastIoWrite(FileObject, FileOffset, Length, Wait, LockKey, Buffer, IoStatus, DeviceObject);
        FsRtlExitFileSystem();
    }

    Log(("VBOXSF: VBoxMrxFastIoWrite: Returned 0x%x\n", ret));
    return ret;
#endif /* !VBOX_FAIL_FASTIO */
}

BOOLEAN VBoxMrxFastIoCheckIfPossible (IN struct _FILE_OBJECT *FileObject, IN PLARGE_INTEGER FileOffset, IN ULONG Length, IN BOOLEAN Wait, IN ULONG LockKey, IN BOOLEAN CheckForReadOperation,
                                      OUT PIO_STATUS_BLOCK IoStatus, IN struct _DEVICE_OBJECT *DeviceObject)
{
    BOOLEAN ret = FALSE;

    if (pfnRDBSSTable && pfnRDBSSTable->FastIoCheckIfPossible)
    {
        ret = pfnRDBSSTable->FastIoCheckIfPossible(FileObject, FileOffset, Length, Wait, LockKey, CheckForReadOperation, IoStatus, DeviceObject);
        Log(("VBOXSF: VBoxMrxFastIoCheckIfPossible: Returned 0x%x\n", ret));
    }
    return ret;
}

BOOLEAN VBoxMrxFastIoQueryBasicInfo (IN struct _FILE_OBJECT *FileObject, IN BOOLEAN Wait, OUT PFILE_BASIC_INFORMATION Buffer, OUT PIO_STATUS_BLOCK IoStatus, IN struct _DEVICE_OBJECT *DeviceObject)
{
    BOOLEAN ret = FALSE;

    if (pfnRDBSSTable && pfnRDBSSTable->FastIoQueryBasicInfo)
    {
        ret = pfnRDBSSTable->FastIoQueryBasicInfo(FileObject, Wait, Buffer, IoStatus, DeviceObject);
        Log(("VBOXSF: VBoxMrxFastIoQueryBasicInfo: Returned 0x%x\n", ret));
    }
    return ret;
}

BOOLEAN VBoxMrxFastIoQueryStandardInfo (IN struct _FILE_OBJECT *FileObject, IN BOOLEAN Wait, OUT PFILE_STANDARD_INFORMATION Buffer, OUT PIO_STATUS_BLOCK IoStatus,
                                        IN struct _DEVICE_OBJECT *DeviceObject)
{
    BOOLEAN ret = FALSE;

    if (pfnRDBSSTable && pfnRDBSSTable->FastIoQueryStandardInfo)
    {
        ret = pfnRDBSSTable->FastIoQueryStandardInfo(FileObject, Wait, Buffer, IoStatus, DeviceObject);
        Log(("VBOXSF: VBoxMrxFastIoQueryStandardInfo: Returned 0x%x\n", ret));
    }
    return ret;
}

BOOLEAN VBoxMrxFastIoLock (IN struct _FILE_OBJECT *FileObject, IN PLARGE_INTEGER FileOffset, IN PLARGE_INTEGER Length, PEPROCESS ProcessId, ULONG Key, BOOLEAN FailImmediately, BOOLEAN ExclusiveLock,
                           OUT PIO_STATUS_BLOCK IoStatus, IN struct _DEVICE_OBJECT *DeviceObject)
{
    BOOLEAN ret = FALSE;

    if (pfnRDBSSTable && pfnRDBSSTable->FastIoLock)
    {
        ret = pfnRDBSSTable->FastIoLock(FileObject, FileOffset, Length, ProcessId, Key, FailImmediately, ExclusiveLock, IoStatus, DeviceObject);
        Log(("VBOXSF: VBoxMrxFastIoLock: Returned 0x%x\n", ret));
    }
    return ret;
}

BOOLEAN VBoxMrxFastIoUnlockSingle (IN struct _FILE_OBJECT *FileObject, IN PLARGE_INTEGER FileOffset, IN PLARGE_INTEGER Length, PEPROCESS ProcessId, ULONG Key, OUT PIO_STATUS_BLOCK IoStatus,
                                   IN struct _DEVICE_OBJECT *DeviceObject)
{
    BOOLEAN ret = FALSE;

    if (pfnRDBSSTable && pfnRDBSSTable->FastIoUnlockSingle)
    {
        ret = pfnRDBSSTable->FastIoUnlockSingle(FileObject, FileOffset, Length, ProcessId, Key, IoStatus, DeviceObject);
        Log(("VBOXSF: VBoxMrxFastIoUnlockSingle: Returned 0x%x\n", ret));
    }
    return ret;
}

BOOLEAN VBoxMrxFastIoUnlockAll (IN struct _FILE_OBJECT *FileObject, PEPROCESS ProcessId, OUT PIO_STATUS_BLOCK IoStatus, IN struct _DEVICE_OBJECT *DeviceObject)
{
    BOOLEAN ret = FALSE;

    if (pfnRDBSSTable && pfnRDBSSTable->FastIoUnlockAll)
    {
        ret = pfnRDBSSTable->FastIoUnlockAll(FileObject, ProcessId, IoStatus, DeviceObject);
        Log(("VBOXSF: VBoxMrxFastIoUnlockAll: Returned 0x%x\n", ret));
    }
    return ret;
}

BOOLEAN VBoxMrxFastIoUnlockAllByKey (IN struct _FILE_OBJECT *FileObject, PVOID ProcessId, ULONG Key, OUT PIO_STATUS_BLOCK IoStatus, IN struct _DEVICE_OBJECT *DeviceObject)
{
    BOOLEAN ret = FALSE;

    if (pfnRDBSSTable && pfnRDBSSTable->FastIoUnlockAllByKey)
    {
        ret = pfnRDBSSTable->FastIoUnlockAllByKey(FileObject, ProcessId, Key, IoStatus, DeviceObject);
        Log(("VBOXSF: VBoxMrxFastIoUnlockAllByKey: Returned 0x%x\n", ret));
    }
    return ret;
}

BOOLEAN VBoxMrxFastIoDeviceControl (IN struct _FILE_OBJECT *FileObject, IN BOOLEAN Wait, IN PVOID InputBuffer OPTIONAL, IN ULONG InputBufferLength, OUT PVOID OutputBuffer OPTIONAL, IN ULONG OutputBufferLength,
                                    IN ULONG IoControlCode, OUT PIO_STATUS_BLOCK IoStatus, IN struct _DEVICE_OBJECT *DeviceObject)
{
    BOOLEAN ret = FALSE;

    if (pfnRDBSSTable && pfnRDBSSTable->FastIoDeviceControl)
    {
        ret = pfnRDBSSTable->FastIoDeviceControl(FileObject, Wait, InputBuffer, InputBufferLength, OutputBuffer, OutputBufferLength, IoControlCode, IoStatus, DeviceObject);
        Log(("VBOXSF: VBoxMrxFastIoDeviceControl: Returned 0x%x\n", ret));
    }
    return ret;
}

VOID VBoxMrxAcquireFileForNtCreateSection (IN struct _FILE_OBJECT *FileObject)
{
    if (pfnRDBSSTable && pfnRDBSSTable->AcquireFileForNtCreateSection)
    {
        pfnRDBSSTable->AcquireFileForNtCreateSection(FileObject);
        Log(("VBOXSF: VBoxMrxAcquireFileForNtCreateSection: Called.\n"));
    }
}

VOID VBoxMrxReleaseFileForNtCreateSection (IN struct _FILE_OBJECT *FileObject)
{
    if (pfnRDBSSTable && pfnRDBSSTable->ReleaseFileForNtCreateSection)
    {
        pfnRDBSSTable->ReleaseFileForNtCreateSection(FileObject);
        Log(("VBOXSF: VBoxMrxReleaseFileForNtCreateSection: Called.\n"));
    }
}

VOID VBoxMrxFastIoDetachDevice (IN struct _DEVICE_OBJECT *SourceDevice, IN struct _DEVICE_OBJECT *TargetDevice)
{
    if (pfnRDBSSTable && pfnRDBSSTable->FastIoDetachDevice)
    {
        pfnRDBSSTable->FastIoDetachDevice(SourceDevice, TargetDevice);
        Log(("VBOXSF: VBoxMrxFastIoDetachDevice: Called.\n"));
    }
}

BOOLEAN VBoxMrxFastIoQueryNetworkOpenInfo (IN struct _FILE_OBJECT *FileObject, IN BOOLEAN Wait, OUT struct _FILE_NETWORK_OPEN_INFORMATION *Buffer, OUT struct _IO_STATUS_BLOCK *IoStatus,
                                           IN struct _DEVICE_OBJECT *DeviceObject)
{
    BOOLEAN ret = FALSE;

    if (pfnRDBSSTable && pfnRDBSSTable->FastIoQueryNetworkOpenInfo)
    {
        ret = pfnRDBSSTable->FastIoQueryNetworkOpenInfo(FileObject, Wait, Buffer, IoStatus, DeviceObject);
        Log(("VBOXSF: VBoxMrxFastIoQueryNetworkOpenInfo: Called.\n"));
    }
    return ret;
}

BOOLEAN VBoxMrxMdlRead (IN struct _FILE_OBJECT *FileObject, IN PLARGE_INTEGER FileOffset, IN ULONG Length, IN ULONG LockKey, OUT PMDL *MdlChain, OUT PIO_STATUS_BLOCK IoStatus,
                        IN struct _DEVICE_OBJECT *DeviceObject)
{
    BOOLEAN ret = FALSE;

    if (pfnRDBSSTable && pfnRDBSSTable->MdlRead)
    {
        ret = pfnRDBSSTable->MdlRead(FileObject, FileOffset, Length, LockKey, MdlChain, IoStatus, DeviceObject);
        Log(("VBOXSF: VBoxMrxMdlRead: Returned 0x%x\n", ret));
    }
    return ret;
}

BOOLEAN VBoxMrxMdlReadComplete (IN struct _FILE_OBJECT *FileObject, IN PMDL MdlChain, IN struct _DEVICE_OBJECT *DeviceObject)
{
    BOOLEAN ret = FALSE;

    if (pfnRDBSSTable && pfnRDBSSTable->MdlReadComplete)
    {
        ret = pfnRDBSSTable->MdlReadComplete(FileObject, MdlChain, DeviceObject);
        Log(("VBOXSF: VBoxMrxMdlReadComplete: Returned 0x%x\n", ret));
    }
    return ret;
}

BOOLEAN VBoxMrxPrepareMdlWrite (IN struct _FILE_OBJECT *FileObject, IN PLARGE_INTEGER FileOffset, IN ULONG Length, IN ULONG LockKey, OUT PMDL *MdlChain, OUT PIO_STATUS_BLOCK IoStatus,
                                IN struct _DEVICE_OBJECT *DeviceObject)
{
    BOOLEAN ret = FALSE;

    if (pfnRDBSSTable && pfnRDBSSTable->PrepareMdlWrite)
    {
        ret = pfnRDBSSTable->PrepareMdlWrite(FileObject, FileOffset, Length, LockKey, MdlChain, IoStatus, DeviceObject);
        Log(("VBOXSF: VBoxMrxPrepareMdlWrite: Returned 0x%x\n", ret));
    }
    return ret;
}

BOOLEAN VBoxMrxMdlWriteComplete (IN struct _FILE_OBJECT *FileObject, IN PLARGE_INTEGER FileOffset, IN PMDL MdlChain, IN struct _DEVICE_OBJECT *DeviceObject)
{
    BOOLEAN ret = FALSE;

    if (pfnRDBSSTable && pfnRDBSSTable->MdlWriteComplete)
    {
        ret = pfnRDBSSTable->MdlWriteComplete(FileObject, FileOffset, MdlChain, DeviceObject);
        Log(("VBOXSF: VBoxMrxMdlWriteComplete: Returned 0x%x\n", ret));
    }
    return ret;
}

NTSTATUS VBoxMrxAcquireForModWrite (IN struct _FILE_OBJECT *FileObject, IN PLARGE_INTEGER EndingOffset, OUT struct _ERESOURCE **ResourceToRelease, IN struct _DEVICE_OBJECT *DeviceObject)
{
    NTSTATUS ret = STATUS_NOT_IMPLEMENTED;

    if (pfnRDBSSTable && pfnRDBSSTable->AcquireForModWrite)
    {
        ret = pfnRDBSSTable->AcquireForModWrite(FileObject, EndingOffset, ResourceToRelease, DeviceObject);
        Log(("VBOXSF: VBoxMrxAcquireForModWrite: Returned 0x%x\n", ret));
    }
    return ret;
}

NTSTATUS VBoxMrxReleaseForModWrite (IN struct _FILE_OBJECT *FileObject, IN struct _ERESOURCE *ResourceToRelease, IN struct _DEVICE_OBJECT *DeviceObject)
{
    NTSTATUS ret = STATUS_NOT_IMPLEMENTED;

    if (pfnRDBSSTable && pfnRDBSSTable->ReleaseForModWrite)
    {
        ret = pfnRDBSSTable->ReleaseForModWrite(FileObject, ResourceToRelease, DeviceObject);
        Log(("VBOXSF: VBoxMrxReleaseForModWrite: Returned 0x%x\n", ret));
    }
    return ret;
}

NTSTATUS VBoxMrxAcquireForCcFlush (IN struct _FILE_OBJECT *FileObject, IN struct _DEVICE_OBJECT *DeviceObject)
{
    NTSTATUS ret = STATUS_NOT_IMPLEMENTED;

    if (pfnRDBSSTable && pfnRDBSSTable->AcquireForCcFlush)
    {
        ret = pfnRDBSSTable->AcquireForCcFlush(FileObject, DeviceObject);
        Log(("VBOXSF: VBoxMrxAcquireForCcFlush: Returned 0x%x\n", ret));
    }
    return ret;
}

NTSTATUS VBoxMrxReleaseForCcFlush (IN struct _FILE_OBJECT *FileObject, IN struct _DEVICE_OBJECT *DeviceObject)
{
    NTSTATUS ret = STATUS_NOT_IMPLEMENTED;

    if (pfnRDBSSTable && pfnRDBSSTable->ReleaseForCcFlush)
    {
        ret = pfnRDBSSTable->ReleaseForCcFlush(FileObject, DeviceObject);
        Log(("VBOXSF: VBoxMrxReleaseForCcFlush: Returned 0x%x\n", ret));
    }
    return ret;
}

BOOLEAN VBoxMrxFastIoReadCompressed (IN struct _FILE_OBJECT *FileObject, IN PLARGE_INTEGER FileOffset, IN ULONG Length, IN ULONG LockKey, OUT PVOID Buffer, OUT PMDL *MdlChain,
                                     OUT PIO_STATUS_BLOCK IoStatus, OUT struct _COMPRESSED_DATA_INFO *CompressedDataInfo, IN ULONG CompressedDataInfoLength, IN struct _DEVICE_OBJECT *DeviceObject)
{
    BOOLEAN ret = FALSE;

    if (pfnRDBSSTable && pfnRDBSSTable->FastIoReadCompressed)
    {
        ret = pfnRDBSSTable->FastIoReadCompressed(FileObject, FileOffset, Length, LockKey, Buffer, MdlChain, IoStatus, CompressedDataInfo, CompressedDataInfoLength, DeviceObject);
        Log(("VBOXSF: VBoxMrxFastIoReadCompressed: Returned 0x%x\n", ret));
    }
    return ret;
}

BOOLEAN VBoxMrxFastIoWriteCompressed (IN struct _FILE_OBJECT *FileObject, IN PLARGE_INTEGER FileOffset, IN ULONG Length, IN ULONG LockKey, IN PVOID Buffer, OUT PMDL *MdlChain,
                                      OUT PIO_STATUS_BLOCK IoStatus, IN struct _COMPRESSED_DATA_INFO *CompressedDataInfo, IN ULONG CompressedDataInfoLength, IN struct _DEVICE_OBJECT *DeviceObject)
{
    BOOLEAN ret = FALSE;

    if (pfnRDBSSTable && pfnRDBSSTable->FastIoWriteCompressed)
    {
        ret = pfnRDBSSTable->FastIoWriteCompressed(FileObject, FileOffset, Length, LockKey, Buffer, MdlChain, IoStatus, CompressedDataInfo, CompressedDataInfoLength, DeviceObject);
        Log(("VBOXSF: VBoxMrxFastIoWriteCompressed: Returned 0x%x\n", ret));
    }
    return ret;
}

BOOLEAN VBoxMrxMdlReadCompleteCompressed (IN struct _FILE_OBJECT *FileObject, IN PMDL MdlChain, IN struct _DEVICE_OBJECT *DeviceObject)
{
    BOOLEAN ret = FALSE;

    if (pfnRDBSSTable && pfnRDBSSTable->MdlReadCompleteCompressed)
    {
        ret = pfnRDBSSTable->MdlReadCompleteCompressed(FileObject, MdlChain, DeviceObject);
        Log(("VBOXSF: VBoxMrxMdlReadCompleteCompressed: Returned 0x%x\n", ret));
    }
    return ret;
}

BOOLEAN VBoxMrxMdlWriteCompleteCompressed (IN struct _FILE_OBJECT *FileObject, IN PLARGE_INTEGER FileOffset, IN PMDL MdlChain, IN struct _DEVICE_OBJECT *DeviceObject)
{
    BOOLEAN ret = FALSE;

    if (pfnRDBSSTable && pfnRDBSSTable->MdlWriteCompleteCompressed)
    {
        ret = pfnRDBSSTable->MdlWriteCompleteCompressed(FileObject, FileOffset, MdlChain, DeviceObject);
        Log(("VBOXSF: VBoxMrxMdlWriteCompleteCompressed: Returned 0x%x\n", ret));
    }
    return ret;
}

BOOLEAN VBoxMrxFastIoQueryOpen (IN struct _IRP *Irp, OUT PFILE_NETWORK_OPEN_INFORMATION NetworkInformation, IN struct _DEVICE_OBJECT *DeviceObject)
{
    BOOLEAN ret = FALSE;

    if (pfnRDBSSTable && pfnRDBSSTable->FastIoQueryOpen)
    {
        ret = pfnRDBSSTable->FastIoQueryOpen(Irp, NetworkInformation, DeviceObject);
        Log(("VBOXSF: VBoxMrxFastIoQueryOpen: Returned 0x%x\n", ret));
    }
    return ret;
}


static FAST_IO_DISPATCH dispatchTable =
{
    sizeof(FAST_IO_DISPATCH),
    VBoxMrxFastIoCheckIfPossible,
    VBoxMrxFastIoRead,
    VBoxMrxFastIoWrite,
    VBoxMrxFastIoQueryBasicInfo,
    VBoxMrxFastIoQueryStandardInfo,
    VBoxMrxFastIoLock,
    VBoxMrxFastIoUnlockSingle,
    VBoxMrxFastIoUnlockAll,
    VBoxMrxFastIoUnlockAllByKey,
    VBoxMrxFastIoDeviceControl,
    VBoxMrxAcquireFileForNtCreateSection,
    VBoxMrxReleaseFileForNtCreateSection,
    VBoxMrxFastIoDetachDevice,
    VBoxMrxFastIoQueryNetworkOpenInfo,
    VBoxMrxAcquireForModWrite,
    VBoxMrxMdlRead,
    VBoxMrxMdlReadComplete,
    VBoxMrxPrepareMdlWrite,
    VBoxMrxMdlWriteComplete,
    VBoxMrxFastIoReadCompressed,
    VBoxMrxFastIoWriteCompressed,
    VBoxMrxMdlReadCompleteCompressed,
    VBoxMrxMdlWriteCompleteCompressed,
    VBoxMrxFastIoQueryOpen,
    VBoxMrxReleaseForModWrite,
    VBoxMrxAcquireForCcFlush,
    VBoxMrxReleaseForCcFlush
};

static BOOL vboxIsPrefixOK (const WCHAR *FilePathName, ULONG PathNameLength)
{
    BOOL PrefixOK;

    /* The FilePathName here looks like: \vboxsrv\... */
    if (PathNameLength >= 8 * sizeof (WCHAR)) /* Number of bytes in '\vboxsrv' unicode string. */
    {
        PrefixOK =  (FilePathName[0] == L'\\');
        PrefixOK &= (FilePathName[1] == L'V') || (FilePathName[1] == L'v');
        PrefixOK &= (FilePathName[2] == L'B') || (FilePathName[2] == L'b');
        PrefixOK &= (FilePathName[3] == L'O') || (FilePathName[3] == L'o');
        PrefixOK &= (FilePathName[4] == L'X') || (FilePathName[4] == L'x');
        PrefixOK &= (FilePathName[5] == L'S') || (FilePathName[5] == L's');
        /* Both vboxsvr & vboxsrv are now accepted */
        if ((FilePathName[6] == L'V') || (FilePathName[6] == L'v'))
        {
            PrefixOK &= (FilePathName[6] == L'V') || (FilePathName[6] == L'v');
            PrefixOK &= (FilePathName[7] == L'R') || (FilePathName[7] == L'r');
        }
        else
        {
            PrefixOK &= (FilePathName[6] == L'R') || (FilePathName[6] == L'r');
            PrefixOK &= (FilePathName[7] == L'V') || (FilePathName[7] == L'v');
        }
        if (PathNameLength > 8 * sizeof (WCHAR))
        {
            /* There is something after '\vboxsrv'. */
            PrefixOK &= (FilePathName[8] == L'\\') || (FilePathName[8] == 0);
        }
    }
    else
    {
        PrefixOK = FALSE;
    }

    return PrefixOK;
}

#if 0
static BOOL vboxIsPathToIntercept (const WCHAR *FilePathName, ULONG PathNameLength)
{
    BOOL PathToIntercept = FALSE;

    /* Detect '\vboxsrv' */
    if (PathNameLength == 8 * sizeof (WCHAR)) /* Number of bytes in '\vboxsrv' unicode string. */
    {
        PathToIntercept = TRUE;
    }

    if (!PathToIntercept)
    {
        /* Detect '\vboxsrv\ipc$' */
        if (PathNameLength >= 13 * sizeof (WCHAR)) /* Number of bytes in '\vboxsrv\ipc$' unicode string. */
        {
            const WCHAR *FilePathNameSuffix = &FilePathName[8];

            PathToIntercept =  (FilePathNameSuffix[0] == L'\\');
            PathToIntercept &= (FilePathNameSuffix[1] == L'I') || (FilePathNameSuffix[1] == L'i');
            PathToIntercept &= (FilePathNameSuffix[2] == L'P') || (FilePathNameSuffix[2] == L'p');
            PathToIntercept &= (FilePathNameSuffix[3] == L'C') || (FilePathNameSuffix[3] == L'c');
            PathToIntercept &= (FilePathNameSuffix[4] == L'$') || (FilePathNameSuffix[4] == L'$');
            if (PathNameLength > 13 * sizeof (WCHAR))
            {
                /* There is something after '\vboxsrv\IPC$'. */
                PathToIntercept &= (FilePathNameSuffix[5] == L'\\') || (FilePathNameSuffix[5] == 0);
            }
        }
    }

    return PathToIntercept;
}
#endif

static NTSTATUS VBoxMRXDeviceControl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    NTSTATUS Status = STATUS_SUCCESS;

    QUERY_PATH_REQUEST *pReq = NULL;
    QUERY_PATH_REQUEST_EX *pReqEx = NULL;
    QUERY_PATH_RESPONSE *pResp = NULL;

    BOOL PrefixOK = FALSE;

    PIO_STACK_LOCATION pStack = IoGetCurrentIrpStackLocation(pIrp);

    /* Make a local copy, it will be needed after the Irp completion. */
    ULONG IoControlCode = pStack->Parameters.DeviceIoControl.IoControlCode;

    PMRX_VBOX_DEVICE_EXTENSION pDeviceExtension = (PMRX_VBOX_DEVICE_EXTENSION)((PBYTE)pDevObj + sizeof(RDBSS_DEVICE_OBJECT));

    Log(("VBoxSF::VBoxMRXDeviceControl: pDevObj %p, pDeviceExtension %p, code %x\n",
         pDevObj, pDevObj->DeviceExtension, IoControlCode));

    switch (IoControlCode)
    {
        case IOCTL_REDIR_QUERY_PATH_EX: /* Vista */
        case IOCTL_REDIR_QUERY_PATH:    /* XP and earlier */
        {
            /* This IOCTL is intercepted for 2 reasons:
             * 1) Claim the vboxsvr and vboxsrv prefixes. All name-based operations for them
             *    will be routed to the VBox provider automatically without any prefix resolution
             *    since the prefix is already in the prefix cache.
             * 2) Reject other prefixes immediately to speed up the UNC path resolution a bit,
             *    because RDBSS will not be involved then.
             */

            const WCHAR *FilePathName = NULL;
            ULONG PathNameLength = 0;

            if (pIrp->RequestorMode != KernelMode)
            {
                /* MSDN: Network redirectors should only honor kernel-mode senders of this IOCTL, by verifying
                 * that RequestorMode member of the IRP structure is KernelMode.
                 */
                Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_REDIR_QUERY_PATH(_EX): not kernel mode!!!\n",
                      pStack->Parameters.DeviceIoControl.InputBufferLength));
                /* Continue to RDBSS. */
                break;
            }

            if (IoControlCode == IOCTL_REDIR_QUERY_PATH)
            {
                Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_REDIR_QUERY_PATH: Called (pid %x).\n", IoGetCurrentProcess()));

                if (pStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(QUERY_PATH_REQUEST))
                {
                    Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_REDIR_QUERY_PATH: short input buffer %d.\n",
                          pStack->Parameters.DeviceIoControl.InputBufferLength));
                    /* Continue to RDBSS. */
                    break;
                }

                pReq = (QUERY_PATH_REQUEST *)pStack->Parameters.DeviceIoControl.Type3InputBuffer;

                Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: PathNameLength = %d.\n", pReq->PathNameLength));
                Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: SecurityContext = %p.\n", pReq->SecurityContext));
                Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: FilePathName = %.*ls.\n", pReq->PathNameLength / sizeof (WCHAR), pReq->FilePathName));

                FilePathName = pReq->FilePathName;
                PathNameLength = pReq->PathNameLength;
            }
            else
            {
                Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_REDIR_QUERY_PATH_EX: Called.\n"));

                if (pStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(QUERY_PATH_REQUEST_EX))
                {
                    Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: IOCTL_REDIR_QUERY_PATH_EX: short input buffer %d.\n",
                          pStack->Parameters.DeviceIoControl.InputBufferLength));
                    /* Continue to RDBSS. */
                    break;
                }

                pReqEx = (QUERY_PATH_REQUEST_EX *)pStack->Parameters.DeviceIoControl.Type3InputBuffer;

                Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: pSecurityContext = %p.\n", pReqEx->pSecurityContext));
                Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: EaLength = %d.\n", pReqEx->EaLength));
                Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: pEaBuffer = %p.\n", pReqEx->pEaBuffer));
                Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: PathNameLength = %d.\n", pReqEx->PathName.Length));
                Log(("VBOXSF: VBoxMRxDevFcbXXXControlFile: FilePathName = %.*ls.\n", pReqEx->PathName.Length / sizeof (WCHAR), pReqEx->PathName.Buffer));

                FilePathName = pReqEx->PathName.Buffer;
                PathNameLength = pReqEx->PathName.Length;
            }

            pResp = (QUERY_PATH_RESPONSE *)pIrp->UserBuffer;

            PrefixOK = vboxIsPrefixOK (FilePathName, PathNameLength);
            Log(("VBoxSF::VBoxMRXDeviceControl PrefixOK %d\n", PrefixOK));

            if (!PrefixOK)
            {
                /* Immediately fail the IOCTL with STATUS_BAD_NETWORK_NAME as recommended by MSDN.
                 * No need to involve RDBSS.
                 */
                Status = STATUS_BAD_NETWORK_NAME;

                pIrp->IoStatus.Status = Status;
                pIrp->IoStatus.Information = 0;

                IoCompleteRequest(pIrp, IO_NO_INCREMENT);

                Log(("VBoxSF::VBoxMRXDeviceControl: returned STATUS_BAD_NETWORK_NAME\n"));
                return Status;
            }

            Log(("VBoxSF::VBoxMRXDeviceControl pResp %p verifying the path.\n", pResp));
            if (pResp)
            {
                /* Always claim entire \vboxsrv prefix. The LengthAccepted initially is equal to entire path.
                 * Here it is assigned to the length of \vboxsrv prefix.
                 */
                pResp->LengthAccepted = 8 * sizeof (WCHAR);

                Status = STATUS_SUCCESS;

                pIrp->IoStatus.Status = Status;
                pIrp->IoStatus.Information = 0;

                IoCompleteRequest(pIrp, IO_NO_INCREMENT);

                Log(("VBoxSF::VBoxMRXDeviceControl: claiming the path.\n"));
                return Status;
            }

            /* No pResp pointer, should not happen. Just a precaution. */
            Status = STATUS_INVALID_PARAMETER;

            pIrp->IoStatus.Status = Status;
            pIrp->IoStatus.Information = 0;

            IoCompleteRequest(pIrp, IO_NO_INCREMENT);

            Log(("VBoxSF::VBoxMRXDeviceControl: returned STATUS_INVALID_PARAMETER\n"));
            return Status;
        }

        default:
            break;
    }

    /* Pass the IOCTL to RDBSS. */
    if (pDeviceExtension && pDeviceExtension->pfnRDBSSDeviceControl)
    {
        Log(("VBoxSF::VBoxMRXDeviceControl calling RDBSS %p\n", pDeviceExtension->pfnRDBSSDeviceControl));
        Status = pDeviceExtension->pfnRDBSSDeviceControl (pDevObj, pIrp);
        Log(("VBoxSF::VBoxMRXDeviceControl RDBSS status %x\n", Status));
    }
    else
    {
        /* No RDBSS, should not happen. Just a precaution. */
        Status = STATUS_NOT_IMPLEMENTED;

        pIrp->IoStatus.Status = Status;
        pIrp->IoStatus.Information = 0;

        IoCompleteRequest(pIrp, IO_NO_INCREMENT);

        Log(("VBoxSF::VBoxMRXDeviceControl: returned STATUS_NOT_IMPLEMENTED\n"));
    }

    return Status;
}


NTSTATUS DriverEntry (IN PDRIVER_OBJECT  DriverObject,
                      IN PUNICODE_STRING RegistryPath)
/*++

 Routine Description:

 This is the initialization routine for the mini redirector

 Arguments:

 DriverObject - Pointer to driver object created by the system.

 Return Value:

 RXSTATUS - The function value is the final status from the initialization
 operation.

 --*/
{
    NTSTATUS Status;
    ULONG Controls = 0;
    MRX_VBOX_INIT_STATES VBoxMRxInitState = MRX_VBOXINIT_UNINT;
    UNICODE_STRING VBoxMRxName;
    UNICODE_STRING UserModeDeviceName;
    PMRX_VBOX_DEVICE_EXTENSION pDeviceExtension;
    ULONG i;
#ifdef VBOX
    int vboxRC;
    VBSFCLIENT hgcmClient =
    {   0};
#endif

    Log(("VBOXSF: DriverEntry: Driver object 0x%08lx loaded!\n", DriverObject));

#ifdef VBOX
    /* Initialize VBox subsystem. */
    vboxRC = vboxInit();
    if (RT_FAILURE(vboxRC))
    {
        Status = STATUS_UNSUCCESSFUL;
        Log(("VBOXSF: DriverEntry: ERROR while initializing Vbox subsystem (%Rrc)!\n", vboxRC));
        return Status;
    }

    /* Set filter mask */
    if (vboxCtlGuestFilterMask (VMMDEV_EVENT_HGCM, 0)) /** @todo r=bird: Why are we doing this? And more importantly, why are we clearing it further down. We're not the only HGCM user, are we? */
    {
        Log (("VBOXSF: DriverEntry: ERROR while setting guest event filter mask!\n"));
        return STATUS_UNSUCCESSFUL;
    }

    /* Connect the HGCM client */
    vboxRC = vboxConnect(&hgcmClient);
    if (RT_FAILURE (vboxRC))
    {
        Status = STATUS_UNSUCCESSFUL;
        Log(("VBOXSF: DriverEntry: ERROR while connecting to host (%Rrc)!\n", vboxRC));
        vboxCtlGuestFilterMask (0, VMMDEV_EVENT_HGCM);
        vboxUninit();
        return Status;
    }
#endif

    if (DriverObject == NULL)
    {
        Status = STATUS_UNSUCCESSFUL;
        Log(("VBOXSF: DriverEntry: Invalid driver object detected!\n"));
        vboxCtlGuestFilterMask(0, VMMDEV_EVENT_HGCM);
        vboxUninit();
        return Status;
    }

    //
    //  Setup Unload Routine
    //

    DriverObject->DriverUnload = VBoxMRxUnload;
    DriverObject->FastIoDispatch = NULL;
    DriverObject->DriverStartIo = NULL;

    //
    //setup the driver dispatch for people who come in here directly....like the browser
    //

    for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
    {
        DriverObject->MajorFunction[i] = (PDRIVER_DISPATCH)VBoxMRxFsdDispatch;
    }

#ifdef VBOX_SF_STATIC_LIB
    Status = RxDriverEntry(DriverObject, RegistryPath);
    if (Status != STATUS_SUCCESS)
    {
        Log(("VBOXSF: DriverEntry: Wrapper failed to initialize. Status = 0x%08lx\n", Status));
        goto failure;
    }
#endif

    try
    {
        VBoxMRxInitState = MRX_VBOXINIT_START;

        //
        //  Register this minirdr with the connection engine. Registration makes the connection
        //  engine aware of the device name, driver object, and other characteristics.
        //  If registration is successful, a new device object is returned
        //
        //


        RtlInitUnicodeString(&VBoxMRxName, DD_MRX_VBOX_FS_DEVICE_NAME_U);
        /* don't set RX_REGISTERMINI_FLAG_DONT_PROVIDE_UNCS or else UNC mappings don't work (including Windows explorer browsing) */

        /** @todo set RX_REGISTERMINI_FLAG_DONT_INIT_PREFIX_N_SCAVENGER and the system will crash on boot */
        SetFlag(Controls,RX_REGISTERMINI_FLAG_DONT_PROVIDE_MAILSLOTS);

        Status = VBoxRxRegisterMinirdr(
                &VBoxMRxDeviceObject, // where the new device object goes
                DriverObject, // the Driver Object to register
                &VBoxMRxDispatch, // the dispatch table for this driver
                Controls, // dont register with unc and for mailslots
                &VBoxMRxName, // the device name for this minirdr
                sizeof(MRX_VBOX_DEVICE_EXTENSION), // IN ULONG DeviceExtensionSize,
                FILE_DEVICE_NETWORK_FILE_SYSTEM, // IN ULONG DeviceType - disk ?
                FILE_REMOTE_DEVICE // IN  ULONG DeviceCharacteristics
        );

        if (Status!=STATUS_SUCCESS)
        {
            Log(("VBOXSF: DriverEntry: Registering mini reader failed: 0x%08lx!\n", Status ));
            try_return(Status);
        }

        //
        //  Init the device extension data
        //  NOTE: the device extension actually points to fields
        //  in the RDBSS_DEVICE_OBJECT. Our space is past the end
        //  of this struct !!
        //

        pDeviceExtension = (PMRX_VBOX_DEVICE_EXTENSION)
        ((PBYTE)(VBoxMRxDeviceObject) + sizeof(RDBSS_DEVICE_OBJECT));

        pDeviceExtension->pDeviceObject = VBoxMRxDeviceObject;

        // initialize local connection list
        for (i = 0; i < RTL_NUMBER_OF(pDeviceExtension->cLocalConnections); i++)
        {
            pDeviceExtension->cLocalConnections[i] = FALSE;
        }
        // Mutex for synchronizining our connection list
        ExInitializeFastMutex( &pDeviceExtension->mtxLocalCon );

        // The device object has been created. Need to setup a symbolic
        // link so that the device may be accessed from a Win32 user mode
        // application.

        RtlInitUnicodeString(&UserModeDeviceName, DD_MRX_VBOX_USERMODE_SHADOW_DEV_NAME_U);
        Log(("VBOXSF: DriverEntry: Calling IoCreateSymbolicLink ...\n"));
        Status = IoCreateSymbolicLink( &UserModeDeviceName, &VBoxMRxName);
        if (Status != STATUS_SUCCESS)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Failed calling IoCreateSymbolicLink: 0x%08lx\n", Status ));
            try_return(Status);
        }
        Log(("VBOXSF: DriverEntry: Symbolic link created.\n"));

        VBoxMRxInitState = MRX_VBOXINIT_MINIRDR_REGISTERED;

        //
        // Build the dispatch tables for the minirdr
        //

        Status = VBoxMRxInitializeTables();

        if (!NT_SUCCESS( Status ))
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: VBoxMRxInitializeTables failed: 0x%08lx\n", Status ));
            try_return(Status);
        }

        //
        // Get information from the registry
        //
        VBoxMRxReadRegistryParameters();

        Log(("VBOXSF: VBoxMRxDriverEntry: First phase done!\n"));

        try_exit:
        NOTHING;
    }
    finally
    {
        if (Status != STATUS_SUCCESS)
        {
            VBoxMRxInitUnwind(DriverObject,VBoxMRxInitState);
        }
    }

    if (Status != STATUS_SUCCESS)
    {

        Log(("VBOXSF: VBoxMRxDriverEntry: VBoxSF.sys failed to start with Status = 0x%08lx, VBoxMRxInitState = 0x%08lx\n", Status, VBoxMRxInitState));
        goto failure;
    }

#ifdef VBOX
    pDeviceExtension->pDriverObject = DriverObject;
    pDeviceExtension->hgcmClient = hgcmClient;
#endif

    Log(("VBOXSF: VBoxMRxDriverEntry: FastIoDispatch address = 0x%x, DriverStartIo address = 0x%x\n", DriverObject->FastIoDispatch, DriverObject->DriverStartIo));

    /* Wrap the fast io path */
    if (DriverObject->FastIoDispatch)
    {
        Log(("VBOXSF: VBoxMRxDriverEntry: SizeOfFastIoDispatch = 0x%x (expected = 0x%x)\n", DriverObject->FastIoDispatch->SizeOfFastIoDispatch, sizeof(dispatchTable)));
        pfnRDBSSTable = DriverObject->FastIoDispatch;
        DriverObject->FastIoDispatch = &dispatchTable;

        if (pfnRDBSSTable->FastIoCheckIfPossible == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: FastIoCheckIfPossible == NULL\n"));
            dispatchTable.FastIoCheckIfPossible = NULL;
        }
        if (pfnRDBSSTable->FastIoRead == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: FastIoRead == NULL\n"));
            dispatchTable.FastIoRead = NULL;
        }
        if (pfnRDBSSTable->FastIoWrite == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: FastIoWrite == NULL\n"));
            dispatchTable.FastIoWrite = NULL;
        }
        if (pfnRDBSSTable->FastIoQueryBasicInfo == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: FastIoQueryBasicInfo == NULL\n"));
            dispatchTable.FastIoQueryBasicInfo = NULL;
        }
        if (pfnRDBSSTable->FastIoQueryStandardInfo == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: FastIoQueryStandardInfo == NULL\n"));
            dispatchTable.FastIoQueryStandardInfo = NULL;
        }
        if (pfnRDBSSTable->FastIoLock == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: FastIoLock == NULL\n"));
            dispatchTable.FastIoLock = NULL;
        }
        if (pfnRDBSSTable->FastIoUnlockSingle == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: FastIoUnlockSingle == NULL\n"));
            dispatchTable.FastIoUnlockSingle = NULL;
        }
        if (pfnRDBSSTable->FastIoUnlockAll == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: FastIoUnlockAll == NULL\n"));
            dispatchTable.FastIoUnlockAll = NULL;
        }
        if (pfnRDBSSTable->FastIoUnlockAllByKey == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: FastIoUnlockAllByKey == NULL\n"));
            dispatchTable.FastIoUnlockAllByKey = NULL;
        }
        if (pfnRDBSSTable->FastIoDeviceControl == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: FastIoDeviceControl == NULL\n"));
            dispatchTable.FastIoDeviceControl = NULL;
        }
        if (pfnRDBSSTable->AcquireFileForNtCreateSection == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: AcquireFileForNtCreateSection == NULL\n"));
            dispatchTable.AcquireFileForNtCreateSection = NULL;
        }
        if (pfnRDBSSTable->ReleaseFileForNtCreateSection == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: ReleaseFileForNtCreateSection == NULL\n"));
            dispatchTable.ReleaseFileForNtCreateSection = NULL;
        }
        if (pfnRDBSSTable->FastIoDetachDevice == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: FastIoDetachDevice == NULL\n"));
            dispatchTable.FastIoDetachDevice = NULL;
        }
        if (pfnRDBSSTable->FastIoQueryNetworkOpenInfo == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: FastIoQueryNetworkOpenInfo == NULL\n"));
            dispatchTable.FastIoQueryNetworkOpenInfo = NULL;
        }
        if (pfnRDBSSTable->AcquireForModWrite == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: AcquireForModWrite == NULL\n"));
            dispatchTable.AcquireForModWrite = NULL;
        }
        if (pfnRDBSSTable->MdlRead == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: MdlRead == NULL\n"));
            dispatchTable.MdlRead = NULL;
        }
        if (pfnRDBSSTable->MdlReadComplete == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: MdlReadComplete == NULL\n"));
            dispatchTable.MdlReadComplete = NULL;
        }
        if (pfnRDBSSTable->PrepareMdlWrite == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: PrepareMdlWrite == NULL\n"));
            dispatchTable.PrepareMdlWrite = NULL;
        }
        if (pfnRDBSSTable->MdlWriteComplete == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: MdlWriteComplete == NULL\n"));
            dispatchTable.MdlWriteComplete = NULL;
        }
        if (pfnRDBSSTable->FastIoReadCompressed == NULL)
        {
            Log(("VVBOXSF: VBoxMRxDriverEntry: Function Table: FastIoReadCompressed == NULL\n"));
            dispatchTable.FastIoReadCompressed = NULL;
        }
        if (pfnRDBSSTable->FastIoWriteCompressed == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: FastIoWriteCompressed == NULL\n"));
            dispatchTable.FastIoWriteCompressed = NULL;
        }
        if (pfnRDBSSTable->MdlReadCompleteCompressed == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: MdlReadCompleteCompressed == NULL\n"));
            dispatchTable.MdlReadCompleteCompressed = NULL;
        }
        if (pfnRDBSSTable->MdlWriteCompleteCompressed == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: MdlWriteCompleteCompressed == NULL\n"));
            dispatchTable.MdlWriteCompleteCompressed = NULL;
        }
        if (pfnRDBSSTable->FastIoQueryOpen == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: FastIoQueryOpen == NULL\n"));
            dispatchTable.FastIoQueryOpen = NULL;
        }
        if (pfnRDBSSTable->ReleaseForModWrite == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: ReleaseForModWrite == NULL\n"));
            dispatchTable.ReleaseForModWrite = NULL;
        }
        if (pfnRDBSSTable->AcquireForCcFlush == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: AcquireForCcFlush == NULL\n"));
            dispatchTable.AcquireForCcFlush = NULL;
        }
        if (pfnRDBSSTable->ReleaseForCcFlush == NULL)
        {
            Log(("VBOXSF: VBoxMRxDriverEntry: Function Table: ReleaseForCcFlush == NULL\n"));
            dispatchTable.ReleaseForCcFlush = NULL;
        }
    }

    /* The redirector driver must intercept the IOCTL to avoid VBOXSVR name resolution
     * by other redirectors. These additional name resolutions cause long delays.
     */
    Log(("VBOXSF: VBoxMRxDriverEntry: VBoxMRxDeviceObject = %p, rdbss %p, devext %p\n",
         VBoxMRxDeviceObject, DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL], pDeviceExtension));
    pDeviceExtension->pfnRDBSSDeviceControl = DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL];
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = VBoxMRXDeviceControl;

    Log(("VBOXSF: VBoxMRxDriverEntry: Init successful!\n"));
    return STATUS_SUCCESS;

    failure:

#ifdef VBOX
    Log(("VBOXSF: VBoxMRxDriverEntry: Failure! Status = 0x%08X\n", Status));

    vboxDisconnect (&hgcmClient);
    vboxCtlGuestFilterMask (0, VMMDEV_EVENT_HGCM);
    vboxUninit();
#endif
    VBoxMRxInitUnwind(DriverObject, VBoxMRxInitState);

    return Status;
}

VOID VBoxMRxInitUnwind (IN PDRIVER_OBJECT DriverObject, IN MRX_VBOX_INIT_STATES VBoxMRxInitState)
/*++

 Routine Description:

 This routine does the common uninit work for unwinding from a bad driver entry or for unloading.

 Arguments:

 VBoxMRxInitState - tells how far we got into the initialization

 Return Value:

 None

 --*/

{
    PAGED_CODE();

    Log(("VBOXSF: VBoxMRxInitUnwind: VBoxMRxInitState = 0x%x\n", VBoxMRxInitState));
    switch (VBoxMRxInitState)
    {
    case MRX_VBOXINIT_ALL_INITIALIZATION_COMPLETED:

        //Nothing extra to do...this is just so that the constant in RxUnload doesn't change.......
        //lack of break intentional

    case MRX_VBOXINIT_MINIRDR_REGISTERED:
        VBoxRxUnregisterMinirdr(VBoxMRxDeviceObject);

        //lack of break intentional

    case MRX_VBOXINIT_START:
        break;
    }

}

VOID VBoxMRxUnload (IN PDRIVER_OBJECT DriverObject)
/*++

 Routine Description:

 This is the unload routine for the Exchange mini redirector.

 Arguments:

 DriverObject - pointer to the driver object for the VBoxMRx

 Return Value:

 None

 --*/

{
    PRX_CONTEXT RxContext;
    NTSTATUS Status;
    UNICODE_STRING UserModeDeviceName;
    PMRX_VBOX_DEVICE_EXTENSION pDeviceExtension;

    PAGED_CODE();

    Log(("VBOXSF: VBoxMRxUnload: Called.\n"));
#ifdef VBOX
    pDeviceExtension = (PMRX_VBOX_DEVICE_EXTENSION)
    ((PBYTE)(VBoxMRxDeviceObject) + sizeof(RDBSS_DEVICE_OBJECT));

    vboxDisconnect (&pDeviceExtension->hgcmClient);
    vboxUninit ();
    vboxCtlGuestFilterMask (0, VMMDEV_EVENT_HGCM);
#endif

    VBoxMRxInitUnwind(DriverObject, MRX_VBOXINIT_ALL_INITIALIZATION_COMPLETED);
    RxContext = VBoxRxCreateRxContext(NULL, VBoxMRxDeviceObject, RX_CONTEXT_FLAG_IN_FSP);

    if (RxContext != NULL)
    {
        Status = VBoxRxStopMinirdr(RxContext, &RxContext->PostRequest);

        if (Status == STATUS_SUCCESS)
        {
            MRX_VBOX_STATE State;

            State = (MRX_VBOX_STATE)InterlockedCompareExchange((LONG *)&VBoxMRxState, MRX_VBOX_STARTABLE, MRX_VBOX_STARTED);

            if (State != MRX_VBOX_STARTABLE)
            {
                Status = STATUS_REDIRECTOR_STARTED;
            }
        }

        VBoxRxDereferenceAndDeleteRxContext(RxContext);
    }
    else
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlInitUnicodeString(&UserModeDeviceName, DD_MRX_VBOX_USERMODE_SHADOW_DEV_NAME_U);
    Status = IoDeleteSymbolicLink(&UserModeDeviceName);
    if (Status != STATUS_SUCCESS)
    {
        Log(("VBOXSF: VBoxMRxUnload: Could not delete symbolic link!\n"));
    }

#ifdef VBOX_SF_STATIC_LIB
    RxUnload(DriverObject);
#endif
    Log(("VBOXSF: VBoxMRxUnload: VBoxSF.sys driver object 0x%08lx unoaded\n", DriverObject));
}

NTSTATUS VBoxMRxInitializeTables (void)
/*++

 Routine Description:

 This routine sets up the mini redirector dispatch vector and also calls
 to initialize any other tables needed.

 Return Value:

 RXSTATUS - The return status for the operation

 --*/
{
    Log(("VBOXSF: VBoxMRxInitializeTables: Called.\n"));

    //
    // Ensure that the Exchange mini redirector context satisfies the size constraints
    //
    //ASSERT(sizeof(MRX_VBOX_RX_CONTEXT) <= MRX_CONTEXT_SIZE);

    //
    // Build the local minirdr dispatch table and initialize
    //

    ZeroAndInitializeNodeType(&VBoxMRxDispatch, RDBSS_NTC_MINIRDR_DISPATCH, sizeof(MINIRDR_DISPATCH));

    //
    // null mini redirector extension sizes and allocation policies.
    //


    VBoxMRxDispatch.MRxFlags = (RDBSS_MANAGE_NET_ROOT_EXTENSION | RDBSS_MANAGE_FOBX_EXTENSION);

    VBoxMRxDispatch.MRxSrvCallSize = 0;
    VBoxMRxDispatch.MRxNetRootSize = sizeof(MRX_VBOX_NETROOT_EXTENSION);
    VBoxMRxDispatch.MRxVNetRootSize = 0;
    VBoxMRxDispatch.MRxFcbSize = 0;
    VBoxMRxDispatch.MRxSrvOpenSize = 0;
    VBoxMRxDispatch.MRxFobxSize = sizeof(MRX_VBOX_FOBX);

    // Mini redirector cancel routine ..

    VBoxMRxDispatch.MRxCancel = NULL;

    //
    // Mini redirector Start/Stop. Each mini-rdr can be started or stopped
    // while the others continue to operate.
    //

    VBoxMRxDispatch.MRxStart = VBoxMRxStart;
    VBoxMRxDispatch.MRxStop = VBoxMRxStop;
    VBoxMRxDispatch.MRxDevFcbXXXControlFile = VBoxMRxDevFcbXXXControlFile;

    //
    // Mini redirector name resolution.
    //

    VBoxMRxDispatch.MRxCreateSrvCall = VBoxMRxCreateSrvCall;
    VBoxMRxDispatch.MRxSrvCallWinnerNotify = VBoxMRxSrvCallWinnerNotify;
    VBoxMRxDispatch.MRxCreateVNetRoot = VBoxMRxCreateVNetRoot;
    VBoxMRxDispatch.MRxUpdateNetRootState = VBoxMRxUpdateNetRootState;
    VBoxMRxDispatch.MRxExtractNetRootName = VBoxMRxExtractNetRootName;
    VBoxMRxDispatch.MRxFinalizeSrvCall = VBoxMRxFinalizeSrvCall;
    VBoxMRxDispatch.MRxFinalizeNetRoot = VBoxMRxFinalizeNetRoot;
    VBoxMRxDispatch.MRxFinalizeVNetRoot = VBoxMRxFinalizeVNetRoot;

    //
    // File System Object Creation/Deletion.
    //

    VBoxMRxDispatch.MRxCreate = VBoxMRxCreate;
    VBoxMRxDispatch.MRxCollapseOpen = VBoxMRxCollapseOpen;
    VBoxMRxDispatch.MRxShouldTryToCollapseThisOpen = VBoxMRxShouldTryToCollapseThisOpen;
    VBoxMRxDispatch.MRxExtendForCache = VBoxMRxExtendStub;
    VBoxMRxDispatch.MRxExtendForNonCache = VBoxMRxExtendStub;
    VBoxMRxDispatch.MRxTruncate = VBoxMRxTruncate;
    VBoxMRxDispatch.MRxCleanupFobx = VBoxMRxCleanupFobx;
    VBoxMRxDispatch.MRxCloseSrvOpen = VBoxMRxCloseSrvOpen;
    VBoxMRxDispatch.MRxFlush = VBoxMRxFlush;
    VBoxMRxDispatch.MRxForceClosed = VBoxMRxForcedClose;
    VBoxMRxDispatch.MRxDeallocateForFcb = VBoxMRxDeallocateForFcb;
    VBoxMRxDispatch.MRxDeallocateForFobx = VBoxMRxDeallocateForFobx;

    //
    // File System Objects query/Set
    //

    VBoxMRxDispatch.MRxQueryDirectory = VBoxMRxQueryDirectory;
    VBoxMRxDispatch.MRxQueryVolumeInfo = VBoxMRxQueryVolumeInformation;
    VBoxMRxDispatch.MRxQueryEaInfo = VBoxMRxQueryEaInformation;
    VBoxMRxDispatch.MRxSetEaInfo = VBoxMRxSetEaInformation;
    VBoxMRxDispatch.MRxQuerySdInfo = VBoxMRxQuerySecurityInformation;
    VBoxMRxDispatch.MRxSetSdInfo = VBoxMRxSetSecurityInformation;
    VBoxMRxDispatch.MRxQueryFileInfo = VBoxMRxQueryFileInformation;
    VBoxMRxDispatch.MRxSetFileInfo = VBoxMRxSetFileInformation;
    VBoxMRxDispatch.MRxSetFileInfoAtCleanup = VBoxMRxSetFileInformationAtCleanup;

    //
    // Buffering state change
    //

    VBoxMRxDispatch.MRxComputeNewBufferingState = VBoxMRxComputeNewBufferingState;

    //
    // File System Object I/O
    //

    VBoxMRxDispatch.MRxLowIOSubmit[LOWIO_OP_READ] = VBoxMRxRead;
    VBoxMRxDispatch.MRxLowIOSubmit[LOWIO_OP_WRITE] = VBoxMRxWrite;
    VBoxMRxDispatch.MRxLowIOSubmit[LOWIO_OP_SHAREDLOCK] = VBoxMRxLocks;
    VBoxMRxDispatch.MRxLowIOSubmit[LOWIO_OP_EXCLUSIVELOCK] = VBoxMRxLocks;
    VBoxMRxDispatch.MRxLowIOSubmit[LOWIO_OP_UNLOCK] = VBoxMRxLocks;
    VBoxMRxDispatch.MRxLowIOSubmit[LOWIO_OP_UNLOCK_MULTIPLE] = VBoxMRxLocks;
    VBoxMRxDispatch.MRxLowIOSubmit[LOWIO_OP_FSCTL] = VBoxMRxFsCtl;
    VBoxMRxDispatch.MRxLowIOSubmit[LOWIO_OP_IOCTL] = VBoxMRxIoCtl;

    VBoxMRxDispatch.MRxLowIOSubmit[LOWIO_OP_NOTIFY_CHANGE_DIRECTORY] = VBoxMRxNotifyChangeDirectory;

    //
    // Miscellaneous
    //

    VBoxMRxDispatch.MRxCompleteBufferingStateChangeRequest = VBoxMRxCompleteBufferingStateChangeRequest;

    Log(("VBOXSF: VBoxMRxInitializeTables: Success.\n"));
    return (STATUS_SUCCESS);
}

NTSTATUS VBoxMRxStart (PRX_CONTEXT RxContext, IN OUT PRDBSS_DEVICE_OBJECT RxDeviceObject)
/*++

 Routine Description:

 This routine completes the initialization of the mini redirector fromn the
 RDBSS perspective. Note that this is different from the initialization done
 in DriverEntry. Any initialization that depends on RDBSS should be done as
 part of this routine while the initialization that is independent of RDBSS
 should be done in the DriverEntry routine.

 Arguments:

 RxContext - Supplies the Irp that was used to startup the rdbss

 Return Value:

 RXSTATUS - The return status for the operation

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    MRX_VBOX_STATE CurrentState;

    Log(("VBOXSF: VBoxMRxStart: Called.\n"));

    CurrentState = (MRX_VBOX_STATE)InterlockedCompareExchange((PLONG) & VBoxMRxState, MRX_VBOX_STARTED, MRX_VBOX_START_IN_PROGRESS);

    if (CurrentState == MRX_VBOX_START_IN_PROGRESS)
    {
        Log(("VBOXSF: VBoxMRxStart: Start in progress -> started\n"));
        Status = STATUS_SUCCESS;
    }
    else if (VBoxMRxState == MRX_VBOX_STARTED)
    {
        Log(("VBOXSF: VBoxMRxStart: Already started!\n"));
        Status = STATUS_REDIRECTOR_STARTED;
    }
    else
    {
        Log(("VBOXSF: VBoxMRxStart: Bad state! VBoxMRxState = %d\n", VBoxMRxState));
        Status = STATUS_UNSUCCESSFUL;
    }

    return Status;
}

NTSTATUS VBoxMRxStop (PRX_CONTEXT RxContext, IN OUT PRDBSS_DEVICE_OBJECT RxDeviceObject)
/*++

 Routine Description:

 This routine is used to activate the mini redirector from the RDBSS perspective

 Arguments:

 RxContext - the context that was used to start the mini redirector

 pContext  - the null mini rdr context passed in at registration time.

 Return Value:

 RXSTATUS - The return status for the operation

 --*/
{
    Log(("VBOXSF: VBoxMRxStop: Called.\n"));
    return (STATUS_SUCCESS);
}

NTSTATUS VBoxMRxInitializeSecurity (VOID)
/*++

 Routine Description:

 This routine initializes the null miniredirector security .

 Arguments:

 None.

 Return Value:

 None.

 Note:

 This API can only be called from a FS process.

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;

    Log(("VBOXSF: VBoxMRxInitializeSecurity: Called.\n"));
    return Status;
}

NTSTATUS VBoxMRxUninitializeSecurity (VOID)
/*++

 Routine Description:

 Arguments:

 None.

 Return Value:

 None.

 Note:

 This API can only be called from a FS process.

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;

    PAGED_CODE();

    Log(("VBOXSF: VBoxMRxUninitializeSecurity: Called.\n"));
    return Status;
}

NTSTATUS VBoxMRxFsdDispatch (IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)

/*++

 Routine Description:

 This routine implements the FSD dispatch for the mini DRIVER object.

 Arguments:

 DeviceObject - Supplies the device object for the packet being processed.

 Irp - Supplies the Irp being processed

 Return Value:

 RXSTATUS - The Fsd status for the Irp

 --*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    UCHAR MajorFunctionCode = IrpSp->MajorFunction;
    ULONG MinorFunctionCode = IrpSp->MinorFunction;

    Log(("VBOXSF: VBoxMRxFsdDispatch: Called.\n"));

    Assert(DeviceObject == (PDEVICE_OBJECT)VBoxMRxDeviceObject);
    if (DeviceObject != (PDEVICE_OBJECT)VBoxMRxDeviceObject)
    {
        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);

        Log(("VBOXSF: VBoxMRxFsdDispatch: Invalid device request detected, this in no error!\n"));
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    Log(("%s\n", MajorFunctionString(MajorFunctionCode, MinorFunctionCode)));

    Status = VBoxRxFsdDispatch((PRDBSS_DEVICE_OBJECT)VBoxMRxDeviceObject, Irp);
    Log(("VBOXSF: VBoxMRxFsdDispatch: Returned 0x%x\n", Status));
    return Status;
}

NTSTATUS VBoxMRxGetUlongRegistryParameter (HANDLE ParametersHandle, PWCHAR ParameterName, PULONG ParamUlong, BOOLEAN LogFailure)
/*++

 Routine Description:

 This routine is called to read a ULONG param from t he registry.

 Arguments:

 ParametersHandle - the handle of the containing registry "folder"
 ParameterName    - name of the parameter to be read
 ParamUlong       - where to store the value, if successful
 LogFailure       - if TRUE and the registry stuff fails, log an error

 Return Value:

 RXSTATUS - STATUS_SUCCESS

 --*/
{
    ULONG Storage[16];
    PKEY_VALUE_PARTIAL_INFORMATION Value;
    ULONG ValueSize;
    UNICODE_STRING UnicodeString;
    NTSTATUS Status;
    ULONG BytesRead;

    PAGED_CODE(); //INIT

    Log(("VBOXSF: VBoxMRxGetUlongRegistryParameter: Called.\n"));

    Value = (PKEY_VALUE_PARTIAL_INFORMATION)Storage;
    ValueSize = sizeof(Storage);

    RtlInitUnicodeString(&UnicodeString, ParameterName);

    Status = ZwQueryValueKey(ParametersHandle, &UnicodeString, KeyValuePartialInformation, Value, ValueSize, &BytesRead);

    if (NT_SUCCESS(Status))
    {
        if (Value->Type == REG_DWORD)
        {
            PULONG ConfigValue = (PULONG) & Value->Data[0];
            *ParamUlong = *((PULONG)ConfigValue);
            Log(("BOXSF: VBoxMRxGetUlongRegistryParameter: Read registry value %.*ls = 0x%08lx\n", UnicodeString.Length / sizeof(WCHAR), UnicodeString.Buffer, *ParamUlong));
            return (STATUS_SUCCESS);
        }
        else
        {
            Status = STATUS_INVALID_PARAMETER;
        }
    }

    if (LogFailure)
        Log(("VBOXSF: VBoxMRxGetUlongRegistryParameter: Failure!\n"));

    return Status;
}

VOID VBoxMRxReadRegistryParameters ()
{
    NTSTATUS Status;
    OBJECT_ATTRIBUTES ObjectAttributes;
    UNICODE_STRING ParametersRegistryKeyName;
    HANDLE ParametersHandle;
    ULONG Temp;

    Log(("VBOXSF: VBoxMRxReadRegistryParameters: Called.\n"));
    RtlInitUnicodeString(&ParametersRegistryKeyName, NULL_MINIRDR_PARAMETERS);
    InitializeObjectAttributes(&ObjectAttributes, &ParametersRegistryKeyName, OBJ_CASE_INSENSITIVE, NULL, NULL
    );

    Status = ZwOpenKey(&ParametersHandle, KEY_READ, &ObjectAttributes);
    if (NT_SUCCESS(Status))
    {
        Status = VBoxMRxGetUlongRegistryParameter(ParametersHandle,L"LogRate" ,
        (PULONG)&Temp,
        FALSE
        );

        ZwClose(ParametersHandle);
    }
    if (NT_SUCCESS(Status)) LogRate = Temp;
}

#if defined(DEBUG) || defined (LOG_ENABLED)
static PCHAR PnPMinorFunctionString(LONG MinorFunction)
{
    switch (MinorFunction)
    {
        case IRP_MN_START_DEVICE:
            return "IRP_MJ_PNP - IRP_MN_START_DEVICE";
        case IRP_MN_QUERY_REMOVE_DEVICE:
            return "IRP_MJ_PNP - IRP_MN_QUERY_REMOVE_DEVICE";
        case IRP_MN_REMOVE_DEVICE:
            return "IRP_MJ_PNP - IRP_MN_REMOVE_DEVICE";
        case IRP_MN_CANCEL_REMOVE_DEVICE:
            return "IRP_MJ_PNP - IRP_MN_CANCEL_REMOVE_DEVICE";
        case IRP_MN_STOP_DEVICE:
            return "IRP_MJ_PNP - IRP_MN_STOP_DEVICE";
        case IRP_MN_QUERY_STOP_DEVICE:
            return "IRP_MJ_PNP - IRP_MN_QUERY_STOP_DEVICE";
        case IRP_MN_CANCEL_STOP_DEVICE:
            return "IRP_MJ_PNP - IRP_MN_CANCEL_STOP_DEVICE";
        case IRP_MN_QUERY_DEVICE_RELATIONS:
            return "IRP_MJ_PNP - IRP_MN_QUERY_DEVICE_RELATIONS";
        case IRP_MN_QUERY_INTERFACE:
            return "IRP_MJ_PNP - IRP_MN_QUERY_INTERFACE";
        case IRP_MN_QUERY_CAPABILITIES:
            return "IRP_MJ_PNP - IRP_MN_QUERY_CAPABILITIES";
        case IRP_MN_QUERY_RESOURCES:
            return "IRP_MJ_PNP - IRP_MN_QUERY_RESOURCES";
        case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
            return "IRP_MJ_PNP - IRP_MN_QUERY_RESOURCE_REQUIREMENTS";
        case IRP_MN_QUERY_DEVICE_TEXT:
            return "IRP_MJ_PNP - IRP_MN_QUERY_DEVICE_TEXT";
        case IRP_MN_FILTER_RESOURCE_REQUIREMENTS:
            return "IRP_MJ_PNP - IRP_MN_FILTER_RESOURCE_REQUIREMENTS";
        case IRP_MN_READ_CONFIG:
            return "IRP_MJ_PNP - IRP_MN_READ_CONFIG";
        case IRP_MN_WRITE_CONFIG:
            return "IRP_MJ_PNP - IRP_MN_WRITE_CONFIG";
        case IRP_MN_EJECT:
            return "IRP_MJ_PNP - IRP_MN_EJECT";
        case IRP_MN_SET_LOCK:
            return "IRP_MJ_PNP - IRP_MN_SET_LOCK";
        case IRP_MN_QUERY_ID:
            return "IRP_MJ_PNP - IRP_MN_QUERY_ID";
        case IRP_MN_QUERY_PNP_DEVICE_STATE:
            return "IRP_MJ_PNP - IRP_MN_QUERY_PNP_DEVICE_STATE";
        case IRP_MN_QUERY_BUS_INFORMATION:
            return "IRP_MJ_PNP - IRP_MN_QUERY_BUS_INFORMATION";
        case IRP_MN_DEVICE_USAGE_NOTIFICATION:
            return "IRP_MJ_PNP - IRP_MN_DEVICE_USAGE_NOTIFICATION";
        case IRP_MN_SURPRISE_REMOVAL:
            return "IRP_MJ_PNP - IRP_MN_SURPRISE_REMOVAL";
        default:
            return "IRP_MJ_PNP - unknown_pnp_irp";
    }
}

static PCHAR MajorFunctionString(UCHAR MajorFunction, LONG MinorFunction)
{
    switch (MinorFunction)
    {
    case IRP_MJ_CREATE:
        return "IRP_MJ_CREATE";
    case IRP_MJ_CREATE_NAMED_PIPE:
        return "IRP_MJ_CREATE_NAMED_PIPE";
    case IRP_MJ_CLOSE:
        return "IRP_MJ_CLOSE";
    case IRP_MJ_READ:
        return "IRP_MJ_READ";
    case IRP_MJ_WRITE:
        return "IRP_MJ_WRITE";
    case IRP_MJ_QUERY_INFORMATION:
        return "IRP_MJ_QUERY_INFORMATION";
    case IRP_MJ_SET_INFORMATION:
        return "IRP_MJ_SET_INFORMATION";
    case IRP_MJ_QUERY_EA:
        return "IRP_MJ_QUERY_EA";
    case IRP_MJ_SET_EA:
        return "IRP_MJ_SET_EA";
    case IRP_MJ_FLUSH_BUFFERS:
        return "IRP_MJ_FLUSH_BUFFERS";
    case IRP_MJ_QUERY_VOLUME_INFORMATION:
        return "IRP_MJ_QUERY_VOLUME_INFORMATION";
    case IRP_MJ_SET_VOLUME_INFORMATION:
        return "IRP_MJ_SET_VOLUME_INFORMATION";
    case IRP_MJ_DIRECTORY_CONTROL:
        return "IRP_MJ_DIRECTORY_CONTROL";
    case IRP_MJ_FILE_SYSTEM_CONTROL:
        return "IRP_MJ_FILE_SYSTEM_CONTROL";
    case IRP_MJ_DEVICE_CONTROL:
        return "IRP_MJ_DEVICE_CONTROL";
    case IRP_MJ_INTERNAL_DEVICE_CONTROL:
        return "IRP_MJ_INTERNAL_DEVICE_CONTROL";
    case IRP_MJ_SHUTDOWN:
        return "IRP_MJ_SHUTDOWN";
    case IRP_MJ_LOCK_CONTROL:
        return "IRP_MJ_LOCK_CONTROL";
    case IRP_MJ_CLEANUP:
        return "IRP_MJ_CLEANUP";
    case IRP_MJ_CREATE_MAILSLOT:
        return "IRP_MJ_CREATE_MAILSLOT";
    case IRP_MJ_QUERY_SECURITY:
        return "IRP_MJ_QUERY_SECURITY";
    case IRP_MJ_SET_SECURITY:
        return "IRP_MJ_SET_SECURITY";
    case IRP_MJ_POWER:
        return "IRP_MJ_POWER";
    case IRP_MJ_SYSTEM_CONTROL:
        return "IRP_MJ_SYSTEM_CONTROL";
    case IRP_MJ_DEVICE_CHANGE:
        return "IRP_MJ_DEVICE_CHANGE";
    case IRP_MJ_QUERY_QUOTA:
        return "IRP_MJ_QUERY_QUOTA";
    case IRP_MJ_SET_QUOTA:
        return "IRP_MJ_SET_QUOTA";
    case IRP_MJ_PNP:
        return PnPMinorFunctionString(MinorFunction);

    default:
        return "unknown_pnp_irp";
    }
}

#endif
