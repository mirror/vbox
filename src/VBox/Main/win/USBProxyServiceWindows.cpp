/* $Id$ */
/** @file
 * VirtualBox USB Proxy Service, Windows Specialization.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
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
     * Call the superclass method first.
     */
    HRESULT hrc = USBProxyService::init();
    AssertComRCReturn(hrc, hrc);

    /*
     * Create the semaphore (considered fatal).
     */
    mhEventInterrupt = CreateEvent(NULL, FALSE, FALSE, NULL);
    AssertReturn(mhEventInterrupt != INVALID_HANDLE_VALUE, E_FAIL);

    /*
     * Initalize the USB lib and stuff.
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
    AssertReturn(aDevice, VERR_GENERAL_FAILURE);
    AssertReturn(aDevice->isWriteLockOnCurrentThread(), VERR_GENERAL_FAILURE);
    Assert(aDevice->getUnistate() == kHostUSBDeviceState_Capturing);

/** @todo pass up a one-shot filter like on darwin?  */
    USHORT vendorId, productId, revision;

    HRESULT rc;

    rc = aDevice->COMGETTER(VendorId)(&vendorId);
    AssertComRCReturn(rc, VERR_INTERNAL_ERROR);

    rc = aDevice->COMGETTER(ProductId)(&productId);
    AssertComRCReturn(rc, VERR_INTERNAL_ERROR);

    rc = aDevice->COMGETTER(Revision)(&revision);
    AssertComRCReturn(rc, VERR_INTERNAL_ERROR);

    return USBLibCaptureDevice(vendorId, productId, revision);
}


int USBProxyServiceWindows::releaseDevice(HostUSBDevice *aDevice)
{
    AssertReturn(aDevice, VERR_GENERAL_FAILURE);
    AssertReturn(aDevice->isWriteLockOnCurrentThread(), VERR_GENERAL_FAILURE);
    Assert(aDevice->getUnistate() == kHostUSBDeviceState_ReleasingToHost);

/** @todo pass up a one-shot filter like on darwin?  */
    USHORT vendorId, productId, revision;
    HRESULT rc;

    rc = aDevice->COMGETTER(VendorId)(&vendorId);
    AssertComRCReturn(rc, VERR_INTERNAL_ERROR);

    rc = aDevice->COMGETTER(ProductId)(&productId);
    AssertComRCReturn(rc, VERR_INTERNAL_ERROR);

    rc = aDevice->COMGETTER(Revision)(&revision);
    AssertComRCReturn(rc, VERR_INTERNAL_ERROR);

    Log(("USBProxyServiceWindows::releaseDevice\n"));
    return USBLibReleaseDevice(vendorId, productId, revision);
}


bool USBProxyServiceWindows::updateDeviceState(HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice, bool *aRunFilters, SessionMachine **aIgnoreMachine)
{
    /* Nothing special here so far, so fall back on parent */
    return USBProxyService::updateDeviceState(aDevice, aUSBDevice, aRunFilters, aIgnoreMachine);

/// @todo remove?
#if 0
    AssertReturn(aDevice, false);
    AssertReturn(aDevice->isWriteLockOnCurrentThread(), false);

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
    DWORD rc;

    /* Not going to do something fancy where we block in the filter
     * driver and are woken up when the state has changed.
     * Would be better, but this is good enough.
     */
    do
    {
        rc = WaitForSingleObject(mhEventInterrupt, RT_MIN(aMillies, 100));
        if (rc == WAIT_OBJECT_0)
            return VINF_SUCCESS;
        /** @todo handle WAIT_FAILED here */

        if (USBLibHasPendingDeviceChanges() == true)
        {
            Log(("wait thread detected usb change\n"));
            return VINF_SUCCESS;
        }

        if (aMillies > 100)
            aMillies -= 100;
    }
    while (aMillies > 100);

    return VERR_TIMEOUT;
}


int USBProxyServiceWindows::interruptWait(void)
{
    SetEvent(mhEventInterrupt);
    return VINF_SUCCESS;
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

