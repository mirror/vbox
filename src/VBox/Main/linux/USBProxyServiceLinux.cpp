/* $Id$ */
/** @file
 * VirtualBox USB Proxy Service, Linux Specialization.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/linux/sysfs.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/statfs.h>
#include <sys/poll.h>
#ifdef VBOX_WITH_LINUX_COMPILER_H
# include <linux/compiler.h>
#endif
#include <linux/usbdevice_fs.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Suffix translation. */
typedef struct USBSUFF
{
    char        szSuff[4];
    unsigned    cchSuff;
    unsigned    uMul;
    unsigned    uDiv;
} USBSUFF, *PUSBSUFF;
typedef const USBSUFF *PCUSBSUFF;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Suffixes for the endpoint polling interval.
 */
static const USBSUFF s_aIntervalSuff[] =
{
    { "ms", 2,    1,       0 },
    { "us", 2,    1,    1000 },
    { "ns", 2,    1, 1000000 },
    { "s",  1, 1000,       0 },
    { "",   0,    0,       0 }  /* term */
};


/**
 * Initialize data members.
 */
USBProxyServiceLinux::USBProxyServiceLinux(Host *aHost, const char *aUsbfsRoot /* = "/proc/bus/usb" */)
    : USBProxyService(aHost), mFile(NIL_RTFILE), mWakeupPipeR(NIL_RTFILE),
      mWakeupPipeW(NIL_RTFILE), mUsbfsRoot(aUsbfsRoot), mUsingUsbfsDevices(true /* see init */), mUdevPolls(0)
{
    LogFlowThisFunc(("aHost=%p aUsbfsRoot=%p:{%s}\n", aHost, aUsbfsRoot, aUsbfsRoot));
}


/**
 * Initializes the object (called right after construction).
 *
 * @returns S_OK on success and non-fatal failures, some COM error otherwise.
 */
HRESULT USBProxyServiceLinux::init(void)
{
    /*
     * Call the superclass method first.
     */
    HRESULT hrc = USBProxyService::init();
    AssertComRCReturn(hrc, hrc);

    /*
     * We have two methods available for getting host USB device data - using
     * USBFS and using sysfs/hal.  The default choice depends on build-time
     * settings and an environment variable; if the default is not available
     * we fall back to the second.
     * In the event of both failing, the error from the second method tried
     * will be presented to the user.
     */
#ifdef VBOX_WITH_SYSFS_BY_DEFAULT
    mUsingUsbfsDevices = false;
#else
    mUsingUsbfsDevices = true;
#endif
    const char *pszUsbFromEnv = RTEnvGet("VBOX_USB");
    if (pszUsbFromEnv)
    {
        if (!RTStrICmp(pszUsbFromEnv, "USBFS"))
        {
            LogRel(("Default USB access method set to \"usbfs\" from environment\n"));
            mUsingUsbfsDevices = true;
        }
        else if (!RTStrICmp(pszUsbFromEnv, "SYSFS"))
        {
            LogRel(("Default USB method set to \"sysfs\" from environment\n"));
            mUsingUsbfsDevices = false;
        }
        else
            LogRel(("Invalid VBOX_USB environment variable setting \"%s\"\n",
                    pszUsbFromEnv));
    }
    int rc = mUsingUsbfsDevices ? initUsbfs() : initSysfs();
    if (RT_FAILURE(rc))
    {
        /* For the day when we have VBoxSVC release logging... */
        LogRel(("Failed to initialise host USB using %s\n",
                mUsingUsbfsDevices ? "USBFS" : "sysfs/hal"));
        mUsingUsbfsDevices = !mUsingUsbfsDevices;
        rc = mUsingUsbfsDevices ? initUsbfs() : initSysfs();
    }
    LogRel((RT_SUCCESS(rc) ? "Successfully initialised host USB using %s\n"
                           : "Failed to initialise host USB using %s\n",
            mUsingUsbfsDevices ? "USBFS" : "sysfs/hal"));
    mLastError = rc;
    return S_OK;
}


/**
 * Initializiation routine for the usbfs based operation.
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
    char *pszDevices;
    RTStrAPrintf(&pszDevices, "%s/devices", mUsbfsRoot.c_str());
    if (pszDevices)
    {
        rc = USBProxyLinuxCheckForUsbfs(pszDevices);
        if (RT_SUCCESS(rc))
        {
            rc = RTFileOpen(&mFile, pszDevices, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
            if (RT_SUCCESS(rc))
            {
                int pipes[2];
                if (!pipe(pipes))
                {
                    mWakeupPipeR = pipes[0];
                    mWakeupPipeW = pipes[1];
                    /*
                     * Start the poller thread.
                     */
                    rc = start();
                    if (RT_SUCCESS(rc))
                    {
                        RTStrFree(pszDevices);
                        LogFlowThisFunc(("returns successfully - mWakeupPipeR/W=%d/%d\n",
                                         mWakeupPipeR, mWakeupPipeW));
                        return VINF_SUCCESS;
                    }

                    RTFileClose(mWakeupPipeR);
                    RTFileClose(mWakeupPipeW);
                    mWakeupPipeW = mWakeupPipeR = NIL_RTFILE;
                }
                else
                {
                    rc = RTErrConvertFromErrno(errno);
                    Log(("USBProxyServiceLinux::USBProxyServiceLinux: pipe failed, errno=%d\n", errno));
                }
                RTFileClose(mFile);
            }

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
 * Initializiation routine for the sysfs based operation.
 *
 * @returns iprt status code
 */
int USBProxyServiceLinux::initSysfs(void)
{
    Assert(!mUsingUsbfsDevices);

#ifdef VBOX_USB_WITH_SYSFS
    int rc = mWaiter.getStatus();
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

    /* (No extra work for !mUsingUsbfsDevices.) */
}


/**
 * If any Usbfs-releated resources are currently allocated, then free them
 * and mark them as freed.
 */
void USBProxyServiceLinux::doUsbfsCleanupAsNeeded()
{
    /*
     * Free resources.
     */
    if (mFile != NIL_RTFILE)
    {
        RTFileClose(mFile);
        mFile = NIL_RTFILE;
    }

    if (mWakeupPipeR != NIL_RTFILE)
        RTFileClose(mWakeupPipeR);
    if (mWakeupPipeW != NIL_RTFILE)
        RTFileClose(mWakeupPipeW);
    mWakeupPipeW = mWakeupPipeR = NIL_RTFILE;
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
    PollFds[0].fd        = mFile;
    PollFds[0].events    = POLLIN;
    PollFds[1].fd        = mWakeupPipeR;
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
            rc = RTFileRead(mWakeupPipeR, szBuf, sizeof(szBuf), NULL);
            AssertRC(rc);
        }
        return VINF_SUCCESS;
    }
    return RTErrConvertFromErrno(errno);
}


int USBProxyServiceLinux::waitSysfs(RTMSINTERVAL aMillies)
{
#ifdef VBOX_USB_WITH_SYSFS
    int rc = mWaiter.Wait(aMillies);
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
        mWaiter.Interrupt();
        LogFlowFunc(("Returning VINF_SUCCESS\n"));
        return VINF_SUCCESS;
    }
#endif /* VBOX_USB_WITH_SYSFS */
    int rc = RTFileWrite(mWakeupPipeW, WAKE_UP_STRING, WAKE_UP_STRING_LEN, NULL);
    if (RT_SUCCESS(rc))
        RTFileFlush(mWakeupPipeW);
    LogFlowFunc(("returning %Rrc\n", rc));
    return rc;
}


PUSBDEVICE USBProxyServiceLinux::getDevices(void)
{
    if (mUsingUsbfsDevices)
        return USBProxyLinuxGetDevices(mUsbfsRoot.c_str());
    else
        return USBProxyLinuxGetDevices(NULL);
}

