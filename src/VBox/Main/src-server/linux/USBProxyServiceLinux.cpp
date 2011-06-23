/* $Id$ */
/** @file
 * VirtualBox USB Proxy Service, Linux Specialization.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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
#include "USBGetDevices.h"
#include "Logging.h"

#include <VBox/usb.h>
#include <VBox/usblib.h>
#include <VBox/err.h>

#include <iprt/string.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/pipe.h>
#include <iprt/stream.h>
#include <iprt/linux/sysfs.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/statfs.h>
#include <sys/poll.h>
#ifdef VBOX_WITH_LINUX_COMPILER_H
# include <linux/compiler.h>
#endif
#include <linux/usbdevice_fs.h>


/**
 * Initialize data members.
 */
USBProxyServiceLinux::USBProxyServiceLinux(Host *aHost)
    : USBProxyService(aHost), mhFile(NIL_RTFILE), mhWakeupPipeR(NIL_RTPIPE),
      mhWakeupPipeW(NIL_RTPIPE), mUsingUsbfsDevices(true /* see init */),
      mUdevPolls(0), mpWaiter(NULL)
{
    LogFlowThisFunc(("aHost=%p\n", aHost));
}

#ifdef UNIT_TEST
#  ifdef UNIT_TEST
    /** The path we pretend the usbfs root is located at, or NULL. */
    const char *s_pcszTestUsbfsRoot;
    /** Should usbfs be accessible to the current user? */
    bool s_fTestUsbfsAccessible;
    /** The path we pretend the device node tree root is located at, or NULL. */
    const char *s_pcszTestDevicesRoot;
    /** Should the device node tree be accessible to the current user? */
    bool s_fTestDevicesAccessible;
    /** The result of the usbfs/inotify-specific init */
    int s_rcTestMethodInitResult;
    /** The value of the VBOX_USB environment variable. */
    const char *s_pcszTestEnvUsb;
    /** The value of the VBOX_USB_ROOT environment variable. */
    const char *s_pcszTestEnvUsbRoot;
#  endif

/** Select which access methods will be available to the @a init method
 * during unit testing, and (hack!) what return code it will see from
 * the access method-specific initialisation. */
void TestUSBSetupInit(const char *pcszUsbfsRoot, bool fUsbfsAccessible,
                      const char *pcszDevicesRoot, bool fDevicesAccessible,
                      int rcMethodInitResult)
{
    s_pcszTestUsbfsRoot = pcszUsbfsRoot;
    s_fTestUsbfsAccessible = fUsbfsAccessible;
    s_pcszTestDevicesRoot = pcszDevicesRoot;
    s_fTestDevicesAccessible = fDevicesAccessible;
    s_rcTestMethodInitResult = rcMethodInitResult;
}

/** Specify the environment that the @a init method will see during unit
 * testing. */
void TestUSBSetEnv(const char *pcszEnvUsb, const char *pcszEnvUsbRoot)
{
    s_pcszTestEnvUsb = pcszEnvUsb;
    s_pcszTestEnvUsbRoot = pcszEnvUsbRoot;
}

/* For testing we redefine anything that accesses the outside world to
 * return test values. */
# define RTEnvGet(a) \
    (  !RTStrCmp(a, "VBOX_USB") ? s_pcszTestEnvUsb \
     : !RTStrCmp(a, "VBOX_USB_ROOT") ? s_pcszTestEnvUsbRoot \
     : NULL)
# define USBProxyLinuxCheckDeviceRoot(pcszPath, fUseNodes) \
    (   ((fUseNodes) && s_fTestDevicesAccessible \
                     && !RTStrCmp(pcszPath, s_pcszTestDevicesRoot)) \
     || (!(fUseNodes) && s_fTestUsbfsAccessible \
                      && !RTStrCmp(pcszPath, s_pcszTestUsbfsRoot)))
# define RTDirExists(pcszDir) \
    (   (pcszDir) \
     && (   !RTStrCmp(pcszDir, s_pcszTestDevicesRoot) \
         || !RTStrCmp(pcszDir, s_pcszTestUsbfsRoot)))
# define RTFileExists(pcszFile) \
    (   (pcszFile) \
     && s_pcszTestUsbfsRoot \
     && !RTStrNCmp(pcszFile, s_pcszTestUsbfsRoot, strlen(s_pcszTestUsbfsRoot)) \
     && !RTStrCmp(pcszFile + strlen(s_pcszTestUsbfsRoot), "/devices"))
#endif

/**
 * Selects the access method that will be used to access USB devices based on
 * what is available on the host and what if anything the user has specified
 * in the environment.
 * @returns iprt status value
 * @param  pfUsingUsbfsDevices  on success this will be set to true if 
 *                              the prefered access method is USBFS-like and to
 *                              false if it is sysfs/device node-like
 * @param  ppcszDevicesRoot     on success the root of the tree of USBFS-like
 *                              device nodes will be stored here
 */
int USBProxyLinuxChooseMethod(bool *pfUsingUsbfsDevices,
                              const char **ppcszDevicesRoot)
{
    /*
     * We have two methods available for getting host USB device data - using
     * USBFS and using sysfs.  The default choice is sysfs; if that is not
     * available we fall back to USBFS.
     * In the event of both failing, an appropriate error will be returned.
     * The user may also specify a method and root using the VBOX_USB and
     * VBOX_USB_ROOT environment variables.  In this case we don't check
     * the root they provide for validity.
     */
    bool fUsbfsChosen = false, fSysfsChosen = false;
    const char *pcszUsbFromEnv = RTEnvGet("VBOX_USB");
    const char *pcszUsbRoot = NULL;
    if (pcszUsbFromEnv)
    {
        bool fValidVBoxUSB = true;

        pcszUsbRoot = RTEnvGet("VBOX_USB_ROOT");
        if (!RTStrICmp(pcszUsbFromEnv, "USBFS"))
        {
            LogRel(("Default USB access method set to \"usbfs\" from environment\n"));
            fUsbfsChosen = true;
        }
        else if (!RTStrICmp(pcszUsbFromEnv, "SYSFS"))
        {
            LogRel(("Default USB method set to \"sysfs\" from environment\n"));
            fSysfsChosen = true;
        }
        else
        {
            LogRel(("Invalid VBOX_USB environment variable setting \"%s\"\n",
                    pcszUsbFromEnv));
            fValidVBoxUSB = false;
            pcszUsbFromEnv = NULL;
        }
        if (!fValidVBoxUSB && pcszUsbRoot)
            pcszUsbRoot = NULL;
    }
    if (!pcszUsbRoot)
    {
        if (   !fUsbfsChosen
            && USBProxyLinuxCheckDeviceRoot("/dev/vboxusb", true))
        {
            fSysfsChosen = true;
            pcszUsbRoot = "/dev/vboxusb";
        }
        else if (   !fSysfsChosen
                 && USBProxyLinuxCheckDeviceRoot("/proc/bus/usb", false))
        {
            fUsbfsChosen = true;
            pcszUsbRoot = "/proc/bus/usb";
        }
    }
    else if (!USBProxyLinuxCheckDeviceRoot(pcszUsbRoot, fSysfsChosen))
        pcszUsbRoot = NULL;
    if (pcszUsbRoot)
    {
        *pfUsingUsbfsDevices = fUsbfsChosen;
        *ppcszDevicesRoot = pcszUsbRoot;
        return VINF_SUCCESS;
    }
    /* else */
    return   pcszUsbFromEnv ? VERR_NOT_FOUND
           : RTDirExists("/dev/vboxusb") ? VERR_VUSB_USB_DEVICE_PERMISSION
           : RTFileExists("/proc/bus/usb/devices") ? VERR_VUSB_USBFS_PERMISSION
           : VERR_NOT_FOUND;
}

/**
 * Initializes the object (called right after construction).
 *
 * @returns S_OK on success and non-fatal failures, some COM error otherwise.
 */
HRESULT USBProxyServiceLinux::init(void)
{
    const char *pcszDevicesRoot;
    int rc = USBProxyLinuxChooseMethod(&mUsingUsbfsDevices, &pcszDevicesRoot);
    if (RT_SUCCESS(rc))
    {
        mDevicesRoot = pcszDevicesRoot;
#ifndef UNIT_TEST /* Hack for now */
        rc = mUsingUsbfsDevices ? initUsbfs() : initSysfs();
#else
        rc = s_rcTestMethodInitResult;
#endif
        /* For the day when we have VBoxSVC release logging... */
        LogRel((RT_SUCCESS(rc) ? "Successfully initialised host USB using %s\n"
                               : "Failed to initialise host USB using %s\n",
                mUsingUsbfsDevices ? "USBFS" : "sysfs"));
    }
    mLastError = rc;
    return S_OK;
}

#ifdef UNIT_TEST
# undef RTEnvGet
# undef USBProxyLinuxCheckDeviceRoot
# undef RTDirExists
# undef RTFileExists
#endif

/**
 * Initialization routine for the usbfs based operation.
 *
 * @returns iprt status code.
 */
int USBProxyServiceLinux::initUsbfs(void)
{
    Assert(mUsingUsbfsDevices);

    /*
     * Open the devices file.
     */
    int rc;
    char *pszDevices = RTPathJoinA(mDevicesRoot.c_str(), "devices");
    if (pszDevices)
    {
        rc = RTFileOpen(&mhFile, pszDevices, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
        if (RT_SUCCESS(rc))
        {
            rc = RTPipeCreate(&mhWakeupPipeR, &mhWakeupPipeW, 0 /*fFlags*/);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Start the poller thread.
                 */
                rc = start();
                if (RT_SUCCESS(rc))
                {
                    RTStrFree(pszDevices);
                    LogFlowThisFunc(("returns successfully\n"));
                    return VINF_SUCCESS;
                }

                RTPipeClose(mhWakeupPipeR);
                RTPipeClose(mhWakeupPipeW);
                mhWakeupPipeW = mhWakeupPipeR = NIL_RTPIPE;
            }
            else
                Log(("USBProxyServiceLinux::USBProxyServiceLinux: RTFilePipe failed with rc=%Rrc\n", rc));
            RTFileClose(mhFile);
        }

        RTStrFree(pszDevices);
    }
    else
    {
        rc = VERR_NO_MEMORY;
        Log(("USBProxyServiceLinux::USBProxyServiceLinux: out of memory!\n"));
    }

    LogFlowThisFunc(("returns failure!!! (rc=%Rrc)\n", rc));
    return rc;
}


/**
 * Initialization routine for the sysfs based operation.
 *
 * @returns iprt status code
 */
int USBProxyServiceLinux::initSysfs(void)
{
    Assert(!mUsingUsbfsDevices);

#ifdef VBOX_USB_WITH_SYSFS
    try
    {
        mpWaiter = new VBoxMainHotplugWaiter(mDevicesRoot.c_str());
    }
    catch(std::bad_alloc &e)
    {
        return VERR_NO_MEMORY;
    }
    int rc = mpWaiter->getStatus();
    if (RT_SUCCESS(rc) || rc == VERR_TIMEOUT || rc == VERR_TRY_AGAIN)
        rc = start();
    else if (rc == VERR_NOT_SUPPORTED)
        /* This can legitimately happen if hal or DBus are not running, but of
         * course we can't start in this case. */
        rc = VINF_SUCCESS;
    return rc;

#else  /* !VBOX_USB_WITH_SYSFS */
    return VERR_NOT_IMPLEMENTED;
#endif /* !VBOX_USB_WITH_SYSFS */
}


/**
 * Stop all service threads and free the device chain.
 */
USBProxyServiceLinux::~USBProxyServiceLinux()
{
    LogFlowThisFunc(("\n"));

    /*
     * Stop the service.
     */
    if (isActive())
        stop();

    /*
     * Free resources.
     */
    doUsbfsCleanupAsNeeded();
#ifdef VBOX_USB_WITH_SYSFS
    if (mpWaiter)
        delete mpWaiter;
#endif
}


/**
 * If any Usbfs-related resources are currently allocated, then free them
 * and mark them as freed.
 */
void USBProxyServiceLinux::doUsbfsCleanupAsNeeded()
{
    /*
     * Free resources.
     */
    RTFileClose(mhFile);
    mhFile = NIL_RTFILE;

    RTPipeClose(mhWakeupPipeR);
    RTPipeClose(mhWakeupPipeW);
    mhWakeupPipeW = mhWakeupPipeR = NIL_RTPIPE;
}


int USBProxyServiceLinux::captureDevice(HostUSBDevice *aDevice)
{
    Log(("USBProxyServiceLinux::captureDevice: %p {%s}\n", aDevice, aDevice->getName().c_str()));
    AssertReturn(aDevice, VERR_GENERAL_FAILURE);
    AssertReturn(aDevice->isWriteLockOnCurrentThread(), VERR_GENERAL_FAILURE);

    /*
     * Don't think we need to do anything when the device is held... fake it.
     */
    Assert(aDevice->getUnistate() == kHostUSBDeviceState_Capturing);
    interruptWait();

    return VINF_SUCCESS;
}


int USBProxyServiceLinux::releaseDevice(HostUSBDevice *aDevice)
{
    Log(("USBProxyServiceLinux::releaseDevice: %p\n", aDevice));
    AssertReturn(aDevice, VERR_GENERAL_FAILURE);
    AssertReturn(aDevice->isWriteLockOnCurrentThread(), VERR_GENERAL_FAILURE);

    /*
     * We're not really holding it atm., just fake it.
     */
    Assert(aDevice->getUnistate() == kHostUSBDeviceState_ReleasingToHost);
    interruptWait();

    return VINF_SUCCESS;
}


bool USBProxyServiceLinux::updateDeviceState(HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice, bool *aRunFilters, SessionMachine **aIgnoreMachine)
{
    if (    aUSBDevice->enmState == USBDEVICESTATE_USED_BY_HOST_CAPTURABLE
        &&  aDevice->mUsb->enmState == USBDEVICESTATE_USED_BY_HOST)
        LogRel(("USBProxy: Device %04x:%04x (%s) has become accessible.\n",
                aUSBDevice->idVendor, aUSBDevice->idProduct, aUSBDevice->pszAddress));
    return updateDeviceStateFake(aDevice, aUSBDevice, aRunFilters, aIgnoreMachine);
}


/**
 * A device was added, we need to adjust mUdevPolls.
 *
 * See USBProxyService::deviceAdded for details.
 */
void USBProxyServiceLinux::deviceAdded(ComObjPtr<HostUSBDevice> &aDevice, SessionMachinesList &llOpenedMachines, PUSBDEVICE aUSBDevice)
{
    if (aUSBDevice->enmState == USBDEVICESTATE_USED_BY_HOST)
    {
        LogRel(("USBProxy: Device %04x:%04x (%s) isn't accessible. giving udev a few seconds to fix this...\n",
                aUSBDevice->idVendor, aUSBDevice->idProduct, aUSBDevice->pszAddress));
        mUdevPolls = 10; /* (10 * 500ms = 5s) */
    }

    USBProxyService::deviceAdded(aDevice, llOpenedMachines, aUSBDevice);
}


int USBProxyServiceLinux::wait(RTMSINTERVAL aMillies)
{
    int rc;
    if (mUsingUsbfsDevices)
        rc = waitUsbfs(aMillies);
    else
        rc = waitSysfs(aMillies);
    return rc;
}


/** String written to the wakeup pipe. */
#define WAKE_UP_STRING      "WakeUp!"
/** Length of the string written. */
#define WAKE_UP_STRING_LEN  ( sizeof(WAKE_UP_STRING) - 1 )

int USBProxyServiceLinux::waitUsbfs(RTMSINTERVAL aMillies)
{
    struct pollfd PollFds[2];

    /* Cap the wait interval if we're polling for udevd changing device permissions. */
    if (aMillies > 500 && mUdevPolls > 0)
    {
        mUdevPolls--;
        aMillies = 500;
    }

    memset(&PollFds, 0, sizeof(PollFds));
    PollFds[0].fd        = RTFileToNative(mhFile);
    PollFds[0].events    = POLLIN;
    PollFds[1].fd        = RTPipeToNative(mhWakeupPipeR);
    PollFds[1].events    = POLLIN | POLLERR | POLLHUP;

    int rc = poll(&PollFds[0], 2, aMillies);
    if (rc == 0)
        return VERR_TIMEOUT;
    if (rc > 0)
    {
        /* drain the pipe */
        if (PollFds[1].revents & POLLIN)
        {
            char szBuf[WAKE_UP_STRING_LEN];
            rc = RTPipeReadBlocking(mhWakeupPipeR, szBuf, sizeof(szBuf), NULL);
            AssertRC(rc);
        }
        return VINF_SUCCESS;
    }
    return RTErrConvertFromErrno(errno);
}


int USBProxyServiceLinux::waitSysfs(RTMSINTERVAL aMillies)
{
#ifdef VBOX_USB_WITH_SYSFS
    int rc = mpWaiter->Wait(aMillies);
    if (rc == VERR_TRY_AGAIN)
    {
        RTThreadYield();
        rc = VINF_SUCCESS;
    }
    return rc;
#else  /* !VBOX_USB_WITH_SYSFS */
    return USBProxyService::wait(aMillies);
#endif /* !VBOX_USB_WITH_SYSFS */
}


int USBProxyServiceLinux::interruptWait(void)
{
#ifdef VBOX_USB_WITH_SYSFS
    LogFlowFunc(("mUsingUsbfsDevices=%d\n", mUsingUsbfsDevices));
    if (!mUsingUsbfsDevices)
    {
        mpWaiter->Interrupt();
        LogFlowFunc(("Returning VINF_SUCCESS\n"));
        return VINF_SUCCESS;
    }
#endif /* VBOX_USB_WITH_SYSFS */
    int rc = RTPipeWriteBlocking(mhWakeupPipeW, WAKE_UP_STRING, WAKE_UP_STRING_LEN, NULL);
    if (RT_SUCCESS(rc))
        RTPipeFlush(mhWakeupPipeW);
    LogFlowFunc(("returning %Rrc\n", rc));
    return rc;
}


PUSBDEVICE USBProxyServiceLinux::getDevices(void)
{
    return USBProxyLinuxGetDevices(mDevicesRoot.c_str(), !mUsingUsbfsDevices);
}
