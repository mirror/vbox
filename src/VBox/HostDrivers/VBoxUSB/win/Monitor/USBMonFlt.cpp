/** @file
 * VBox host drivers - USB drivers - Win32 USB monitor driver
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "USBMon.h"
#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/process.h>
#include <iprt/assert.h>
#include <VBox/sup.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <stdio.h>

#pragma warning(disable : 4200)
#include "usbdi.h"
#pragma warning(default : 4200)
#include "usbdlib.h"
#include "VBoxUSBFilterMgr.h"
#include <VBox/usblib.h>
#include <devguid.h>

/*
 * Note: Must match the VID & PID in the USB driver .inf file!!
 */
/*
  BusQueryDeviceID USB\Vid_80EE&Pid_CAFE
  BusQueryInstanceID 2
  BusQueryHardwareIDs USB\Vid_80EE&Pid_CAFE&Rev_0100
  BusQueryHardwareIDs USB\Vid_80EE&Pid_CAFE
  BusQueryCompatibleIDs USB\Class_ff&SubClass_00&Prot_00
  BusQueryCompatibleIDs USB\Class_ff&SubClass_00
  BusQueryCompatibleIDs USB\Class_ff
*/

#define szBusQueryDeviceId                  L"USB\\Vid_80EE&Pid_CAFE"
#define szBusQueryHardwareIDs               L"USB\\Vid_80EE&Pid_CAFE&Rev_0100\0USB\\Vid_80EE&Pid_CAFE\0\0"
#define szBusQueryCompatibleIDs             L"USB\\Class_ff&SubClass_00&Prot_00\0USB\\Class_ff&SubClass_00\0USB\\Class_ff\0\0"

#define szDeviceTextDescription             L"VirtualBox USB"

/* Possible USB bus driver names. */
static LPWSTR lpszStandardControllerName[1] =
{
    L"\\Driver\\usbhub",
};

#define MAX_ATTACHED_USB_DEVICES        64

typedef struct
{
    bool            fAttached, fCaptured;
    PDEVICE_OBJECT  Pdo;
    uint16_t        idVendor;
    uint16_t        idProduct;
    uint16_t        bcdDevice;
    uint8_t         bClass;
    uint8_t         bSubClass;
    uint8_t         bProtocol;
    char            szSerial[MAX_USB_SERIAL_STRING];
    char            szMfgName[MAX_USB_SERIAL_STRING];
    char            szProduct[MAX_USB_SERIAL_STRING];
} FLTUSB_DEVICE, *PFLTUSB_DEVICE;

/* Device driver instance data */
typedef struct
{
    LONG            cUSBDevices;
    LONG            CaptureCount;

    LONG            cUSBStateChange;

    KSPIN_LOCK      lock;

    FLTUSB_DEVICE   USBDevice[MAX_ATTACHED_USB_DEVICES];

    /* Set to force grabbing of newly arrived devices */
    bool            fForceGrab;
    /* Set to disable all filters */
    bool            fDisableFilters;

} DRVINSTANCE, *PDRVINSTANCE;

DRVINSTANCE DrvInstance = {0};

/* Forward declarations. */
NTSTATUS VBoxUSBGetDeviceDescription(PDEVICE_OBJECT pDevObj, USHORT *pusVendorId, USHORT *pusProductId, USHORT *pusRevision);
NTSTATUS VBoxUSBGetDeviceIdStrings(PDEVICE_OBJECT pDevObj, PFLTUSB_DEVICE pFltDev);

#define ACQUIRE_LOCK()  \
    KIRQL oldIrql; \
    KeAcquireSpinLock(&DrvInstance.lock, &oldIrql);

#define RELEASE_LOCK() \
    KeReleaseSpinLock(&DrvInstance.lock, oldIrql);


NTSTATUS _stdcall VBoxUSBInit()
{
    memset(&DrvInstance, 0, sizeof(DrvInstance));
    KeInitializeSpinLock(&DrvInstance.lock);
    VBoxUSBFilterInit();
    return STATUS_SUCCESS;
}

NTSTATUS _stdcall VBoxUSBTerm()
{
    VBoxUSBFilterTerm();
    return STATUS_SUCCESS;
}


/**
 * Device I/O Control entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
NTSTATUS _stdcall VBoxUSBDispatchIO(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION  irpStack;
    NTSTATUS            status;
    ULONG               info  = 0;
    PVOID               ioBuffer;
    ULONG               inputBufferLength;
    ULONG               outputBufferLength;

    status                    = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    irpStack                  = IoGetCurrentIrpStackLocation (Irp);

    ioBuffer                  = Irp->AssociatedIrp.SystemBuffer;
    inputBufferLength         = irpStack->Parameters.DeviceIoControl.InputBufferLength;
    outputBufferLength        = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode)
    {
    case SUPUSBFLT_IOCTL_USB_CHANGE:
    {
        PUSBSUP_USB_CHANGE pOut = (PUSBSUP_USB_CHANGE)ioBuffer;

        if (!ioBuffer || outputBufferLength != sizeof(*pOut) || inputBufferLength != 0)
        {
            AssertMsgFailed(("SUPUSBFLT_IOCTL_USB_CHANGE: Invalid input/output sizes. cbIn=%d expected %d. cbOut=%d expected %d.\n",
                             inputBufferLength, 0, outputBufferLength, sizeof(*pOut)));
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        Assert(sizeof(DrvInstance.cUSBStateChange) == sizeof(uint32_t));

////        DebugPrint(("SUPUSBFLT_IOCTL_USB_CHANGE -> %d\n", DrvInstance.cUSBStateChange));
        pOut->cUSBStateChange = DrvInstance.cUSBStateChange;

        info = sizeof(*pOut);
        break;
    }

    case SUPUSBFLT_IOCTL_GET_NUM_DEVICES:
    {
        PUSBSUP_GETNUMDEV pOut = (PUSBSUP_GETNUMDEV)ioBuffer;

        if (!ioBuffer || outputBufferLength != sizeof(*pOut) || inputBufferLength != 0)
        {
            AssertMsgFailed(("SUPUSBFLT_IOCTL_GET_NUM_DEVICES: Invalid input/output sizes. cbIn=%d expected %d. cbOut=%d expected %d.\n",
                             inputBufferLength, 0, outputBufferLength, sizeof(*pOut)));
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        DebugPrint(("SUPUSBFLT_IOCTL_GET_NUM_DEVICES -> %d devices\n", DrvInstance.cUSBDevices));
        pOut->cUSBDevices = DrvInstance.cUSBDevices;
        info = sizeof(*pOut);
        break;
    }

    case SUPUSBFLT_IOCTL_CAPTURE_DEVICE:
    {
        PUSBSUP_CAPTURE pIn = (PUSBSUP_CAPTURE)ioBuffer;

        DebugPrint(("SUPUSBFLT_IOCTL_CAPTURE_DEVICE\n"));
        if (!ioBuffer || inputBufferLength != sizeof(*pIn) || outputBufferLength != 0)
        {
            AssertMsgFailed(("SUPUSBFLT_IOCTL_CAPTURE_DEVICE: Invalid input/output sizes. cbIn=%d expected %d. cbOut=%d expected %d.\n",
                             inputBufferLength, sizeof(*pIn), outputBufferLength, 0));
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        status = VBoxUSBGrabDevice(pIn->usVendorId, pIn->usProductId, pIn->usRevision);
        break;
    }

    case SUPUSBFLT_IOCTL_RELEASE_DEVICE:
    {
        PUSBSUP_RELEASE pIn = (PUSBSUP_RELEASE)ioBuffer;

        DebugPrint(("SUPUSBFLT_IOCTL_RELEASE_DEVICE\n"));
        if (!ioBuffer || inputBufferLength != sizeof(*pIn) || outputBufferLength != 0)
        {
            AssertMsgFailed(("SUPUSBFLT_IOCTL_RELEASE_DEVICE: Invalid input/output sizes. cbIn=%d expected %d. cbOut=%d expected %d.\n",
                             inputBufferLength, sizeof(*pIn), outputBufferLength, 0));
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        status = VBoxUSBReleaseDevice(pIn->usVendorId, pIn->usProductId, pIn->usRevision);
        break;
    }

    case SUPUSBFLT_IOCTL_GET_VERSION:
    {
        PUSBSUP_VERSION pOut = (PUSBSUP_VERSION)ioBuffer;

        DebugPrint(("SUPUSBFLT_IOCTL_GET_VERSION\n"));
        if (!ioBuffer || outputBufferLength != sizeof(*pOut) || inputBufferLength != 0)
        {
            AssertMsgFailed(("SUPUSBFLT_IOCTL_GET_VERSION: Invalid input/output sizes. cbIn=%d expected %d. cbOut=%d expected %d.\n",
                             inputBufferLength, 0, outputBufferLength, sizeof(*pOut)));
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        pOut->u32Major = USBMON_MAJOR_VERSION;
        pOut->u32Minor = USBMON_MINOR_VERSION;
        info = sizeof(*pOut);
        break;
    }

    case SUPUSBFLT_IOCTL_ADD_FILTER:
    {
        PUSBFILTER          pFilter = (PUSBFILTER)ioBuffer;
        PUSBSUP_FLTADDOUT   pOut = (PUSBSUP_FLTADDOUT)ioBuffer;
        RTPROCESS           pid = RTProcSelf();
        uintptr_t           uId = 0;

        /* Validate input. */
        if (RT_UNLIKELY(!ioBuffer || inputBufferLength != sizeof(*pFilter) || outputBufferLength != sizeof(*pOut)))
        {
            AssertMsgFailed(("SUPUSBFLT_IOCTL_ADD_FILTER: Invalid input/output sizes. cbIn=%d expected %d. cbOut=%d expected %d.\n",
                             inputBufferLength, sizeof(pFilter), outputBufferLength, 0));
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        /* Log the filter details. */
        DebugPrint(("SUPUSBFLT_IOCTL_ADD_FILTER %s %s %s\n",
            USBFilterGetString(pFilter, USBFILTERIDX_MANUFACTURER_STR)  ? USBFilterGetString(pFilter, USBFILTERIDX_MANUFACTURER_STR)  : "<null>",
            USBFilterGetString(pFilter, USBFILTERIDX_PRODUCT_STR)       ? USBFilterGetString(pFilter, USBFILTERIDX_PRODUCT_STR)       : "<null>",
            USBFilterGetString(pFilter, USBFILTERIDX_SERIAL_NUMBER_STR) ? USBFilterGetString(pFilter, USBFILTERIDX_SERIAL_NUMBER_STR) : "<null>"));
#ifdef DEBUG
        DebugPrint(("VBoxUSBClient::addFilter: idVendor=%#x idProduct=%#x bcdDevice=%#x bDeviceClass=%#x bDeviceSubClass=%#x bDeviceProtocol=%#x bBus=%#x bPort=%#x\n",
                  USBFilterGetNum(pFilter, USBFILTERIDX_VENDOR_ID),
                  USBFilterGetNum(pFilter, USBFILTERIDX_PRODUCT_ID),
                  USBFilterGetNum(pFilter, USBFILTERIDX_DEVICE_REV),
                  USBFilterGetNum(pFilter, USBFILTERIDX_DEVICE_CLASS),
                  USBFilterGetNum(pFilter, USBFILTERIDX_DEVICE_SUB_CLASS),
                  USBFilterGetNum(pFilter, USBFILTERIDX_DEVICE_PROTOCOL),
                  USBFilterGetNum(pFilter, USBFILTERIDX_BUS),
                  USBFilterGetNum(pFilter, USBFILTERIDX_PORT)));
#endif

        /* We can't get the bus/port numbers. Ignore them while matching. */
        USBFilterSetMustBePresent(pFilter, USBFILTERIDX_BUS, false);
        USBFilterSetMustBePresent(pFilter, USBFILTERIDX_PORT, false);

        /* Add the filter. */
        pOut->rc  = VBoxUSBFilterAdd(pFilter, pid, &uId);
        pOut->uId = uId;

        info = sizeof(*pOut);
        break;
    }

    case SUPUSBFLT_IOCTL_REMOVE_FILTER:
    {
        uintptr_t   *pIn = (uintptr_t *)ioBuffer;
        RTPROCESS   pid = RTProcSelf();

        if (!ioBuffer || inputBufferLength != sizeof(*pIn))
        {
            AssertMsgFailed(("SUPUSBFLT_IOCTL_REMOVE_FILTER: Invalid input/output sizes. cbIn=%d expected %d. cbOut=%d expected %d.\n",
                             inputBufferLength, sizeof(pIn), outputBufferLength, 0));
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        DebugPrint(("SUPUSBFLT_IOCTL_REMOVE_FILTER %x\n", *pIn));
        VBoxUSBFilterRemove(pid, *pIn);
        break;
    }

    default:
        status = STATUS_INVALID_PARAMETER;
        break;
    }
    Irp->IoStatus.Information = info;

    return status;
}

NTSTATUS _stdcall VBoxUSBCreate()
{
    Assert(sizeof(DrvInstance.CaptureCount) == sizeof(LONG));
    InterlockedIncrement(&DrvInstance.CaptureCount);
    return STATUS_SUCCESS;
}

NTSTATUS _stdcall VBoxUSBClose()
{
    ACQUIRE_LOCK();
    Assert(sizeof(DrvInstance.CaptureCount) == sizeof(LONG));
    InterlockedDecrement(&DrvInstance.CaptureCount);
    Assert(DrvInstance.CaptureCount >= 0);
    VBoxUSBFilterRemoveOwner(RTProcSelf());

    if (DrvInstance.CaptureCount == 0)
    {
        DrvInstance.cUSBDevices = 0;
    }
    RELEASE_LOCK();
    return STATUS_SUCCESS;
}


unsigned myxdigit(char c)
{
    if (c >= 'a' && c <= 'z')
        c = c - 'a' + 'A';
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 0;
}

#if 1
bool VBoxMatchFilter(PDEVICE_OBJECT pdo)
{
    USBFILTER       Device;
    int             index;
    PFLTUSB_DEVICE  USBDevice;

    if (DrvInstance.fForceGrab)
    {
        DebugPrint(("VBoxMatchFilter -> Force Grab -> TRUE\n"));
        return true;
    }
    if (DrvInstance.fDisableFilters)
    {
        DebugPrint(("VBoxMatchFilter -> filters disabled -> FALSE\n"));
        return false;
    }

    index = VBoxUSBIsKnownPDO(pdo);
    if (index == -1)
    {
        DebugPrint(("VBoxMatchFilter -> unknown PDO -> FALSE\n"));
        return false;
    }
    ACQUIRE_LOCK();

    USBDevice = &DrvInstance.USBDevice[index];
    USBFilterInit(&Device, USBFILTERTYPE_CAPTURE);

    USBFilterSetNumExact(&Device, USBFILTERIDX_VENDOR_ID, USBDevice->idVendor, true);
    USBFilterSetNumExact(&Device, USBFILTERIDX_PRODUCT_ID, USBDevice->idProduct, true);
    USBFilterSetNumExact(&Device, USBFILTERIDX_DEVICE_REV, USBDevice->bcdDevice, true);
    USBFilterSetNumExact(&Device, USBFILTERIDX_DEVICE_CLASS, USBDevice->bClass, true);
    USBFilterSetNumExact(&Device, USBFILTERIDX_DEVICE_SUB_CLASS, USBDevice->bSubClass, true);
    USBFilterSetNumExact(&Device, USBFILTERIDX_DEVICE_PROTOCOL, USBDevice->bProtocol, true);
    USBFilterSetStringExact(&Device, USBFILTERIDX_MANUFACTURER_STR, USBDevice->szMfgName, true);
    USBFilterSetStringExact(&Device, USBFILTERIDX_PRODUCT_STR, USBDevice->szProduct, true);
    USBFilterSetStringExact(&Device, USBFILTERIDX_SERIAL_NUMBER_STR, USBDevice->szSerial, true);

    /* Run filters on the thing. */
    uintptr_t uId = 0;
    RTPROCESS Owner = VBoxUSBFilterMatch(&Device, &uId);
    USBFilterDelete(&Device);
    if (Owner == NIL_RTPROCESS)
    {
        RELEASE_LOCK();
        return false;
    }
    DebugPrint(("VBoxMatchFilter: HIT\n"));
    RELEASE_LOCK();
    return true;
}
#else
bool VBoxMatchFilter(WCHAR *pszDeviceId)
{
    //#define szBusQueryDeviceId       L"USB\\Vid_80EE&Pid_CAFE"
    uint16_t    pId;
    uint16_t    vId;
//    char        szRevision[4];
#ifdef DEBUG
    WCHAR       *pszOrgDeviceId = pszDeviceId;
#endif
    USBFILTER   Device;

    if (DrvInstance.fForceGrab)
    {
        DebugPrint(("VBoxMatchFilter -> Force Grab -> TRUE\n"));
        return true;
    }
    if (DrvInstance.fDisableFilters)
    {
        DebugPrint(("VBoxMatchFilter -> filters disabled -> FALSE\n"));
        return false;
    }

    if (RtlCompareMemory(pszDeviceId, L"USB\\", 4*sizeof(WCHAR)) != 4*sizeof(WCHAR))
        return false;

    while(*pszDeviceId != 0 && *pszDeviceId != '_')
        pszDeviceId++;
    if(*pszDeviceId == 0)
        return false;
    pszDeviceId++;

    /* Vid_ skipped */
    vId = myxdigit((CHAR)pszDeviceId[0]) << 12
        | myxdigit((CHAR)pszDeviceId[1]) << 8
        | myxdigit((CHAR)pszDeviceId[2]) << 4
        | myxdigit((CHAR)pszDeviceId[3]) << 0;

    while(*pszDeviceId != 0 && *pszDeviceId != '_')
        pszDeviceId++;
    if(*pszDeviceId == 0)
        return false;
    pszDeviceId++;

    /* Pid_ skipped */
    pId = myxdigit((CHAR)pszDeviceId[0]) << 12
        | myxdigit((CHAR)pszDeviceId[1]) << 8
        | myxdigit((CHAR)pszDeviceId[2]) << 4
        | myxdigit((CHAR)pszDeviceId[3]) << 0;

    ACQUIRE_LOCK();

    USBFilterInit(&Device, USBFILTERTYPE_CAPTURE);

    USBFilterSetNumExact(&Device, USBFILTERIDX_VENDOR_ID, vId, true);
    USBFilterSetNumExact(&Device, USBFILTERIDX_PRODUCT_ID, pId, true);

    /* Run filters on the thing. */
    uintptr_t uId = 0;
    RTPROCESS Owner = VBoxUSBFilterMatch(&Device, &uId);
    USBFilterDelete(&Device);
    if (Owner == NIL_RTPROCESS)
    {
        RELEASE_LOCK();
        return false;
    }
    DebugPrint(("VBoxMatchFilter: HIT\n"));
    RELEASE_LOCK();
    return true;
}
#endif

bool VBoxUSBAddDevice(PDEVICE_OBJECT pdo)
{
    if (VBoxUSBIsKnownPDO(pdo) != -1)
        return true;

    ACQUIRE_LOCK()
    for (int i=0;i<MAX_ATTACHED_USB_DEVICES;i++)
    {
        if (DrvInstance.USBDevice[i].fAttached == false)
        {
            DebugPrint(("VBoxUSBAddDevice %p\n", pdo));
            DrvInstance.USBDevice[i].fAttached = true;
            DrvInstance.USBDevice[i].Pdo       = pdo;
            DrvInstance.USBDevice[i].fCaptured = false;
            Assert(DrvInstance.cUSBDevices <= MAX_ATTACHED_USB_DEVICES);

            DebugPrint(("Signal USB change ADD\n"));
            RELEASE_LOCK();
            /* There doesn't appear to be any good way to get device information
             * from Windows. Reading its descriptor should do the trick though.
             */
            VBoxUSBGetDeviceIdStrings(pdo, &DrvInstance.USBDevice[i]);

            InterlockedIncrement(&DrvInstance.cUSBStateChange);
            return true;
        }
    }
    RELEASE_LOCK();
    return false;
}

int VBoxUSBIsKnownPDO(PDEVICE_OBJECT pdo)
{
    Assert(pdo);
    ACQUIRE_LOCK();
    for (int i=0;i<MAX_ATTACHED_USB_DEVICES;i++)
    {
        if (DrvInstance.USBDevice[i].Pdo == pdo)
        {
            RELEASE_LOCK();
            return i;
        }
    }
    RELEASE_LOCK();
    return -1;
}

bool VBoxUSBRemoveDevice(PDEVICE_OBJECT pdo)
{
    ACQUIRE_LOCK();

    for (int i=0;i<MAX_ATTACHED_USB_DEVICES;i++)
    {
        if (DrvInstance.USBDevice[i].Pdo == pdo)
        {
            DebugPrint(("VBoxUSBRemoveDevice %p\n", pdo));
            DrvInstance.USBDevice[i].fAttached = false;
            DrvInstance.USBDevice[i].Pdo       = NULL;

            if (DrvInstance.USBDevice[i].fCaptured == true)
            {
                InterlockedDecrement(&DrvInstance.cUSBDevices);
                Assert(DrvInstance.cUSBDevices >= 0);

                DrvInstance.USBDevice[i].fCaptured = false;
            }

            DebugPrint(("Signal USB change REMOVE\n"));
            InterlockedIncrement(&DrvInstance.cUSBStateChange);

            RELEASE_LOCK();
            return true;
        }
    }
    RELEASE_LOCK();
    return false;
}

bool VBoxUSBCaptureDevice(PDEVICE_OBJECT pdo)
{
    Assert(DrvInstance.CaptureCount);
    if (DrvInstance.CaptureCount == 0)
        return false;

    ACQUIRE_LOCK();
    for (int i=0;i<MAX_ATTACHED_USB_DEVICES;i++)
    {
        if (    DrvInstance.USBDevice[i].Pdo == pdo
            &&  DrvInstance.USBDevice[i].fCaptured == false)
        {
            DebugPrint(("VBoxUSBCaptureDevice %p\n", pdo));

            Assert(DrvInstance.USBDevice[i].fAttached);

            DrvInstance.USBDevice[i].fCaptured = true;
            InterlockedIncrement(&DrvInstance.cUSBDevices);
            RELEASE_LOCK();
            return true;
        }
    }
    RELEASE_LOCK();
    return false;
}

void VBoxUSBDeviceArrived(PDEVICE_OBJECT pdo)
{
    /* If we manually release a device, then all filters will be temporarily disabled; Enable them again when the
     * device has been started.
     */
    DrvInstance.fDisableFilters = false;

    /* If we manually capture a device, we are forced to grab the next device that arrives. Disable this mode here */
    DrvInstance.fForceGrab = false;
    return;
}

bool VBoxUSBDeviceIsCaptured(PDEVICE_OBJECT pdo)
{
    bool ret;

    ACQUIRE_LOCK();
    for (int i=0;i<MAX_ATTACHED_USB_DEVICES;i++)
    {
        if (DrvInstance.USBDevice[i].Pdo == pdo)
        {
            ret = DrvInstance.USBDevice[i].fCaptured;
            RELEASE_LOCK();
            return ret;
        }
    }
    RELEASE_LOCK();
    return false;
}

/**
 * Send USB ioctl
 *
 * @returns NT Status
 * @param   pDevObj         USB device pointer
 * @param   control_code    ioctl
 * @param   buffer          Descriptor buffer
 * @param   size            size of buffer
 */
NTSTATUS VBoxUSBSendIOCTL(PDEVICE_OBJECT pDevObj, ULONG control_code, void *buffer, uint32_t size)
{
    IO_STATUS_BLOCK io_status;
    KEVENT          event;
    NTSTATUS        status;
    IRP            *pIrp;
    PIO_STACK_LOCATION stackloc;

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    pIrp = IoBuildDeviceIoControlRequest(control_code,  pDevObj, NULL, 0, NULL, 0, TRUE, &event, &io_status);
    if (!pIrp)
    {
        AssertMsgFailed(("IoBuildDeviceIoControlRequest failed!!\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* Get the next stack location as that is used for the new irp */
    stackloc = IoGetNextIrpStackLocation(pIrp);
    stackloc->Parameters.Others.Argument1 = buffer;
    stackloc->Parameters.Others.Argument2 = NULL;

    status = IoCallDriver(pDevObj, pIrp);
    if (status == STATUS_PENDING)
    {
        DebugPrint(("IoCallDriver returned STATUS_PENDING!!\n"));
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        status = io_status.Status;
    }

    DebugPrint(("IoCallDriver returned %x\n", status));

    return status;
}

/**
 * Get USB descriptor
 *
 * @returns NT Status
 * @param   pDevObj     USB device pointer
 * @param   buffer      Descriptor buffer
 * @param   size        size of buffer
 * @param   type        descriptor type
 * @param   index       descriptor index
 * @param   language_id descriptor language id
 */
NTSTATUS VBoxUSBGetDescriptor(PDEVICE_OBJECT pDevObj, void *buffer, int size, int type, int index, int language_id)
{
    NTSTATUS rc;
    PURB     urb;

    urb = (PURB)ExAllocatePool(NonPagedPool,sizeof(URB));
    if(urb == NULL)
    {
        DebugPrint(("Failed to alloc mem for urb\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    memset(urb, 0, sizeof(*urb));

    urb->UrbHeader.Function = URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE;
    urb->UrbHeader.Length = sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST);
    urb->UrbControlDescriptorRequest.TransferBufferLength = size;
    urb->UrbControlDescriptorRequest.TransferBuffer       = buffer;
    urb->UrbControlDescriptorRequest.Index                = (UCHAR)index;
    urb->UrbControlDescriptorRequest.DescriptorType       = (UCHAR)type;
    urb->UrbControlDescriptorRequest.LanguageId           = (USHORT)language_id;

    rc = VBoxUSBSendIOCTL(pDevObj, IOCTL_INTERNAL_USB_SUBMIT_URB, urb, sizeof(*urb));
#ifdef DEBUG
    if(!NT_SUCCESS(rc) || !USBD_SUCCESS(urb->UrbHeader.Status))
        DebugPrint(("VBoxUSBGetDescriptor: VBoxUSBSendIOCTL failed with %x (%x)\n", rc, urb->UrbHeader.Status));
#endif

    ExFreePool(urb);
    return rc;
}

/**
 * Get a valid USB string descriptor language ID (the first ID found).
 *
 * @returns NT Status
 * @param   pDevObj     USB device pointer
 * @param   lang_id     pointer to language id
 */
NTSTATUS VBoxUSBGetLangID(PDEVICE_OBJECT pDevObj, int *lang_id)
{
    NTSTATUS                status;
    unsigned                length;
    char                    buffer[MAXIMUM_USB_STRING_LENGTH];
    PUSB_STRING_DESCRIPTOR  pstrdescr = (PUSB_STRING_DESCRIPTOR)&buffer;

    Assert(lang_id);
    *lang_id = 0;

    length = sizeof(buffer);
    memset(pstrdescr, 0, length);
    pstrdescr->bLength         = length;
    pstrdescr->bDescriptorType = USB_STRING_DESCRIPTOR_TYPE;

    status = VBoxUSBGetDescriptor(pDevObj, pstrdescr, length, USB_STRING_DESCRIPTOR_TYPE, 0, 0);
    if (!NT_SUCCESS(status))
    {
        DebugPrint(("VBoxUSBGetLangID: language ID table not present (?)\n"));
        goto fail;
    }
    /* Just grab the first lang ID if available. In 99% cases, it will be US English (0x0409).*/
    if (pstrdescr->bLength >= sizeof(USB_STRING_DESCRIPTOR))
    {
        Assert(sizeof(pstrdescr->bString[0]) == sizeof(uint16_t));
        *lang_id = pstrdescr->bString[0];
        status = STATUS_SUCCESS;
    }
    else
        status = STATUS_INVALID_PARAMETER;
fail:
    return status;
}

NTSTATUS VBoxUSBGetStringDescriptor(PDEVICE_OBJECT pDevObj, char *dest, unsigned size, int index, int lang_id)
{
    NTSTATUS                status;
    PUSB_STRING_DESCRIPTOR  pstrdescr = NULL;
    unsigned                length;
    UNICODE_STRING          ustr;
    ANSI_STRING             astr;

    *dest = '\0';
    if (index)
    {
        /* An entire USB string descriptor is Unicode and can't be longer than 256 bytes.
         * Hence 128 bytes is enough for an ASCII string.
         */
        length = sizeof(USB_STRING_DESCRIPTOR) + MAX_USB_SERIAL_STRING * sizeof(pstrdescr->bString[0]);
        pstrdescr = (PUSB_STRING_DESCRIPTOR)ExAllocatePool(NonPagedPool, length);
        if (!pstrdescr)
        {
            AssertMsgFailed(("VBoxUSBGetStringDescriptor: ExAllocatePool failed\n"));
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto fail;
        }
        memset(pstrdescr, 0, length);
        pstrdescr->bLength         = length;
        pstrdescr->bDescriptorType = USB_STRING_DESCRIPTOR_TYPE;

        status = VBoxUSBGetDescriptor(pDevObj, pstrdescr, length, USB_STRING_DESCRIPTOR_TYPE, index, lang_id);
        if (!NT_SUCCESS(status))
        {
            DebugPrint(("VBoxUSBGetStringDescriptor: requested string not present (?)\n"));
            status = STATUS_SUCCESS;    //not fatal
            goto fail;
        }
        if (pstrdescr->bLength > sizeof(USB_STRING_DESCRIPTOR))
        {
            RtlInitUnicodeString(&ustr, pstrdescr->bString);
            RtlInitAnsiString(&astr, NULL);
            RtlUnicodeStringToAnsiString(&astr, &ustr, TRUE);
            strncpy(dest, astr.Buffer, size);
            RtlFreeAnsiString(&astr);
        }
    }
    status = STATUS_SUCCESS;
fail:
    if (pstrdescr)
        ExFreePool(pstrdescr);
    return status;
}

NTSTATUS VBoxUSBGetDeviceIdStrings(PDEVICE_OBJECT pDevObj, PFLTUSB_DEVICE pFltDev)
{
    NTSTATUS                status;
    PUSB_DEVICE_DESCRIPTOR  devdescr = 0;

    devdescr = (PUSB_DEVICE_DESCRIPTOR)ExAllocatePool(NonPagedPool, sizeof(USB_DEVICE_DESCRIPTOR));
    if (devdescr == NULL)
    {
        DebugPrint(("Failed to alloc mem for urb\n"));
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto fail;
    }
    memset(devdescr, 0, sizeof(*devdescr));

    status = VBoxUSBGetDescriptor(pDevObj, devdescr, sizeof(*devdescr), USB_DEVICE_DESCRIPTOR_TYPE, 0, 0);
    if (!NT_SUCCESS(status))
    {
        AssertMsgFailed(("VBoxUSBGetDeviceDescription: getting device descriptor failed\n"));
        goto fail;
    }
    DebugPrint(("Device pid=%x vid=%x rev=%x\n", devdescr->idVendor, devdescr->idProduct, devdescr->bcdDevice));
    pFltDev->idVendor     = devdescr->idVendor;
    pFltDev->idProduct    = devdescr->idProduct;
    pFltDev->bcdDevice    = devdescr->bcdDevice;
    pFltDev->bClass       = devdescr->bDeviceClass;
    pFltDev->bSubClass    = devdescr->bDeviceSubClass;
    pFltDev->bProtocol    = devdescr->bDeviceProtocol;
    pFltDev->szSerial[0]  = 0;
    pFltDev->szMfgName[0] = 0;
    pFltDev->szProduct[0] = 0;

    /* If there are no strings, don't even try to get any string descriptors. */
    if (devdescr->iSerialNumber || devdescr->iManufacturer || devdescr->iProduct)
    {
        int             langId;

        status = VBoxUSBGetLangID(pDevObj, &langId);
        if (!NT_SUCCESS(status))
        {
            AssertMsgFailed(("VBoxUSBGetDeviceDescription: reading language ID failed\n"));
            goto fail;
        }
        status = VBoxUSBGetStringDescriptor(pDevObj, pFltDev->szSerial, sizeof(pFltDev->szSerial), devdescr->iSerialNumber, langId);
        if (!NT_SUCCESS(status))
        {
            AssertMsgFailed(("VBoxUSBGetDeviceDescription: reading serial number failed\n"));
            goto fail;
        }
        status = VBoxUSBGetStringDescriptor(pDevObj, pFltDev->szMfgName, sizeof(pFltDev->szMfgName), devdescr->iManufacturer, langId);
        if (!NT_SUCCESS(status))
        {
            AssertMsgFailed(("VBoxUSBGetDeviceDescription: reading manufacturer name failed\n"));
            goto fail;
        }
        status = VBoxUSBGetStringDescriptor(pDevObj, pFltDev->szProduct, sizeof(pFltDev->szProduct), devdescr->iProduct, langId);
        if (!NT_SUCCESS(status))
        {
            AssertMsgFailed(("VBoxUSBGetDeviceDescription: reading product name failed\n"));
            goto fail;
        }

        DebugPrint(("VBoxUSBGetStringDescriptor: strings: '%s':'%s':'%s' (lang ID %x)\n",
                    pFltDev->szMfgName, pFltDev->szProduct, pFltDev->szSerial, langId));
    }
    status = STATUS_SUCCESS;

fail:
    if (devdescr)
        ExFreePool(devdescr);
    return status;
}

/**
 * Get USB device description
 *
 * @returns NT Status
 * @param   pDevObj             USB device pointer
 * @param   pusVendorId         Vendor id (out)
 * @param   pusProductId        Product id (out)
 * @param   pusRevision         Revision (out)
 */
NTSTATUS VBoxUSBGetDeviceDescription(PDEVICE_OBJECT pDevObj, USHORT *pusVendorId, USHORT *pusProductId, USHORT *pusRevision)
{
    NTSTATUS status;
    PUSB_DEVICE_DESCRIPTOR devdescr = 0;

    devdescr = (PUSB_DEVICE_DESCRIPTOR)ExAllocatePool(NonPagedPool,sizeof(USB_DEVICE_DESCRIPTOR));
    if(devdescr == NULL)
    {
        DebugPrint(("Failed to alloc mem for urb\n"));
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto fail;
    }
    memset(devdescr, 0, sizeof(*devdescr));

    status = VBoxUSBGetDescriptor(pDevObj, devdescr, sizeof(*devdescr),  USB_DEVICE_DESCRIPTOR_TYPE, 0, 0);
    if (!NT_SUCCESS(status))
    {
        DebugPrint(("VBoxUSBGetDeviceDescription: getting device descriptor failed\n"));
        goto fail;
    }
    DebugPrint(("Device pid=%x vid=%x rev=%x\n", devdescr->idVendor, devdescr->idProduct, devdescr->bcdDevice));
    *pusVendorId    = devdescr->idVendor;
    *pusProductId   = devdescr->idProduct;
    *pusRevision    = devdescr->bcdDevice;

    ExFreePool(devdescr);
    return STATUS_SUCCESS;

fail:
    if (devdescr)
        ExFreePool(devdescr);
    return status;
}

/**
 * Unplug and replug the specified USB device
 *
 * @returns NT status code
 * @param   usVendorId      Vendor id
 * @param   usProductId     Product id
 * @param   usRevision      Revision
 * @param   fCaptured       Already captured or not
 * @param   pfReplugged     Replugged or not (out)
 */
NTSTATUS VBoxUSBReplugDevice(USHORT usVendorId, USHORT usProductId, USHORT usRevision, bool fCaptured, bool *pfReplugged)
{
    NTSTATUS        status;
    UNICODE_STRING  szStandardControllerName[RT_ELEMENTS(lpszStandardControllerName)];

    DebugPrint(("VBoxUSBReplugDevice: %04X %04X %04X\n", usVendorId, usProductId, usRevision));

    Assert(pfReplugged);
    *pfReplugged = false;

    for (int i=0;i<RT_ELEMENTS(lpszStandardControllerName);i++)
    {
        szStandardControllerName[i].Length = 0;
        szStandardControllerName[i].MaximumLength = 0;
        szStandardControllerName[i].Buffer = 0;

        RtlInitUnicodeString(&szStandardControllerName[i], lpszStandardControllerName[i]);
    }

    for (int i=0;i<16;i++)
    {
        char            szHubName[32];
        UNICODE_STRING  UnicodeName;
        ANSI_STRING     AnsiName;
        PDEVICE_OBJECT  pHubDevObj;
        PFILE_OBJECT    pHubFileObj;

        sprintf(szHubName, "\\Device\\USBPDO-%d", i);

        UnicodeName.Length = 0;
        UnicodeName.MaximumLength = 0;
        UnicodeName.Buffer = 0;

        RtlInitAnsiString(&AnsiName, szHubName);
        RtlAnsiStringToUnicodeString(&UnicodeName, &AnsiName, TRUE);

        status = IoGetDeviceObjectPointer(&UnicodeName, FILE_READ_DATA, &pHubFileObj, &pHubDevObj);
        if (status == STATUS_SUCCESS)
        {
            DebugPrint(("IoGetDeviceObjectPointer for %s returned %p %p\n", szHubName, pHubDevObj, pHubFileObj));

            if (    pHubDevObj->DriverObject
                &&  pHubDevObj->DriverObject->DriverName.Buffer
                &&  pHubDevObj->DriverObject->DriverName.Length
               )
            {
                for (int j=0;j<RT_ELEMENTS(lpszStandardControllerName);j++)
                {
                    if (!RtlCompareUnicodeString(&szStandardControllerName[j], &pHubDevObj->DriverObject->DriverName, TRUE /* case insensitive */))
                    {
                        PDEVICE_RELATIONS pDevRelations = NULL;

                        DebugPrint(("Associated driver %wZ -> related dev obj=%p\n", pHubDevObj->DriverObject->DriverName, IoGetRelatedDeviceObject(pHubFileObj)));

                        status = VBoxUSBQueryBusRelations(pHubDevObj, pHubFileObj, &pDevRelations);
                        if (    status == STATUS_SUCCESS
                            &&  pDevRelations)
                        {
                            for (unsigned k=0;k<pDevRelations->Count;k++)
                            {
                                USHORT usPDOVendorId, usPDOProductId, usPDORevision;

                                DebugPrint(("Found existing USB PDO %p\n", pDevRelations->Objects[k]));
                                VBoxUSBGetDeviceDescription(pDevRelations->Objects[k], &usPDOVendorId, &usPDOProductId, &usPDORevision);

                                if (    VBoxUSBDeviceIsCaptured(pDevRelations->Objects[k]) == fCaptured
                                    &&  usPDOVendorId   == usVendorId
                                    &&  usPDOProductId  == usProductId
                                    &&  usPDORevision   == usRevision)
                                {
                                    DebugPrint(("REPLUG device -> \n"));
                                    /* Simulate a device replug */
                                    status = VBoxUSBSendIOCTL(pDevRelations->Objects[k], IOCTL_INTERNAL_USB_CYCLE_PORT, NULL, 0);

                                    *pfReplugged = true;
                                }
                                ObDereferenceObject(pDevRelations->Objects[k]);
                                if (*pfReplugged == true)
                                    break;
                            }
                            ExFreePool(pDevRelations);
                        }
                        if (*pfReplugged == true)
                            break;
                    }
                }
            }
            ObDereferenceObject(pHubFileObj);
        }
        RtlFreeUnicodeString(&UnicodeName);
        if (*pfReplugged == true)
            break;
    }

    return STATUS_SUCCESS;
}

/**
 * Capture specified USB device
 *
 * @returns NT status code
 * @param   usVendorId      Vendor id
 * @param   usProductId     Product id
 * @param   usRevision      Revision
 */
NTSTATUS VBoxUSBReleaseDevice(USHORT usVendorId, USHORT usProductId, USHORT usRevision)
{
    NTSTATUS status;
    bool     fReplugged;

    DebugPrint(("VBoxUSBReleaseDevice\n"));

    DrvInstance.fDisableFilters = true;
    status = VBoxUSBReplugDevice(usVendorId, usProductId, usRevision, true, &fReplugged);
    if (    status != STATUS_SUCCESS
        ||  !fReplugged)
        DrvInstance.fDisableFilters = false;
    return status;
}

/**
 * Capture specified USB device
 *
 * @returns NT status code
 * @param   usVendorId      Vendor id
 * @param   usProductId     Product id
 * @param   usRevision      Revision
 */
NTSTATUS VBoxUSBGrabDevice(USHORT usVendorId, USHORT usProductId, USHORT usRevision)
{
    NTSTATUS status;
    bool     fReplugged;

    DebugPrint(("VBoxUSBGrabDevice\n"));

    DrvInstance.fForceGrab = true;
    status = VBoxUSBReplugDevice(usVendorId, usProductId, usRevision, false, &fReplugged);
    if (    status != STATUS_SUCCESS
        ||  !fReplugged)
        DrvInstance.fForceGrab = false;
    return status;
}

