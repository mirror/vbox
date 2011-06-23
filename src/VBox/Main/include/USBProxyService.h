/* $Id$ */
/** @file
 * VirtualBox USB Proxy Service (base) class.
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


#ifndef ____H_USBPROXYSERVICE
#define ____H_USBPROXYSERVICE

#include <VBox/usb.h>
#include <VBox/usbfilter.h>

#include "VirtualBoxBase.h"
#include "VirtualBoxImpl.h"
#include "HostUSBDeviceImpl.h"
class Host;

/**
 * Base class for the USB Proxy service.
 */
class USBProxyService
    : public VirtualBoxTranslatable
{
public:
    USBProxyService(Host *aHost);
    virtual HRESULT init(void);
    virtual ~USBProxyService();

    /**
     * Override of the default locking class to be used for validating lock
     * order with the standard member lock handle.
     */
    virtual VBoxLockingClass getLockingClass() const
    {
        // the USB proxy service uses the Host object lock, so return the
        // same locking class as the host
        return LOCKCLASS_HOSTOBJECT;
    }

    bool isActive(void);
    int getLastError(void);
    HRESULT getLastErrorMessage(BSTR *aError);

    RWLockHandle *lockHandle() const;

    /** @name Host Interfaces
     * @{ */
    HRESULT getDeviceCollection(ComSafeArrayOut(IHostUSBDevice *, aUSBDevices));
    /** @} */

    /** @name SessionMachine Interfaces
     * @{ */
    HRESULT captureDeviceForVM(SessionMachine *aMachine, IN_GUID aId);
    HRESULT detachDeviceFromVM(SessionMachine *aMachine, IN_GUID aId, bool aDone);
    HRESULT autoCaptureDevicesForVM(SessionMachine *aMachine);
    HRESULT detachAllDevicesFromVM(SessionMachine *aMachine, bool aDone, bool aAbnormal);
    /** @} */

    /** @name Interface for the USBController and the Host object.
     * @{ */
    virtual void *insertFilter(PCUSBFILTER aFilter);
    virtual void removeFilter(void *aId);
    /** @} */

    /** @name Interfaces for the HostUSBDevice
     * @{ */
    virtual int captureDevice(HostUSBDevice *aDevice);
    virtual void captureDeviceCompleted(HostUSBDevice *aDevice, bool aSuccess);
    virtual void detachingDevice(HostUSBDevice *aDevice);
    virtual int releaseDevice(HostUSBDevice *aDevice);
    virtual void releaseDeviceCompleted(HostUSBDevice *aDevice, bool aSuccess);
    /** @} */

protected:
    int start(void);
    int stop(void);
    virtual void serviceThreadInit(void);
    virtual void serviceThreadTerm(void);

    virtual int wait(RTMSINTERVAL aMillies);
    virtual int interruptWait(void);
    virtual PUSBDEVICE getDevices(void);
    virtual void deviceAdded(ComObjPtr<HostUSBDevice> &aDevice, SessionMachinesList &llOpenedMachines, PUSBDEVICE aUSBDevice);
    virtual void deviceRemoved(ComObjPtr<HostUSBDevice> &aDevice);
    virtual void deviceChanged(ComObjPtr<HostUSBDevice> &aDevice, SessionMachinesList *pllOpenedMachines, SessionMachine *aIgnoreMachine);
    bool updateDeviceStateFake(HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice, bool *aRunFilters, SessionMachine **aIgnoreMachine);
    virtual bool updateDeviceState(HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice, bool *aRunFilters, SessionMachine **aIgnoreMachine);

    ComObjPtr<HostUSBDevice> findDeviceById(IN_GUID aId);

    static HRESULT setError(HRESULT aResultCode, const char *aText, ...);

    static void initFilterFromDevice(PUSBFILTER aFilter, HostUSBDevice *aDevice);
    static void freeDeviceMembers(PUSBDEVICE pDevice);
public:
    static void freeDevice(PUSBDEVICE pDevice);

private:
    HRESULT runAllFiltersOnDevice(ComObjPtr<HostUSBDevice> &aDevice,
                                  SessionMachinesList &llOpenedMachines,
                                  SessionMachine *aIgnoreMachine);
    bool runMachineFilters(SessionMachine *aMachine, ComObjPtr<HostUSBDevice> &aDevice);
    void processChanges(void);
    static DECLCALLBACK(int) serviceThread(RTTHREAD Thread, void *pvUser);

protected:
    /** Pointer to the Host object. */
    Host *mHost;
    /** Thread handle of the service thread. */
    RTTHREAD mThread;
    /** Flag which stop() sets to cause serviceThread to return. */
    bool volatile mTerminate;
    /** VBox status code of the last failure.
     * (Only used by start(), stop() and the child constructors.) */
    int mLastError;
    /** Optional error message to complement mLastError. */
    Bstr mLastErrorMessage;
    /** List of smart HostUSBDevice pointers. */
    typedef std::list<ComObjPtr<HostUSBDevice> > HostUSBDeviceList;
    /** List of the known USB devices. */
    HostUSBDeviceList mDevices;
};


# ifdef RT_OS_DARWIN
#  include <VBox/param.h>
#  undef PAGE_SHIFT
#  undef PAGE_SIZE
#  define OSType Carbon_OSType
#  include <Carbon/Carbon.h>
#  undef OSType

/**
 * The Darwin hosted USB Proxy Service.
 */
class USBProxyServiceDarwin : public USBProxyService
{
public:
    USBProxyServiceDarwin(Host *aHost);
    HRESULT init(void);
    ~USBProxyServiceDarwin();

#ifdef VBOX_WITH_NEW_USB_CODE_ON_DARWIN
    virtual void *insertFilter(PCUSBFILTER aFilter);
    virtual void removeFilter(void *aId);
#endif

    virtual int captureDevice(HostUSBDevice *aDevice);
    virtual void captureDeviceCompleted(HostUSBDevice *aDevice, bool aSuccess);
    virtual void detachingDevice(HostUSBDevice *aDevice);
    virtual int releaseDevice(HostUSBDevice *aDevice);
    virtual void releaseDeviceCompleted(HostUSBDevice *aDevice, bool aSuccess);

protected:
    virtual int wait(RTMSINTERVAL aMillies);
    virtual int interruptWait (void);
    virtual PUSBDEVICE getDevices (void);
    virtual void serviceThreadInit (void);
    virtual void serviceThreadTerm (void);
    virtual bool updateDeviceState (HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice, bool *aRunFilters, SessionMachine **aIgnoreMachine);

private:
    /** Reference to the runloop of the service thread.
     * This is NULL if the service thread isn't running. */
    CFRunLoopRef mServiceRunLoopRef;
    /** The opaque value returned by DarwinSubscribeUSBNotifications. */
    void *mNotifyOpaque;
    /** A hack to work around the problem with the usb device enumeration
     * not including newly attached devices. */
    bool mWaitABitNextTime;
#ifndef VBOX_WITH_NEW_USB_CODE_ON_DARWIN
    /** Whether we've got a fake async event and should return without entering the runloop. */
    bool volatile mFakeAsync;
#endif
    /** Whether we've successfully initialized the USBLib and should call USBLibTerm in the destructor. */
    bool mUSBLibInitialized;
};
# endif /* RT_OS_DARWIN */


# ifdef RT_OS_LINUX
#  include <stdio.h>
#  ifdef VBOX_USB_WITH_SYSFS
#   include <HostHardwareLinux.h>
#  endif

#ifdef UNIT_TEST
void TestUSBSetupInit(const char *pcszUsbfsRoot, bool fUsbfsAccessible,
                      const char *pcszDevicesRoot, bool fDevicesAccessible,
                      int rcMethodInitResult);
void TestUSBSetEnv(const char *pcszEnvUsb, const char *pcszEnvUsbRoot);
#endif

/**
 * The Linux hosted USB Proxy Service.
 */
class USBProxyServiceLinux
    : public USBProxyService
{
public:
    USBProxyServiceLinux(Host *aHost);
    HRESULT init(void);
    ~USBProxyServiceLinux();

    virtual int captureDevice(HostUSBDevice *aDevice);
    virtual int releaseDevice(HostUSBDevice *aDevice);

#  ifdef UNIT_TEST
    /* Test getters for querying internal state.  This will go away. */
    bool testGetUsingUsbfs(void) { return mUsingUsbfsDevices; }
    const char *testGetDevicesRoot(void) { return mDevicesRoot.c_str(); }
#  endif

protected:
    int initUsbfs(void);
    int initSysfs(void);
    void doUsbfsCleanupAsNeeded(void);
    virtual int wait(RTMSINTERVAL aMillies);
    virtual int interruptWait(void);
    virtual PUSBDEVICE getDevices(void);
    virtual void deviceAdded(ComObjPtr<HostUSBDevice> &aDevice, SessionMachinesList &llOpenedMachines, PUSBDEVICE aUSBDevice);
    virtual bool updateDeviceState(HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice, bool *aRunFilters, SessionMachine **aIgnoreMachine);

private:
    int waitUsbfs(RTMSINTERVAL aMillies);
    int waitSysfs(RTMSINTERVAL aMillies);

private:
    /** File handle to the '/proc/bus/usb/devices' file. */
    RTFILE mhFile;
    /** Pipe used to interrupt wait(), the read end. */
    RTPIPE mhWakeupPipeR;
    /** Pipe used to interrupt wait(), the write end. */
    RTPIPE mhWakeupPipeW;
    /** The root of usbfs. */
    Utf8Str mDevicesRoot;
    /** Whether we're using <mUsbfsRoot>/devices or /sys/whatever. */
    bool mUsingUsbfsDevices;
    /** Number of 500ms polls left to do. See usbDeterminState for details. */
    unsigned mUdevPolls;
#  ifdef VBOX_USB_WITH_SYSFS
    /** Object used for polling for hotplug events from hal. */
    VBoxMainHotplugWaiter *mpWaiter;
#  endif
};
# endif /* RT_OS_LINUX */


# ifdef RT_OS_OS2
#  include <usbcalls.h>

/**
 * The Linux hosted USB Proxy Service.
 */
class USBProxyServiceOs2 : public USBProxyService
{
public:
    USBProxyServiceOs2 (Host *aHost);
    /// @todo virtual HRESULT init(void);
    ~USBProxyServiceOs2();

    virtual int captureDevice (HostUSBDevice *aDevice);
    virtual int releaseDevice (HostUSBDevice *aDevice);

protected:
    virtual int wait(RTMSINTERVAL aMillies);
    virtual int interruptWait(void);
    virtual PUSBDEVICE getDevices(void);
    int addDeviceToChain(PUSBDEVICE pDev, PUSBDEVICE *ppFirst, PUSBDEVICE **pppNext, int rc);
    virtual bool updateDeviceState(HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice, bool *aRunFilters, SessionMachine **aIgnoreMachine);

private:
    /** The notification event semaphore */
    HEV mhev;
    /** The notification id. */
    USBNOTIFY mNotifyId;
    /** The usbcalls.dll handle. */
    HMODULE mhmod;
    /** UsbRegisterChangeNotification */
    APIRET (APIENTRY *mpfnUsbRegisterChangeNotification)(PUSBNOTIFY, HEV, HEV);
    /** UsbDeregisterNotification */
    APIRET (APIENTRY *mpfnUsbDeregisterNotification)(USBNOTIFY);
    /** UsbQueryNumberDevices */
    APIRET (APIENTRY *mpfnUsbQueryNumberDevices)(PULONG);
    /** UsbQueryDeviceReport */
    APIRET (APIENTRY *mpfnUsbQueryDeviceReport)(ULONG, PULONG, PVOID);
};
# endif /* RT_OS_LINUX */


# ifdef RT_OS_SOLARIS
#  include <libdevinfo.h>

/**
 * The Solaris hosted USB Proxy Service.
 */
class USBProxyServiceSolaris : public USBProxyService
{
public:
    USBProxyServiceSolaris(Host *aHost);
    HRESULT init(void);
    ~USBProxyServiceSolaris();

    virtual void *insertFilter (PCUSBFILTER aFilter);
    virtual void removeFilter (void *aID);

    virtual int captureDevice (HostUSBDevice *aDevice);
    virtual int releaseDevice (HostUSBDevice *aDevice);
    virtual void captureDeviceCompleted(HostUSBDevice *aDevice, bool aSuccess);
    virtual void releaseDeviceCompleted(HostUSBDevice *aDevice, bool aSuccess);

protected:
    virtual int wait(RTMSINTERVAL aMillies);
    virtual int interruptWait(void);
    virtual PUSBDEVICE getDevices(void);
    virtual bool updateDeviceState(HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice, bool *aRunFilters, SessionMachine **aIgnoreMachine);

private:
    RTSEMEVENT mNotifyEventSem;
    /** Whether we've successfully initialized the USBLib and should call USBLibTerm in the destructor. */
    bool mUSBLibInitialized;
};
#endif  /* RT_OS_SOLARIS */


# ifdef RT_OS_WINDOWS
/**
 * The Windows hosted USB Proxy Service.
 */
class USBProxyServiceWindows : public USBProxyService
{
public:
    USBProxyServiceWindows(Host *aHost);
    HRESULT init(void);
    ~USBProxyServiceWindows();

    virtual void *insertFilter (PCUSBFILTER aFilter);
    virtual void removeFilter (void *aID);

    virtual int captureDevice (HostUSBDevice *aDevice);
    virtual int releaseDevice (HostUSBDevice *aDevice);

protected:
    virtual int wait(RTMSINTERVAL aMillies);
    virtual int interruptWait(void);
    virtual PUSBDEVICE getDevices(void);
    virtual bool updateDeviceState(HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice, bool *aRunFilters, SessionMachine **aIgnoreMachine);

private:

    HANDLE mhEventInterrupt;
};
# endif /* RT_OS_WINDOWS */

# ifdef RT_OS_FREEBSD
/**
 * The FreeBSD hosted USB Proxy Service.
 */
class USBProxyServiceFreeBSD : public USBProxyService
{
public:
    USBProxyServiceFreeBSD(Host *aHost);
    HRESULT init(void);
    ~USBProxyServiceFreeBSD();

    virtual int captureDevice(HostUSBDevice *aDevice);
    virtual int releaseDevice(HostUSBDevice *aDevice);

protected:
    int initUsbfs(void);
    int initSysfs(void);
    virtual int wait(RTMSINTERVAL aMillies);
    virtual int interruptWait(void);
    virtual PUSBDEVICE getDevices(void);
    int addDeviceToChain(PUSBDEVICE pDev, PUSBDEVICE *ppFirst, PUSBDEVICE **pppNext, int rc);
    virtual void deviceAdded(ComObjPtr<HostUSBDevice> &aDevice, SessionMachinesList &llOpenedMachines, PUSBDEVICE aUSBDevice);
    virtual bool updateDeviceState(HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice, bool *aRunFilters, SessionMachine **aIgnoreMachine);

private:
    RTSEMEVENT mNotifyEventSem;
};
# endif /* RT_OS_FREEBSD */

#endif /* !____H_USBPROXYSERVICE */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
