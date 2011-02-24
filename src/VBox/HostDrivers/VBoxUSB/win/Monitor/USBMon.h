#ifndef __USBFilter_h__
#define __USBFilter_h__

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/assert.h>
#include <VBox/sup.h>
#include <iprt/asm.h>

RT_C_DECLS_BEGIN
#if (_MSC_VER >= 1400) && !defined(VBOX_WITH_PATCHED_DDK)
# define _InterlockedExchange           _InterlockedExchange_StupidDDKVsCompilerCrap
# define _InterlockedExchangeAdd        _InterlockedExchangeAdd_StupidDDKVsCompilerCrap
# define _InterlockedCompareExchange    _InterlockedCompareExchange_StupidDDKVsCompilerCrap
# define _InterlockedAddLargeStatistic  _InterlockedAddLargeStatistic_StupidDDKVsCompilerCrap
# pragma warning(disable : 4163)
#endif
#if (_MSC_VER >= 1600) && !defined(VBOX_WITH_PATCHED_DDK)
# define _interlockedbittestandset      _interlockedbittestandset_StillStupidDdkVsCompilerCrap
# define _interlockedbittestandreset    _interlockedbittestandreset_StillStupidDdkVsCompilerCrap
# define _interlockedbittestandset64    _interlockedbittestandset64_StillStupidDdkVsCompilerCrap
# define _interlockedbittestandreset64  _interlockedbittestandreset64_StillStupidDdkVsCompilerCrap
# pragma warning(disable : 4163)
#endif

#include <initguid.h>
#include <wdm.h>
#include <wmilib.h>
#include <wmistr.h>

#if (_MSC_VER >= 1400) && !defined(VBOX_WITH_PATCHED_DDK)
# pragma warning(default : 4163)
# undef  _InterlockedExchange
# undef  _InterlockedExchangeAdd
# undef  _InterlockedCompareExchange
# undef  _InterlockedAddLargeStatistic
#endif
#if (_MSC_VER >= 1600) && !defined(VBOX_WITH_PATCHED_DDK)
# pragma warning(default : 4163)
# undef _interlockedbittestandset
# undef _interlockedbittestandreset
# undef _interlockedbittestandset64
# undef _interlockedbittestandreset64
#endif
RT_C_DECLS_END

extern "C" {
/* from ntddk.h */
NTSYSAPI
CHAR
NTAPI
RtlUpperChar (
    CHAR Character
    );
}

#ifdef DEBUG
#define DebugPrint(_x_) DbgPrint _x_

#define TRAP() DbgBreakPoint()

#else
#define DebugPrint(_x_)
#define TRAP()
#endif


#ifndef  STATUS_CONTINUE_COMPLETION
#define STATUS_CONTINUE_COMPLETION      STATUS_SUCCESS
#endif

#define POOL_TAG   'VBox'

typedef struct _DEVICE_EXTENSION
{
    //
    // Removelock to track IRPs so that device can be removed and
    // the driver can be unloaded safely.
    //
    IO_REMOVE_LOCK      RemoveLock;

    /* Whether the lock has been initialized. */
    LONG                fRemoveLockInitialized;

    /* Number of times the device was opened. */
    LONG                cOpened;

    BOOLEAN             fHookDevice;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;


#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG
PCHAR
PnPMinorFunctionString (
    UCHAR MinorFunction
);

void DebugPrintUnicodeString(PUNICODE_STRING pString);
#else

#define DebugPrintUnicodeString(x)

#endif

NTSTATUS _stdcall
VBoxUSBMonAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
    );


NTSTATUS _stdcall
VBoxUSBMonDispatchPnp (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP pIrp
    );

NTSTATUS _stdcall
VBoxUSBMonDispatchPower(
    IN PDEVICE_OBJECT    DeviceObject,
    IN PIRP              pIrp
    );

/**
 * Unload the driver.
 *
 * @param   pDrvObj     Driver object.
 */
void _stdcall VBoxUSBMonUnload(PDRIVER_OBJECT pDrvObj);


/**
 * Driver entry point.
 *
 * @returns appropriate status code.
 * @param   pDrvObj     Pointer to driver object.
 * @param   pRegPath    Registry base path.
 */
NTSTATUS _stdcall DriverEntry(PDRIVER_OBJECT pDrvObj, PUNICODE_STRING pRegPath);

/**
 * Device I/O Control entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
NTSTATUS _stdcall VBoxUSBMonDeviceControl(PDEVICE_OBJECT pDevObj, PIRP pIrp);

/**
 * Pass on or refuse entry point
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
NTSTATUS _stdcall VBoxUSBMonStub(IN PDEVICE_OBJECT DeviceObject, IN PIRP pIrp);

/**
 * Create (i.e. Open) file entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
NTSTATUS _stdcall VBoxUSBMonCreate(PDEVICE_OBJECT pDevObj, PIRP pIrp);

/**
 * Close file entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
NTSTATUS _stdcall VBoxUSBMonClose(PDEVICE_OBJECT pDevObj, PIRP pIrp);

/**
 * Device PnP hook
 *
 * @param   pDevObj     Device object.
 * @param   pIrp         Request packet.
 */
NTSTATUS _stdcall VBoxUSBMonPnPHook(IN PDEVICE_OBJECT DeviceObject, IN PIRP pIrp);

/**
 * Send IRP_MN_QUERY_DEVICE_RELATIONS
 *
 * @returns NT Status
 * @param   pDevObj         USB device pointer
 * @param   pFileObj        Valid file object pointer
 * @param   pDevRelations   Pointer to DEVICE_RELATIONS pointer (out)
 */
NTSTATUS VBoxUSBQueryBusRelations(PDEVICE_OBJECT pDevObj, PFILE_OBJECT pFileObj, PDEVICE_RELATIONS *pDevRelations);



NTSTATUS _stdcall VBoxUSBPnPCompletion(DEVICE_OBJECT *fido, IRP *pIrp, void *context);
NTSTATUS _stdcall VBoxUSBDispatchIO(PDEVICE_OBJECT DeviceObject, PIRP pIrp);
NTSTATUS _stdcall VBoxUSBCreate();
NTSTATUS _stdcall VBoxUSBClose();
NTSTATUS _stdcall VBoxUSBInit();
NTSTATUS _stdcall VBoxUSBTerm();

/**
 * Capture specified USB device
 *
 * @returns NT status code
 * @param   usVendorId      Vendor id
 * @param   usProductId     Product id
 * @param   usRevision      Revision
 */
NTSTATUS VBoxUSBGrabDevice(USHORT usVendorId, USHORT usProductId, USHORT usRevision);

/**
 * Capture specified USB device
 *
 * @returns NT status code
 * @param   usVendorId      Vendor id
 * @param   usProductId     Product id
 * @param   usRevision      Revision
 */
NTSTATUS VBoxUSBReleaseDevice(USHORT usVendorId, USHORT usProductId, USHORT usRevision);

bool    VBoxMatchFilter(PDEVICE_OBJECT pdo);

bool    VBoxUSBAddDevice(PDEVICE_OBJECT pdo);
int     VBoxUSBIsKnownPDO(PDEVICE_OBJECT pdo);
bool    VBoxUSBRemoveDevice(PDEVICE_OBJECT pdo);
void    VBoxUSBDeviceArrived(PDEVICE_OBJECT pdo);
bool    VBoxUSBDeviceIsCaptured(PDEVICE_OBJECT pdo);
void    VBoxUSBSignalChange();
bool    VBoxUSBCaptureDevice(PDEVICE_OBJECT pdo);


#ifdef __cplusplus
}
#endif

#endif /* __USBFilter_h__ */

