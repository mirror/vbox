/* $Id$ */
/** @file
 * VirtualBox USB Proxy Service (base) class.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#ifndef ____H_USBPROXYSERVICE
#define ____H_USBPROXYSERVICE

#include "HostImpl.h"
#include "HostUSBDeviceImpl.h"

/**
 * Base class for the USB Proxy service.
 */
class USBProxyService
{
public:
    USBProxyService (Host *aHost);
    virtual ~USBProxyService();

    /**
     * A filter was inserted / loaded.
     *
     * @param   aFilter         Pointer to the inserted filter.
     * @return  ID of the inserted filter
     */
#ifdef VBOX_WITH_USBFILTER
    virtual void *insertFilter (PCUSBFILTER aFilter);
#else
    virtual void *insertFilter (IUSBDeviceFilter *aFilter);
#endif

    /**
     * A filter was removed.
     *
     * @param   aId             ID of the filter to remove
     */
    virtual void removeFilter (void *aId);

    /**
     * A VM is trying to capture a device, do necessary preperations.
     *
     * @returns VBox status code.
     * @param   aDevice     The device in question.
     */
    virtual int captureDevice (HostUSBDevice *aDevice);

    /**
     * The device is going to be detached from a VM.
     *
     * @param   aDevice     The device in question.
     */
    virtual void detachingDevice (HostUSBDevice *aDevice);

    /**
     * A VM is releasing a device back to the host.
     *
     * @returns VBox status code.
     * @param   aDevice     The device in question.
     */
    virtual int releaseDevice (HostUSBDevice *aDevice);

    /**
     * Updates the device state.
     * This is responsible for calling HostUSBDevice::updateState() and check for async completion.
     *
     * @returns true if there is a state change.
     * @param   aDevice     The device in question.
     * @param   aUSBDevice  The USB device structure for the last enumeration.
     */
    virtual bool updateDeviceState (HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice);

    /**
     * Add device notification hook for the OS specific code.
     *
     * @param   aDevice     The device in question.
     * @param   aUSBDevice  The USB device structure.
     */
    virtual void deviceAdded (HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice);

    /**
     * Remove device notification hook for the OS specific code.
     *
     * @param   aDevice     The device in question.
     */
    virtual void deviceRemoved (HostUSBDevice *aDevice);

    /**
     * Query if the service is active and working.
     *
     * @returns true if the service is up running.
     * @returns false if the service isn't running.
     */
    bool isActive (void);

    /**
     * Get last error.
     * Can be used to check why the proxy !isActive() upon construction.
     *
     * @returns VBox status code.
     */
    int getLastError (void);

    /**
     * Calculate the hash of the serial string.
     *
     * 64bit FNV1a, chosen because it is designed to hash in to a power of two
     * space, and is much quicker and simpler than, say, a half MD4.
     *
     * @returns the hash.
     * @param   aSerial     The serial string.
     */
    static uint64_t calcSerialHash (const char *aSerial);

#ifdef VBOX_WITH_USBFILTER
    /**
     * Initializes a filter with the data from the specified device.
     *
     * @param   aFilter     The filter to fill.
     * @param   aDevice     The device to fill it with.
     */
    static void initFilterFromDevice (PUSBFILTER aFilter, HostUSBDevice *aDevice);
#endif

protected:

    /**
     * Starts the service.
     *
     * @returns VBox status.
     */
    int start (void);

    /**
     * Stops the service.
     *
     * @returns VBox status.
     */
    int stop (void);

    /**
     * Wait for a change in the USB devices attached to the host.
     *
     * @returns VBox status (ignored).
     * @param   aMillies    Number of milliseconds to wait.
     */
    virtual int wait (unsigned aMillies);

    /**
     * Interrupt any wait() call in progress.
     *
     * @returns VBox status.
     */
    virtual int interruptWait (void);

    /**
     * Get a list of USB device currently attached to the host.
     *
     * @returns Pointer to a list of USB devices.
     *          The list nodes are freed individually by calling freeDevice().
     */
    virtual PUSBDEVICE getDevices (void);

    /**
     * First call made on the service thread, use it to do
     * thread initialization.
     */
    virtual void serviceThreadInit (void);

    /**
     * First call made on the service thread, use it to do
     * thread termination.
     */
    virtual void serviceThreadTerm (void);

    /**
     * Implement fake capture, ++.
     *
     * @returns true if there is a state change.
     * @param   pDevice     The device in question.
     * @param   pUSBDevice  The USB device structure for the last enumeration.
     */
    bool updateDeviceStateFake (HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice);


public:
    /**
     * Free all the members of a USB interface returned by getDevice().
     *
     * @param   pIf         Pointer to the interface.
     * @param   cIfs        Number of consecutive interfaces pIf points to
     */
    static void freeInterfaceMembers (PUSBINTERFACE pIf, unsigned cIfs);

    /**
     * Free all the members of a USB device returned by getDevice().
     *
     * @param   pDevice     Pointer to the device.
     */
    static void freeDeviceMembers (PUSBDEVICE pDevice);

    /**
     * Free one USB device returned by getDevice().
     *
     * @param   pDevice     Pointer to the device.
     */
    static void freeDevice (PUSBDEVICE pDevice);

private:
    /**
     * Process any relevant changes in the attached USB devices.
     */
    void processChanges (void);

    /**
     * The service thread created by start().
     *
     * @param   Thread      The thread handle.
     * @param   pvUser      Pointer to the USBProxyService instance.
     */
    static DECLCALLBACK (int) serviceThread (RTTHREAD Thread, void *pvUser);

protected:
    /** Pointer to the Host object. */
    Host *mHost;
    /** Thread handle of the service thread. */
    RTTHREAD mThread;
    /** Flag which stop() sets to cause serviceThread to return. */
    bool volatile mTerminate;
    /** List of smart HostUSBDevice pointers. */
    typedef std::list <ComObjPtr <HostUSBDevice> > HostUSBDeviceList;
    /** List of the known USB devices. */
    HostUSBDeviceList mDevices;
    /** VBox status code of the last failure.
     * (Only used by start(), stop() and the child constructors.) */
    int mLastError;
};


#ifdef VBOX_WITH_USB

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
    USBProxyServiceDarwin (Host *aHost);
    ~USBProxyServiceDarwin();

#ifdef VBOX_WITH_USBFILTER
    virtual void *insertFilter (PCUSBFILTER aFilter);
    virtual void removeFilter (void *aId);
#endif

    virtual int captureDevice (HostUSBDevice *aDevice);
    virtual void detachingDevice (HostUSBDevice *aDevice);
    virtual int releaseDevice (HostUSBDevice *aDevice);
    virtual bool updateDeviceState (HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice);

protected:
    virtual int wait (unsigned aMillies);
    virtual int interruptWait (void);
    virtual PUSBDEVICE getDevices (void);
    virtual void serviceThreadInit (void);
    virtual void serviceThreadTerm (void);

private:
    /** Reference to the runloop of the service thread.
     * This is NULL if the service thread isn't running. */
    CFRunLoopRef mServiceRunLoopRef;
    /** The opaque value returned by DarwinSubscribeUSBNotifications. */
    void *mNotifyOpaque;
    /** A hack to work around the problem with the usb device enumeration
     * not including newly attached devices. */
    bool mWaitABitNextTime;
#ifndef VBOX_WITH_USBFILTER
    /** Whether we've got a fake async event and should return without entering the runloop. */
    bool volatile mFakeAsync;
#endif
    /** Whether we've successfully initialized the USBLib and should call USBLibTerm in the destructor. */
    bool mUSBLibInitialized;
};
# endif /* RT_OS_DARWIN */


# ifdef RT_OS_LINUX
#  include <stdio.h>

/**
 * The Linux hosted USB Proxy Service.
 */
class USBProxyServiceLinux : public USBProxyService
{
public:
    USBProxyServiceLinux (Host *aHost, const char *aUsbfsRoot = "/proc/bus/usb");
    ~USBProxyServiceLinux();

    virtual int captureDevice (HostUSBDevice *aDevice);
    virtual int releaseDevice (HostUSBDevice *aDevice);
    virtual bool updateDeviceState (HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice);
    virtual void deviceAdded (HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice);

protected:
    virtual int wait (unsigned aMillies);
    virtual int interruptWait (void);
    virtual PUSBDEVICE getDevices (void);
    int addDeviceToChain (PUSBDEVICE pDev, PUSBDEVICE *ppFirst, PUSBDEVICE **pppNext, int rc);

private:
    /** File handle to the '/proc/bus/usb/devices' file. */
    RTFILE mFile;
    /** Stream for mFile. */
    FILE *mStream;
    /** Pipe used to interrupt wait(), the read end. */
    RTFILE mWakeupPipeR;
    /** Pipe used to interrupt wait(), the write end. */
    RTFILE mWakeupPipeW;
    /** The root of usbfs. */
    Utf8Str mUsbfsRoot;
    /** Number of 500ms polls left to do. See usbDeterminState for details. */
    unsigned mUdevPolls;
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
    ~USBProxyServiceOs2();

    virtual int captureDevice (HostUSBDevice *aDevice);
    virtual int releaseDevice (HostUSBDevice *aDevice);
    virtual bool updateDeviceState (HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice);

protected:
    virtual int wait (unsigned aMillies);
    virtual int interruptWait (void);
    virtual PUSBDEVICE getDevices (void);
    int addDeviceToChain (PUSBDEVICE pDev, PUSBDEVICE *ppFirst, PUSBDEVICE **pppNext, int rc);

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


# ifdef RT_OS_WINDOWS
/**
 * The Win32 hosted USB Proxy Service.
 */
class USBProxyServiceWin32 : public USBProxyService
{
public:
    USBProxyServiceWin32 (Host *aHost);
    ~USBProxyServiceWin32();

    virtual void *insertFilter (IUSBDeviceFilter *aFilter);
    virtual void removeFilter (void *aID);

    virtual int captureDevice (HostUSBDevice *aDevice);
    virtual int releaseDevice (HostUSBDevice *aDevice);
    virtual bool updateDeviceState (HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice);

protected:
    virtual int wait (unsigned aMillies);
    virtual int interruptWait (void);
    virtual PUSBDEVICE getDevices (void);

private:

    HANDLE hEventInterrupt;
};
# endif /* RT_OS_WINDOWS */

#endif /* VBOX_WITH_USB */


#endif /* !____H_USBPROXYSERVICE */
