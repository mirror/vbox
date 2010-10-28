/*++

Copyright (c) 2000  Microsoft Corporation

Module Name:

    vboxusb.h

Abstract:

Environment:

    Kernel mode

Notes:

    Copyright (c) 2000 Microsoft Corporation.
    All Rights Reserved.

--*/
#ifndef _VBOXUSB_H
#define _VBOXUSB_H


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
/* The pragma shuts up the following warning:
   ...inc\ddk\wnet\usbioctl.h(447) : warning C4200: nonstandard extension used : zero-sized array in struct/union
        Cannot generate copy-ctor or copy-assignment operator when UDT contains a zero-sized array */
#pragma warning( disable : 4200 )
#include "usbdi.h"
#pragma warning( default : 4200 )
#include "usbdlib.h"
#include <VBox/usblib.h>

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

#if defined(DEBUG) && !defined(NO_LOGGING)
# define dprintf(a) DbgPrint a
#else
# define dprintf(a) do {} while (0)
#endif

// from usb.h
#define USBD_DEFAULT_PIPE_TRANSFER            0x00000008

#define VBOXUSBTAG (ULONG) 'UxBV'

#undef ExAllocatePool
#define ExAllocatePool(type, size) \
    ExAllocatePoolWithTag(type, size, VBOXUSBTAG);

typedef struct _GLOBALS {

    UNICODE_STRING VBoxUSB_RegistryPath;

} GLOBALS;

#define IDLE_INTERVAL 5000

typedef enum _DEVSTATE {

    NotStarted,         // not started
    Stopped,            // device stopped
    Working,            // started and working
    PendingStop,        // stop pending
    PendingRemove,      // remove pending
    SurpriseRemoved,    // removed by surprise
    Removed             // removed

} DEVSTATE;

typedef enum _QUEUE_STATE {

    HoldRequests,       // device is not started yet
    AllowRequests,      // device is ready to process
    FailRequests        // fail both existing and queued up requests

} QUEUE_STATE;

typedef enum _WDM_VERSION {

    WinXpOrBetter,
    Win2kOrBetter,
    WinMeOrBetter,
    Win98OrBetter

} WDM_VERSION;

#define INITIALIZE_PNP_STATE(_Data_)    \
        (_Data_)->DeviceState =  NotStarted;\
        (_Data_)->PrevDevState = NotStarted;

#define SET_NEW_PNP_STATE(_Data_, _state_) \
        (_Data_)->PrevDevState =  (_Data_)->DeviceState;\
        (_Data_)->DeviceState = (_state_);

#define RESTORE_PREVIOUS_PNP_STATE(_Data_)   \
        (_Data_)->DeviceState =   (_Data_)->PrevDevState;


#define VBOXUSB_MAX_TRANSFER_SIZE   256
#define VBOXUSB_TEST_BOARD_TRANSFER_BUFFER_SIZE (64 *1024 )

//
// registry path used for parameters
// global to all instances of the driver
//

#define VBOXUSB_REGISTRY_PARAMETERS_PATH  \
    L"\\REGISTRY\\Machine\\System\\CurrentControlSet\\SERVICES\\VBOXUSB\\Parameters"


typedef struct _VBOXUSB_PIPE_CONTEXT {

    BOOLEAN PipeOpen;

} VBOXUSB_PIPE_CONTEXT, *PVBOXUSB_PIPE_CONTEXT;

typedef struct _VBOXUSB_PIPE_INFO {
    UCHAR       EndpointAddress;
    ULONG       NextScheduledFrame;
} VBOXUSB_PIPE_INFO;

typedef struct _VBOXUSB_IFACE_INFO {
    USBD_INTERFACE_INFORMATION      *pInterfaceInfo;
    VBOXUSB_PIPE_INFO               *pPipeInfo;
} VBOXUSB_IFACE_INFO;


// A number that should cover the vast majority of USB devices
#define MAX_CFGS    4

//
// A structure representing the instance information associated with
// this particular device.
//

typedef struct _DEVICE_EXTENSION {

    // Functional Device Object
    PDEVICE_OBJECT FunctionalDeviceObject;

    // Device object we call when submitting Urbs
    PDEVICE_OBJECT TopOfStackDeviceObject;

    // The bus driver object
    PDEVICE_OBJECT PhysicalDeviceObject;

    // Name buffer for our named Functional device object link
    // The name is generated based on the driver's class GUID
    UNICODE_STRING InterfaceName;

    // Bus drivers set the appropriate values in this structure in response
    // to an IRP_MN_QUERY_CAPABILITIES IRP. Function and filter drivers might
    // alter the capabilities set by the bus driver.
    DEVICE_CAPABILITIES DeviceCapabilities;

    // Configuration Descriptor
    PUSB_CONFIGURATION_DESCRIPTOR UsbConfigurationDescriptor;

    // Interface Information structure
    PUSBD_INTERFACE_INFORMATION UsbInterface;

    // Pipe context for the vboxusb driver
    PVBOXUSB_PIPE_CONTEXT PipeContext;

    // current state of device
    DEVSTATE DeviceState;

    // state prior to removal query
    DEVSTATE PrevDevState;

    // obtain and hold this lock while changing the device state,
    // the queue state and while processing the queue.
    KSPIN_LOCK DevStateLock;

    // current system power state
    SYSTEM_POWER_STATE SysPower;

    // current device power state
    DEVICE_POWER_STATE DevPower;

    // Pending I/O queue state
    QUEUE_STATE QueueState;

    // Pending I/O queue
    LIST_ENTRY NewRequestsQueue;

    // I/O Queue Lock
    KSPIN_LOCK QueueLock;

    KEVENT RemoveEvent;

    KEVENT StopEvent;

    ULONG OutStandingIO;

    KSPIN_LOCK IOCountLock;

    // selective suspend variables

    LONG SSEnable;

    LONG SSRegistryEnable;

    PUSB_IDLE_CALLBACK_INFO IdleCallbackInfo;

    PIRP PendingIdleIrp;

    LONG IdleReqPend;

    LONG FreeIdleIrpCount;

    KSPIN_LOCK IdleReqStateLock;

    KEVENT NoIdleReqPendEvent;

    // default power state to power down to on self-suspend
    ULONG PowerDownLevel;

    // remote wakeup variables
    PIRP WaitWakeIrp;

    LONG FlagWWCancel;

    LONG FlagWWOutstanding;

    LONG WaitWakeEnable;

    // open handle count
    LONG OpenHandleCount;

    // selective suspend model uses timers, dpcs and work item.
    KTIMER Timer;

    KDPC DeferredProcCall;

    // This event is cleared when a DPC/Work Item is queued.
    // and signaled when the work-item completes.
    // This is essential to prevent the driver from unloading
    // while we have DPC or work-item queued up.
    KEVENT NoDpcWorkItemPendingEvent;

#ifdef SUPPORT_WMI
    // WMI information
    WMILIB_CONTEXT WmiLibInfo;
#endif

    // WDM version
    WDM_VERSION WdmVersion;

    struct
    {
        HANDLE                          hConfiguration;
        uint32_t                        uConfigValue;

        LONG                            fClaimed;

        uint32_t                        uNumInterfaces;
        USB_DEVICE_DESCRIPTOR           *devdescr;
        USB_CONFIGURATION_DESCRIPTOR    *cfgdescr[MAX_CFGS];

        VBOXUSB_IFACE_INFO              *pVBIfaceInfo;

        uint16_t                        idVendor, idProduct, bcdDevice;
        char                            szSerial[MAX_USB_SERIAL_STRING];
        uint8_t                         fIsHighSpeed;

    } usbdev;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;


typedef struct _IRP_COMPLETION_CONTEXT {

    PDEVICE_EXTENSION DeviceExtension;

    PKEVENT Event;

} IRP_COMPLETION_CONTEXT, *PIRP_COMPLETION_CONTEXT;

extern GLOBALS Globals;


typedef struct {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} USBSETUP, *PUSBSETUP;


RT_C_DECLS_BEGIN

/* Hack to get imported names correctly in both win32 & win64 */
#ifdef _WIN64
#define DECLSPEC_USBIMPORT                      DECLSPEC_IMPORT
#else
#define DECLSPEC_USBIMPORT

#define USBD_ParseDescriptors                   _USBD_ParseDescriptors
#define USBD_ParseConfigurationDescriptorEx     _USBD_ParseConfigurationDescriptorEx
#define USBD_CreateConfigurationRequestEx       _USBD_CreateConfigurationRequestEx
#endif

DECLSPEC_USBIMPORT PUSB_COMMON_DESCRIPTOR
USBD_ParseDescriptors(
    IN PVOID DescriptorBuffer,
    IN ULONG TotalLength,
    IN PVOID StartPosition,
    IN LONG DescriptorType
    );

DECLSPEC_USBIMPORT PUSB_INTERFACE_DESCRIPTOR
USBD_ParseConfigurationDescriptorEx(
    IN PUSB_CONFIGURATION_DESCRIPTOR ConfigurationDescriptor,
    IN PVOID StartPosition,
    IN LONG InterfaceNumber,
    IN LONG AlternateSetting,
    IN LONG InterfaceClass,
    IN LONG InterfaceSubClass,
    IN LONG InterfaceProtocol
    );

DECLSPEC_USBIMPORT PURB
USBD_CreateConfigurationRequestEx(
    IN PUSB_CONFIGURATION_DESCRIPTOR ConfigurationDescriptor,
    IN PUSBD_INTERFACE_LIST_ENTRY InterfaceList
    );

RT_C_DECLS_END

#endif
