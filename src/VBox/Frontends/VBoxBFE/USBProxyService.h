/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Declaration of USBProxyService and USBProxyServiceLinux classes
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ____H_USBPROXYSERVICE
#define ____H_USBPROXYSERVICE

#ifndef VBOXBFE_WITH_USB
# error "misconfiguration VBOXBFE_WITH_USB isn't defined and USBProxyService.h was included."
#endif

#include "HostUSBImpl.h"
#include "HostUSBDeviceImpl.h"

/**
 * Base class for the USB Proxy service.
 */
class USBProxyService
{
public:
    USBProxyService (HostUSB *aHost);
    virtual ~USBProxyService();

    /**
     * A VM is trying to capture a device, do necessary preperations.
     *
     * @returns VBox status code.
     * @param   pDevice     The device in question.
     */
    virtual int captureDevice (HostUSBDevice *pDevice);

    /**
     * The device is to be held so that the host OS will not start using it.
     *
     * @returns VBox status code.
     * @param   pDevice     The device in question.
     */
    virtual int holdDevice (HostUSBDevice *pDevice);

    /**
     * A VM is releasing a device back to the host.
     *
     * @returns VBox status code.
     * @param   pDevice     The device in question.
     */
    virtual int releaseDevice (HostUSBDevice *pDevice);

    /**
     * A VM is releasing a device back to be held or assigned to another VM.
     * A port reset should be performed.
     *
     * @returns VBox status code.
     * @param   pDevice     The device in question.
     */
    virtual int resetDevice (HostUSBDevice *pDevice);

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

public:
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
    /** Pointer to the HostUSB object. */
    HostUSB *mHost;
    /** Thread handle of the service thread. */
    RTTHREAD mThread;
    /** Flag which stop() sets to cause serviceThread to return. */
    bool volatile mTerminate;
    /** List of smart HostUSBDevice pointers. */
    typedef std::list <HostUSBDevice *> HostUSBDeviceList;
    /** List of the known USB devices. */
    HostUSBDeviceList mDevices;
    /** VBox status code of the last failure.
     * (Only used by start(), stop() and the child constructors.) */
    int mLastError;
};


#if defined(RT_OS_LINUX) || defined(RT_OS_L4)
#include <stdio.h>

/**
 * The Linux hosted USB Proxy Service.
 */
class USBProxyServiceLinux : public USBProxyService
{
public:
    USBProxyServiceLinux (HostUSB *aHost, const char *aUsbfsRoot = "/proc/bus/usb");
    ~USBProxyServiceLinux();

    virtual int captureDevice (HostUSBDevice *aDevice);
    virtual int holdDevice (HostUSBDevice *aDevice);
    virtual int releaseDevice (HostUSBDevice *aDevice);
    virtual int resetDevice (HostUSBDevice *aDevice);

protected:
    virtual int wait (unsigned aMillies);
    virtual int interruptWait (void);
    virtual PUSBDEVICE getDevices (void);

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
    std::string mUsbfsRoot;
};
#endif /* RT_OS_LINUX */


#ifdef RT_OS_WINDOWS
/**
 * The Win32/Win64 hosted USB Proxy Service.
 */
class USBProxyServiceWin32 : public USBProxyService
{
public:
    USBProxyServiceWin32 (HostUSB *aHost);
    ~USBProxyServiceWin32();

    virtual int captureDevice (HostUSBDevice *aDevice);
    virtual int holdDevice (HostUSBDevice *aDevice);
    virtual int releaseDevice (HostUSBDevice *aDevice);
    virtual int resetDevice (HostUSBDevice *aDevice);

protected:
    virtual int wait (unsigned aMillies);
    virtual int interruptWait (void);
    virtual PUSBDEVICE getDevices (void);

private:

    HANDLE hEventInterrupt;
};
#endif


#endif /* !____H_USBPROXYSERVICE */
