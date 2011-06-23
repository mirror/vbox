/* $Id$ */
/** @file
 * USBProxyServiceLinux test case.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/******************************************************************************
*   Header Files                                                              *
******************************************************************************/

#include "USBProxyService.h"
#include "USBGetDevices.h"

#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/env.h>
#include <iprt/string.h>
#include <iprt/test.h>

/*** BEGIN STUBS ***/

USBProxyService::USBProxyService(Host*) {}
USBProxyService::~USBProxyService() {}
HRESULT USBProxyService::init() { return S_OK; }
int USBProxyService::start() { return VINF_SUCCESS; }
int USBProxyService::stop() { return VINF_SUCCESS; }
RWLockHandle *USBProxyService::lockHandle() const { return NULL; }
void *USBProxyService::insertFilter(USBFILTER const*) { return NULL; }
void USBProxyService::removeFilter(void*) {}
int USBProxyService::captureDevice(HostUSBDevice*) { return VINF_SUCCESS; }
void USBProxyService::captureDeviceCompleted(HostUSBDevice*, bool) {}
void USBProxyService::detachingDevice(HostUSBDevice*) {}
int USBProxyService::releaseDevice(HostUSBDevice*) { return VINF_SUCCESS; }
void USBProxyService::releaseDeviceCompleted(HostUSBDevice*, bool) {}
void USBProxyService::serviceThreadInit() {}
void USBProxyService::serviceThreadTerm() {}
int USBProxyService::wait(unsigned int) { return VINF_SUCCESS; }
int USBProxyService::interruptWait() { return VINF_SUCCESS; }
PUSBDEVICE USBProxyService::getDevices() { return NULL; }
void USBProxyService::deviceAdded(ComObjPtr<HostUSBDevice> &aDevice, SessionMachinesList &llOpenedMachines, PUSBDEVICE aUSBDevice) {}
void USBProxyService::deviceRemoved(ComObjPtr<HostUSBDevice> &aDevice) {}
void USBProxyService::deviceChanged(ComObjPtr<HostUSBDevice> &aDevice, SessionMachinesList*, SessionMachine*) {}
bool USBProxyService::updateDeviceState(HostUSBDevice*, USBDEVICE*, bool*, SessionMachine**) { return true; }
bool USBProxyService::updateDeviceStateFake(HostUSBDevice*, USBDEVICE*, bool*, SessionMachine**) { return true; }
bool USBProxyService::isActive() { return true; }

VBoxMainHotplugWaiter::VBoxMainHotplugWaiter(char const*) {}

com::Utf8Str HostUSBDevice::getName()
{
    return Utf8Str();
}

int USBProxyService::getLastError(void)
{
    return mLastError;
}

void SysFreeString(BSTR bstr)
{
    Assert(0);
}

static struct
{
    const char *pcszEnvUsb;
    const char *pcszEnvUsbRoot;
    const char *pcszDevicesRoot;
    bool fDevicesAccessible;
    const char *pcszUsbfsRoot;
    bool fUsbfsAccessible;
    int rcMethodInit;
    const char *pcszDevicesRootExpected;
    bool fUsingUsbfsExpected;
    int rcExpected;
} s_testEnvironment[] =
{
    /* "sysfs" and valid root in the environment */
    { "sysfs", "/dev/bus/usb", "/dev/bus/usb", true, NULL, false, VINF_SUCCESS, "/dev/bus/usb", false, VINF_SUCCESS },
    /* "sysfs" and valid root in the environment, method-specific init failed */
    { "sysfs", "/dev/bus/usb",  "/dev/bus/usb", true, NULL, false, VERR_NO_MEMORY, "/dev/bus/usb", false, VERR_NO_MEMORY },
    /* "sysfs" and bad root in the environment */
    { "sysfs", "/dev/bus/usb", "/dev/vboxusb", false, "/proc/usb/bus", false, VINF_SUCCESS, "", true, VERR_NOT_FOUND },
    /* "sysfs" and no root in the environment */
    { "sysfs", NULL, "/dev/vboxusb", true, NULL, false, VINF_SUCCESS, "/dev/vboxusb", false, VINF_SUCCESS },
    /* "usbfs" and valid root in the environment */
    { "usbfs", "/dev/bus/usb", NULL, false, "/dev/bus/usb", true, VINF_SUCCESS, "/dev/bus/usb", true, VINF_SUCCESS },
    /* "usbfs" and valid root in the environment, method-specific init failed */
    { "usbfs", "/dev/bus/usb", NULL, false, "/dev/bus/usb", true, VERR_NO_MEMORY, "/dev/bus/usb", true, VERR_NO_MEMORY },
    /* "usbfs" and bad root in the environment */
    { "usbfs", "/dev/bus/usb", "/dev/vboxusb", false, "/proc/usb/bus", false, VINF_SUCCESS, "", true, VERR_NOT_FOUND },
    /* "usbfs" and no root in the environment */
    { "usbfs", NULL, NULL, false, "/proc/bus/usb", true, VINF_SUCCESS, "/proc/bus/usb", true, VINF_SUCCESS },
    /* invalid method in the environment, sysfs available */
    { "invalid", "/dev/bus/usb", "/dev/vboxusb", true, NULL, false, VINF_SUCCESS, "/dev/vboxusb", false, VINF_SUCCESS },
    /* invalid method in the environment, usbfs available */
    { "invalid", "/dev/bus/usb", NULL, true, "/proc/bus/usb", true, VINF_SUCCESS, "/proc/bus/usb", true, VINF_SUCCESS },
    /* invalid method in the environment, sysfs inaccessible */
    { "invalid", "/dev/bus/usb", "/dev/vboxusb", false, NULL, false, VINF_SUCCESS, "", true, VERR_VUSB_USB_DEVICE_PERMISSION },
    /* invalid method in the environment, usbfs inaccessible */
    { "invalid", "/dev/bus/usb", NULL, false, "/proc/bus/usb", false, VINF_SUCCESS, "", true, VERR_VUSB_USBFS_PERMISSION },
    /* No environment, sysfs and usbfs available but without access permissions. */
    { NULL, NULL, "/dev/vboxusb", false, "/proc/bus/usb", false, VERR_NO_MEMORY, "", true, VERR_VUSB_USB_DEVICE_PERMISSION },
    /* No environment, sysfs and usbfs available, access permissions for sysfs. */
    { NULL, NULL, "/dev/vboxusb", true, "/proc/bus/usb", false, VINF_SUCCESS, "/dev/vboxusb", false, VINF_SUCCESS },
    /* No environment, sysfs and usbfs available, access permissions for usbfs. */
    { NULL, NULL, "/dev/vboxusb", false, "/proc/bus/usb", true, VINF_SUCCESS, "/proc/bus/usb", true, VINF_SUCCESS },
    /* No environment, sysfs available but without access permissions. */
    { NULL, NULL, "/dev/vboxusb", false, NULL, false, VERR_NO_MEMORY, "", true, VERR_VUSB_USB_DEVICE_PERMISSION },
    /* No environment, sysfs available with access permissions, method-specific init failed. */
    { NULL, NULL, "/dev/vboxusb", true, NULL, false, VERR_NO_MEMORY, "/dev/vboxusb", false, VERR_NO_MEMORY },
    /* No environment, usbfs available but without access permissions. */
    { NULL, NULL, NULL, false, "/proc/bus/usb", false, VERR_NO_MEMORY, "", true, VERR_VUSB_USBFS_PERMISSION },
    /* No environment, usbfs available with access permissions, method-specific
     * init failed. */
    { NULL, NULL, NULL, false, "/proc/bus/usb", true, VERR_NO_MEMORY, "/proc/bus/usb", true, VERR_NO_MEMORY }
};

static void testInit(RTTEST hTest)
{
    RTTestSub(hTest, "Testing USBProxyServiceLinux initialisation");
    for (unsigned i = 0; i < RT_ELEMENTS(s_testEnvironment); ++i)
    {
        USBProxyServiceLinux test(NULL);
        TestUSBSetEnv(s_testEnvironment[i].pcszEnvUsb,
                        s_testEnvironment[i].pcszEnvUsbRoot);
        TestUSBSetupInit(s_testEnvironment[i].pcszUsbfsRoot,
                           s_testEnvironment[i].fUsbfsAccessible,
                           s_testEnvironment[i].pcszDevicesRoot,
                           s_testEnvironment[i].fDevicesAccessible,
                           s_testEnvironment[i].rcMethodInit);
        HRESULT hrc = test.init();
        RTTESTI_CHECK_MSG(hrc == S_OK,
                           ("init() returned 0x%x (test index %i)!\n", hrc, i));
        int rc = test.getLastError();
        RTTESTI_CHECK_MSG(rc == s_testEnvironment[i].rcExpected,
                          ("getLastError() returned %Rrc (test index %i) instead of %Rrc!\n",
                           rc, i, s_testEnvironment[i].rcExpected));
        const char *pcszDevicesRoot = test.testGetDevicesRoot();
        RTTESTI_CHECK_MSG(!RTStrCmp(pcszDevicesRoot,
                               s_testEnvironment[i].pcszDevicesRootExpected),
                          ("testGetDevicesRoot() returned %s (test index %i) instead of %s!\n",
                           pcszDevicesRoot, i,
                           s_testEnvironment[i].pcszDevicesRootExpected));
        bool fUsingUsbfs = test.testGetUsingUsbfs();
        RTTESTI_CHECK_MSG(   fUsingUsbfs
                          == s_testEnvironment[i].fUsingUsbfsExpected,
                          ("testGetUsingUsbfs() returned %RTbool (test index %i) instead of %RTbool!\n",
                           fUsingUsbfs, i,
                           s_testEnvironment[i].fUsingUsbfsExpected));
    }
}

static struct
{
    const char *pacszDeviceAddresses[16];
    const char *pacszAccessibleFiles[16];
    const char *pcszRoot;
    bool fIsDeviceNodes;
    bool fAvailableExpected;
} s_testCheckDeviceRoot[] =
{
    /* /dev/vboxusb accessible -> device nodes method available */
    { { NULL }, { "/dev/vboxusb" }, "/dev/vboxusb", true, true },
    /* /dev/vboxusb present but not accessible -> device nodes method not
     * available */
    { { NULL }, { NULL }, "/dev/vboxusb", true, false },
    /* /proc/bus/usb available but empty -> usbfs method available (we can't
     * really check in this case) */
    { { NULL }, { "/proc/bus/usb" }, "/proc/bus/usb", false, true },
    /* /proc/bus/usb not available or not accessible -> usbfs method not available */
    { { NULL }, { NULL }, "/proc/bus/usb", false, false },
    /* /proc/bus/usb available, one inaccessible device -> usbfs method not
     * available */
    { { "/proc/bus/usb/001/001" }, { "/proc/bus/usb" }, "/proc/bus/usb", false, false },
    /* /proc/bus/usb available, one device of two inaccessible -> usbfs method
     * not available */
    { { "/proc/bus/usb/001/001", "/proc/bus/usb/002/002" },
      { "/proc/bus/usb", "/proc/bus/usb/001/001" }, "/proc/bus/usb", false, false },
    /* /proc/bus/usb available, two accessible devices -> usbfs method
     * available */
    { { "/proc/bus/usb/001/001", "/proc/bus/usb/002/002" },
      { "/proc/bus/usb", "/proc/bus/usb/001/001", "/proc/bus/usb/002/002" },
      "/proc/bus/usb", false, true }
};

static void testCheckDeviceRoot(RTTEST hTest)
{
    RTTestSub(hTest, "Testing the USBProxyLinuxCheckDeviceRoot API");
    for (unsigned i = 0; i < RT_ELEMENTS(s_testCheckDeviceRoot); ++i)
    {
        TestUSBSetAvailableUsbfsDevices(s_testCheckDeviceRoot[i]
                                                .pacszDeviceAddresses);
        TestUSBSetAccessibleFiles(s_testCheckDeviceRoot[i]
                                                .pacszAccessibleFiles);
        bool fAvailable = USBProxyLinuxCheckDeviceRoot
                                  (s_testCheckDeviceRoot[i].pcszRoot,
                                   s_testCheckDeviceRoot[i].fIsDeviceNodes);
        RTTESTI_CHECK_MSG(   fAvailable
                          == s_testCheckDeviceRoot[i].fAvailableExpected,
                           ("USBProxyLinuxCheckDeviceRoot() returned %RTbool (test index %i) instead of %RTbool!\n",
                            fAvailable, i,
                            s_testCheckDeviceRoot[i].fAvailableExpected));
    }
}

int main(void)
{
    /*
     * Init the runtime, test and say hello.
     */
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate("tstUSBProxyLinux", &hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(hTest);

    /*
     * Run the tests.
     */
    testInit(hTest);
    testCheckDeviceRoot(hTest);

    /*
     * Summary
     */
    return RTTestSummaryAndDestroy(hTest);
}
