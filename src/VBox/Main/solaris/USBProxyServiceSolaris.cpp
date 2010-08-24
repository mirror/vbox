/* $Id$ */
/** @file
 * VirtualBox USB Proxy Service, Solaris Specialization.
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
#include "USBProxyService.h"
#include "Logging.h"

#include <VBox/usb.h>
#include <VBox/usblib.h>
#include <VBox/err.h>
#include <iprt/semaphore.h>
#include <iprt/path.h>

#include <sys/usb/usba.h>
#include <syslog.h>

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int solarisWalkDeviceNode(di_node_t Node, void *pvArg);
static void solarisFreeUSBDevice(PUSBDEVICE pDevice);
static USBDEVICESTATE solarisDetermineUSBDeviceState(PUSBDEVICE pDevice, di_node_t Node);


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct USBDEVICELIST
{
    PUSBDEVICE pHead;
    PUSBDEVICE pTail;
} USBDEVICELIST;
typedef USBDEVICELIST *PUSBDEVICELIST;


/**
 * Initialize data members.
 */
USBProxyServiceSolaris::USBProxyServiceSolaris (Host *aHost)
    : USBProxyService (aHost), mUSBLibInitialized(false)
{
    LogFlowThisFunc(("aHost=%p\n", aHost));
}


/**
 * Initializes the object (called right after construction).
 *
 * @returns S_OK on success and non-fatal failures, some COM error otherwise.
 */
HRESULT USBProxyServiceSolaris::init(void)
{
    /*
     * Call the superclass method first.
     */
    HRESULT hrc = USBProxyService::init();
    AssertComRCReturn(hrc, hrc);

    /*
     * Create semaphore.
     */
    int rc = RTSemEventCreate(&mNotifyEventSem);
    if (RT_FAILURE(rc))
    {
        mLastError = rc;
        return E_FAIL;
    }

    /*
     * Initialize the USB library.
     */
    rc = USBLibInit();
    if (RT_FAILURE(rc))
    {
        mLastError = rc;
        return S_OK;
    }
    mUSBLibInitialized = true;

    /*
     * Start the poller thread.
     */
    start();
    return S_OK;
}


/**
 * Stop all service threads and free the device chain.
 */
USBProxyServiceSolaris::~USBProxyServiceSolaris()
{
    LogFlowThisFunc(("destruct\n"));

    /*
     * Stop the service.
     */
    if (isActive())
        stop();

    /*
     * Terminate the USB library
     */
    if (mUSBLibInitialized)
    {
        USBLibTerm();
        mUSBLibInitialized = false;
    }

    RTSemEventDestroy(mNotifyEventSem);
    mNotifyEventSem = NULL;
}


void *USBProxyServiceSolaris::insertFilter(PCUSBFILTER aFilter)
{
    return USBLibAddFilter(aFilter);
}


void USBProxyServiceSolaris::removeFilter(void *pvID)
{
    USBLibRemoveFilter(pvID);
}


int USBProxyServiceSolaris::wait(RTMSINTERVAL aMillies)
{
    return RTSemEventWait(mNotifyEventSem, aMillies < 1000 ? 1000 : RT_MIN(aMillies, 5000));
}


int USBProxyServiceSolaris::interruptWait(void)
{
    return RTSemEventSignal(mNotifyEventSem);
}


PUSBDEVICE USBProxyServiceSolaris::getDevices(void)
{
    USBDEVICELIST DevList;
    DevList.pHead = NULL;
    DevList.pTail = NULL;
    di_node_t RootNode = di_init("/", DINFOCPYALL);
    if (RootNode != DI_NODE_NIL)
        di_walk_node(RootNode, DI_WALK_CLDFIRST, &DevList, solarisWalkDeviceNode);

    di_fini(RootNode);
    return DevList.pHead;
}

#if 0
static int solarisWalkMinor(di_node_t Node, di_minor_t Minor, void *pvArg)
{
    char *pszDevFsPath = di_devfs_path(Node);
    char *pszMinorName = di_minor_name(Minor);
    PUSBDEVICE pDev = (PUSBDEVICE)pvArg;

    AssertRelease(pDev);

    if (!pszDevFsPath || !pszMinorName)
        return DI_WALK_CONTINUE;

    RTStrAPrintf(&pDev->pszApId, "/devices%s:%s", pszDevFsPath, pszMinorName);
    di_devfs_path_free(pszDevFsPath);

    syslog(LOG_ERR, "VBoxUsbApId:%s\n", pDev->pszApId);
    return DI_WALK_TERMINATE;
}

static bool solarisGetApId(PUSBDEVICE pDev, char *pszDevicePath, di_node_t RootNode)
{
    pDev->pszApId = NULL;

    /* Skip "/devices" prefix if any */
    char achDevicesDir[] = "/devices/";
    if (strncmp(pszDevicePath, achDevicesDir, sizeof(achDevicesDir)) == 0)
        pszDevicePath += sizeof(achDevicesDir);

    char *pszPhysical = RTStrDup(pszDevicePath);
    char *pszTmp = NULL;

    /* Remove dynamic component "::" if any */
    if ((pszTmp = strstr(pszPhysical, "::")) != NULL)
        *pszTmp = '\0';

    /* Remove minor name if any */
    if ((pszTmp = strrchr(pszPhysical, ':')) != NULL)
        *pszTmp = '\0';

    /* Walk device tree */
//    di_node_t RootNode = di_init("/", DINFOCPYALL);
//    if (RootNode != DI_NODE_NIL)
//    {
//        di_node_t MinorNode = di_lookup_node(RootNode, pszPhysical);
//        if (MinorNode != DI_NODE_NIL)
        {
            di_walk_minor(RootNode, NULL, DI_CHECK_ALIAS | DI_CHECK_INTERNAL_PATH, pDev, solarisWalkMinor);
            return true;
        }
//        di_fini(RootNode);
//    }

    return false;
}
#endif

static int solarisWalkDeviceNode(di_node_t Node, void *pvArg)
{
    PUSBDEVICELIST pList = (PUSBDEVICELIST)pvArg;
    AssertPtrReturn(pList, DI_WALK_TERMINATE);

    /*
     * Check if it's a USB device in the first place.
     */
    bool fUSBDevice = false;
    char *pszCompatNames = NULL;
    int cCompatNames = di_compatible_names(Node, &pszCompatNames);
    for (int i = 0; i < cCompatNames; i++, pszCompatNames += strlen(pszCompatNames) + 1)
        if (!strncmp(pszCompatNames, "usb", 3))
        {
            fUSBDevice = true;
            break;
        }

    if (!fUSBDevice)
        return DI_WALK_CONTINUE;

    /*
     * Check if it's a device node or interface.
     */
    int *pInt = NULL;
    char *pStr = NULL;
    int rc = DI_WALK_CONTINUE;
    if (di_prop_lookup_ints(DDI_DEV_T_ANY, Node, "interface", &pInt) < 0)
    {
        /* It's a device node. */
        char *pszDevicePath = di_devfs_path(Node);
        PUSBDEVICE pCur = (PUSBDEVICE)RTMemAllocZ(sizeof(*pCur));
        if (!pCur)
        {
            LogRel(("USBService: failed to allocate %d bytes for PUSBDEVICE.\n", sizeof(*pCur)));
            return DI_WALK_TERMINATE;
        }

        bool fValidDevice = false;
        do
        {
            AssertBreak(pszDevicePath);

            char *pszDriverName = di_driver_name(Node);

            /*
             * Skip hubs
             */
            if (   pszDriverName
                && !strcmp(pszDriverName, "hubd"))
            {
                break;
            }

            /*
             * Mandatory.
             * snv_85 and above have usb-dev-descriptor node properties, but older one's do not.
             * So if we cannot obtain the entire device descriptor, we try falling back to the
             * invidividual properties (those must not fail, if it does we drop the device).
             */
            uchar_t *pDevData = NULL;
            int cbProp = di_prop_lookup_bytes(DDI_DEV_T_ANY, Node, "usb-dev-descriptor", &pDevData);
            if (   cbProp > 0
                && pDevData)
            {
                usb_dev_descr_t *pDeviceDescriptor = (usb_dev_descr_t *)pDevData;
                pCur->bDeviceClass = pDeviceDescriptor->bDeviceClass;
                pCur->bDeviceSubClass = pDeviceDescriptor->bDeviceSubClass;
                pCur->bDeviceProtocol = pDeviceDescriptor->bDeviceProtocol;
                pCur->idVendor = pDeviceDescriptor->idVendor;
                pCur->idProduct = pDeviceDescriptor->idProduct;
                pCur->bcdDevice = pDeviceDescriptor->bcdDevice;
                pCur->bcdUSB = pDeviceDescriptor->bcdUSB;
                pCur->bNumConfigurations = pDeviceDescriptor->bNumConfigurations;
                pCur->fPartialDescriptor = false;
            }
            else
            {
                AssertBreak(di_prop_lookup_ints(DDI_DEV_T_ANY, Node, "usb-vendor-id", &pInt) > 0);
                pCur->idVendor = (uint16_t)*pInt;

                AssertBreak(di_prop_lookup_ints(DDI_DEV_T_ANY, Node, "usb-product-id", &pInt) > 0);
                pCur->idProduct = (uint16_t)*pInt;

                AssertBreak(di_prop_lookup_ints(DDI_DEV_T_ANY, Node, "usb-revision-id", &pInt) > 0);
                pCur->bcdDevice = (uint16_t)*pInt;

                AssertBreak(di_prop_lookup_ints(DDI_DEV_T_ANY, Node, "usb-release", &pInt) > 0);
                pCur->bcdUSB = (uint16_t)*pInt;

                pCur->fPartialDescriptor = true;
            }

            char *pszPortAddr = di_bus_addr(Node);
            if (pszPortAddr)
                pCur->bPort = RTStrToUInt8(pszPortAddr);     /* Bus & Port are mixed up (kernel driver/userland) */
            else
                pCur->bPort = 0;

#if 0
            /*
             * Obtain the dev_t of the device.
             */
            di_minor_t Minor = di_minor_next(Node, DI_MINOR_NIL);
            AssertBreak(Minor != DI_MINOR_NIL);
            dev_t DeviceNum = di_minor_devt(Minor);

            int DevInstance = 0;
            rc = solarisUSBGetInstance(pszDevicePath, &DevInstance);

            char szAddress[PATH_MAX + 128];
            RTStrPrintf(szAddress, sizeof(szAddress), "/dev/usb/%x.%x|%s", pCur->idVendor, pCur->idProduct, pszDevicePath);
            /* @todo after binding ugen we need to append the instance number to the address. Not yet sure how we can update PUSBDEVICE at that time. */

            pCur->pszAddress = RTStrDup(szAddress);
            AssertBreak(pCur->pszAddress);
#endif

#ifdef VBOX_WITH_NEW_USB_CODE_ON_SOLARIS
            char pathBuf[PATH_MAX];
            RTStrPrintf(pathBuf, sizeof(pathBuf), "%s", pszDevicePath);
            RTPathStripFilename(pathBuf);

            char szBuf[PATH_MAX + 48];
            RTStrPrintf(szBuf, sizeof(szBuf), "%#x:%#x:%d:%s", pCur->idVendor, pCur->idProduct, pCur->bcdDevice, pathBuf);
#else
            char szBuf[2 * PATH_MAX];
            RTStrPrintf(szBuf, sizeof(szBuf), "/dev/usb/%x.%x/%d|%s", pCur->idVendor, pCur->idProduct, 0, pszDevicePath);
#endif
            pCur->pszAddress = RTStrDup(szBuf);

            pCur->pszDevicePath = RTStrDup(pszDevicePath);
            AssertBreak(pCur->pszDevicePath);

            /*
             * Optional (some devices don't have all these)
             */
            if (di_prop_lookup_strings(DDI_DEV_T_ANY, Node, "usb-product-name", &pStr) > 0)
                pCur->pszProduct = RTStrDup(pStr);

            if (di_prop_lookup_strings(DDI_DEV_T_ANY, Node, "usb-vendor-name", &pStr) > 0)
                pCur->pszManufacturer = RTStrDup(pStr);

            if (di_prop_lookup_strings(DDI_DEV_T_ANY, Node, "usb-serialno", &pStr) > 0)
                pCur->pszSerialNumber = RTStrDup(pStr);

            if (di_prop_lookup_ints(DDI_DEV_T_ANY, Node, "low-speed", &pInt) >= 0)
                pCur->enmSpeed = USBDEVICESPEED_LOW;
            else if (di_prop_lookup_ints(DDI_DEV_T_ANY, Node, "high-speed", &pInt) >= 0)
                pCur->enmSpeed = USBDEVICESPEED_HIGH;
            else
                pCur->enmSpeed = USBDEVICESPEED_FULL;

            /* Determine state of the USB device. */
            pCur->enmState = solarisDetermineUSBDeviceState(pCur, Node);

//            fValidDevice = solarisGetApId(pCur, pszDevicePath, Node);
            fValidDevice = true;

            /*
             * Valid device, add it to the list.
             */
            if (fValidDevice)
            {
                pCur->pPrev = pList->pTail;
                if (pList->pTail)
                    pList->pTail = pList->pTail->pNext = pCur;
                else
                    pList->pTail = pList->pHead = pCur;
            }
            rc = DI_WALK_CONTINUE;
        } while(0);

        di_devfs_path_free(pszDevicePath);
        if (!fValidDevice)
            solarisFreeUSBDevice(pCur);
    }
    return rc;
}


static USBDEVICESTATE solarisDetermineUSBDeviceState(PUSBDEVICE pDevice, di_node_t Node)
{
    char *pszDriverName = di_driver_name(Node);

    /* Not possible unless a user explicitly unbinds the default driver. */
    if (!pszDriverName)
        return USBDEVICESTATE_UNUSED;

#ifdef VBOX_WITH_NEW_USB_CODE_ON_SOLARIS
    if (!strncmp(pszDriverName, VBOXUSB_DRIVER_NAME, sizeof(VBOXUSB_DRIVER_NAME) - 1))
        return USBDEVICESTATE_HELD_BY_PROXY;

    NOREF(pDevice);
    return USBDEVICESTATE_USED_BY_HOST_CAPTURABLE;
#else

    USBDEVICESTATE enmState = USBDEVICESTATE_USED_BY_HOST_CAPTURABLE;

    /* Filter out keyboards, mouse */
    if (!pDevice->fPartialDescriptor)
    {
        /*
         * Here we have a fully valid device descriptor, so we fine
         * tune what's usable and what's not.
         */
        if (   pDevice->bDeviceClass == 3           /* HID */
            && (   pDevice->bDeviceProtocol == 1        /* Mouse */
                || pDevice->bDeviceProtocol == 2))      /* Keyboard */
        {
            return USBDEVICESTATE_USED_BY_HOST;
        }
    }
    else
    {
        /*
         * Old Nevadas don't give full device descriptor in ring-3.
         * So those Nevadas we just filter out all HIDs as unusable.
         */
        if (!strcmp(pszDriverName, "hid"))
            return USBDEVICESTATE_USED_BY_HOST;
    }

    return enmState;
#endif
}


int USBProxyServiceSolaris::captureDevice(HostUSBDevice *aDevice)
{
    /*
     * Check preconditions.
     */
    AssertReturn(aDevice, VERR_GENERAL_FAILURE);
    LogFlowThisFunc(("aDevice=%s\n", aDevice->getName().c_str()));
    AssertReturn(aDevice->isWriteLockOnCurrentThread(), VERR_GENERAL_FAILURE);
    Assert(aDevice->getUnistate() == kHostUSBDeviceState_Capturing);
    AssertReturn(aDevice->mUsb, VERR_INVALID_POINTER);

#ifdef VBOX_WITH_NEW_USB_CODE_ON_SOLARIS
    /*
     * Create a one-shot capture filter for the device and reset the device.
     */
    USBFILTER Filter;
    USBFilterInit(&Filter, USBFILTERTYPE_ONESHOT_CAPTURE);
    initFilterFromDevice(&Filter, aDevice);

    void *pvId = USBLibAddFilter(&Filter);
    if (!pvId)
    {
        LogRel(("USBService: failed to add filter\n"));
        return VERR_GENERAL_FAILURE;
    }

    PUSBDEVICE pDev = aDevice->mUsb;
    int rc = USBLibResetDevice(pDev->pszDevicePath, true);
    if (RT_SUCCESS(rc))
        aDevice->mOneShotId = pvId;
    else
    {
        USBLibRemoveFilter(pvId);
        pvId = NULL;
    }
    LogFlowThisFunc(("returns %Rrc pvId=%p\n", rc, pvId));
    return rc;

#else

    /*
     * Add the driver alias for binding the USB device to our driver.
     */
    PUSBDEVICE pDev = aDevice->mUsb;
    int rc = USBLibAddDeviceAlias(pDev);
    if (RT_SUCCESS(rc))
    {
        /*
         * Reconnect and configure the device into an 'online' state because hard reset via
         * usb_reset_device(9F) leaves the devices in an indeterminent state sometimes.
         * In case of errors here, ignore them and prod further...
         */
        rc = USBLibResetDevice(pDev->pszDevicePath, true);
        if (rc)
            LogRel(("USBService: USBLibResetDevice failed. device=%s rc=%d\n", pDev->pszDevicePath, rc));

        /*
         * Get device driver instance number.
         */
        int iInstance;
        rc = USBLibDeviceInstance(pDev->pszDevicePath, &iInstance);
        if (RT_SUCCESS(rc))
        {
            /*
             * Check device node to verify driver binding.
             */
            char szDevNode[PATH_MAX];
            RTStrPrintf(szDevNode, sizeof(szDevNode), "/dev/usb/%x.%x/%d", pDev->idVendor, pDev->idProduct, 0);

            /*
             * Wait for the driver to export the device nodes with a timeout of ~5 seconds.
             */
            unsigned cTimeout = 0;
            bool fExportedNodes = false;
            while (cTimeout < 5000)
            {
                if (RTPathExists(szDevNode))
                {
                    fExportedNodes = true;
                    break;
                }
                RTThreadSleep(500);
                cTimeout += 500;
            }
            if (fExportedNodes)
            {
#if 0
                /*
                 * This does not work. ProxyDevice somehow still gets the old values.
                 * Update our device with the node path as well as the device tree path.
                 */
                RTStrFree((char *)pDev->pszAddress);
                RTStrAPrintf(const_cast<char **>(&pDev->pszAddress), "%s|/devices%s", szDevNode, pDev->pszDevicePath);
#endif

                /*
                 * We don't need the system alias for this device anymore now that we've captured it.
                 * This will also ensure that in case VirtualBox crashes at a later point with
                 * a captured device it will not affect the user's USB devices.
                 */
                USBLibRemoveDeviceAlias(pDev);
                syslog(LOG_ERR, "USBPService: Success captured %s\n", aDevice->getName().c_str());
                return VINF_SUCCESS;
            }
            else
            {
                rc = VERR_PATH_NOT_FOUND;
                LogRel(("USBService: failed to stat %s for device.\n", szDevNode));
                syslog(LOG_ERR, "USBService: failed to stat %s for device.\n", szDevNode);
            }
        }
        else
            LogRel(("USBService: failed to obtain device instance number for %s rc=%Rrc\n", aDevice->getName().c_str(), rc));
    }
    else
        LogRel(("USBService: failed to add alias for device %s devicepath=%s rc=%Rrc\n", aDevice->getName().c_str(), pDev->pszDevicePath, rc));

    USBLibRemoveDeviceAlias(pDev);
    syslog(LOG_ERR, "USBPService: failed to capture device %s.\n", aDevice->getName().c_str());
    return VERR_OPEN_FAILED;

#endif

}


void USBProxyServiceSolaris::captureDeviceCompleted(HostUSBDevice *aDevice, bool aSuccess)
{
#ifdef VBOX_WITH_NEW_USB_CODE_ON_SOLARIS
    /*
     * Remove the one-shot filter if necessary.
     */
    LogFlowThisFunc(("aDevice=%s aSuccess=%RTbool mOneShotId=%p\n", aDevice->getName().c_str(), aSuccess, aDevice->mOneShotId));
    if (!aSuccess && aDevice->mOneShotId)
        USBLibRemoveFilter(aDevice->mOneShotId);
    aDevice->mOneShotId = NULL;
#endif
}


int USBProxyServiceSolaris::releaseDevice(HostUSBDevice *aDevice)
{
    /*
     * Check preconditions.
     */
    AssertReturn(aDevice, VERR_GENERAL_FAILURE);
    LogFlowThisFunc(("aDevice=%s\n", aDevice->getName().c_str()));
    AssertReturn(aDevice->isWriteLockOnCurrentThread(), VERR_GENERAL_FAILURE);
    Assert(aDevice->getUnistate() == kHostUSBDeviceState_ReleasingToHost);
    AssertReturn(aDevice->mUsb, VERR_INVALID_POINTER);

#ifdef VBOX_WITH_NEW_USB_CODE_ON_SOLARIS

    /*
     * Create a one-shot ignore filter for the device and reset it.
     */
    USBFILTER Filter;
    USBFilterInit(&Filter, USBFILTERTYPE_ONESHOT_IGNORE);
    initFilterFromDevice(&Filter, aDevice);

    void *pvId = USBLibAddFilter(&Filter);
    if (!pvId)
    {
        LogRel(("USBService: Adding ignore filter failed!\n"));
        return VERR_GENERAL_FAILURE;
    }

    PUSBDEVICE pDev = aDevice->mUsb;
    int rc = USBLibResetDevice(pDev->pszDevicePath, true /* Re-attach */);
    if (RT_SUCCESS(rc))
        aDevice->mOneShotId = pvId;
    else
    {
        USBLibRemoveFilter(pvId);
        pvId = NULL;
    }
    LogFlowThisFunc(("returns %Rrc pvId=%p\n", rc, pvId));
    return rc;

#else

    /*
     * Though may not be strictly remove the driver alias for releasing the USB device
     * from our driver, ignore errors here as we usually remove the alias immediately after capturing.
     */
    PUSBDEVICE pDev = aDevice->mUsb;
    int rc = USBLibRemoveDeviceAlias(pDev);
    Assert(pDev->pszDevicePath);
    rc = USBLibResetDevice(pDev->pszDevicePath, true /* Re-attach */);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    /* The Solaris specifics are free'd in USBProxyService::freeDeviceMembers */

    LogRel(("USBService: failed to reset device %s devicepath=%s rc=%Rrc\n", aDevice->getName().c_str(), pDev->pszDevicePath, rc));
    return rc;

#endif

}


void USBProxyServiceSolaris::releaseDeviceCompleted(HostUSBDevice *aDevice, bool aSuccess)
{
#ifdef VBOX_WITH_NEW_USB_CODE_ON_SOLARIS
    /*
     * Remove the one-shot filter if necessary.
     */
    LogFlowThisFunc(("aDevice=%s aSuccess=%RTbool mOneShotId=%p\n", aDevice->getName().c_str(), aSuccess, aDevice->mOneShotId));
    if (!aSuccess && aDevice->mOneShotId)
        USBLibRemoveFilter(aDevice->mOneShotId);
    aDevice->mOneShotId = NULL;
#endif
}


bool USBProxyServiceSolaris::updateDeviceState(HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice, bool *aRunFilters, SessionMachine **aIgnoreMachine)
{
#ifdef VBOX_WITH_NEW_USB_CODE_ON_SOLARIS
    return USBProxyService::updateDeviceState(aDevice, aUSBDevice, aRunFilters, aIgnoreMachine);
#else
    return updateDeviceStateFake(aDevice, aUSBDevice, aRunFilters, aIgnoreMachine);
#endif
}

/**
 * Wrapper called by walkDeviceNode.
 *
 * @param   pDevice    The USB device to free.
 */
void solarisFreeUSBDevice(PUSBDEVICE pDevice)
{
    USBProxyService::freeDevice(pDevice);
}

