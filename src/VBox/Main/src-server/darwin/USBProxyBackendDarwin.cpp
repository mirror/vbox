/* $Id$ */
/** @file
 * VirtualBox USB Proxy Service (in VBoxSVC), Darwin Specialization.
 */

/*
 * Copyright (C) 2005-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "USBProxyService.h"
#include "Logging.h"
#include "iokit.h"

#include <VBox/usb.h>
#include <VBox/usblib.h>
#include <VBox/err.h>

#include <iprt/string.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/asm.h>


/**
 * Initialize data members.
 */
USBProxyServiceDarwin::USBProxyServiceDarwin(Host *aHost)
    : USBProxyService(aHost), mServiceRunLoopRef(NULL), mNotifyOpaque(NULL), mWaitABitNextTime(false), mUSBLibInitialized(false)
{
    LogFlowThisFunc(("aHost=%p\n", aHost));
}


/**
 * Initializes the object (called right after construction).
 *
 * @returns VBox status code.
 */
int USBProxyServiceDarwin::init(void)
{
    /*
     * Initialize the USB library.
     */
    int rc = USBLibInit();
    if (RT_FAILURE(rc))
        return rc;

    mUSBLibInitialized = true;

    /*
     * Start the poller thread.
     */
    start();
    return VINF_SUCCESS;
}


/**
 * Stop all service threads and free the device chain.
 */
USBProxyServiceDarwin::~USBProxyServiceDarwin()
{
    LogFlowThisFunc(("\n"));

    /*
     * Stop the service.
     */
    if (isActive())
        stop();

    /*
     * Terminate the USB library - it'll
     */
    if (mUSBLibInitialized)
    {
        USBLibTerm();
        mUSBLibInitialized = false;
    }
}


void *USBProxyServiceDarwin::insertFilter(PCUSBFILTER aFilter)
{
    return USBLibAddFilter(aFilter);
}


void USBProxyServiceDarwin::removeFilter(void *aId)
{
    USBLibRemoveFilter(aId);
}


int USBProxyServiceDarwin::captureDevice(HostUSBDevice *aDevice)
{
    /*
     * Check preconditions.
     */
    AssertReturn(aDevice, VERR_GENERAL_FAILURE);
    AssertReturn(!aDevice->isWriteLockOnCurrentThread(), VERR_GENERAL_FAILURE);

    AutoReadLock devLock(aDevice COMMA_LOCKVAL_SRC_POS);
    LogFlowThisFunc(("aDevice=%s\n", aDevice->i_getName().c_str()));

    Assert(aDevice->i_getUnistate() == kHostUSBDeviceState_Capturing);

    /*
     * Create a one-shot capture filter for the device (don't
     * match on port) and trigger a re-enumeration of it.
     */
    USBFILTER Filter;
    USBFilterInit(&Filter, USBFILTERTYPE_ONESHOT_CAPTURE);
    initFilterFromDevice(&Filter, aDevice);

    void *pvId = USBLibAddFilter(&Filter);
    if (!pvId)
        return VERR_GENERAL_FAILURE;

    int rc = DarwinReEnumerateUSBDevice(aDevice->mUsb);
    if (RT_SUCCESS(rc))
        aDevice->i_setBackendUserData(pvId);
    else
    {
        USBLibRemoveFilter(pvId);
        pvId = NULL;
    }
    LogFlowThisFunc(("returns %Rrc pvId=%p\n", rc, pvId));
    return rc;
}


void USBProxyServiceDarwin::captureDeviceCompleted(HostUSBDevice *aDevice, bool aSuccess)
{
    AssertReturnVoid(aDevice->isWriteLockOnCurrentThread());

    /*
     * Remove the one-shot filter if necessary.
     */
    LogFlowThisFunc(("aDevice=%s aSuccess=%RTbool mOneShotId=%p\n", aDevice->i_getName().c_str(), aSuccess, aDevice->i_getBackendUserData()));
    if (!aSuccess && aDevice->i_getBackendUserData())
        USBLibRemoveFilter(aDevice->i_getBackendUserData());
    aDevice->i_setBackendUserData(NULL);
}


int USBProxyServiceDarwin::releaseDevice(HostUSBDevice *aDevice)
{
    /*
     * Check preconditions.
     */
    AssertReturn(aDevice, VERR_GENERAL_FAILURE);
    AssertReturn(!aDevice->isWriteLockOnCurrentThread(), VERR_GENERAL_FAILURE);

    AutoReadLock devLock(aDevice COMMA_LOCKVAL_SRC_POS);
    LogFlowThisFunc(("aDevice=%s\n", aDevice->i_getName().c_str()));

    Assert(aDevice->i_getUnistate() == kHostUSBDeviceState_ReleasingToHost);

    /*
     * Create a one-shot ignore filter for the device
     * and trigger a re-enumeration of it.
     */
    USBFILTER Filter;
    USBFilterInit(&Filter, USBFILTERTYPE_ONESHOT_IGNORE);
    initFilterFromDevice(&Filter, aDevice);
    Log(("USBFILTERIDX_PORT=%#x\n", USBFilterGetNum(&Filter, USBFILTERIDX_PORT)));
    Log(("USBFILTERIDX_BUS=%#x\n", USBFilterGetNum(&Filter, USBFILTERIDX_BUS)));

    void *pvId = USBLibAddFilter(&Filter);
    if (!pvId)
        return VERR_GENERAL_FAILURE;

    int rc = DarwinReEnumerateUSBDevice(aDevice->mUsb);
    if (RT_SUCCESS(rc))
        aDevice->i_setBackendUserData(pvId);
    else
    {
        USBLibRemoveFilter(pvId);
        pvId = NULL;
    }
    LogFlowThisFunc(("returns %Rrc pvId=%p\n", rc, pvId));
    return rc;
}


void USBProxyServiceDarwin::releaseDeviceCompleted(HostUSBDevice *aDevice, bool aSuccess)
{
    AssertReturnVoid(aDevice->isWriteLockOnCurrentThread());

    /*
     * Remove the one-shot filter if necessary.
     */
    LogFlowThisFunc(("aDevice=%s aSuccess=%RTbool mOneShotId=%p\n", aDevice->i_getName().c_str(), aSuccess, aDevice->i_getBackendUserData()));
    if (!aSuccess && aDevice->i_getBackendUserData())
        USBLibRemoveFilter(aDevice->i_getBackendUserData());
    aDevice->i_setBackendUserData(NULL);
}


/** @todo unused */
void USBProxyServiceDarwin::detachingDevice(HostUSBDevice *aDevice)
{
    NOREF(aDevice);
}


bool USBProxyServiceDarwin::updateDeviceState(HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice, bool *aRunFilters, SessionMachine **aIgnoreMachine)
{
    AssertReturn(aDevice, false);
    AssertReturn(!aDevice->isWriteLockOnCurrentThread(), false);
    /* Nothing special here so far, so fall back on parent. */
    return USBProxyService::updateDeviceState(aDevice, aUSBDevice, aRunFilters, aIgnoreMachine);
}


int USBProxyServiceDarwin::wait(RTMSINTERVAL aMillies)
{
    SInt32 rc = CFRunLoopRunInMode(CFSTR(VBOX_IOKIT_MODE_STRING),
                                   mWaitABitNextTime && aMillies >= 1000
                                   ? 1.0 /* seconds */
                                   : aMillies >= 5000 /* Temporary measure to poll for status changes (MSD). */
                                   ? 5.0 /* seconds */
                                   : aMillies / 1000.0,
                                   true);
    mWaitABitNextTime = rc != kCFRunLoopRunTimedOut;

    return VINF_SUCCESS;
}


int USBProxyServiceDarwin::interruptWait(void)
{
    if (mServiceRunLoopRef)
        CFRunLoopStop(mServiceRunLoopRef);
    return 0;
}


PUSBDEVICE USBProxyServiceDarwin::getDevices(void)
{
    /* call iokit.cpp */
    return DarwinGetUSBDevices();
}


void USBProxyServiceDarwin::serviceThreadInit(void)
{
    mServiceRunLoopRef = CFRunLoopGetCurrent();
    mNotifyOpaque = DarwinSubscribeUSBNotifications();
}


void USBProxyServiceDarwin::serviceThreadTerm(void)
{
    DarwinUnsubscribeUSBNotifications(mNotifyOpaque);
    mServiceRunLoopRef = NULL;
}


/**
 * Wrapper called from iokit.cpp.
 *
 * @param   pCur    The USB device to free.
 */
void DarwinFreeUSBDeviceFromIOKit(PUSBDEVICE pCur)
{
    USBProxyService::freeDevice(pCur);
}

