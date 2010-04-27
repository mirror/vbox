/* $Id$ */
/** @file
 * Classes for handling hardware detection under Linux.
 *
 * Please feel free to expand these to work for other systems (Solaris!) or to
 * add new ones for other systems.
 */

/*
 * Copyright (C) 2008-2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_HOSTHARDWARELINUX
# define ____H_HOSTHARDWARELINUX

#include <iprt/err.h>
#include <iprt/cpp/ministring.h>
#include <vector>

/**
 * Class for probing and returning information about host DVD and floppy
 * drives.  To use this class, create an instance, call one of the update
 * methods to do the actual probing and use the iterator methods to get the
 * result of the probe.
 */
class VBoxMainDriveInfo
{
public:
    /** Structure describing a host drive */
    struct DriveInfo
    {
        /** The device node of the drive. */
        iprt::MiniString mDevice;
        /** A unique identifier for the device, if available.  This should be
         * kept consistant accross different probing methods of a given
         * platform if at all possible. */
        iprt::MiniString mUdi;
        /** A textual description of the drive. */
        iprt::MiniString mDescription;

        /** Constructors */
        DriveInfo(const iprt::MiniString &aDevice,
                  const iprt::MiniString &aUdi = "",
                  const iprt::MiniString &aDescription = "")
            : mDevice(aDevice),
              mUdi(aUdi),
              mDescription(aDescription)
        { }
    };

    /** List (resp vector) holding drive information */
    typedef std::vector<DriveInfo> DriveInfoList;

    /**
     * Search for host floppy drives and rebuild the list, which remains empty
     * until the first time this method is called.
     * @returns iprt status code
     */
    int updateFloppies();

    /**
     * Search for host DVD drives and rebuild the list, which remains empty
     * until the first time this method is called.
     * @returns iprt status code
     */
    int updateDVDs();

    /** Get the first element in the list of floppy drives. */
    DriveInfoList::const_iterator FloppyBegin()
    {
        return mFloppyList.begin();
    }

    /** Get the last element in the list of floppy drives. */
    DriveInfoList::const_iterator FloppyEnd()
    {
        return mFloppyList.end();
    }

    /** Get the first element in the list of DVD drives. */
    DriveInfoList::const_iterator DVDBegin()
    {
        return mDVDList.begin();
    }

    /** Get the last element in the list of DVD drives. */
    DriveInfoList::const_iterator DVDEnd()
    {
        return mDVDList.end();
    }
private:
    /** The list of currently available floppy drives */
    DriveInfoList mFloppyList;
    /** The list of currently available DVD drives */
    DriveInfoList mDVDList;
};

/** Convenience typedef. */
typedef VBoxMainDriveInfo::DriveInfoList DriveInfoList;
/** Convenience typedef. */
typedef VBoxMainDriveInfo::DriveInfo DriveInfo;

/**
 * Class for probing and returning information about host USB devices.
 * To use this class, create an instance, call the update methods to do the
 * actual probing and use the iterator methods to get the result of the probe.
 */
class VBoxMainUSBDeviceInfo
{
public:
    /** Structure describing a host USB device */
    struct USBDeviceInfo
    {
        /** The device node of the device. */
        iprt::MiniString mDevice;
        /** The system identifier of the device.  Specific to the probing
         * method. */
        iprt::MiniString mSysfsPath;
        /** Type for the list of interfaces. */
        typedef std::vector<iprt::MiniString> InterfaceList;
        /** The system IDs of the device's interfaces. */
        InterfaceList mInterfaces;

        /** Constructors */
        USBDeviceInfo(const iprt::MiniString &aDevice,
                      const iprt::MiniString &aSystemID)
            : mDevice(aDevice),
              mSysfsPath(aSystemID)
        { }
    };

    /** List (resp vector) holding drive information */
    typedef std::vector<USBDeviceInfo> DeviceInfoList;

    /**
     * Search for host USB devices and rebuild the list, which remains empty
     * until the first time this method is called.
     * @returns iprt status code
     */
    int UpdateDevices ();

    /** Get the first element in the list of USB devices. */
    DeviceInfoList::const_iterator DevicesBegin()
    {
        return mDeviceList.begin();
    }

    /** Get the last element in the list of USB devices. */
    DeviceInfoList::const_iterator DevicesEnd()
    {
        return mDeviceList.end();
    }

private:
    /** The list of currently available USB devices */
    DeviceInfoList mDeviceList;
};

/** Convenience typedef. */
typedef VBoxMainUSBDeviceInfo::DeviceInfoList USBDeviceInfoList;
/** Convenience typedef. */
typedef VBoxMainUSBDeviceInfo::USBDeviceInfo USBDeviceInfo;
/** Convenience typedef. */
typedef VBoxMainUSBDeviceInfo::USBDeviceInfo::InterfaceList USBInterfaceList;

/** Implementation of the hotplug waiter class below */
class VBoxMainHotplugWaiterImpl
{
public:
    VBoxMainHotplugWaiterImpl (void) {}
    virtual ~VBoxMainHotplugWaiterImpl (void) {}
    /** @copydoc VBoxMainHotplugWaiter::Wait */
    virtual int Wait (RTMSINTERVAL cMillies) = 0;
    /** @copydoc VBoxMainHotplugWaiter::Interrupt */
    virtual void Interrupt (void) = 0;
};

/**
 * Class for waiting for a hotplug event.  To use this class, create an
 * instance and call the @a Wait() method, which blocks until an event or a
 * user-triggered interruption occurs.  Call @a Interrupt() to interrupt the
 * wait before an event occurs.
 */
class VBoxMainHotplugWaiter
{
    /** Class implementation. */
    VBoxMainHotplugWaiterImpl *mImpl;
public:
    /** Constructor.  Responsible for selecting the implementation. */
    VBoxMainHotplugWaiter (void);
    /** Destructor. */
    ~VBoxMainHotplugWaiter (void)
    {
        delete mImpl;
    }
    /**
     * Wait for a hotplug event.
     *
     * @returns  VINF_SUCCESS if an event occurred or if Interrupt() was called.
     * @returns  VERR_TRY_AGAIN if the wait failed but this might (!) be a
     *           temporary failure.
     * @returns  VERR_NOT_SUPPORTED if the wait failed and will definitely not
     *           succeed if retried.
     * @returns  Possibly other iprt status codes otherwise.
     * @param    cMillies   How long to wait for at most.
     */
    int Wait (RTMSINTERVAL cMillies)
    {
        return mImpl->Wait(cMillies);
    }
    /**
     * Interrupts an active wait.  In the current implementation, the wait
     * may not return until up to two seconds after calling this method.
     */
    void Interrupt (void)
    {
        mImpl->Interrupt();
    }
};

#endif /* ____H_HOSTHARDWARELINUX */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
