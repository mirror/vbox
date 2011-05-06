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
    int rcExpected;
    const char *pcszDevicesRootExpected;
    bool fUsingUsbfsExpected;
} s_testEnvironment[] =
{
    { "sysfs", "/dev/bus/usb", NULL, false, NULL, false, VINF_SUCCESS, VINF_SUCCESS, "/dev/bus/usb", false },
    { "sysfs", "/dev/bus/usb", NULL, false, NULL, false, VERR_NO_MEMORY, VERR_NO_MEMORY, "/dev/bus/usb", false },
    { "sysfs", "/dev/bus/usb", "/dev/usbvbox", false, "/proc/usb/bus", false, VINF_SUCCESS, VINF_SUCCESS, "/dev/bus/usb", false },
    { "sysfs", "/dev/bus/usb", "/dev/usbvbox", false, "/proc/usb/bus", false, VERR_NO_MEMORY, VERR_NO_MEMORY, "/dev/bus/usb", false },
    { "sysfs", NULL, "/dev/vboxusb", true, NULL, false, VINF_SUCCESS, VINF_SUCCESS, "/dev/vboxusb", false },
    { "usbfs", "/dev/bus/usb", NULL, false, NULL, false, VINF_SUCCESS, VINF_SUCCESS, "/dev/bus/usb", true },
    { "usbfs", "/dev/bus/usb", NULL, false, NULL, false, VERR_NO_MEMORY, VERR_NO_MEMORY, "/dev/bus/usb", true },
    { "usbfs", "/dev/bus/usb", "/dev/usbvbox", false, "/proc/usb/bus", false, VINF_SUCCESS, VINF_SUCCESS, "/dev/bus/usb", true },
    { "usbfs", "/dev/bus/usb", "/dev/usbvbox", false, "/proc/usb/bus", false, VERR_NO_MEMORY, VERR_NO_MEMORY, "/dev/bus/usb", true },
    { "usbfs", NULL, NULL, false, "/proc/bus/usb", true, VINF_SUCCESS, VINF_SUCCESS, "/proc/bus/usb", true },
    { NULL, NULL, "/dev/vboxusb", false, "/proc/bus/usb", false, VERR_NO_MEMORY, VERR_VUSB_USB_DEVICE_PERMISSION, "", true },
    { NULL, NULL, "/dev/vboxusb", true, "/proc/bus/usb", false, VERR_NO_MEMORY, VERR_NO_MEMORY, "/dev/vboxusb", false },
    { NULL, NULL, NULL, false, "/proc/bus/usb", false, VERR_NO_MEMORY, VERR_VUSB_USBFS_PERMISSION, "", true },
    { NULL, NULL, NULL, false, "/proc/bus/usb", true, VERR_NO_MEMORY, VERR_NO_MEMORY, "/proc/bus/usb", true }
};

static void testInit(RTTEST hTest)
{
    RTTestSub(hTest, "Testing USBProxyServiceLinux initialisation");
    for (unsigned i = 0; i < RT_ELEMENTS(s_testEnvironment); ++i)
    {
        USBProxyServiceLinux test(NULL);
        test.testSetEnv(s_testEnvironment[i].pcszEnvUsb,
                        s_testEnvironment[i].pcszEnvUsbRoot);
        test.testSetupInit(s_testEnvironment[i].pcszUsbfsRoot,
                           s_testEnvironment[i].fUsbfsAccessible,
                           s_testEnvironment[i].pcszDevicesRoot,
                           s_testEnvironment[i].fDevicesAccessible,
                           s_testEnvironment[i].rcMethodInit);
        HRESULT hrc = test.init();
        RTTESTI_CHECK_MSG(hrc == S_OK,
                           ("init() returned 0x%x on line %i!\n", hrc, i));
        int rc = test.getLastError();
        RTTESTI_CHECK_MSG(rc == s_testEnvironment[i].rcExpected,
                          ("getLastError() returned %Rrc on line %i instead of %Rrc!\n",
                           rc, i, s_testEnvironment[i].rcExpected));
        const char *pcszDevicesRoot = test.testGetDevicesRoot();
        RTTESTI_CHECK_MSG(!RTStrCmp(pcszDevicesRoot,
                               s_testEnvironment[i].pcszDevicesRootExpected),
                          ("testGetDevicesRoot() returned %s on line %i instead of %s!\n",
                           pcszDevicesRoot, i,
                           s_testEnvironment[i].pcszDevicesRootExpected));
        bool fUsingUsbfs = test.testGetUsingUsbfs();
        RTTESTI_CHECK_MSG(   fUsingUsbfs
                          == s_testEnvironment[i].fUsingUsbfsExpected,
                          ("testGetUsingUsbfs() returned %RTbool on line %i instead of %RTbool!\n",
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
    { { NULL }, { "/dev/vboxusb" }, "/dev/vboxusb", true, true },
    { { NULL }, { NULL }, "/dev/vboxusb", true, false },
    { { NULL }, { NULL }, "/proc/bus/usb", false, true },
    { { "/proc/bus/usb/001/001" }, { NULL }, "/proc/bus/usb", false, false },
    { { "/proc/bus/usb/001/001", "/proc/bus/usb/002/002" },
      { "/proc/bus/usb/001/001" }, "/proc/bus/usb", false, false },
    { { "/proc/bus/usb/001/001", "/proc/bus/usb/002/002" },
      { "/proc/bus/usb/001/001", "/proc/bus/usb/002/002" },
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
                           ("USBProxyLinuxCheckDeviceRoot() returned %RTbool on line %i instead of %RTbool!\n",
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
