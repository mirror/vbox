#include "precomp.h"

NTSTATUS VBoxRxRegisterMinirdr (PRDBSS_DEVICE_OBJECT *DeviceObject, PDRIVER_OBJECT DriverObject, PMINIRDR_DISPATCH MrdrDispatch, ULONG Controls, PUNICODE_STRING DeviceName, ULONG DeviceExtensionSize,
                                DEVICE_TYPE DeviceType, ULONG DeviceCharacteristics);

VOID VBoxRxUnregisterMinirdr (PRDBSS_DEVICE_OBJECT RxDeviceObject);

NTSTATUS VBoxRxStartMinirdr (PRX_CONTEXT RxContext, PBOOLEAN PostToFsp);

NTSTATUS VBoxRxStopMinirdr (PRX_CONTEXT RxContext, PBOOLEAN PostToFsp);

PRX_CONTEXT VBoxRxCreateRxContext (PIRP Irp, PRDBSS_DEVICE_OBJECT RxDeviceObject, ULONG InitialContextFlags);

VOID VBoxRxDereferenceAndDeleteRxContext_Real (PRX_CONTEXT RxContext);

NTSTATUS VBoxRxFinalizeConnection (OUT PNET_ROOT NetRoot, OUT PV_NET_ROOT VNetRoot, LOGICAL Level);

NTSTATUS VBoxRxFsdDispatch (PRDBSS_DEVICE_OBJECT RxDeviceObject, PIRP Irp);

VOID VBoxRxGetFileSizeWithLock (PFCB Fcb, OUT PLONGLONG FileSize);

PVOID VBoxRxLowIoGetBufferAddress (IN PRX_CONTEXT RxContext);

NTSTATUS VBoxRxDispatchToWorkerThread (OUT PRDBSS_DEVICE_OBJECT pMRxDeviceObject, WORK_QUEUE_TYPE WorkQueueType, PRX_WORKERTHREAD_ROUTINE Routine, PVOID pContext);

VOID VboxRxFinishFcbInitialization (PMRX_FCB MrxFcb, RDBSS_STORAGE_TYPE_CODES RdbssStorageType, PFCB_INIT_PACKET InitPacket);

NTSTATUS VboxRxAcquireExclusiveFcbResourceInMRx (PMRX_FCB pFcb);

BOOLEAN VBoxRxIsFcbAcquiredExclusive (PMRX_FCB pFcb);

PMRX_FOBX VboxRxCreateNetFobx (OUT PRX_CONTEXT RxContext, IN PMRX_SRV_OPEN MrxSrvOpen);
