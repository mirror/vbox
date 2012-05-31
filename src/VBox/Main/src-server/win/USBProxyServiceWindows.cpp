/* $Id$ */
/** @file
 * VirtualBox USB Proxy Service, Windows Specialization.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "USBProxyService.h"
#include "Logging.h"

#include <VBox/usb.h>
#include <VBox/err.h>

#include <iprt/string.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/err.h>

#include <VBox/usblib.h>


/**
 * Initialize data members.
 */
USBProxyServiceWindows::USBProxyServiceWindows(Host *aHost)
    : USBProxyService(aHost), mhEventInterrupt(INVALID_HANDLE_VALUE)
{
    LogFlowThisFunc(("aHost=%p\n", aHost));
}


/**
 * Initializes the object (called right after construction).
 *
 * @returns S_OK on success and non-fatal failures, some COM error otherwise.
 */
HRESULT USBProxyServiceWindows::init(void)
{
    /*
     * Create the semaphore (considered fatal).
     */
    mhEventInterrupt = CreateEvent(NULL, FALSE, FALSE, NULL);
    AssertReturn(mhEventInterrupt != INVALID_HANDLE_VALUE, E_FAIL);

    /*
     * Initialize the USB lib and stuff.
     */
    int rc = USBLibInit();
    if (RT_SUCCESS(rc))
    {
        /*
         * Start the poller thread.
         */
        rc = start();
        if (RT_SUCCESS(rc))
        {
            LogFlowThisFunc(("returns successfully\n"));
            return S_OK;
        }

        USBLibTerm();
    }

    CloseHandle(mhEventInterrupt);
    mhEventInterrupt = INVALID_HANDLE_VALUE;

    LogFlowThisFunc(("returns failure!!! (rc=%Rrc)\n", rc));
    mLastError = rc;
    return S_OK;
}


/**
 * Stop all service threads and free the device chain.
 */
USBProxyServiceWindows::~USBProxyServiceWindows()
{
    LogFlowThisFunc(("\n"));

    /*
     * Stop the service.
     */
    if (isActive())
        stop();

    if (mhEventInterrupt != INVALID_HANDLE_VALUE)
        CloseHandle(mhEventInterrupt);
    mhEventInterrupt = INVALID_HANDLE_VALUE;

    /*
     * Terminate the library...
     */
    int rc = USBLibTerm();
    AssertRC(rc);
}


void *USBProxyServiceWindows::insertFilter(PCUSBFILTER aFilter)
{
    AssertReturn(aFilter, NULL);

    LogFlow(("USBProxyServiceWindows::insertFilter()\n"));

    void *pvId = USBLibAddFilter(aFilter);

    LogFlow(("USBProxyServiceWindows::insertFilter(): returning pvId=%p\n", pvId));

    return pvId;
}


void USBProxyServiceWindows::removeFilter(void *aID)
{
    LogFlow(("USBProxyServiceWindows::removeFilter(): id=%p\n", aID));

    AssertReturnVoid(aID);

    USBLibRemoveFilter(aID);
}


int USBProxyServiceWindows::captureDevice(HostUSBDevice *aDevice)
{
    /*
     * Check preconditions.
     */
    AssertReturn(aDevice, VERR_GENERAL_FAILURE);
    AssertReturn(!aDevice->isWriteLockOnCurrentThread(), VERR_GENERAL_FAILURE);

    AutoReadLock devLock(aDevice COMMA_LOCKVAL_SRC_POS);
    LogFlowThisFunc(("aDevice=%s\n", aDevice->getName().c_str()));

    Assert(aDevice->getUnistate() == kHostUSBDeviceState_Capturing);

    /*
     * Create a one-shot ignore filter for the device
     * and trigger a re-enumeration of it.
     */
    USBFILTER Filter;
    USBFilterInit(&Filter, USBFILTERTYPE_ONESHOT_CAPTURE);
    initFilterFromDevice(&Filter, aDevice);
    Log(("USBFILTERIDX_PORT=%#x\n", USBFilterGetNum(&Filter, USBFILTERIDX_PORT)));
    Log(("USBFILTERIDX_BUS=%#x\n", USBFilterGetNum(&Filter, USBFILTERIDX_BUS)));

    void *pvId = USBLibAddFilter(&Filter);
    if (!pvId)
    {
        AssertMsgFailed(("Add one-shot Filter failed\n"));
        return VERR_GENERAL_FAILURE;
    }

    int rc = USBLibRunFilters();
    if (!RT_SUCCESS(rc))
    {
        AssertMsgFailed(("Run Filters failed\n"));
        USBLibRemoveFilter(pvId);
        return rc;
    }

    return VINF_SUCCESS;
}


int USBProxyServiceWindows::releaseDevice(HostUSBDevice *aDevice)
{
    /*
     * Check preconditions.
     */
    AssertReturn(aDevice, VERR_GENERAL_FAILURE);
    AssertReturn(!aDevice->isWriteLockOnCurrentThread(), VERR_GENERAL_FAILURE);

    AutoReadLock devLock(aDevice COMMA_LOCKVAL_SRC_POS);
    LogFlowThisFunc(("aDevice=%s\n", aDevice->getName().c_str()));

    Assert(aDevice->getUnistate() == kHostUSBDeviceState_Capturing);

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
    {
        AssertMsgFailed(("Add one-shot Filter failed\n"));
        return VERR_GENERAL_FAILURE;
    }

    int rc = USBLibRunFilters();
    if (!RT_SUCCESS(rc))
    {
        AssertMsgFailed(("Run Filters failed\n"));
        USBLibRemoveFilter(pvId);
        return rc;
    }


    return VINF_SUCCESS;
}


bool USBProxyServiceWindows::updateDeviceState(HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice, bool *aRunFilters, SessionMachine **aIgnoreMachine)
{
    AssertReturn(aDevice, false);
    AssertReturn(!aDevice->isWriteLockOnCurrentThread(), false);
    /* Nothing special here so far, so fall back on parent */
    return USBProxyService::updateDeviceState(aDevice, aUSBDevice, aRunFilters, aIgnoreMachine);

/// @todo remove?
#if 0

    /*
     * We're only called in the 'existing device' state, so if there is a pending async
     * operation we can check if it completed here and suppress state changes if it hasn't.
     */
    /* TESTME */
    if (aDevice->isStatePending())
    {
        bool fRc = aDevice->updateState(aUSBDevice);
        if (fRc)
        {
            if (aDevice->state() != aDevice->pendingState())
                fRc = false;
        }
        return fRc;
    }

    /* fall back on parent. */
    return USBProxyService::updateDeviceState(aDevice, aUSBDevice, aRunFilters, aIgnoreMachine);
#endif
}


int USBProxyServiceWindows::wait(unsigned aMillies)
{
    return USBLibWaitChange(aMillies);
}


int USBProxyServiceWindows::interruptWait(void)
{
    return USBLibInterruptWaitChange();
}

/**
 * Gets a list of all devices the VM can grab
 */
PUSBDEVICE USBProxyServiceWindows::getDevices(void)
{
    PUSBDEVICE pDevices = NULL;
    uint32_t cDevices = 0;

    Log(("USBProxyServiceWindows::getDevices\n"));
    USBLibGetDevices(&pDevices, &cDevices);
    return pDevices;
}

