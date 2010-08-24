/** @file
 *
 * VBox host drivers - USB drivers - Win32 USB filter driver
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
#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/assert.h>
#include <VBox/sup.h>

RT_C_DECLS_BEGIN
#include <ntddk.h>
RT_C_DECLS_END

#include <VBox/log.h>
#include <iprt/assert.h>
#include <VBox/usblib.h>

#include "USBFilter.h"

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

#define szBusQueryDeviceId       L"USB\\Vid_80EE&Pid_CAFE"
#define szBusQueryHardwareIDs    L"USB\\Vid_80EE&Pid_CAFE&Rev_0100\0USB\\Vid_80EE&Pid_CAFE\0\0"
#define szBusQueryCompatibleIDs  L"USB\\Class_ff&SubClass_00&Prot_00\0USB\\Class_ff&SubClass_00\0USB\\Class_ff\0\0"

#define szDeviceTextDescription          L"VirtualBox USB"


#define MAX_ATTACHED_USB_DEVICES        128

typedef struct
{
    bool            fAttached, fCaptured;

    PDEVICE_OBJECT  Pdo, Fdo;
} FLTUSB_DEVICE, *PFLTUSB_DEVICE;

typedef struct
{
    char            szVendor[MAX_VENDOR_NAME];
    char            szProduct[MAX_PRODUCT_NAME];
    char            szRevision[MAX_REVISION_NAME];
    uintptr_t       id;
    int             cActive;
} USBDEV_FILTER, *PUSBDEV_FILTER;

/* Device driver instance data */
typedef struct
{
    LONG            cUSBDevices;
    LONG            CaptureCount;

    LONG            cUSBChangeNotification;

    KSPIN_LOCK      lock;

    FLTUSB_DEVICE   USBDevice[MAX_ATTACHED_USB_DEVICES];

    LONG            cFilters;
    USBDEV_FILTER   aFilter[MAX_ATTACHED_USB_DEVICES];

} DRVINSTANCE, *PDRVINSTANCE;

DRVINSTANCE DrvInstance = {0};


#define ACQUIRE_LOCK()  \
    KIRQL oldIrql; \
    KeAcquireSpinLock(&DrvInstance.lock, &oldIrql);

#define RELEASE_LOCK() \
    KeReleaseSpinLock(&DrvInstance.lock, oldIrql);


/*
 * Internal functions
 */
static NTSTATUS VBoxAddFilter(PUSBSUP_FILTER pFilter);
static NTSTATUS VBoxRemoveFilter(PUSBSUP_FILTER pFilter);
static NTSTATUS VBoxClearFilters();
static bool     VBoxMatchFilter(WCHAR *pszDeviceId);


NTSTATUS _stdcall VBoxUSBInit()
{
    KeInitializeSpinLock(&DrvInstance.lock);
    return STATUS_SUCCESS;
}


/*++
Routine Description:
    A completion routine for use when calling the lower device objects to
    which our filter deviceobject is attached.

Arguments:

    DeviceObject - Pointer to deviceobject
    Irp          - Pointer to a PnP Irp.
    Context      - NULL
Return Value:

    NT Status is returned.

--*/

NTSTATUS _stdcall VBoxUSBPnPCompletion(DEVICE_OBJECT *fido, IRP *Irp, void *context)
{
    PDEVICE_EXTENSION  pdx    = (PDEVICE_EXTENSION) fido->DeviceExtension;
    PIO_STACK_LOCATION stack  = IoGetCurrentIrpStackLocation(Irp);

    UCHAR              type   = stack->MajorFunction;
    ULONG              dwControlCode = stack->Parameters.DeviceIoControl.IoControlCode;

    if (Irp->PendingReturned) {
        DebugPrint(("Pending IRP returned\n"));
        IoMarkIrpPending(Irp);
    }

    //
    // On the way up, pagable might become clear. Mimic the driver below us.
    //
    if (!(pdx->NextLowerDriver->Flags & DO_POWER_PAGABLE)) {

        fido->Flags &= ~DO_POWER_PAGABLE;
    }

    ULONG fcn = stack->MinorFunction;
    if (type == IRP_MJ_PNP && DrvInstance.CaptureCount > 0)
    {
#ifdef DEBUG
        const char * MinorFunctionName = PnPMinorFunctionString((UCHAR)fcn);
        if (MinorFunctionName != NULL)
            DebugPrint(("VBoxUSB - Finished %x %x: IRP_MJ_PNP (%s) rc=%x\n", fido, pdx, MinorFunctionName, Irp->IoStatus.Status));
        else
            DebugPrint(("VBoxUSB - Finished %x %x: IRP_MJ_PNP (0x%x) rc=%x\n", fido, pdx, fcn, Irp->IoStatus.Status));
#endif

        if (fcn == IRP_MN_QUERY_DEVICE_RELATIONS)
        {
            switch(stack->Parameters.QueryDeviceRelations.Type)
            {
            case BusRelations:
            {
                DebugPrint(("BusRelations\n"));

                if (Irp->IoStatus.Status == STATUS_SUCCESS)
                {
                    PDEVICE_RELATIONS pRel = (PDEVICE_RELATIONS)Irp->IoStatus.Information;
                    DebugPrint(("pRel = %08x\n", pRel));
                    if ((ULONG_PTR)pRel > 0x10000)
                    {
                        for (unsigned i=0;i<pRel->Count;i++)
                        {
                            DebugPrint(("PDO: %x\n", pRel->Objects[i]));
                            if (ListKnownPDO(pRel->Objects[i]) == false)
                            {
                                DebugPrint(("Hooking new PDO\n"));
                                FilterAddDevice(fido->DriverObject, pRel->Objects[i]);
                            }
                        }
                    }
                }
                break;
            }
            case TargetDeviceRelation:
                DebugPrint(("TargetDeviceRelation\n"));
                break;
            case RemovalRelations:
                DebugPrint(("RemovalRelations\n"));
                break;
            case EjectionRelations:
                DebugPrint(("EjectionRelations\n"));
                break;
            }
        }
        else
        if (fcn == IRP_MN_QUERY_ID)
        {
           if (Irp->IoStatus.Status == STATUS_SUCCESS)
           {
                WCHAR *pId = (WCHAR *)Irp->IoStatus.Information;
                WCHAR *pTmp;

                DebugPrint(("pId = %08x\n", pId));
                if ((ULONG_PTR)pId > 0x10000)
                {
                    switch(stack->Parameters.QueryDeviceRelations.Type)
                    {
                    case BusQueryInstanceID:
                        DebugPrint(("BusQueryInstanceID %ws\n", pId));
                        break;

                    case BusQueryDeviceID:
                        DebugPrint(("BusQueryDeviceID %ws\n", pId));

                        if (VBoxMatchFilter(pId) == true)
                        {
                            pId = (WCHAR *)ExAllocatePool(PagedPool, sizeof(szBusQueryDeviceId));
                            if (!pId)
                            {
                                AssertFailed();
                                break;
                            }
                            memcpy(pId, szBusQueryDeviceId, sizeof(szBusQueryDeviceId));
                            DebugPrint(("NEW BusQueryDeviceID %ws\n", pId));
                            ExFreePool((PVOID)Irp->IoStatus.Information);
                            Irp->IoStatus.Information = (ULONG_PTR)pId;

                            ListCaptureDevice(fido);
                        }
                        break;

                    case BusQueryHardwareIDs:
#ifdef DEBUG
                        while(*pId) //MULTI_SZ
                        {
                            DebugPrint(("BusQueryHardwareIDs %ws\n", pId));
                            while(*pId) pId++;
                            pId++;
                        }
#endif
                        if (VBoxMatchFilter((WCHAR *)Irp->IoStatus.Information) == true)
                        {
                            pTmp = pId = (WCHAR *)ExAllocatePool(PagedPool, sizeof(szBusQueryHardwareIDs));
                            if (!pId)
                            {
                                AssertFailed();
                                break;
                            }
                            memcpy(pId, szBusQueryHardwareIDs, sizeof(szBusQueryHardwareIDs));
    #ifdef DEBUG
                            while(*pTmp) //MULTI_SZ
                            {
                                DebugPrint(("NEW BusQueryHardwareIDs %ws\n", pTmp));
                                while(*pTmp) pTmp++;
                                pTmp++;
                            }
    #endif
                            ExFreePool((PVOID)Irp->IoStatus.Information);
                            Irp->IoStatus.Information = (ULONG_PTR)pId;

                            ListCaptureDevice(fido);
                        }
                        break;

                    case BusQueryCompatibleIDs:
#ifdef DEBUG
                        while(*pId) //MULTI_SZ
                        {
                            DebugPrint(("BusQueryCompatibleIDs %ws\n", pId));
                            while(*pId) pId++;
                            pId++;
                        }
#endif
                        if (ListDeviceIsCaptured(fido))
                        {
                            pTmp = pId = (WCHAR *)ExAllocatePool(PagedPool, sizeof(szBusQueryCompatibleIDs));
                            if (!pId)
                            {
                                AssertFailed();
                                break;
                            }
                            memcpy(pId, szBusQueryCompatibleIDs, sizeof(szBusQueryCompatibleIDs));
    #ifdef DEBUG
                            while(*pTmp) //MULTI_SZ
                            {
                                DebugPrint(("NEW BusQueryCompatibleIDs %ws\n", pTmp));
                                while(*pTmp) pTmp++;
                                pTmp++;
                            }
    #endif
                            ExFreePool((PVOID)Irp->IoStatus.Information);
                            Irp->IoStatus.Information = (ULONG_PTR)pId;
                        }
                        break;
                    }
                }
            }
        }
        else
        if (fcn == IRP_MN_QUERY_DEVICE_TEXT)
        {
            if (Irp->IoStatus.Status == STATUS_SUCCESS)
            {
                WCHAR *pId = (WCHAR *)Irp->IoStatus.Information;
                if ((ULONG_PTR)pId > 0x10000)
                {
                    switch(stack->Parameters.QueryDeviceText.DeviceTextType)
                    {
                    case DeviceTextLocationInformation:
                        DebugPrint(("DeviceTextLocationInformation %ws\n", pId));
                        break;
                    case DeviceTextDescription:
                        DebugPrint(("DeviceTextDescription %ws\n", pId));
                        if (ListDeviceIsCaptured(fido))
                        {
                            WCHAR *pId = (WCHAR *)ExAllocatePool(PagedPool, sizeof(szDeviceTextDescription));
                            if (!pId)
                            {
                                AssertFailed();
                                break;
                            }
                            memcpy(pId, szDeviceTextDescription, sizeof(szDeviceTextDescription));
                            DebugPrint(("NEW szDeviceTextDescription %ws\n", pId));
                            ExFreePool((PVOID)Irp->IoStatus.Information);
                            Irp->IoStatus.Information = (ULONG_PTR)pId;
                        }
                    }
                }
            }
        }
#ifdef DEBUG
        else
        if (fcn == IRP_MN_QUERY_CAPABILITIES)
        {
            if (Irp->IoStatus.Status == STATUS_SUCCESS)
            {
                PDEVICE_CAPABILITIES pCaps = stack->Parameters.DeviceCapabilities.Capabilities;
                if ((ULONG_PTR)pCaps > 0x10000)
                {
                    DebugPrint(("Caps.SilentInstall  = %d\n", pCaps->SilentInstall));
                    DebugPrint(("Caps.UniqueID       = %d\n", pCaps->UniqueID ));
                    DebugPrint(("Caps.Address        = %d\n", pCaps->Address ));
                    DebugPrint(("Caps.UINumber       = %d\n", pCaps->UINumber ));
#if 0
                    if (ListDeviceIsCaptured(fido))
                    {
                        pCaps->SilentInstall = 1;
                        DebugPrint(("NEW: Caps.SilentInstall  = %d\n", pCaps->SilentInstall));
                    }
#endif
                }
            }
        }
#endif
    }

    IoReleaseRemoveLock(&pdx->RemoveLock, Irp);
    return STATUS_CONTINUE_COMPLETION;
}

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
        Assert(sizeof(DrvInstance.cUSBChangeNotification) == sizeof(LONG));

        if (DrvInstance.cUSBChangeNotification > 0)
        {
            InterlockedDecrement((volatile LONG *)&DrvInstance.cUSBChangeNotification);
            pOut->fUSBChange = true;
        }
        else {
            pOut->fUSBChange = false;
        }

        if (pOut->fUSBChange)
            DebugPrint(("SUPUSBFLT_IOCTL_USB_CHANGE -> %d\n", pOut->fUSBChange));

        info = sizeof(*pOut);
        break;
    }

    case SUPUSBFLT_IOCTL_ENABLE_CAPTURE:
        DebugPrint(("SUPUSBFLT_IOCTL_ENABLE_CAPTURE\n"));
        break;

    case SUPUSBFLT_IOCTL_DISABLE_CAPTURE:
        DebugPrint(("SUPUSBFLT_IOCTL_DISABLE_CAPTURE\n"));
        break;

    case SUPUSBFLT_IOCTL_GET_NUM_DEVICES:
    {
        PUSBSUP_GETNUMDEV pOut = (PUSBSUP_GETNUMDEV)ioBuffer;

        DebugPrint(("SUPUSBFLT_IOCTL_GET_NUM_DEVICES\n"));
        if (!ioBuffer || outputBufferLength != sizeof(*pOut) || inputBufferLength != 0)
        {
            AssertMsgFailed(("SUPUSBFLT_IOCTL_GET_NUM_DEVICES: Invalid input/output sizes. cbIn=%d expected %d. cbOut=%d expected %d.\n",
                             inputBufferLength, 0, outputBufferLength, sizeof(*pOut)));
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        pOut->cUSBDevices = DrvInstance.cUSBDevices;
        info = sizeof(*pOut);
        break;
    }

    /* depricated */
    case SUPUSBFLT_IOCTL_IGNORE_DEVICE:
    {
        break;
    }

    case SUPUSBFLT_IOCTL_GET_VERSION:
    {
        PUSBSUP_VERSION pOut = (PUSBSUP_VERSION)ioBuffer;

        if (!ioBuffer || outputBufferLength != sizeof(*pOut) || inputBufferLength != 0)
        {
            AssertMsgFailed(("SUPUSBFLT_IOCTL_GET_VERSION: Invalid input/output sizes. cbIn=%d expected %d. cbOut=%d expected %d.\n",
                             inputBufferLength, 0, outputBufferLength, sizeof(*pOut)));
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        pOut->u32Major = USBFLT_MAJOR_VERSION;
        pOut->u32Minor = USBFLT_MINOR_VERSION;
        info = sizeof(*pOut);
        break;
    }

    case SUPUSBFLT_IOCTL_ADD_FILTER:
    {
        PUSBSUP_FILTER pIn = (PUSBSUP_FILTER)ioBuffer;
        PUSBSUP_FILTER pOut = pIn;

        if (!ioBuffer || inputBufferLength != sizeof(*pIn) || outputBufferLength != sizeof(*pOut))
        {
            AssertMsgFailed(("SUPUSBFLT_IOCTL_ADD_FILTER: Invalid input/output sizes. cbIn=%d expected %d. cbOut=%d expected %d.\n",
                             inputBufferLength, sizeof(pIn), outputBufferLength, 0));
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        DebugPrint(("SUPUSBFLT_IOCTL_ADD_FILTER %s %s %s\n", pIn->szVendor, pIn->szProduct, pIn->szRevision));
        VBoxAddFilter(pIn);
        pOut->id = pIn->id;
        info = sizeof(*pOut);
        break;
    }

    case SUPUSBFLT_IOCTL_REMOVE_FILTER:
    {
        PUSBSUP_FILTER pIn = (PUSBSUP_FILTER)ioBuffer;

        if (!ioBuffer || inputBufferLength != sizeof(*pIn))
        {
            AssertMsgFailed(("SUPUSBFLT_IOCTL_REMOVE_FILTER: Invalid input/output sizes. cbIn=%d expected %d. cbOut=%d expected %d.\n",
                             inputBufferLength, sizeof(pIn), outputBufferLength, 0));
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        DebugPrint(("SUPUSBFLT_IOCTL_REMOVE_FILTER %x\n", pIn->id));
        VBoxRemoveFilter(pIn);
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

    if (DrvInstance.CaptureCount == 0)
    {
        VBoxClearFilters();
    }
    if (DrvInstance.CaptureCount == 0 && DrvInstance.cUSBDevices > 0)
    {
        for (int i=0;i<MAX_ATTACHED_USB_DEVICES;i++)
        {
            if (DrvInstance.USBDevice[i].fAttached == true)
            {
                if (DrvInstance.USBDevice[i].fCaptured == true)
                {
                    PDEVICE_OBJECT Pdo = DrvInstance.USBDevice[i].Pdo;
                    RELEASE_LOCK();

                    DebugPrint(("Invalidate PDO %08x\n", Pdo));
////                    IoInvalidateDeviceRelations(Pdo, BusRelations);
                }
                else
                    RELEASE_LOCK();

                return STATUS_SUCCESS;
            }
        }
    }
    RELEASE_LOCK();
    return STATUS_SUCCESS;
}


static NTSTATUS VBoxAddFilter(PUSBSUP_FILTER pFilter)
{
    NTSTATUS rc = STATUS_SUCCESS;

    Assert(sizeof(DrvInstance.aFilter[0].szProduct)  == sizeof(pFilter->szProduct));
    Assert(sizeof(DrvInstance.aFilter[0].szVendor)   == sizeof(pFilter->szVendor));
    Assert(sizeof(DrvInstance.aFilter[0].szRevision) == sizeof(pFilter->szRevision));

    ACQUIRE_LOCK();
    /* We ignore duplicate filters as they must get a unique id */
    if (DrvInstance.cFilters < MAX_ATTACHED_USB_DEVICES)
    {
        int i;
        for (i=0;i<MAX_ATTACHED_USB_DEVICES;i++)
        {
            if (DrvInstance.aFilter[i].cActive == 0)
            {
                DrvInstance.aFilter[i].cActive = 1;
                strncpy(DrvInstance.aFilter[i].szProduct, pFilter->szProduct, sizeof(DrvInstance.aFilter[i].szProduct));
                strncpy(DrvInstance.aFilter[i].szVendor, pFilter->szVendor, sizeof(DrvInstance.aFilter[i].szVendor));
                strncpy(DrvInstance.aFilter[i].szRevision, pFilter->szRevision, sizeof(DrvInstance.aFilter[i].szRevision));
                strupr(DrvInstance.aFilter[i].szProduct);
                strupr(DrvInstance.aFilter[i].szVendor);
                strupr(DrvInstance.aFilter[i].szRevision);
                DrvInstance.aFilter[i].id = i;
                pFilter->id = i;
                break;
            }
        }
        if (i != MAX_ATTACHED_USB_DEVICES)
        {
            InterlockedIncrement(&DrvInstance.cFilters);
        }
        else
        {
            AssertFailed();
            rc = STATUS_NO_MEMORY;
        }

    }
    RELEASE_LOCK();
    return rc;
}

static NTSTATUS VBoxRemoveFilter(PUSBSUP_FILTER pFilter)
{
    NTSTATUS rc = STATUS_SUCCESS;

    ACQUIRE_LOCK();
    Assert(DrvInstance.cFilters);
    Assert(sizeof(DrvInstance.aFilter[0].szProduct)  == sizeof(pFilter->szProduct));
    Assert(sizeof(DrvInstance.aFilter[0].szVendor)   == sizeof(pFilter->szVendor));
    Assert(sizeof(DrvInstance.aFilter[0].szRevision) == sizeof(pFilter->szRevision));

    if (DrvInstance.cFilters)
    {
        int i;
        for (i=0;i<MAX_ATTACHED_USB_DEVICES;i++)
        {
            if (   DrvInstance.aFilter[i].cActive > 0
                && DrvInstance.aFilter[i].id == pFilter->id)
            {
                DrvInstance.aFilter[i].cActive--;
                /* simplify the lookup later on; no need for the loop */
                Assert(DrvInstance.aFilter[i].id == i);
                Assert(DrvInstance.aFilter[i].cActive == 0);
                break;
            }
        }
        if (i != MAX_ATTACHED_USB_DEVICES)
        {
            InterlockedDecrement(&DrvInstance.cFilters);
        }
        else
        {
            AssertFailed();
            rc = STATUS_NO_SUCH_FILE;
        }

    }
    RELEASE_LOCK();
    return rc;
}

static NTSTATUS VBoxClearFilters()
{
    /* Clear all active filters */
    memset(DrvInstance.aFilter, 0, sizeof(DrvInstance.aFilter));
    return STATUS_SUCCESS;
}

char * _cdecl strupr(char *psz)
{
    for(int i=0;psz[i] != 0;i++)
    {
        psz[i] = RtlUpperChar(psz[i]);
    }
    return psz;
}

static bool MatchFilter(char *pszString, char *pszStringFilter)
{
    for (int i=0;i<4;i++)
    {
        if (pszStringFilter[i] == 0)
            break;

        if (pszStringFilter[i] == ' ')
            continue;

        if (pszStringFilter[i] != pszString[i])
            return false;
    }
    return true;
}

static bool VBoxMatchFilter(WCHAR *pszDeviceId)
{
    //#define szBusQueryDeviceId       L"USB\\Vid_80EE&Pid_CAFE"
    char     szProduct[4];
    char     szVendor[4];
//    char     szRevision[4];

    if (RtlCompareMemory(pszDeviceId, L"USB\\", 4*sizeof(WCHAR)) != 4*sizeof(WCHAR))
    {
        return false;
    }

    while(*pszDeviceId != 0 && *pszDeviceId != '_')
        pszDeviceId++;
    if(*pszDeviceId == 0)
        return false;
    pszDeviceId++;

    /* Vid_ skipped */
    szVendor[0] = RtlUpperChar((CHAR)pszDeviceId[0]);
    szVendor[1] = RtlUpperChar((CHAR)pszDeviceId[1]);
    szVendor[2] = RtlUpperChar((CHAR)pszDeviceId[2]);
    szVendor[3] = RtlUpperChar((CHAR)pszDeviceId[3]);

    while(*pszDeviceId != 0 && *pszDeviceId != '_')
        pszDeviceId++;
    if(*pszDeviceId == 0)
        return false;
    pszDeviceId++;

    /* Pid_ skipped */
    szProduct[0] = RtlUpperChar((CHAR)pszDeviceId[0]);
    szProduct[1] = RtlUpperChar((CHAR)pszDeviceId[1]);
    szProduct[2] = RtlUpperChar((CHAR)pszDeviceId[2]);
    szProduct[3] = RtlUpperChar((CHAR)pszDeviceId[3]);

    ACQUIRE_LOCK();
    for (int i=0;i<MAX_ATTACHED_USB_DEVICES;i++)
    {
        if (    DrvInstance.aFilter[i].cActive > 0
             && MatchFilter(szVendor,  DrvInstance.aFilter[i].szVendor) == true
             && MatchFilter(szProduct, DrvInstance.aFilter[i].szProduct) == true
            )
        {
            RELEASE_LOCK();
            return true;
        }
    }
    RELEASE_LOCK();
    return false;
}

int ListAddDevice(PDEVICE_OBJECT pdo, PDEVICE_OBJECT fdo)
{
    ACQUIRE_LOCK()
    for (int i=0;i<MAX_ATTACHED_USB_DEVICES;i++)
    {
        if (DrvInstance.USBDevice[i].fAttached == false)
        {
            DebugPrint(("ListAddDevice %08x %08x\n", pdo, fdo));
            DrvInstance.USBDevice[i].fAttached = true;
            DrvInstance.USBDevice[i].Pdo       = pdo;
            DrvInstance.USBDevice[i].Fdo       = fdo;
            DrvInstance.USBDevice[i].fCaptured = false;
            Assert(DrvInstance.cUSBDevices <= MAX_ATTACHED_USB_DEVICES);

            RELEASE_LOCK();
            return true;
        }
    }
    RELEASE_LOCK();
    return false;
}

int ListCaptureDevice(PDEVICE_OBJECT fdo)
{
    Assert(fdo);
    ACQUIRE_LOCK();

    for (int i=0;i<MAX_ATTACHED_USB_DEVICES;i++)
    {
        if (DrvInstance.USBDevice[i].Fdo == fdo)
        {
            if (DrvInstance.USBDevice[i].fCaptured == false)
            {
                Assert(DrvInstance.CaptureCount);
                Assert(DrvInstance.USBDevice[i].fAttached);

                DebugPrint(("ListCaptureDevice %08x\n", fdo));
                if (DrvInstance.CaptureCount > 0)
                {
                    DrvInstance.USBDevice[i].fCaptured = true;
                    DrvInstance.cUSBDevices++;
                }
            }
            //else already captured
            break;
        }
    }
    RELEASE_LOCK();
    return true;
}

int ListKnownPDO(PDEVICE_OBJECT pdo)
{
    Assert(pdo);
    ACQUIRE_LOCK();
    for (int i=0;i<MAX_ATTACHED_USB_DEVICES;i++)
    {
        if (DrvInstance.USBDevice[i].Pdo == pdo)
        {
            RELEASE_LOCK();
            return true;
        }
    }
    RELEASE_LOCK();
    return false;
}

int ListStartDevice(PDEVICE_OBJECT fdo)
{
    Assert(fdo);
    ACQUIRE_LOCK();
    for (int i=0;i<MAX_ATTACHED_USB_DEVICES;i++)
    {
        if (DrvInstance.USBDevice[i].Fdo == fdo)
        {
            if (DrvInstance.USBDevice[i].fCaptured == true)
            {
                DebugPrint(("Signal USB change ADD\n"));
                InterlockedExchange(&DrvInstance.cUSBChangeNotification, DrvInstance.CaptureCount);
            }
            RELEASE_LOCK();
            return true;
        }
    }
    RELEASE_LOCK();
    return false;
}

int ListRemoveDevice(PDEVICE_OBJECT fdo)
{
    ACQUIRE_LOCK();
    for (int i=0;i<MAX_ATTACHED_USB_DEVICES;i++)
    {
        if (DrvInstance.USBDevice[i].Fdo == fdo)
        {
            DebugPrint(("ListRemoveDevice %08x\n", fdo));
            DrvInstance.USBDevice[i].fAttached = false;
            DrvInstance.USBDevice[i].Pdo       = NULL;
            DrvInstance.USBDevice[i].Fdo       = NULL;

            if (DrvInstance.USBDevice[i].fCaptured == true)
            {
                InterlockedDecrement(&DrvInstance.cUSBDevices);
                Assert(DrvInstance.cUSBDevices >= 0);
                DebugPrint(("Signal USB change REMOVE\n"));
                InterlockedExchange(&DrvInstance.cUSBChangeNotification, DrvInstance.CaptureCount);

                DrvInstance.USBDevice[i].fCaptured = false;
            }

            RELEASE_LOCK();
            return true;
        }
    }
    RELEASE_LOCK();
    return false;
}

int ListDeviceIsCaptured(PDEVICE_OBJECT fdo)
{
    bool ret;

    ACQUIRE_LOCK();
    for (int i=0;i<MAX_ATTACHED_USB_DEVICES;i++)
    {
        if (DrvInstance.USBDevice[i].Fdo == fdo)
        {
            ret = DrvInstance.USBDevice[i].fCaptured;
            RELEASE_LOCK();
            return ret;
        }
    }
    RELEASE_LOCK();
    return false;
}

