#include "rdbss_vbox.h"
#pragma hdrstop

#ifdef __cplusplus
extern "C"
{
#endif

/* KeXXXXXXX or ExXXXXXXXXX */

NTSTATUS VBoxRxRegisterMinirdr (PRDBSS_DEVICE_OBJECT *DeviceObject, PDRIVER_OBJECT DriverObject, PMINIRDR_DISPATCH MrdrDispatch, ULONG Controls, PUNICODE_STRING DeviceName, ULONG DeviceExtensionSize,
                                DEVICE_TYPE DeviceType, ULONG DeviceCharacteristics)
{
    return RxRegisterMinirdr(DeviceObject, DriverObject, MrdrDispatch, Controls, DeviceName, DeviceExtensionSize, DeviceType, DeviceCharacteristics);
}

VOID VBoxRxUnregisterMinirdr (PRDBSS_DEVICE_OBJECT RxDeviceObject)
{
    RxUnregisterMinirdr(RxDeviceObject);
}

NTSTATUS VBoxRxStartMinirdr (PRX_CONTEXT RxContext, PBOOLEAN PostToFsp)
{
    return RxStartMinirdr(RxContext, PostToFsp);
}

NTSTATUS VBoxRxStopMinirdr (PRX_CONTEXT RxContext, PBOOLEAN PostToFsp)
{
    return RxStopMinirdr(RxContext, PostToFsp);
}

PRX_CONTEXT VBoxRxCreateRxContext (PIRP Irp, PRDBSS_DEVICE_OBJECT RxDeviceObject, ULONG InitialContextFlags)
{
    return RxCreateRxContext(Irp, RxDeviceObject, InitialContextFlags);
}

VOID VBoxRxDereferenceAndDeleteRxContext (PRX_CONTEXT RxContext)
{
    RxDereferenceAndDeleteRxContext(RxContext);
}

NTSTATUS VBoxRxFinalizeConnection (OUT PNET_ROOT NetRoot, OUT PV_NET_ROOT VNetRoot, LOGICAL Level)
{
    return RxFinalizeConnection(NetRoot, VNetRoot, Level);
}

NTSTATUS VBoxRxFsdDispatch (PRDBSS_DEVICE_OBJECT RxDeviceObject, PIRP Irp)
{
    return RxFsdDispatch(RxDeviceObject, Irp);
}

VOID VBoxRxGetFileSizeWithLock (PFCB Fcb, OUT PLONGLONG FileSize)
{
    RxGetFileSizeWithLock(Fcb, FileSize);
}

PVOID VBoxRxLowIoGetBufferAddress (IN PRX_CONTEXT RxContext)
{
    return RxLowIoGetBufferAddress(RxContext);
}

NTSTATUS VBoxRxDispatchToWorkerThread (OUT PRDBSS_DEVICE_OBJECT pMRxDeviceObject, WORK_QUEUE_TYPE WorkQueueType, PRX_WORKERTHREAD_ROUTINE Routine, PVOID pContext)
{
    return RxDispatchToWorkerThread(pMRxDeviceObject, WorkQueueType, Routine, pContext);
}

/* FCB stuff */

VOID VboxRxFinishFcbInitialization (PMRX_FCB MrxFcb, RDBSS_STORAGE_TYPE_CODES RdbssStorageType, PFCB_INIT_PACKET InitPacket)
{
    RxFinishFcbInitialization(MrxFcb, RdbssStorageType, InitPacket);
}

NTSTATUS VboxRxAcquireExclusiveFcbResourceInMRx (PMRX_FCB pFcb)
{
    return RxAcquireExclusiveFcbResourceInMRx(pFcb);
}

BOOLEAN VBoxRxIsFcbAcquiredExclusive (PMRX_FCB pFcb)
{
    // ExIsResourceAcquiredExclusive is obsolete since W2K and XP!
    return ExIsResourceAcquiredExclusiveLite(RX_GET_MRX_FCB(pFcb)->Header.Resource);
}

/* FOBX stuff */

PMRX_FOBX VboxRxCreateNetFobx (OUT PRX_CONTEXT RxContext, IN PMRX_SRV_OPEN MrxSrvOpen)
{
    return RxCreateNetFobx(RxContext, MrxSrvOpen);
}

