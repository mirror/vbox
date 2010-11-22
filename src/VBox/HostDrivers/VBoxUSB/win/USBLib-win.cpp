/* $Id$ */
/** @file
 * USBLIB - USB support library interface, Windows.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_USBPROXY
#include <windows.h>

#include <VBox/sup.h>
#include <VBox/types.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <iprt/path.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <VBox/log.h>
#include <VBox/usblib.h>
#include <VBox/usb.h>
#include "../USBLibInternal.h"
#include <stdio.h>
#pragma warning (disable:4200) /* shuts up the empty array member warnings */
#include <setupapi.h>
#include <usbdi.h>
#include <hidsdi.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Flags whether or not we started the service. */
static bool     g_fStartedService = false;
static char     completeDeviceName[256] = "";  //generated from the GUID registered by the driver itself

typedef struct
{
    char    szName[512];
    char    szDriverRegName[512];
} USBDEV, *PUSBDEV;

static PUSBDEV  g_pUSBDev = NULL;
static uint32_t g_cMaxDevices = 0;
static uint32_t g_cUSBStateChange = 0;

static HANDLE   g_hUSBMonitor = INVALID_HANDLE_VALUE;


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @def VBOX_WITH_ANNOYING_USB_ASSERTIONS
 * Enable this to get assertions on various failures.
 */
//#define VBOX_WITH_ANNOYING_USB_ASSERTIONS
#ifdef DOXYGEN_RUNNING
# define VBOX_WITH_ANNOYING_USB_ASSERTIONS
#endif

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/

bool ValidateUSBDevice(char *pszName)
{
    HANDLE hOut = INVALID_HANDLE_VALUE;

    hOut = CreateFile(pszName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM, NULL);

    if (INVALID_HANDLE_VALUE == hOut)
    {
        Log(( "USB: FAILED to open %s\n", pszName));
        goto failure;
    }
    else
    {
        /*
         * Check the version
         */
        USBSUP_VERSION version = {0};
        DWORD          cbReturned = 0;

        if (!DeviceIoControl(hOut, SUPUSB_IOCTL_GET_VERSION, NULL, 0,&version, sizeof(version),  &cbReturned, NULL))
        {
            Log(("DeviceIoControl SUPUSB_IOCTL_GET_VERSION failed with rc=%d\n", GetLastError()));
            goto failure;
        }

        if (version.u32Major != USBDRV_MAJOR_VERSION ||
            version.u32Minor <  USBDRV_MINOR_VERSION)
        {
            Log(("USB: Invalid version %d:%d vs %d:%d\n", version.u32Major, version.u32Minor, USBDRV_MAJOR_VERSION, USBDRV_MINOR_VERSION));
            goto failure;
        }
        if (!DeviceIoControl(hOut, SUPUSB_IOCTL_IS_OPERATIONAL, NULL, 0, NULL, NULL, &cbReturned, NULL))
        {
            Log(("DeviceIoControl SUPUSB_IOCTL_IS_OPERATIONAL failed with rc=%d\n", GetLastError()));
            goto failure;
        }

    }
    CloseHandle(hOut);
    return true;

failure:
    if (hOut != INVALID_HANDLE_VALUE)
        CloseHandle(hOut);

    return false;
}

bool OpenOneDevice (HDEVINFO HardwareDeviceInfo,
                    PSP_DEVICE_INTERFACE_DATA   DeviceInfoData,
                    USBDEV *pUsbDev)
{
    PSP_DEVICE_INTERFACE_DETAIL_DATA     functionClassDeviceData = NULL;
    ULONG                                predictedLength = 0;
    ULONG                                requiredLength = 0;
    SP_DEVINFO_DATA                      devInfo;

    //
    // allocate a function class device data structure to receive the
    // goods about this particular device.
    //
    SetupDiGetDeviceInterfaceDetail (
            HardwareDeviceInfo,
            DeviceInfoData,
            NULL, // probing so no output buffer yet
            0, // probing so output buffer length of zero
            &requiredLength,
            NULL); // not interested in the specific dev-node

    predictedLength = requiredLength;

    functionClassDeviceData = (PSP_DEVICE_INTERFACE_DETAIL_DATA) RTMemAllocZ(predictedLength);
    if(NULL == functionClassDeviceData)
    {
        return false;
    }
    functionClassDeviceData->cbSize = sizeof (SP_DEVICE_INTERFACE_DETAIL_DATA);

    //
    // Retrieve the information from Plug and Play.
    //
    devInfo.cbSize = sizeof(devInfo);
    if (! SetupDiGetDeviceInterfaceDetail(HardwareDeviceInfo, DeviceInfoData,
                                          functionClassDeviceData, predictedLength,
                                          &requiredLength, &devInfo))
    {
        goto failure;
    }

    strcpy(pUsbDev->szName, functionClassDeviceData->DevicePath) ;
    Log(( "USB: Attempting to open %s\n", pUsbDev->szName));

    /* Query the registry path of the associated driver */
    requiredLength = 0;
    SetupDiGetDeviceRegistryProperty(HardwareDeviceInfo, &devInfo, SPDRP_DRIVER, NULL, NULL, 0, &requiredLength);
    Assert(sizeof(pUsbDev->szDriverRegName) >= requiredLength);
    if (requiredLength && sizeof(pUsbDev->szDriverRegName) >= requiredLength)
    {
        predictedLength = requiredLength;

        if (!SetupDiGetDeviceRegistryProperty(HardwareDeviceInfo, &devInfo, SPDRP_DRIVER, NULL, (PBYTE)pUsbDev->szDriverRegName, predictedLength, &requiredLength))
            goto failure;

        Log(("USB: Driver name %s\n", pUsbDev->szDriverRegName));
    }

    bool rc = ValidateUSBDevice(functionClassDeviceData->DevicePath);
    RTMemFree(functionClassDeviceData);

    return rc;

failure:
    if (functionClassDeviceData)
        RTMemFree(functionClassDeviceData);

    return false;
}

bool OpenUsbDevices(LPGUID  pGuid, uint32_t *pcNumDevices)
{
    HDEVINFO                 hardwareDeviceInfo;
    SP_DEVICE_INTERFACE_DATA deviceInfoData;
    USBDEV                   usbDev;

    //
    // Open a handle to the plug and play dev node.
    // SetupDiGetClassDevs() returns a device information set that contains info on all
    // installed devices of a specified class.
    //
    hardwareDeviceInfo = SetupDiGetClassDevs (
                           pGuid,
                           NULL, // Define no enumerator (global)
                           NULL, // Define no
                           (DIGCF_PRESENT | // Only Devices present
                            DIGCF_DEVICEINTERFACE)); // Function class devices.

    deviceInfoData.cbSize = sizeof (SP_DEVICE_INTERFACE_DATA);

    int j = 0, i = 0;

    while (true)
    {
        // SetupDiEnumDeviceInterfaces() returns information about device interfaces
        // exposed by one or more devices. Each call returns information about one interface;
        // the routine can be called repeatedly to get information about several interfaces
        // exposed by one or more devices.

        Log(("OpenUsbDevices: SetupDiEnumDeviceInterfaces\n"));
        if (SetupDiEnumDeviceInterfaces (hardwareDeviceInfo,
                                         0, // We don't care about specific PDOs
                                         pGuid,
                                         i,
                                         &deviceInfoData))
        {
            if (OpenOneDevice (hardwareDeviceInfo, &deviceInfoData, &usbDev) == true)
            {
                uint32_t z;
                for (z = 0; z < g_cMaxDevices; z++)
                {
                    if (g_pUSBDev[z].szName[0] == 0)
                    {
                        memcpy(&g_pUSBDev[z], &usbDev, sizeof(usbDev));
                        Log(("Captured device %s\n", g_pUSBDev[z].szName));
                        j++;
                        break;
                    }
                }
                AssertMsg(z < g_cMaxDevices, ("z=%d g_cMaxDevices=%d\n", z, g_cMaxDevices));
            }
        }
        else
        {
            if (ERROR_NO_MORE_ITEMS == GetLastError())
            {
                Log(("OpenUsbDevices: No more items\n"));
                break;
            }
        }
        i++;
    }

    *pcNumDevices = j;

    // SetupDiDestroyDeviceInfoList() destroys a device information set
    // and frees all associated memory.

    SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);
    return true;
}

/**
 * Initialize the OS specific part of the library.
 * On Win32 this involves:
 *      - registering the device driver
 *      - start device driver.
 *      - open driver.
 *
 * @returns VBox status code
 */
static int usblibEnumDevices(uint32_t cNumNewDevices, uint32_t *pcNumDevices)
{
    if (g_pUSBDev)
        RTMemFree(g_pUSBDev);
    g_pUSBDev         = NULL;
    g_cMaxDevices     = 0;
    *pcNumDevices   = 0;

    if (cNumNewDevices == 0)
        return 0;   /* nothing to do */

    g_pUSBDev = (PUSBDEV)RTMemAllocZ(cNumNewDevices*sizeof(USBDEV));
    if (!g_pUSBDev)
    {
        AssertFailed();
        return VERR_NO_MEMORY;
    }
    g_cMaxDevices = cNumNewDevices;

    if (OpenUsbDevices((LPGUID)&GUID_CLASS_VBOXUSB, pcNumDevices) == false)
    {
        AssertFailed();
        return VERR_INTERNAL_ERROR;
    }
    AssertMsg(*pcNumDevices <= cNumNewDevices, ("cNumDevices = %d, cNumNewDevices = %d\n", *pcNumDevices, cNumNewDevices));
    return VINF_SUCCESS;
}

/**
 * Returns the nth USB device name
 *
 * @returns NULL on failure, otherwise the requested name
 */
char  *usblibQueryDeviceName(uint32_t idxDev)
{
    int j=0;

    Assert(idxDev < g_cMaxDevices);
    for (uint32_t i=0; i<g_cMaxDevices; i++)
    {
        if (g_pUSBDev[i].szName[0])
        {
            if (j == idxDev)
                return g_pUSBDev[i].szName;
            j++;
        }
    }

    Log(("USB: usblibQueryHandle returned -1; g_cMaxDevices = %d\n", g_cMaxDevices));
    return NULL;
}

/**
 * Returns the nth USB device registry path
 *
 * @returns NULL on failure, otherwise the requested name
 */
char  *usblibQueryDeviceRegPath(uint32_t idxDev)
{
    int j=0;

    Assert(idxDev < g_cMaxDevices);
    for (uint32_t i=0; i<g_cMaxDevices; i++)
    {
        if (g_pUSBDev[i].szName[0])
        {
            if (j == idxDev)
                return g_pUSBDev[i].szDriverRegName;
            j++;
        }
    }

    Log(("USB: usblibQueryHandle returned -1; g_cMaxDevices = %d\n", g_cMaxDevices));
    return NULL;
}

/**
 * Converts a supdrv error code to an nt status code.
 *
 * @returns corresponding SUPDRV_ERR_*.
 * @param   rc  Win32 error code.
 */
static int     suplibConvertWin32Err(int rc)
{
    /* Conversion program (link with ntdll.lib from ddk):
        #define _WIN32_WINNT 0x0501
        #include <windows.h>
        #include <ntstatus.h>
        #include <winternl.h>
        #include <stdio.h>

        int main()
        {
            #define CONVERT(a)  printf(#a " %#x -> %d\n", a, RtlNtStatusToDosError((a)))
            CONVERT(STATUS_SUCCESS);
            CONVERT(STATUS_NOT_SUPPORTED);
            CONVERT(STATUS_INVALID_PARAMETER);
            CONVERT(STATUS_ACCESS_DENIED);
            CONVERT(STATUS_INVALID_HANDLE);
            CONVERT(STATUS_INVALID_ADDRESS);
            CONVERT(STATUS_NOT_LOCKED);
            CONVERT(STATUS_IMAGE_ALREADY_LOADED);
            return 0;
        }
     */

    switch (rc)
    {
        //case 0:                             return STATUS_SUCCESS;
        case 0:                             return 0;
        //case SUPDRV_ERR_GENERAL_FAILURE:    return STATUS_NOT_SUPPORTED;
        case ERROR_NOT_SUPPORTED:           return VERR_GENERAL_FAILURE;
        //case SUPDRV_ERR_INVALID_PARAM:      return STATUS_INVALID_PARAMETER;
        case ERROR_INVALID_PARAMETER:       return VERR_INVALID_PARAMETER;
        //case SUPDRV_ERR_INVALID_MAGIC:      return STATUS_ACCESS_DENIED;
        case ERROR_ACCESS_DENIED:           return VERR_INVALID_MAGIC;
        //case SUPDRV_ERR_INVALID_HANDLE:     return STATUS_INVALID_HANDLE;
        case ERROR_INVALID_HANDLE:          return VERR_INVALID_HANDLE;
        //case SUPDRV_ERR_INVALID_POINTER:    return STATUS_INVALID_ADDRESS;
        case ERROR_UNEXP_NET_ERR:           return VERR_INVALID_POINTER;
        //case SUPDRV_ERR_LOCK_FAILED:        return STATUS_NOT_LOCKED;
        case ERROR_NOT_LOCKED:              return VERR_LOCK_FAILED;
        //case SUPDRV_ERR_ALREADY_LOADED:     return STATUS_IMAGE_ALREADY_LOADED;
        case ERROR_SERVICE_ALREADY_RUNNING: return VERR_ALREADY_LOADED;
    }

    return VERR_GENERAL_FAILURE;
}



//*****************************************************************************
// D E F I N E S
//*****************************************************************************

#define NUM_HCS_TO_CHECK 10

#include <cfgmgr32.h>
#include "usbdesc.h"
//
// Structure used to build a linked list of String Descriptors
// retrieved from a device.
//

typedef struct _STRING_DESCRIPTOR_NODE
{
    struct _STRING_DESCRIPTOR_NODE *Next;
    UCHAR                           DescriptorIndex;
    USHORT                          LanguageID;
    USB_STRING_DESCRIPTOR           StringDescriptor[0];
} STRING_DESCRIPTOR_NODE, *PSTRING_DESCRIPTOR_NODE;

//
// Structures associated with TreeView items through the lParam.  When an item
// is selected, the lParam is retrieved and the structure it which it points
// is used to display information in the edit control.
//

typedef struct
{
    PUSB_NODE_INFORMATION               HubInfo;        // NULL if not a HUB

    PCHAR                               HubName;        // NULL if not a HUB

    PUSB_NODE_CONNECTION_INFORMATION_EX ConnectionInfo; // NULL if root HUB

    PUSB_DESCRIPTOR_REQUEST             ConfigDesc;     // NULL if root HUB

    PSTRING_DESCRIPTOR_NODE             StringDescs;

} USBDEVICEINFO, *PUSBDEVICEINFO;

//*****************************************************************************
// L O C A L    F U N C T I O N    P R O T O T Y P E S
//*****************************************************************************

VOID
EnumerateHub (
    PCHAR                               HubName,
    PUSB_NODE_CONNECTION_INFORMATION_EX ConnectionInfo,
    PUSB_DESCRIPTOR_REQUEST             ConfigDesc,
    PSTRING_DESCRIPTOR_NODE             StringDescs,
    PCHAR                               DeviceDesc
);

VOID
EnumerateHubPorts (
    HANDLE      hHubDevice,
    ULONG       NumPorts,
    const char *pszHubName
);

PCHAR GetRootHubName (
    HANDLE HostController
);

PCHAR GetExternalHubName (
    HANDLE  Hub,
    ULONG   ConnectionIndex
);

PCHAR GetHCDDriverKeyName (
    HANDLE  HCD
);

PCHAR GetDriverKeyName (
    HANDLE  Hub,
    ULONG   ConnectionIndex
);

PUSB_DESCRIPTOR_REQUEST
GetConfigDescriptor (
    HANDLE  hHubDevice,
    ULONG   ConnectionIndex,
    UCHAR   DescriptorIndex
);

BOOL
AreThereStringDescriptors (
    PUSB_DEVICE_DESCRIPTOR          DeviceDesc,
    PUSB_CONFIGURATION_DESCRIPTOR   ConfigDesc
);

PSTRING_DESCRIPTOR_NODE
GetAllStringDescriptors (
    HANDLE                          hHubDevice,
    ULONG                           ConnectionIndex,
    PUSB_DEVICE_DESCRIPTOR          DeviceDesc,
    PUSB_CONFIGURATION_DESCRIPTOR   ConfigDesc
);

PSTRING_DESCRIPTOR_NODE
GetStringDescriptor (
    HANDLE  hHubDevice,
    ULONG   ConnectionIndex,
    UCHAR   DescriptorIndex,
    USHORT  LanguageID
);

PSTRING_DESCRIPTOR_NODE
GetStringDescriptors (
    HANDLE  hHubDevice,
    ULONG   ConnectionIndex,
    UCHAR   DescriptorIndex,
    ULONG   NumLanguageIDs,
    USHORT  *LanguageIDs,
    PSTRING_DESCRIPTOR_NODE StringDescNodeTail
);

PCHAR DriverNameToDeviceDesc (PCHAR DriverName);

//*****************************************************************************
// G L O B A L S    P R I V A T E    T O    T H I S    F I L E
//*****************************************************************************

PCHAR ConnectionStatuses[] =
{
    "NoDeviceConnected",
    "DeviceConnected",
    "DeviceFailedEnumeration",
    "DeviceGeneralFailure",
    "DeviceCausedOvercurrent",
    "DeviceNotEnoughPower"
};

static ULONG TotalDevicesConnected;

static BOOL gDoConfigDesc = TRUE;
static int TotalHubs;
static PUSBDEVICE *ppDeviceList = NULL;

PCHAR ConnectionStatuses[];

VOID AddLeaf(PUSBDEVICEINFO info, PCHAR pszLeafName, PCHAR pszDriverKeyName, const char *pszHubName, ULONG iHubPort)
{
    Log3(("usbproxy:AddLeaf: pszDriverKeyName=%s pszLeafName=%s pszHubName=%s iHubPort=%d\n", pszDriverKeyName, pszLeafName, pszHubName, iHubPort));
    PUSBDEVICE pDevice = (PUSBDEVICE)RTMemAllocZ(sizeof(USBDEVICE));

    if (pDevice)
    {
        Assert(info->ConnectionInfo);

        if (info->ConnectionInfo)
        {
            PSTRING_DESCRIPTOR_NODE pStrDesc;
            char **pString;

            pDevice->bcdUSB             = info->ConnectionInfo->DeviceDescriptor.bcdUSB;
            pDevice->bDeviceClass       = info->ConnectionInfo->DeviceDescriptor.bDeviceClass;
            pDevice->bDeviceSubClass    = info->ConnectionInfo->DeviceDescriptor.bDeviceSubClass;
            pDevice->bDeviceProtocol    = info->ConnectionInfo->DeviceDescriptor.bDeviceProtocol;
            pDevice->idVendor           = info->ConnectionInfo->DeviceDescriptor.idVendor;
            pDevice->idProduct          = info->ConnectionInfo->DeviceDescriptor.idProduct;
            pDevice->bcdDevice          = info->ConnectionInfo->DeviceDescriptor.bcdDevice;
            pDevice->bBus               = 0; /** @todo figure out bBus on windows... */
            pDevice->bPort              = iHubPort;
            /** @todo check which devices are used for primary input (keyboard & mouse) */
            if (!pszDriverKeyName || *pszDriverKeyName == 0)
                pDevice->enmState       = USBDEVICESTATE_UNUSED;
            else
                pDevice->enmState       = USBDEVICESTATE_USED_BY_HOST_CAPTURABLE;
            pDevice->enmSpeed           = USBDEVICESPEED_UNKNOWN;

            pDevice->pszAddress         = RTStrDup(pszDriverKeyName);
            pDevice->pszHubName         = RTStrDup(pszHubName);
            pDevice->bNumConfigurations = 0;
            pDevice->u64SerialHash      = 0;
            pStrDesc                    = info->StringDescs;
            while (pStrDesc)
            {
                pString = NULL;
                if (    info->ConnectionInfo->DeviceDescriptor.iManufacturer
                    &&  pStrDesc->DescriptorIndex == info->ConnectionInfo->DeviceDescriptor.iManufacturer)
                {
                    pString = (char **)&pDevice->pszManufacturer;
                }
                else
                if (    info->ConnectionInfo->DeviceDescriptor.iProduct
                    &&  pStrDesc->DescriptorIndex == info->ConnectionInfo->DeviceDescriptor.iProduct)
                {
                    pString = (char **)&pDevice->pszProduct;
                }
                else
                if (    info->ConnectionInfo->DeviceDescriptor.iSerialNumber
                    &&  pStrDesc->DescriptorIndex == info->ConnectionInfo->DeviceDescriptor.iSerialNumber)
                {
                    pString = (char **)&pDevice->pszSerialNumber;
                }
                if (pString)
                {
                    char *pStringUTF8 = NULL;
                    RTUtf16ToUtf8((PCRTUTF16)&pStrDesc->StringDescriptor->bString[0], &pStringUTF8);
                    RTStrUtf8ToCurrentCP(pString, pStringUTF8);
                    RTStrFree(pStringUTF8);
                    if (pStrDesc->DescriptorIndex == info->ConnectionInfo->DeviceDescriptor.iSerialNumber)
                    {
                        pDevice->u64SerialHash = USBLibHashSerial(pDevice->pszSerialNumber);
                    }
                }

                pStrDesc = pStrDesc->Next;
            }
            if (*ppDeviceList == NULL)
            {
                pDevice->pNext = NULL;
                *ppDeviceList = pDevice;
            }
            else
            {
                pDevice->pNext = *ppDeviceList;
                *ppDeviceList = pDevice;
            }
        }
        return;
    }
    AssertFailed();
}

//*****************************************************************************
//
// EnumerateHostController()
//
// hTreeParent - Handle of the TreeView item under which host controllers
// should be added.
//
//*****************************************************************************

VOID
EnumerateHostController (
    HANDLE     hHCDev,
    PCHAR      leafName
)
{
    PCHAR       driverKeyName;
    PCHAR       deviceDesc;
    PCHAR       rootHubName;

    driverKeyName = GetHCDDriverKeyName(hHCDev);

    if (driverKeyName)
    {
        deviceDesc = DriverNameToDeviceDesc(driverKeyName);

        if (deviceDesc)
        {
            leafName = deviceDesc;
        }

        RTMemFree(driverKeyName);
    }

    rootHubName = GetRootHubName(hHCDev);

    if (rootHubName != NULL)
    {
        EnumerateHub(rootHubName,
                     NULL,      // ConnectionInfo
                     NULL,      // ConfigDesc
                     NULL,      // StringDescs
                     "RootHub"  // DeviceDesc
                    );
    }
}


//*****************************************************************************
//
// EnumerateHostControllers()
//
// hTreeParent - Handle of the TreeView item under which host controllers
// should be added.
//
//*****************************************************************************

void   usbLibEnumerateHostControllers(PUSBDEVICE *ppDevices, uint32_t *DevicesConnected)
{
    char        HCName[16];
    int         HCNum;
    HANDLE      hHCDev;
    PCHAR       leafName;

    TotalDevicesConnected = 0;
    TotalHubs = 0;
    ppDeviceList = ppDevices;

    // Iterate over some Host Controller names and try to open them.
    //
    for (HCNum = 0; HCNum < NUM_HCS_TO_CHECK; HCNum++)
    {
        sprintf(HCName, "\\\\.\\HCD%d", HCNum);

        hHCDev = CreateFile(HCName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

        // If the handle is valid, then we've successfully opened a Host
        // Controller.  Display some info about the Host Controller itself,
        // then enumerate the Root Hub attached to the Host Controller.
        //
        if (hHCDev != INVALID_HANDLE_VALUE)
        {
            leafName = HCName + sizeof("\\\\.\\") - sizeof("");

            EnumerateHostController(hHCDev,
                                    leafName);

            CloseHandle(hHCDev);
        }
    }

    ppDeviceList = NULL;
    *DevicesConnected = TotalDevicesConnected - TotalHubs;
}

//*****************************************************************************
//
// EnumerateHub()
//
// hTreeParent - Handle of the TreeView item under which this hub should be
// added.
//
// HubName - Name of this hub.  This pointer is kept so the caller can neither
// free nor reuse this memory.
//
// ConnectionInfo - NULL if this is a root hub, else this is the connection
// info for an external hub.  This pointer is kept so the caller can neither
// free nor reuse this memory.
//
// ConfigDesc - NULL if this is a root hub, else this is the Configuration
// Descriptor for an external hub.  This pointer is kept so the caller can
// neither free nor reuse this memory.
//
//*****************************************************************************

VOID
EnumerateHub (
    PCHAR                               HubName,
    PUSB_NODE_CONNECTION_INFORMATION_EX ConnectionInfo,
    PUSB_DESCRIPTOR_REQUEST             ConfigDesc,
    PSTRING_DESCRIPTOR_NODE             StringDescs,
    PCHAR                               DeviceDesc
)
{
    HANDLE          hHubDevice;
    PCHAR           deviceName;
    BOOL            success;
    ULONG           nBytes;
    PUSBDEVICEINFO  info;
    CHAR            leafName[512]; // XXXXX how big does this have to be?

    Log3(("usbproxy::EnumerateHub: HubName=%s\n", HubName));

    // Initialize locals to not allocated state so the error cleanup routine
    // only tries to cleanup things that were successfully allocated.
    //
    info        = NULL;
    hHubDevice  = INVALID_HANDLE_VALUE;

    // Allocate some space for a USBDEVICEINFO structure to hold the
    // hub info, hub name, and connection info pointers.  GPTR zero
    // initializes the structure for us.
    //
    info = (PUSBDEVICEINFO) RTMemAllocZ(sizeof(USBDEVICEINFO));

    if (info == NULL)
    {
        AssertFailed();
        goto EnumerateHubError;
    }

    // Keep copies of the Hub Name, Connection Info, and Configuration
    // Descriptor pointers
    //
    info->HubName = HubName;

    info->ConnectionInfo = ConnectionInfo;

    info->ConfigDesc = ConfigDesc;

    info->StringDescs = StringDescs;


    // Allocate some space for a USB_NODE_INFORMATION structure for this Hub,
    //
    info->HubInfo = (PUSB_NODE_INFORMATION)RTMemAllocZ(sizeof(USB_NODE_INFORMATION));

    if (info->HubInfo == NULL)
    {
        AssertFailed();
        goto EnumerateHubError;
    }

    // Allocate a temp buffer for the full hub device name.
    //
    deviceName = (PCHAR)RTMemAllocZ(strlen(HubName) + sizeof("\\\\.\\"));

    if (deviceName == NULL)
    {
        AssertFailed();
        goto EnumerateHubError;
    }

    // Create the full hub device name
    //
    strcpy(deviceName, "\\\\.\\");
    strcpy(deviceName + sizeof("\\\\.\\") - 1, info->HubName);

    // Try to hub the open device
    //
    hHubDevice = CreateFile(deviceName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    // Done with temp buffer for full hub device name
    //
    RTMemFree(deviceName);

    if (hHubDevice == INVALID_HANDLE_VALUE)
    {
        AssertFailed();
        goto EnumerateHubError;
    }

    //
    // Now query USBHUB for the USB_NODE_INFORMATION structure for this hub.
    // This will tell us the number of downstream ports to enumerate, among
    // other things.
    //
    success = DeviceIoControl(hHubDevice,
                              IOCTL_USB_GET_NODE_INFORMATION,
                              info->HubInfo,
                              sizeof(USB_NODE_INFORMATION),
                              info->HubInfo,
                              sizeof(USB_NODE_INFORMATION),
                              &nBytes,
                              NULL);

    if (!success)
    {
        AssertFailed();
        goto EnumerateHubError;
    }

    // Build the leaf name from the port number and the device description
    //
    if (ConnectionInfo)
    {
        sprintf(leafName, "[Port%d] ", ConnectionInfo->ConnectionIndex);
        strcat(leafName, ConnectionStatuses[ConnectionInfo->ConnectionStatus]);
        strcat(leafName, " :  ");
    }
    else
    {
        leafName[0] = 0;
    }

    if (DeviceDesc)
    {

        strcat(leafName, DeviceDesc);
    }
    else
    {
        strcat(leafName, info->HubName);
    }

    // Now recursively enumerate the ports of this hub.
    //
    EnumerateHubPorts(
        hHubDevice,
        info->HubInfo->u.HubInformation.HubDescriptor.bNumberOfPorts,
        HubName
        );


    CloseHandle(hHubDevice);
    return;

EnumerateHubError:
    //
    // Clean up any stuff that got allocated
    //

    if (hHubDevice != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hHubDevice);
        hHubDevice = INVALID_HANDLE_VALUE;
    }

    if (info != NULL)
    {
        if (info->HubName != NULL)
        {
            RTMemFree(info->HubName);
            info->HubName = NULL;
        }

        if (info->HubInfo != NULL)
        {
            RTMemFree(info->HubInfo);
            info->HubInfo;
        }

        RTMemFree(info);
        info = NULL;
    }

    if (ConnectionInfo)
    {
        RTMemFree(ConnectionInfo);
    }

    if (ConfigDesc)
    {
        RTMemFree(ConfigDesc);
    }

    if (StringDescs != NULL)
    {
        PSTRING_DESCRIPTOR_NODE Next;

        do {

            Next = StringDescs->Next;
            RTMemFree(StringDescs);
            StringDescs = Next;

        } while (StringDescs != NULL);
    }
}

//*****************************************************************************
//
// EnumerateHubPorts()
//
// hTreeParent - Handle of the TreeView item under which the hub port should
// be added.
//
// hHubDevice - Handle of the hub device to enumerate.
//
// NumPorts - Number of ports on the hub.
//
//*****************************************************************************

VOID
EnumerateHubPorts (
    HANDLE      hHubDevice,
    ULONG       NumPorts,
    const char *pszHubName
)
{
    ULONG       index;
    BOOL        success;

    PUSB_NODE_CONNECTION_INFORMATION_EX    connectionInfo;
    PUSB_DESCRIPTOR_REQUEST             configDesc;
    PSTRING_DESCRIPTOR_NODE             stringDescs;
    PUSBDEVICEINFO                      info;

    PCHAR pszDriverKeyName;
    PCHAR deviceDesc;
    CHAR  leafName[512]; // XXXXX how big does this have to be?
    CHAR  szDriverKeyName[512];


    // Loop over all ports of the hub.
    //
    // Port indices are 1 based, not 0 based.
    //
    for (index = 1; index <= NumPorts; index++)
    {
        ULONG nBytes;

        // Allocate space to hold the connection info for this port.
        // For now, allocate it big enough to hold info for 30 pipes.
        //
        // Endpoint numbers are 0-15.  Endpoint number 0 is the standard
        // control endpoint which is not explicitly listed in the Configuration
        // Descriptor.  There can be an IN endpoint and an OUT endpoint at
        // endpoint numbers 1-15 so there can be a maximum of 30 endpoints
        // per device configuration.
        //
        // Should probably size this dynamically at some point.
        //
        nBytes = sizeof(USB_NODE_CONNECTION_INFORMATION_EX) +
                 sizeof(USB_PIPE_INFO) * 30;

        connectionInfo = (PUSB_NODE_CONNECTION_INFORMATION_EX)RTMemAllocZ(nBytes);

        if (connectionInfo == NULL)
        {
            AssertFailed();
            break;
        }

        //
        // Now query USBHUB for the USB_NODE_CONNECTION_INFORMATION_EX structure
        // for this port.  This will tell us if a device is attached to this
        // port, among other things.
        //
        connectionInfo->ConnectionIndex = index;

        success = DeviceIoControl(hHubDevice,
                                  IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,
                                  connectionInfo,
                                  nBytes,
                                  connectionInfo,
                                  nBytes,
                                  &nBytes,
                                  NULL);

        if (!success)
        {
            RTMemFree(connectionInfo);
            continue;
        }

        // Update the count of connected devices
        //
        if (connectionInfo->ConnectionStatus == DeviceConnected)
        {
            TotalDevicesConnected++;
        }
        else
        {
            RTMemFree(connectionInfo);
            continue;
        }

        if (connectionInfo->DeviceIsHub)
        {
            TotalHubs++;
        }

        // If there is a device connected, get the Device Description
        //
        deviceDesc = NULL;
        szDriverKeyName[0] = 0;
        if (connectionInfo->ConnectionStatus != NoDeviceConnected)
        {
            pszDriverKeyName = GetDriverKeyName(hHubDevice,
                                                index);

            if (pszDriverKeyName)
            {
                Log(("Attached driver %s [port=%d]%s\n", pszDriverKeyName, index, connectionInfo->DeviceIsHub ? " DeviceIsHub" : ""));
                deviceDesc = DriverNameToDeviceDesc(pszDriverKeyName);
                Assert(strlen(pszDriverKeyName)+1 < sizeof(szDriverKeyName));
                if (strlen(pszDriverKeyName)+1 < sizeof(szDriverKeyName))
                    strcpy(szDriverKeyName, pszDriverKeyName);

                RTMemFree(pszDriverKeyName);
            }
        }

        // If there is a device connected to the port, try to retrieve the
        // Configuration Descriptor from the device.
        //
        if (gDoConfigDesc &&
            connectionInfo->ConnectionStatus == DeviceConnected)
        {
            configDesc = GetConfigDescriptor(hHubDevice,
                                             index,
                                             0);
        }
        else
        {
            configDesc = NULL;
        }

        if (configDesc != NULL &&
            AreThereStringDescriptors(&connectionInfo->DeviceDescriptor,
                                      (PUSB_CONFIGURATION_DESCRIPTOR)(configDesc+1)))
        {
            stringDescs = GetAllStringDescriptors(
                              hHubDevice,
                              index,
                              &connectionInfo->DeviceDescriptor,
                              (PUSB_CONFIGURATION_DESCRIPTOR)(configDesc+1));
        }
        else
        {
            stringDescs = NULL;
        }

        // If the device connected to the port is an external hub, get the
        // name of the external hub and recursively enumerate it.
        //
        if (connectionInfo->DeviceIsHub)
        {
            PCHAR extHubName;

            extHubName = GetExternalHubName(hHubDevice,
                                            index);

            if (extHubName != NULL)
            {
                EnumerateHub(extHubName,
                             connectionInfo,
                             configDesc,
                             stringDescs,
                             deviceDesc);

                // On to the next port
                //
                continue;
            }
        }

        // Allocate some space for a USBDEVICEINFO structure to hold the
        // hub info, hub name, and connection info pointers.  GPTR zero
        // initializes the structure for us.
        //
        info = (PUSBDEVICEINFO) RTMemAllocZ(sizeof(USBDEVICEINFO));

        if (info == NULL)
        {
            AssertFailed();
            if (configDesc != NULL)
            {
                RTMemFree(configDesc);
            }
            RTMemFree(connectionInfo);
            break;
        }

        info->ConnectionInfo = connectionInfo;

        info->ConfigDesc = configDesc;

        info->StringDescs = stringDescs;

        sprintf(leafName, "[Port%d] ", index);

        strcat(leafName, ConnectionStatuses[connectionInfo->ConnectionStatus]);

        if (deviceDesc)
        {
            strcat(leafName, " :  ");
            strcat(leafName, deviceDesc);
        }

        AddLeaf(info, leafName, szDriverKeyName, pszHubName, index);
    }
}


//*****************************************************************************
//
// WideStrToMultiStr()
//
//*****************************************************************************

PCHAR WideStrToMultiStr (PWCHAR WideStr)
{
    ULONG nBytes;
    PCHAR MultiStr;

    // Get the length of the converted string
    //
    nBytes = WideCharToMultiByte(
                 CP_ACP,
                 0,
                 WideStr,
                 -1,
                 NULL,
                 0,
                 NULL,
                 NULL);

    if (nBytes == 0)
    {
        return NULL;
    }

    // Allocate space to hold the converted string
    //
    MultiStr = (PCHAR)RTMemAllocZ(nBytes);

    if (MultiStr == NULL)
    {
        return NULL;
    }

    // Convert the string
    //
    nBytes = WideCharToMultiByte(
                 CP_ACP,
                 0,
                 WideStr,
                 -1,
                 MultiStr,
                 nBytes,
                 NULL,
                 NULL);

    if (nBytes == 0)
    {
        RTMemFree(MultiStr);
        return NULL;
    }

    return MultiStr;
}


//*****************************************************************************
//
// GetRootHubName()
//
//*****************************************************************************

PCHAR GetRootHubName (
    HANDLE HostController
)
{
    BOOL                success;
    ULONG               nBytes;
    USB_ROOT_HUB_NAME   rootHubName;
    PUSB_ROOT_HUB_NAME  rootHubNameW;
    PCHAR               rootHubNameA;

    rootHubNameW = NULL;
    rootHubNameA = NULL;

    // Get the length of the name of the Root Hub attached to the
    // Host Controller
    //
    success = DeviceIoControl(HostController,
                              IOCTL_USB_GET_ROOT_HUB_NAME,
                              0,
                              0,
                              &rootHubName,
                              sizeof(rootHubName),
                              &nBytes,
                              NULL);

    if (!success)
    {
        AssertFailed();
        goto GetRootHubNameError;
    }

    // Allocate space to hold the Root Hub name
    //
    nBytes = rootHubName.ActualLength;

    rootHubNameW = (PUSB_ROOT_HUB_NAME)RTMemAllocZ(nBytes);

    if (rootHubNameW == NULL)
    {
        AssertFailed();
        goto GetRootHubNameError;
    }

    // Get the name of the Root Hub attached to the Host Controller
    //
    success = DeviceIoControl(HostController,
                              IOCTL_USB_GET_ROOT_HUB_NAME,
                              NULL,
                              0,
                              rootHubNameW,
                              nBytes,
                              &nBytes,
                              NULL);

    if (!success)
    {
        AssertFailed();
        goto GetRootHubNameError;
    }

    // Convert the Root Hub name
    //
    rootHubNameA = WideStrToMultiStr(rootHubNameW->RootHubName);

    // All done, free the uncoverted Root Hub name and return the
    // converted Root Hub name
    //
    RTMemFree(rootHubNameW);

    return rootHubNameA;


GetRootHubNameError:
    // There was an error, free anything that was allocated
    //
    if (rootHubNameW != NULL)
    {
        RTMemFree(rootHubNameW);
        rootHubNameW = NULL;
    }

    return NULL;
}


//*****************************************************************************
//
// GetExternalHubName()
//
//*****************************************************************************

PCHAR GetExternalHubName (
    HANDLE  Hub,
    ULONG   ConnectionIndex
)
{
    BOOL                        success;
    ULONG                       nBytes;
    USB_NODE_CONNECTION_NAME    extHubName;
    PUSB_NODE_CONNECTION_NAME   extHubNameW;
    PCHAR                       extHubNameA;

    extHubNameW = NULL;
    extHubNameA = NULL;

    // Get the length of the name of the external hub attached to the
    // specified port.
    //
    extHubName.ConnectionIndex = ConnectionIndex;

    success = DeviceIoControl(Hub,
                              IOCTL_USB_GET_NODE_CONNECTION_NAME,
                              &extHubName,
                              sizeof(extHubName),
                              &extHubName,
                              sizeof(extHubName),
                              &nBytes,
                              NULL);

    if (!success)
    {
        AssertFailed();
        goto GetExternalHubNameError;
    }

    // Allocate space to hold the external hub name
    //
    nBytes = extHubName.ActualLength;

    if (nBytes <= sizeof(extHubName))
    {
        AssertFailed();
        goto GetExternalHubNameError;
    }

    extHubNameW = (PUSB_NODE_CONNECTION_NAME)RTMemAllocZ(nBytes);

    if (extHubNameW == NULL)
    {
        AssertFailed();
        goto GetExternalHubNameError;
    }

    // Get the name of the external hub attached to the specified port
    //
    extHubNameW->ConnectionIndex = ConnectionIndex;

    success = DeviceIoControl(Hub,
                              IOCTL_USB_GET_NODE_CONNECTION_NAME,
                              extHubNameW,
                              nBytes,
                              extHubNameW,
                              nBytes,
                              &nBytes,
                              NULL);

    if (!success)
    {
        AssertFailed();
        goto GetExternalHubNameError;
    }

    // Convert the External Hub name
    //
    extHubNameA = WideStrToMultiStr(extHubNameW->NodeName);

    // All done, free the uncoverted external hub name and return the
    // converted external hub name
    //
    RTMemFree(extHubNameW);

    return extHubNameA;


GetExternalHubNameError:
    // There was an error, free anything that was allocated
    //
    if (extHubNameW != NULL)
    {
        RTMemFree(extHubNameW);
        extHubNameW = NULL;
    }

    return NULL;
}


//*****************************************************************************
//
// GetDriverKeyName()
//
//*****************************************************************************

PCHAR GetDriverKeyName (
    HANDLE  Hub,
    ULONG   ConnectionIndex
)
{
    BOOL                                success;
    ULONG                               nBytes;
    USB_NODE_CONNECTION_DRIVERKEY_NAME  driverKeyName;
    PUSB_NODE_CONNECTION_DRIVERKEY_NAME driverKeyNameW;
    PCHAR                               driverKeyNameA;

    driverKeyNameW = NULL;
    driverKeyNameA = NULL;

    // Get the length of the name of the driver key of the device attached to
    // the specified port.
    //
    driverKeyName.ConnectionIndex = ConnectionIndex;

    success = DeviceIoControl(Hub,
                              IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME,
                              &driverKeyName,
                              sizeof(driverKeyName),
                              &driverKeyName,
                              sizeof(driverKeyName),
                              &nBytes,
                              NULL);

    if (!success)
    {
        goto GetDriverKeyNameError;
    }

    // Allocate space to hold the driver key name
    //
    nBytes = driverKeyName.ActualLength;

    if (nBytes <= sizeof(driverKeyName))
    {
        AssertFailed();
        goto GetDriverKeyNameError;
    }

    driverKeyNameW = (PUSB_NODE_CONNECTION_DRIVERKEY_NAME)RTMemAllocZ(nBytes);

    if (driverKeyNameW == NULL)
    {
        AssertFailed();
        goto GetDriverKeyNameError;
    }

    // Get the name of the driver key of the device attached to
    // the specified port.
    //
    driverKeyNameW->ConnectionIndex = ConnectionIndex;

    success = DeviceIoControl(Hub,
                              IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME,
                              driverKeyNameW,
                              nBytes,
                              driverKeyNameW,
                              nBytes,
                              &nBytes,
                              NULL);

    if (!success)
    {
        AssertFailed();
        goto GetDriverKeyNameError;
    }

    // Convert the driver key name
    //
    driverKeyNameA = WideStrToMultiStr(driverKeyNameW->DriverKeyName);

    // All done, free the uncoverted driver key name and return the
    // converted driver key name
    //
    RTMemFree(driverKeyNameW);

    return driverKeyNameA;


GetDriverKeyNameError:
    // There was an error, free anything that was allocated
    //
    if (driverKeyNameW != NULL)
    {
        RTMemFree(driverKeyNameW);
        driverKeyNameW = NULL;
    }

    return NULL;
}


//*****************************************************************************
//
// GetHCDDriverKeyName()
//
//*****************************************************************************

PCHAR GetHCDDriverKeyName (
    HANDLE  HCD
)
{
    BOOL                    success;
    ULONG                   nBytes;
    USB_HCD_DRIVERKEY_NAME  driverKeyName;
    PUSB_HCD_DRIVERKEY_NAME driverKeyNameW;
    PCHAR                   driverKeyNameA;

    driverKeyNameW = NULL;
    driverKeyNameA = NULL;

    // Get the length of the name of the driver key of the HCD
    //
    success = DeviceIoControl(HCD,
                              IOCTL_GET_HCD_DRIVERKEY_NAME,
                              &driverKeyName,
                              sizeof(driverKeyName),
                              &driverKeyName,
                              sizeof(driverKeyName),
                              &nBytes,
                              NULL);

    if (!success)
    {
        AssertFailed();
        goto GetHCDDriverKeyNameError;
    }

    // Allocate space to hold the driver key name
    //
    nBytes = driverKeyName.ActualLength;

    if (nBytes <= sizeof(driverKeyName))
    {
        AssertFailed();
        goto GetHCDDriverKeyNameError;
    }

    driverKeyNameW = (PUSB_HCD_DRIVERKEY_NAME)RTMemAllocZ(nBytes);

    if (driverKeyNameW == NULL)
    {
        AssertFailed();
        goto GetHCDDriverKeyNameError;
    }

    // Get the name of the driver key of the device attached to
    // the specified port.
    //
    success = DeviceIoControl(HCD,
                              IOCTL_GET_HCD_DRIVERKEY_NAME,
                              driverKeyNameW,
                              nBytes,
                              driverKeyNameW,
                              nBytes,
                              &nBytes,
                              NULL);

    if (!success)
    {
        AssertFailed();
        goto GetHCDDriverKeyNameError;
    }

    // Convert the driver key name
    //
    driverKeyNameA = WideStrToMultiStr(driverKeyNameW->DriverKeyName);

    // All done, free the uncoverted driver key name and return the
    // converted driver key name
    //
    RTMemFree(driverKeyNameW);

    return driverKeyNameA;


GetHCDDriverKeyNameError:
    // There was an error, free anything that was allocated
    //
    if (driverKeyNameW != NULL)
    {
        RTMemFree(driverKeyNameW);
        driverKeyNameW = NULL;
    }

    return NULL;
}


//*****************************************************************************
//
// GetConfigDescriptor()
//
// hHubDevice - Handle of the hub device containing the port from which the
// Configuration Descriptor will be requested.
//
// ConnectionIndex - Identifies the port on the hub to which a device is
// attached from which the Configuration Descriptor will be requested.
//
// DescriptorIndex - Configuration Descriptor index, zero based.
//
//*****************************************************************************

PUSB_DESCRIPTOR_REQUEST
GetConfigDescriptor (
    HANDLE  hHubDevice,
    ULONG   ConnectionIndex,
    UCHAR   DescriptorIndex
)
{
    BOOL    success;
    ULONG   nBytes;
    ULONG   nBytesReturned;

    UCHAR   configDescReqBuf[sizeof(USB_DESCRIPTOR_REQUEST) +
                             sizeof(USB_CONFIGURATION_DESCRIPTOR)];

    PUSB_DESCRIPTOR_REQUEST         configDescReq;
    PUSB_CONFIGURATION_DESCRIPTOR   configDesc;


    // Request the Configuration Descriptor the first time using our
    // local buffer, which is just big enough for the Cofiguration
    // Descriptor itself.
    //
    nBytes = sizeof(configDescReqBuf);

    configDescReq = (PUSB_DESCRIPTOR_REQUEST)configDescReqBuf;
    configDesc = (PUSB_CONFIGURATION_DESCRIPTOR)(configDescReq+1);

    // Zero fill the entire request structure
    //
    memset(configDescReq, 0, nBytes);

    // Indicate the port from which the descriptor will be requested
    //
    configDescReq->ConnectionIndex = ConnectionIndex;

    //
    // USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
    // IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
    //
    // USBD will automatically initialize these fields:
    //     bmRequest = 0x80
    //     bRequest  = 0x06
    //
    // We must initialize these fields:
    //     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
    //     wIndex    = Zero (or Language ID for String Descriptors)
    //     wLength   = Length of descriptor buffer
    //
    configDescReq->SetupPacket.wValue = (USB_CONFIGURATION_DESCRIPTOR_TYPE << 8)
                                        | DescriptorIndex;

    configDescReq->SetupPacket.wLength = (USHORT)(nBytes - sizeof(USB_DESCRIPTOR_REQUEST));

    // Now issue the get descriptor request.
    //
    success = DeviceIoControl(hHubDevice,
                              IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
                              configDescReq,
                              nBytes,
                              configDescReq,
                              nBytes,
                              &nBytesReturned,
                              NULL);

    if (!success)
    {
#ifdef VBOX_WITH_ANNOYING_USB_ASSERTIONS
        AssertMsgFailed(("DeviceIoControl IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION failed with %d\n", GetLastError()));
#else
        LogRel(("DeviceIoControl IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION failed with %d\n", GetLastError()));
#endif
        return NULL;
    }

    if (nBytes != nBytesReturned)
    {
        AssertFailed();
        return NULL;
    }

    if (configDesc->wTotalLength < sizeof(USB_CONFIGURATION_DESCRIPTOR))
    {
        AssertFailed();
        return NULL;
    }

    // Now request the entire Configuration Descriptor using a dynamically
    // allocated buffer which is sized big enough to hold the entire descriptor
    //
    nBytes = sizeof(USB_DESCRIPTOR_REQUEST) + configDesc->wTotalLength;

    configDescReq = (PUSB_DESCRIPTOR_REQUEST)RTMemAllocZ(nBytes);

    if (configDescReq == NULL)
    {
        AssertFailed();
        return NULL;
    }

    configDesc = (PUSB_CONFIGURATION_DESCRIPTOR)(configDescReq+1);

    // Indicate the port from which the descriptor will be requested
    //
    configDescReq->ConnectionIndex = ConnectionIndex;

    //
    // USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
    // IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
    //
    // USBD will automatically initialize these fields:
    //     bmRequest = 0x80
    //     bRequest  = 0x06
    //
    // We must initialize these fields:
    //     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
    //     wIndex    = Zero (or Language ID for String Descriptors)
    //     wLength   = Length of descriptor buffer
    //
    configDescReq->SetupPacket.wValue = (USB_CONFIGURATION_DESCRIPTOR_TYPE << 8)
                                        | DescriptorIndex;

    configDescReq->SetupPacket.wLength = (USHORT)(nBytes - sizeof(USB_DESCRIPTOR_REQUEST));

    // Now issue the get descriptor request.
    //
    success = DeviceIoControl(hHubDevice,
                              IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
                              configDescReq,
                              nBytes,
                              configDescReq,
                              nBytes,
                              &nBytesReturned,
                              NULL);

    if (!success)
    {
#ifdef VBOX_WITH_ANNOYING_USB_ASSERTIONS
        AssertMsgFailed(("DeviceIoControl IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION failed with %d\n", GetLastError()));
#else
        LogRel(("DeviceIoControl IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION failed with %d\n", GetLastError()));
#endif
        RTMemFree(configDescReq);
        return NULL;
    }

    if (nBytes != nBytesReturned)
    {
#ifdef VBOX_WITH_ANNOYING_USB_ASSERTIONS
        AssertMsgFailed(("DeviceIoControl IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION %d != %d\n", nBytes, nBytesReturned));
#else
        LogRel(("DeviceIoControl IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION %d != %d\n", nBytes, nBytesReturned));
#endif
        RTMemFree(configDescReq);
        return NULL;
    }

    if (configDesc->wTotalLength != (nBytes - sizeof(USB_DESCRIPTOR_REQUEST)))
    {
#ifdef VBOX_WITH_ANNOYING_USB_ASSERTIONS
        AssertMsgFailed(("DeviceIoControl IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION %d != %d\n", configDesc->wTotalLength, (nBytes - sizeof(USB_DESCRIPTOR_REQUEST))));
#else
        LogRel(("DeviceIoControl IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION %d != %d\n", configDesc->wTotalLength, (nBytes - sizeof(USB_DESCRIPTOR_REQUEST)) ));
#endif
        RTMemFree(configDescReq);
        return NULL;
    }

    return configDescReq;
}


//*****************************************************************************
//
// AreThereStringDescriptors()
//
// DeviceDesc - Device Descriptor for which String Descriptors should be
// checked.
//
// ConfigDesc - Configuration Descriptor (also containing Interface Descriptor)
// for which String Descriptors should be checked.
//
//*****************************************************************************

BOOL
AreThereStringDescriptors (
    PUSB_DEVICE_DESCRIPTOR          DeviceDesc,
    PUSB_CONFIGURATION_DESCRIPTOR   ConfigDesc
)
{
    PUCHAR                  descEnd;
    PUSB_COMMON_DESCRIPTOR  commonDesc;

    //
    // Check Device Descriptor strings
    //

    if (DeviceDesc->iManufacturer ||
        DeviceDesc->iProduct      ||
        DeviceDesc->iSerialNumber
       )
    {
        return TRUE;
    }


    //
    // Check the Configuration and Interface Descriptor strings
    //

    descEnd = (PUCHAR)ConfigDesc + ConfigDesc->wTotalLength;

    commonDesc = (PUSB_COMMON_DESCRIPTOR)ConfigDesc;

    while ((PUCHAR)commonDesc + sizeof(USB_COMMON_DESCRIPTOR) < descEnd &&
           (PUCHAR)commonDesc + commonDesc->bLength <= descEnd)
    {
        switch (commonDesc->bDescriptorType)
        {
            case USB_CONFIGURATION_DESCRIPTOR_TYPE:
                if (commonDesc->bLength != sizeof(USB_CONFIGURATION_DESCRIPTOR))
                {
                    AssertFailed();
                    break;
                }
                if (((PUSB_CONFIGURATION_DESCRIPTOR)commonDesc)->iConfiguration)
                {
                    return TRUE;
                }
                commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
                continue;

            case USB_INTERFACE_DESCRIPTOR_TYPE:
                if (commonDesc->bLength != sizeof(USB_INTERFACE_DESCRIPTOR) &&
                    commonDesc->bLength != sizeof(USB_INTERFACE_DESCRIPTOR2))
                {
                    AssertFailed();
                    break;
                }
                if (((PUSB_INTERFACE_DESCRIPTOR)commonDesc)->iInterface)
                {
                    return TRUE;
                }
                commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
                continue;

            default:
                commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
                continue;
        }
        break;
    }

    return FALSE;
}


//*****************************************************************************
//
// GetAllStringDescriptors()
//
// hHubDevice - Handle of the hub device containing the port from which the
// String Descriptors will be requested.
//
// ConnectionIndex - Identifies the port on the hub to which a device is
// attached from which the String Descriptors will be requested.
//
// DeviceDesc - Device Descriptor for which String Descriptors should be
// requested.
//
// ConfigDesc - Configuration Descriptor (also containing Interface Descriptor)
// for which String Descriptors should be requested.
//
//*****************************************************************************

PSTRING_DESCRIPTOR_NODE
GetAllStringDescriptors (
    HANDLE                          hHubDevice,
    ULONG                           ConnectionIndex,
    PUSB_DEVICE_DESCRIPTOR          DeviceDesc,
    PUSB_CONFIGURATION_DESCRIPTOR   ConfigDesc
)
{
    PSTRING_DESCRIPTOR_NODE supportedLanguagesString;
    PSTRING_DESCRIPTOR_NODE stringDescNodeTail;
    ULONG                   numLanguageIDs;
    USHORT                  *languageIDs;

    PUCHAR                  descEnd;
    PUSB_COMMON_DESCRIPTOR  commonDesc;

    //
    // Get the array of supported Language IDs, which is returned
    // in String Descriptor 0
    //
    supportedLanguagesString = GetStringDescriptor(hHubDevice,
                                                   ConnectionIndex,
                                                   0,
                                                   0);

    if (supportedLanguagesString == NULL)
    {
        return NULL;
    }

    numLanguageIDs = (supportedLanguagesString->StringDescriptor->bLength - 2) / 2;

    languageIDs = (PUSHORT)&supportedLanguagesString->StringDescriptor->bString[0];

    stringDescNodeTail = supportedLanguagesString;

    //
    // Get the Device Descriptor strings
    //

    if (DeviceDesc->iManufacturer)
    {
        stringDescNodeTail = GetStringDescriptors(hHubDevice,
                                                  ConnectionIndex,
                                                  DeviceDesc->iManufacturer,
                                                  numLanguageIDs,
                                                  languageIDs,
                                                  stringDescNodeTail);
    }

    if (DeviceDesc->iProduct)
    {
        stringDescNodeTail = GetStringDescriptors(hHubDevice,
                                                  ConnectionIndex,
                                                  DeviceDesc->iProduct,
                                                  numLanguageIDs,
                                                  languageIDs,
                                                  stringDescNodeTail);
    }

    if (DeviceDesc->iSerialNumber)
    {
        stringDescNodeTail = GetStringDescriptors(hHubDevice,
                                                  ConnectionIndex,
                                                  DeviceDesc->iSerialNumber,
                                                  numLanguageIDs,
                                                  languageIDs,
                                                  stringDescNodeTail);
    }


    //
    // Get the Configuration and Interface Descriptor strings
    //

    descEnd = (PUCHAR)ConfigDesc + ConfigDesc->wTotalLength;

    commonDesc = (PUSB_COMMON_DESCRIPTOR)ConfigDesc;

    while ((PUCHAR)commonDesc + sizeof(USB_COMMON_DESCRIPTOR) < descEnd &&
           (PUCHAR)commonDesc + commonDesc->bLength <= descEnd)
    {
        switch (commonDesc->bDescriptorType)
        {
            case USB_CONFIGURATION_DESCRIPTOR_TYPE:
                if (commonDesc->bLength != sizeof(USB_CONFIGURATION_DESCRIPTOR))
                {
                    AssertFailed();
                    break;
                }
                if (((PUSB_CONFIGURATION_DESCRIPTOR)commonDesc)->iConfiguration)
                {
                    stringDescNodeTail = GetStringDescriptors(
                                             hHubDevice,
                                             ConnectionIndex,
                                             ((PUSB_CONFIGURATION_DESCRIPTOR)commonDesc)->iConfiguration,
                                             numLanguageIDs,
                                             languageIDs,
                                             stringDescNodeTail);
                }
                commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
                continue;

            case USB_INTERFACE_DESCRIPTOR_TYPE:
                if (commonDesc->bLength != sizeof(USB_INTERFACE_DESCRIPTOR) &&
                    commonDesc->bLength != sizeof(USB_INTERFACE_DESCRIPTOR2))
                {
                    AssertFailed();
                    break;
                }
                if (((PUSB_INTERFACE_DESCRIPTOR)commonDesc)->iInterface)
                {
                    stringDescNodeTail = GetStringDescriptors(
                                             hHubDevice,
                                             ConnectionIndex,
                                             ((PUSB_INTERFACE_DESCRIPTOR)commonDesc)->iInterface,
                                             numLanguageIDs,
                                             languageIDs,
                                             stringDescNodeTail);
                }
                commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
                continue;

            default:
                commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
                continue;
        }
        break;
    }

    return supportedLanguagesString;
}


//*****************************************************************************
//
// GetStringDescriptor()
//
// hHubDevice - Handle of the hub device containing the port from which the
// String Descriptor will be requested.
//
// ConnectionIndex - Identifies the port on the hub to which a device is
// attached from which the String Descriptor will be requested.
//
// DescriptorIndex - String Descriptor index.
//
// LanguageID - Language in which the string should be requested.
//
//*****************************************************************************

PSTRING_DESCRIPTOR_NODE
GetStringDescriptor (
    HANDLE  hHubDevice,
    ULONG   ConnectionIndex,
    UCHAR   DescriptorIndex,
    USHORT  LanguageID
)
{
    BOOL    success;
    ULONG   nBytes;
    ULONG   nBytesReturned;

    UCHAR   stringDescReqBuf[sizeof(USB_DESCRIPTOR_REQUEST) +
                             MAXIMUM_USB_STRING_LENGTH];

    PUSB_DESCRIPTOR_REQUEST stringDescReq;
    PUSB_STRING_DESCRIPTOR  stringDesc;
    PSTRING_DESCRIPTOR_NODE stringDescNode;

    nBytes = sizeof(stringDescReqBuf);

    stringDescReq = (PUSB_DESCRIPTOR_REQUEST)stringDescReqBuf;
    stringDesc = (PUSB_STRING_DESCRIPTOR)(stringDescReq+1);

    // Zero fill the entire request structure
    //
    memset(stringDescReq, 0, nBytes);

    // Indicate the port from which the descriptor will be requested
    //
    stringDescReq->ConnectionIndex = ConnectionIndex;

    //
    // USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
    // IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
    //
    // USBD will automatically initialize these fields:
    //     bmRequest = 0x80
    //     bRequest  = 0x06
    //
    // We must initialize these fields:
    //     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
    //     wIndex    = Zero (or Language ID for String Descriptors)
    //     wLength   = Length of descriptor buffer
    //
    stringDescReq->SetupPacket.wValue = (USB_STRING_DESCRIPTOR_TYPE << 8)
                                        | DescriptorIndex;

    stringDescReq->SetupPacket.wIndex = LanguageID;

    stringDescReq->SetupPacket.wLength = (USHORT)(nBytes - sizeof(USB_DESCRIPTOR_REQUEST));

    // Now issue the get descriptor request.
    //
    success = DeviceIoControl(hHubDevice,
                              IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
                              stringDescReq,
                              nBytes,
                              stringDescReq,
                              nBytes,
                              &nBytesReturned,
                              NULL);

    //
    // Do some sanity checks on the return from the get descriptor request.
    //

    if (!success)
    {
#ifdef VBOX_WITH_ANNOYING_USB_ASSERTIONS
        AssertMsgFailed(("DeviceIoControl IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION failed with %d\n", GetLastError()));
#else
        LogRel(("DeviceIoControl IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION failed with %d\n", GetLastError()));
#endif
        return NULL;
    }

    if (nBytesReturned < 2)
    {
        AssertFailed();
        return NULL;
    }

    if (stringDesc->bDescriptorType != USB_STRING_DESCRIPTOR_TYPE)
    {
        AssertFailed();
        return NULL;
    }

    if (stringDesc->bLength != nBytesReturned - sizeof(USB_DESCRIPTOR_REQUEST))
    {
        AssertFailed();
        return NULL;
    }

    if (stringDesc->bLength % 2 != 0)
    {
        AssertFailed();
        return NULL;
    }

    //
    // Looks good, allocate some (zero filled) space for the string descriptor
    // node and copy the string descriptor to it.
    //

    stringDescNode = (PSTRING_DESCRIPTOR_NODE)RTMemAllocZ(sizeof(STRING_DESCRIPTOR_NODE) +
                                                          stringDesc->bLength + 2);

    if (stringDescNode == NULL)
    {
        AssertFailed();
        return NULL;
    }

    stringDescNode->DescriptorIndex = DescriptorIndex;
    stringDescNode->LanguageID = LanguageID;

    memcpy(stringDescNode->StringDescriptor,
           stringDesc,
           stringDesc->bLength);

    return stringDescNode;
}


//*****************************************************************************
//
// GetStringDescriptors()
//
// hHubDevice - Handle of the hub device containing the port from which the
// String Descriptor will be requested.
//
// ConnectionIndex - Identifies the port on the hub to which a device is
// attached from which the String Descriptor will be requested.
//
// DescriptorIndex - String Descriptor index.
//
// NumLanguageIDs -  Number of languages in which the string should be
// requested.
//
// LanguageIDs - Languages in which the string should be requested.
//
//*****************************************************************************

PSTRING_DESCRIPTOR_NODE
GetStringDescriptors (
    HANDLE  hHubDevice,
    ULONG   ConnectionIndex,
    UCHAR   DescriptorIndex,
    ULONG   NumLanguageIDs,
    USHORT  *LanguageIDs,
    PSTRING_DESCRIPTOR_NODE StringDescNodeTail
)
{
    ULONG i;

    for (i = 0; i < NumLanguageIDs; i++)
    {
        StringDescNodeTail->Next = GetStringDescriptor(hHubDevice,
                                                       ConnectionIndex,
                                                       DescriptorIndex,
                                                       *LanguageIDs);

        if (StringDescNodeTail->Next)
        {
            StringDescNodeTail = StringDescNodeTail->Next;
        }

        LanguageIDs++;
    }

    return StringDescNodeTail;
}


//*****************************************************************************
//
// DriverNameToDeviceDesc()
//
// Returns the Device Description of the DevNode with the matching DriverName.
// Returns NULL if the matching DevNode is not found.
//
// The caller should copy the returned string buffer instead of just saving
// the pointer value.  XXXXX Dynamically allocate return buffer?
//
//*****************************************************************************
CHAR        buf[512];  // XXXXX How big does this have to be? Dynamically size it?

PCHAR DriverNameToDeviceDesc (PCHAR DriverName)
{
    DEVINST     devInst;
    DEVINST     devInstNext;
    CONFIGRET   cr;
    ULONG       walkDone = 0;
    ULONG       len;

    // Get Root DevNode
    //
    cr = CM_Locate_DevNode(&devInst,
                           NULL,
                           0);

    if (cr != CR_SUCCESS)
    {
        return NULL;
    }

    // Do a depth first search for the DevNode with a matching
    // DriverName value
    //
    while (!walkDone)
    {
        // Get the DriverName value
        //
        len = sizeof(buf);
        cr = CM_Get_DevNode_Registry_Property(devInst,
                                              CM_DRP_DRIVER,
                                              NULL,
                                              buf,
                                              &len,
                                              0);

        // If the DriverName value matches, return the DeviceDescription
        //
        if (cr == CR_SUCCESS && _stricmp(DriverName, buf) == 0)
        {
            len = sizeof(buf);
            cr = CM_Get_DevNode_Registry_Property(devInst,
                                                  CM_DRP_DEVICEDESC,
                                                  NULL,
                                                  buf,
                                                  &len,
                                                  0);

            if (cr == CR_SUCCESS)
            {
                return buf;
            }
            else
            {
                return NULL;
            }
        }

        // This DevNode didn't match, go down a level to the first child.
        //
        cr = CM_Get_Child(&devInstNext,
                          devInst,
                          0);

        if (cr == CR_SUCCESS)
        {
            devInst = devInstNext;
            continue;
        }

        // Can't go down any further, go across to the next sibling.  If
        // there are no more siblings, go back up until there is a sibling.
        // If we can't go up any further, we're back at the root and we're
        // done.
        //
        for (;;)
        {
            cr = CM_Get_Sibling(&devInstNext,
                                devInst,
                                0);

            if (cr == CR_SUCCESS)
            {
                devInst = devInstNext;
                break;
            }

            cr = CM_Get_Parent(&devInstNext,
                               devInst,
                               0);


            if (cr == CR_SUCCESS)
            {
                devInst = devInstNext;
            }
            else
            {
                walkDone = 1;
                break;
            }
        }
    }

    return NULL;
}


/**
 * Attempts to start the service, creating it if necessary.
 *
 * @returns 0 on success.
 * @returns -1 on failure.
 * @param   fRetry  Indicates retry call.
 */
int usbMonStartService(void)
{
    /*
     * Check if the driver service is there.
     */
    SC_HANDLE hSMgr = OpenSCManager(NULL, NULL, SERVICE_QUERY_STATUS | SERVICE_START);
    if (hSMgr == NULL)
    {
        AssertMsgFailed(("couldn't open service manager in SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS mode!\n"));
        return -1;
    }

    /*
     * Try open our service to check it's status.
     */
    SC_HANDLE hService = OpenService(hSMgr, USBMON_SERVICE_NAME, SERVICE_QUERY_STATUS | SERVICE_START);
    if (!hService)
        return -1;

    /*
     * Check if open and on demand create succeeded.
     */
    int rc = -1;
    if (hService)
    {

        /*
         * Query service status to see if we need to start it or not.
         */
        SERVICE_STATUS  Status;
        BOOL fRc = QueryServiceStatus(hService, &Status);
        Assert(fRc);
        if (    Status.dwCurrentState != SERVICE_RUNNING
            &&  Status.dwCurrentState != SERVICE_START_PENDING)
        {
            /*
             * Start it.
             */
            LogRel(("usbMonStartService -> start it\n"));

            fRc = StartService(hService, 0, NULL);
            DWORD LastError = GetLastError(); NOREF(LastError);
            AssertMsg(fRc, ("StartService failed with LastError=%Rwa\n", LastError));
            if (fRc)
                g_fStartedService = true;
        }

        /*
         * Wait for the service to finish starting.
         * We'll wait for 10 seconds then we'll give up.
         */
        QueryServiceStatus(hService, &Status);
        if (Status.dwCurrentState == SERVICE_START_PENDING)
        {
            int iWait;
            for (iWait = 100; iWait > 0 && Status.dwCurrentState == SERVICE_START_PENDING; iWait--)
            {
                Sleep(100);
                QueryServiceStatus(hService, &Status);
            }
            DWORD LastError = GetLastError(); NOREF(LastError);
            AssertMsg(Status.dwCurrentState != SERVICE_RUNNING,
                      ("Failed to start. LastError=%Rwa iWait=%d status=%d\n",
                       LastError, iWait, Status.dwCurrentState));
        }

        if (Status.dwCurrentState == SERVICE_RUNNING)
            rc = 0;

        /*
         * Close open handles.
         */
        CloseServiceHandle(hService);
    }
    else
    {
        DWORD LastError = GetLastError(); NOREF(LastError);
        AssertMsgFailed(("OpenService failed! LastError=%Rwa\n", LastError));
    }
    if (!CloseServiceHandle(hSMgr))
        AssertFailed();

    return rc;
}

/**
 * Stops a possibly running service.
 *
 * @returns 0 on success.
 * @returns -1 on failure.
 */
int usbMonStopService(void)
{
    LogRel(("usbMonStopService\n"));
    /*
     * Assume it didn't exist, so we'll create the service.
     */
    int rc = -1;
    SC_HANDLE   hSMgr = OpenSCManager(NULL, NULL, SERVICE_STOP | SERVICE_QUERY_STATUS);
    DWORD LastError = GetLastError(); NOREF(LastError);
    AssertMsg(hSMgr, ("OpenSCManager(,,delete) failed rc=%d\n", LastError));
    if (hSMgr)
    {
        SC_HANDLE hService = OpenService(hSMgr, USBMON_SERVICE_NAME, SERVICE_STOP | SERVICE_QUERY_STATUS);
        if (hService)
        {
            /*
             * Stop the service.
             */
            SERVICE_STATUS  Status;
            QueryServiceStatus(hService, &Status);
            if (Status.dwCurrentState == SERVICE_STOPPED)
                rc = 0;
            else if (ControlService(hService, SERVICE_CONTROL_STOP, &Status))
            {
                int iWait = 100;
                while (Status.dwCurrentState == SERVICE_STOP_PENDING && iWait-- > 0)
                {
                    Sleep(100);
                    QueryServiceStatus(hService, &Status);
                }
                if (Status.dwCurrentState == SERVICE_STOPPED)
                    rc = 0;
                else
                   AssertMsgFailed(("Failed to stop service. status=%d\n", Status.dwCurrentState));
            }
            else
            {
                DWORD LastError = GetLastError(); NOREF(LastError);
                AssertMsgFailed(("ControlService failed with LastError=%Rwa. status=%d\n", LastError, Status.dwCurrentState));
            }
            CloseServiceHandle(hService);
        }
        else if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
            rc = 0;
        else
        {
            DWORD LastError = GetLastError(); NOREF(LastError);
            AssertMsgFailed(("OpenService failed LastError=%Rwa\n", LastError));
        }
        CloseServiceHandle(hSMgr);
    }
    return rc;
}


/**
 * Initialize the USB library
 *
 * @returns VBox status code.
 */
USBLIB_DECL(int) USBLibInit(void)
{
    int            rc;
    USBSUP_VERSION version = {0};
    DWORD          cbReturned;

    Log(("usbproxy: usbLibInit\n"));

    g_hUSBMonitor = CreateFile(USBMON_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM, NULL);

    if (g_hUSBMonitor == INVALID_HANDLE_VALUE)
    {
        usbMonStartService();

        g_hUSBMonitor = CreateFile(USBMON_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM, NULL);

        if (g_hUSBMonitor == INVALID_HANDLE_VALUE)
        {
            /* AssertFailed(); */
            LogRel(("usbproxy: Unable to open monitor driver!! (rc=%d)\n", GetLastError()));
            rc = VERR_FILE_NOT_FOUND;
            goto failure;
        }
    }

    /*
     * Check the version
     */
    cbReturned = 0;
    if (!DeviceIoControl(g_hUSBMonitor, SUPUSBFLT_IOCTL_GET_VERSION, NULL, 0,&version, sizeof(version),  &cbReturned, NULL))
    {
        LogRel(("usbproxy: Unable to query filter version!! (rc=%d)\n", GetLastError()));
        rc = VERR_VERSION_MISMATCH;
        goto failure;
    }

    if (version.u32Major != USBMON_MAJOR_VERSION ||
        version.u32Minor <  USBMON_MINOR_VERSION)
    {
        LogRel(("usbproxy: Filter driver version mismatch!!\n"));
        rc = VERR_VERSION_MISMATCH;
        goto failure;
    }
    return VINF_SUCCESS;

failure:
    if (g_hUSBMonitor != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_hUSBMonitor);
        g_hUSBMonitor = INVALID_HANDLE_VALUE;
    }
    return rc;
}


/**
 * Terminate the USB library
 *
 * @returns VBox status code.
 */
USBLIB_DECL(int) USBLibTerm(void)
{
    if (g_hUSBMonitor != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_hUSBMonitor);
        g_hUSBMonitor = INVALID_HANDLE_VALUE;
    }
#if 0
    /*
     * If we started the service we might consider stopping it too.
     *
     * Since this won't work unless the process starting it is the
     * last user we might wanna skip this...
     */
    if (g_fStartedService)
    {
        usbMonStopService();
        g_fStartedService = false;
    }
#endif

    return VINF_SUCCESS;
}

/**
 * Capture specified USB device
 *
 * @returns VBox status code
 * @param   usVendorId      Vendor id
 * @param   usProductId     Product id
 * @param   usRevision      Revision
 */
USBLIB_DECL(int) USBLibCaptureDevice(uint16_t usVendorId, uint16_t usProductId, uint16_t usRevision)
{
    USBSUP_CAPTURE capture;
    DWORD          cbReturned = 0;

    Log(("usbLibCaptureDevice %x %x %x\n", usVendorId, usProductId, usRevision));

    if (g_hUSBMonitor == INVALID_HANDLE_VALUE)
        return VERR_NOT_SUPPORTED;

    capture.usVendorId  = usVendorId;
    capture.usProductId = usProductId;
    capture.usRevision  = usRevision;

    if (!DeviceIoControl(g_hUSBMonitor, SUPUSBFLT_IOCTL_CAPTURE_DEVICE, &capture, sizeof(capture),  NULL, 0, &cbReturned, NULL))
    {
        AssertMsgFailed(("DeviceIoControl failed with %d\n", GetLastError()));
        return RTErrConvertFromWin32(GetLastError());
    }

    return VINF_SUCCESS;
}

/**
 * Release specified USB device to the host.
 *
 * @returns VBox status code
 * @param usVendorId        Vendor id
 * @param usProductId       Product id
 * @param usRevision        Revision
 */
USBLIB_DECL(int) USBLibReleaseDevice(uint16_t usVendorId, uint16_t usProductId, uint16_t usRevision)
{
    USBSUP_RELEASE release;
    DWORD          cbReturned = 0;

    Log(("usbLibReleaseDevice %x %x %x\n", usVendorId, usProductId, usRevision));

    if (g_hUSBMonitor == INVALID_HANDLE_VALUE)
        return VERR_NOT_SUPPORTED;

    release.usVendorId  = usVendorId;
    release.usProductId = usProductId;
    release.usRevision  = usRevision;

    if (!DeviceIoControl(g_hUSBMonitor, SUPUSBFLT_IOCTL_RELEASE_DEVICE, &release, sizeof(release),  NULL, 0, &cbReturned, NULL))
    {
        AssertMsgFailed(("DeviceIoControl failed with %d\n", GetLastError()));
        return RTErrConvertFromWin32(GetLastError());
    }

    return VINF_SUCCESS;
}


USBLIB_DECL(void *) USBLibAddFilter(PCUSBFILTER pFilter)
{
    USBSUP_FLTADDOUT    add_out;
    DWORD               cbReturned = 0;

    if (g_hUSBMonitor == INVALID_HANDLE_VALUE)
        return NULL;

    Log(("usblibInsertFilter: Manufacturer=%s Product=%s Serial=%s\n",
         USBFilterGetString(pFilter, USBFILTERIDX_MANUFACTURER_STR)  ? USBFilterGetString(pFilter, USBFILTERIDX_MANUFACTURER_STR)  : "<null>",
         USBFilterGetString(pFilter, USBFILTERIDX_PRODUCT_STR)       ? USBFilterGetString(pFilter, USBFILTERIDX_PRODUCT_STR)       : "<null>",
         USBFilterGetString(pFilter, USBFILTERIDX_SERIAL_NUMBER_STR) ? USBFilterGetString(pFilter, USBFILTERIDX_SERIAL_NUMBER_STR) : "<null>"));

    if (!DeviceIoControl(g_hUSBMonitor, SUPUSBFLT_IOCTL_ADD_FILTER, (LPVOID)pFilter, sizeof(*pFilter), &add_out, sizeof(add_out), &cbReturned, NULL))
    {
        AssertMsgFailed(("DeviceIoControl failed with %d\n", GetLastError()));
        return NULL;
    }
    if (RT_FAILURE(add_out.rc))
    {
        AssertMsgFailed(("Adding filter failed with %d\n", add_out.rc));
        return NULL;
    }
    return (void *)add_out.uId;
}


USBLIB_DECL(void) USBLibRemoveFilter(void *pvId)
{
    uintptr_t     uId;
    DWORD         cbReturned = 0;

    if (g_hUSBMonitor == INVALID_HANDLE_VALUE)
        return;

    Log(("usblibRemoveFilter %p\n", pvId));

    uId = (uintptr_t)pvId;
    if (!DeviceIoControl(g_hUSBMonitor, SUPUSBFLT_IOCTL_REMOVE_FILTER, &uId, sizeof(uId),  NULL, 0,&cbReturned, NULL))
        AssertMsgFailed(("DeviceIoControl failed with %d\n", GetLastError()));
}


/**
 * Return all attached USB devices that are captured by the filter
 *
 * @returns VBox status code
 * @param ppDevices         Receives pointer to list of devices
 * @param pcDevices         Number of USB devices in the list
 */
USBLIB_DECL(int) USBLibGetDevices(PUSBDEVICE *ppDevices, uint32_t *pcDevices)
{
    USBSUP_GETNUMDEV numdev;
    PUSBDEVICE pDevice = NULL;
    Log(("usbLibGetDevices: enter\n"));

    if (g_hUSBMonitor == INVALID_HANDLE_VALUE)
        return VERR_NOT_SUPPORTED;

    /* 1: Enumerate all usb devices attached to the host */
    PUSBDEVICE pHostDevices = NULL;
    uint32_t   cHostDevices = 0;
    usbLibEnumerateHostControllers(&pHostDevices, &cHostDevices);
#ifdef LOG_ENABLED
    Log(("usbLibGetDevices: Detected %d host devices\n", cHostDevices));
    pDevice = pHostDevices;
    int iDevice = 0;
    while (pDevice)
    {
        iDevice++;
        Log(("Detected host device: #%d\n", iDevice));
        Log((" Vendor Id:       0x%04X\n", pDevice->idVendor));
        Log((" Product Id:      0x%04X\n", pDevice->idProduct));
        Log((" Revision:        0x%04X\n", pDevice->bcdDevice));
        Log((" Address:         %s\n", pDevice->pszAddress));
        Log((" HubName:         %s\n", pDevice->pszHubName));
        Log((" Port:            %u\n", pDevice->bPort));
        Log((" Manufacturer:    %s\n", pDevice->pszManufacturer));
        Log((" Product:         %s\n", pDevice->pszProduct));
        if (pDevice->pszSerialNumber)
            Log((" Serial Nr.:      %s\n", pDevice->pszSerialNumber));

        switch(pDevice->enmState)
        {
        case USBDEVICESTATE_UNSUPPORTED:
            Log((" State            USBDEVICESTATE_UNSUPPORTED\n"));
            break;
        case USBDEVICESTATE_USED_BY_HOST:
            Log((" State            USBDEVICESTATE_USED_BY_HOST\n"));
            break;
        case USBDEVICESTATE_USED_BY_HOST_CAPTURABLE:
            Log((" State            USBDEVICESTATE_USED_BY_HOST_CAPTURABLE\n"));
            break;
        case USBDEVICESTATE_UNUSED:
            Log((" State            USBDEVICESTATE_UNUSED\n"));
            break;
        case USBDEVICESTATE_HELD_BY_PROXY:
            Log((" State            USBDEVICESTATE_HELD_BY_PROXY\n"));
            break;
        case USBDEVICESTATE_USED_BY_GUEST:
            Log((" State            USBDEVICESTATE_USED_BY_GUEST\n"));
            break;
        }

        pDevice = pDevice->pNext;
    }
#endif

    *ppDevices = 0;
    *pcDevices = 0;

    /*
     * Get the return data.
     * Note that we might be called a bit too early here. Give windows time to register the new USB driver/device.
     * It's no problem to block here as we're in the async usb detection thread (not EMT)
     */

    for (int i = 0; i < 100; i++)
    {
        /*
         * Get the number of USB devices.
         */
        DWORD cbReturned = 0;
        if (!DeviceIoControl(g_hUSBMonitor, SUPUSBFLT_IOCTL_GET_NUM_DEVICES, NULL, 0, &numdev, sizeof(numdev), &cbReturned, NULL))
        {
            AssertMsgFailed(("DeviceIoControl failed with %d\n", GetLastError()));
            return RTErrConvertFromWin32(GetLastError());
        }

        AssertMsg((int32_t)numdev.cUSBDevices >= 0, ("%d", numdev.cUSBDevices));
        if ((int32_t)numdev.cUSBDevices <= 0) /** @todo why does this return -1. Happened here when detaching a captured device which hadn't yet been opened by a VM process. */
            break;

        Log(("Monitor detected %d captured devices\n", numdev.cUSBDevices));

        usblibEnumDevices(numdev.cUSBDevices, pcDevices);
        Log(("usblibEnumDevices detected %d devices\n", *pcDevices));

        if (numdev.cUSBDevices == *pcDevices)
            break;
        RTThreadSleep(100);
    }
    Assert(numdev.cUSBDevices == *pcDevices);

    if ((int32_t)numdev.cUSBDevices <= 0 || *pcDevices == 0)
    {
        /* Only return the host devices */
        *ppDevices = pHostDevices;
        *pcDevices = cHostDevices;
        Log(("usbLibGetDevices: returns %d host device - no captured devices\n", cHostDevices));
        return VINF_SUCCESS;
    }

    /* 2: Get all the USB devices that the filter has captured for us */

    /* Get the required info for each captured device */
    PUSBDEVICE *ppCaptured = (PUSBDEVICE*)RTMemAllocZ(sizeof(PUSBDEVICE) * numdev.cUSBDevices);
    if (!ppCaptured)
        goto failure;

    uint32_t cCaptured = 0;
    for (uint32_t i = 0; i < numdev.cUSBDevices; i++)
    {
        USBSUP_GETDEV dev = {0};
        HANDLE hDev;

        char *pszDevname = usblibQueryDeviceName(i);
        hDev = CreateFile(pszDevname, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL,
                          OPEN_EXISTING,  FILE_ATTRIBUTE_SYSTEM, NULL);

        if (hDev != INVALID_HANDLE_VALUE)
        {
            DWORD cbReturned = 0;
            if (!DeviceIoControl(hDev, SUPUSB_IOCTL_GET_DEVICE, &dev, sizeof(dev), &dev, sizeof(dev), &cbReturned, NULL))
            {
                int iErr = GetLastError();
                /* ERROR_DEVICE_NOT_CONNECTED -> device was removed just now */
                AssertMsg(iErr == ERROR_DEVICE_NOT_CONNECTED, ("DeviceIoControl %d failed with %d\n", i, iErr));
                Log(("SUPUSB_IOCTL_GET_DEVICE: DeviceIoControl %d no longer connected\n", i));
            }
            else
            {
                pDevice = (PUSBDEVICE)RTMemAllocZ(sizeof(USBDEVICE));
                if (!pDevice)
                    goto failure;

                pDevice->idVendor      = dev.vid;
                pDevice->idProduct     = dev.did;
                pDevice->bcdDevice     = dev.rev;
                pDevice->bBus          = 0;
                pDevice->u64SerialHash = USBLibHashSerial(dev.serial_hash);
                pDevice->enmState      = USBDEVICESTATE_HELD_BY_PROXY;
                pDevice->pszAddress    = RTStrDup(pszDevname);
                /* The following is not 100% accurate but we only care about high-speed vs non-high-speed */
                pDevice->enmSpeed      = dev.fHiSpeed ? USBDEVICESPEED_HIGH : USBDEVICESPEED_FULL;
                Log(("usbLibGetDevices: Detected device vid=%x did=%x rev=%x hispd=%d hash=%s hash=%RX64\n", dev.vid, dev.did, dev.rev, dev.fHiSpeed, dev.serial_hash, pDevice->u64SerialHash));

                ppCaptured[i] = pDevice;
                cCaptured++;
            }

            CloseHandle(hDev);
        }
        else
            AssertMsgFailed(("Unexpected failure to open %s. lasterr=%d\n", pszDevname, GetLastError()));
    }

    if (!cCaptured)
    {
        /* Only return the host devices */
        *ppDevices = pHostDevices;
        *pcDevices = cHostDevices;
        Log(("usbLibGetDevices: returns %d host device - no captured devices after all\n", cHostDevices));
        return VINF_SUCCESS;
    }

    /* 3: Go through the list of captured devices, lookup the corresponding device
     *    in the list of host devices and update the information there. */
    Assert(pHostDevices);
    for (uint32_t i = 0; i < numdev.cUSBDevices; i++)
    {
        char *pszDeviceRegPath = usblibQueryDeviceRegPath(i);
        Assert(pszDeviceRegPath);

        pDevice = pHostDevices;
        uint32_t j;
        for (j = 0; j < cHostDevices; j++)
        {
            if (pszDeviceRegPath)
            {
                if (!strcmp(pszDeviceRegPath, pDevice->pszAddress))
                {
                    Log(("usbLibGetDevices: Duplicate device %s (%s)\n", pszDeviceRegPath, usblibQueryDeviceName(i)));
                    break;
                }
            }
            pDevice = pDevice->pNext;
        }

        if (j == cHostDevices)
        {
            Assert(!pDevice);

            /* Probably in the process of being reattached */
            Log(("usbLibGetDevices: Captured device %s not found in host device list\n", usblibQueryDeviceRegPath(i)));
            /* do nothing */
        }
        else
        {
            Assert(pDevice);

            pDevice->enmState = USBDEVICESTATE_HELD_BY_PROXY;
            if (ppCaptured[i])
                pDevice->enmSpeed = ppCaptured[i]->enmSpeed;    // @todo: Is that right?
            RTStrFree(pDevice->pszAltAddress);
            pDevice->pszAltAddress = (char *)pDevice->pszAddress;
            pDevice->pszAddress = RTStrDup(usblibQueryDeviceName(i));
        }
    }
    *pcDevices = cHostDevices;

    /* Free captured devices list */
    for (uint32_t i = 0; i < numdev.cUSBDevices; i++)
    {
        pDevice = ppCaptured[i];
        if (pDevice)
        {
            RTMemFree((void *)pDevice->pszAddress);
            RTStrFree(pDevice->pszAltAddress);
            RTStrFree(pDevice->pszHubName);
            RTMemFree((void *)pDevice->pszManufacturer);
            RTMemFree((void *)pDevice->pszProduct);
            RTMemFree((void *)pDevice->pszSerialNumber);
            RTMemFree(pDevice);
        }
    }
    RTMemFree(ppCaptured);

    Log(("usbLibGetDevices: returns %d devices\n", *pcDevices));
#ifdef LOG_ENABLED
    pDevice = pHostDevices;
    iDevice = 0;
    while (pDevice)
    {
        iDevice++;
        Log(("Detected device: #%d\n", iDevice));
        Log((" Vendor Id:       0x%04X\n", pDevice->idVendor));
        Log((" Product Id:      0x%04X\n", pDevice->idProduct));
        Log((" Revision:        0x%04X\n", pDevice->bcdDevice));
        Log((" Address:         %s\n", pDevice->pszAddress));
        Log((" AltAddress:      %s\n", pDevice->pszAltAddress));
        Log((" HubName:         %s\n", pDevice->pszHubName));
        Log((" Port:            %u\n", pDevice->bPort));
        Log((" Manufacturer:    %s\n", pDevice->pszManufacturer));
        Log((" Product:         %s\n", pDevice->pszProduct));
        if (pDevice->pszSerialNumber)
            Log((" Serial Nr.:      %s\n", pDevice->pszSerialNumber));

        switch(pDevice->enmState)
        {
        case USBDEVICESTATE_UNSUPPORTED:
            Log((" State            USBDEVICESTATE_UNSUPPORTED\n"));
            break;
        case USBDEVICESTATE_USED_BY_HOST:
            Log((" State            USBDEVICESTATE_USED_BY_HOST\n"));
            break;
        case USBDEVICESTATE_USED_BY_HOST_CAPTURABLE:
            Log((" State            USBDEVICESTATE_USED_BY_HOST_CAPTURABLE\n"));
            break;
        case USBDEVICESTATE_UNUSED:
            Log((" State            USBDEVICESTATE_UNUSED\n"));
            break;
        case USBDEVICESTATE_HELD_BY_PROXY:
            Log((" State            USBDEVICESTATE_HELD_BY_PROXY\n"));
            break;
        case USBDEVICESTATE_USED_BY_GUEST:
            Log((" State            USBDEVICESTATE_USED_BY_GUEST\n"));
            break;
        }
        pDevice = pDevice->pNext;
    }
#endif
    *ppDevices = pHostDevices;
    return VINF_SUCCESS;

failure:
    /* free host devices */
    pDevice      = pHostDevices;
    pHostDevices = NULL;
    while (pDevice)
    {
        PUSBDEVICE pNext = pDevice->pNext;

        RTMemFree((void *)pDevice->pszAddress);
        RTMemFree((void *)pDevice->pszManufacturer);
        RTMemFree((void *)pDevice->pszProduct);
        RTMemFree((void *)pDevice->pszSerialNumber);
        RTMemFree(pDevice);

        pDevice = Next;
    }

    /* free captured devices */
    for (uint32_t i = 0; i < numdev.cUSBDevices; i++)
    {
        pDevice = ppCaptured[i];
        if (pDevice)
        {
            RTMemFree((void *)pDevice->pszAddress);
            RTMemFree((void *)pDevice->pszManufacturer);
            RTMemFree((void *)pDevice->pszProduct);
            RTMemFree((void *)pDevice->pszSerialNumber);
            RTMemFree(pDevice);
        }
    }
    RTMemFree(ppCaptured);

    *ppDevices = NULL;
    Log(("usbLibGetDevices: returns VERR_NO_MEMORY\n"));
    return VERR_NO_MEMORY;
}

/**
 * Return all USB devices attached to the host
 *
 * @returns VBox status code
 * @param ppDevices         Receives pointer to list of devices
 * @param pcDevices         Number of USB devices in the list
 */
USBLIB_DECL(int) usbLibGetHostDevices(PUSBDEVICE *ppDevices,  uint32_t *pcDevices)
{
    usbLibEnumerateHostControllers(ppDevices, pcDevices);

    return VINF_SUCCESS;
}

/**
 * Check for USB device arrivals or removals
 *
 * @returns boolean
 */
USBLIB_DECL(bool) USBLibHasPendingDeviceChanges(void)
{
    USBSUP_USB_CHANGE out;
    DWORD cbReturned;

    if (g_hUSBMonitor == INVALID_HANDLE_VALUE)
        return false;
    cbReturned = 0;
    if (   DeviceIoControl(g_hUSBMonitor, SUPUSBFLT_IOCTL_USB_CHANGE, NULL, 0, &out, sizeof(out), &cbReturned, NULL)
        && out.cUSBStateChange != g_cUSBStateChange)
    {
        g_cUSBStateChange = out.cUSBStateChange;
        Log(("usbLibHasPendingDeviceChanges: Detected USB state change!!\n"));
        return true;
    }
    return false;
}

