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

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/env.h>
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

static USBDEVTREELOCATION s_deviceRoot = { "", false };
static bool s_fGetDeviceRootPreferSysfs = false;

PCUSBDEVTREELOCATION USBProxyLinuxGetDeviceRoot(bool fPreferSysfs)
{
    s_fGetDeviceRootPreferSysfs = fPreferSysfs;
    return &s_deviceRoot;
}

static struct
{
    const char *pcszDevicesRoot;
    bool fUseSysfs;
} s_getDevices;

PUSBDEVICE USBProxyLinuxGetDevices(const char *pcszDevicesRoot,
                                   bool fUseSysfs)
{
    s_getDevices.pcszDevicesRoot = pcszDevicesRoot;
    s_getDevices.fUseSysfs = fUseSysfs;
    return NULL;
}

void SysFreeString(BSTR bstr)
{
    Assert(0);
}

/*** END STUBS ***/

class tstUSBProxyLinux : public USBProxyServiceLinux
{
protected:
    virtual int initUsbfs(void) { return VINF_SUCCESS; }
    virtual int initSysfs(void) { return VINF_SUCCESS; }
public:
    tstUSBProxyLinux(void) : USBProxyServiceLinux(NULL) {}
    PUSBDEVICE getDevices(void)
    {
        return USBProxyServiceLinux::getDevices();
    }
};

static struct 
{
    const char *pcszVBOX_USB;
    const char *pcszVBOX_USB_ROOT;
    const char *pcszReturnedRoot;
    bool fReturnedUseSysfs;
    bool fRequestedUseSysfs;
    const char *pcszFinalRoot;
    bool fFinalUseSysfs;
} s_testEnvironment[] =
{
    { "sysfs", "/dev/bus/usb", "/proc/bus/usb", false, true, "/dev/bus/usb", true },
    { "sysfs", "/dev/bus/usb", "/dev/vboxusb", true, true, "/dev/bus/usb", true },
    { "sysfs", NULL, "/proc/bus/usb", false, true, "/proc/bus/usb", false },
    { "sysfs", NULL, "/dev/vboxusb", true, true, "/dev/vboxusb", true },
    { "usbfs", "/dev/bus/usb", "/proc/bus/usb", false, false, "/dev/bus/usb", false },
    { "usbfs", "/dev/bus/usb", "/dev/vboxusb", true, false, "/dev/bus/usb", false },
    { "usbfs", NULL, "/proc/bus/usb", false, false, "/proc/bus/usb", false },
    { "usbfs", NULL, "/dev/vboxusb", true, false, "/dev/vboxusb", true },
    { "nofs", "/dev/bus/usb", "/proc/bus/usb", false, true, "/proc/bus/usb", false },
    { "nofs", "/dev/bus/usb", "/dev/vboxusb", true, true, "/dev/vboxusb", true },
    { "nofs", NULL, "/proc/bus/usb", false, true, "/proc/bus/usb", false },
    { "nofs", NULL, "/dev/vboxusb", true, true, "/dev/vboxusb", true },
    { "", "/dev/bus/usb", "/proc/bus/usb", false, true, "/proc/bus/usb", false },
    { "", "/dev/bus/usb", "/dev/vboxusb", true, true, "/dev/vboxusb", true },
    { "", NULL, "/proc/bus/usb", false, true, "/proc/bus/usb", false },
    { "", NULL, "/dev/vboxusb", true, true, "/dev/vboxusb", true },
    { NULL, "/dev/bus/usb", "/proc/bus/usb", false, true, "/proc/bus/usb", false },
    { NULL, "/dev/bus/usb", "/dev/vboxusb", true, true, "/dev/vboxusb", true }
};

/** @note Fiddling with the real environment for this is not nice, in my
 * opinion at least. */
static void testEnvironment(RTTEST hTest)
{
    RTTestSub(hTest, "Testing environment variable handling");
    for (unsigned i = 0; i < RT_ELEMENTS(s_testEnvironment); ++i)
    {
        tstUSBProxyLinux test;
        if (s_testEnvironment[i].pcszVBOX_USB)
            RTEnvSet("VBOX_USB", s_testEnvironment[i].pcszVBOX_USB);
        else
            RTEnvUnset("VBOX_USB");
        if (s_testEnvironment[i].pcszVBOX_USB_ROOT)
            RTEnvSet("VBOX_USB_ROOT",
                     s_testEnvironment[i].pcszVBOX_USB_ROOT);
        else
            RTEnvUnset("VBOX_USB_ROOT");
        strcpy(s_deviceRoot.szDevicesRoot,
               s_testEnvironment[i].pcszReturnedRoot);
        s_deviceRoot.fUseSysfs = s_testEnvironment[i].fReturnedUseSysfs;
        RTTESTI_CHECK(test.init() == S_OK);
        test.getDevices();
        RTTESTI_CHECK_MSG(!strcmp(s_getDevices.pcszDevicesRoot,
                          s_testEnvironment[i].pcszFinalRoot),
                          ("i=%u: %s, %s\n", i, s_getDevices.pcszDevicesRoot,
                          s_testEnvironment[i].pcszFinalRoot));
        RTTESTI_CHECK_MSG(   s_getDevices.fUseSysfs
                          == s_testEnvironment[i].fFinalUseSysfs,
                          ("i=%u, %d, %d\n", i, s_getDevices.fUseSysfs,
                          s_testEnvironment[i].fFinalUseSysfs));
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
    testEnvironment(hTest);

    /*
     * Summary
     */
    return RTTestSummaryAndDestroy(hTest);
}

