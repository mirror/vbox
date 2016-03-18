/* $Id$ */
/** @file
 * VirtualBox USB Proxy Service (base) class.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "USBProxyBackend.h"
#include "USBProxyService.h"
#include "HostUSBDeviceImpl.h"
#include "HostImpl.h"
#include "MachineImpl.h"
#include "VirtualBoxImpl.h"

#include "AutoCaller.h"
#include "Logging.h"

#include <VBox/com/array.h>
#include <VBox/err.h>
#include <iprt/asm.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/cpp/utils.h>


/**
 * Empty constructor.
 */
USBProxyBackend::USBProxyBackend()
{
    LogFlowThisFunc(("\n"));
}


/**
 * Empty destructor.
 */
USBProxyBackend::~USBProxyBackend()
{
}


HRESULT USBProxyBackend::FinalConstruct()
{
    return BaseFinalConstruct();
}

void USBProxyBackend::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

/**
 * Stub needed as long as the class isn't virtual
 */
int USBProxyBackend::init(USBProxyService *pUsbProxyService, const com::Utf8Str &strId, const com::Utf8Str &strAddress)
{
    NOREF(strAddress);

    m_pUsbProxyService = pUsbProxyService;
    mThread            = NIL_RTTHREAD;
    mTerminate         = false;
    unconst(m_strId)   = strId;
    m_cRefs            = 0;

    return VINF_SUCCESS;
}


void USBProxyBackend::uninit()
{
    LogFlowThisFunc(("\n"));
    Assert(mThread == NIL_RTTHREAD);
    mTerminate = true;
    m_pUsbProxyService = NULL;
}

/**
 * Query if the service is active and working.
 *
 * @returns true if the service is up running.
 * @returns false if the service isn't running.
 */
bool USBProxyBackend::isActive(void)
{
    return mThread != NIL_RTTHREAD;
}


/**
 * Returns the ID of the instance.
 *
 * @returns ID string for the instance.
 */
const com::Utf8Str &USBProxyBackend::i_getId()
{
    return m_strId;
}


/**
 * Returns the current reference counter for the backend.
 */
uint32_t USBProxyBackend::i_getRefCount()
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    return m_cRefs;
}


/**
 * Runs all the filters on the specified device.
 *
 * All filters mean global and active VM, with the exception of those
 * belonging to \a aMachine. If a global ignore filter matched or if
 * none of the filters matched, the device will be released back to
 * the host.
 *
 * The device calling us here will be in the HeldByProxy, Unused, or
 * Capturable state. The caller is aware that locks held might have
 * to be abandond because of IPC and that the device might be in
 * almost any state upon return.
 *
 *
 * @returns COM status code (only parameter & state checks will fail).
 * @param   aDevice         The USB device to apply filters to.
 * @param   aIgnoreMachine  The machine to ignore filters from (we've just
 *                          detached the device from this machine).
 *
 * @note    The caller is expected to own no locks.
 */
HRESULT USBProxyBackend::runAllFiltersOnDevice(ComObjPtr<HostUSBDevice> &aDevice,
                                               SessionMachinesList &llOpenedMachines,
                                               SessionMachine *aIgnoreMachine)
{
    LogFlowThisFunc(("{%s} ignoring=%p\n", aDevice->i_getName().c_str(), aIgnoreMachine));

    /*
     * Verify preconditions.
     */
    AssertReturn(!isWriteLockOnCurrentThread(), E_FAIL);
    AssertReturn(!aDevice->isWriteLockOnCurrentThread(), E_FAIL);

    /*
     * Get the lists we'll iterate.
     */
    Host::USBDeviceFilterList globalFilters;
    m_pUsbProxyService->i_getUSBFilters(&globalFilters);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock devLock(aDevice COMMA_LOCKVAL_SRC_POS);
    AssertMsgReturn(aDevice->i_isCapturableOrHeld(), ("{%s} %s\n", aDevice->i_getName().c_str(),
                                                      aDevice->i_getStateName()), E_FAIL);

    /*
     * Run global filters filters first.
     */
    bool fHoldIt = false;
    for (Host::USBDeviceFilterList::const_iterator it = globalFilters.begin();
         it != globalFilters.end();
         ++it)
    {
        AutoWriteLock filterLock(*it COMMA_LOCKVAL_SRC_POS);
        const HostUSBDeviceFilter::Data &data = (*it)->i_getData();
        if (aDevice->i_isMatch(data))
        {
            USBDeviceFilterAction_T action = USBDeviceFilterAction_Null;
            (*it)->COMGETTER(Action)(&action);
            if (action == USBDeviceFilterAction_Ignore)
            {
                /*
                 * Release the device to the host and we're done.
                 */
                filterLock.release();
                devLock.release();
                alock.release();
                aDevice->i_requestReleaseToHost();
                return S_OK;
            }
            if (action == USBDeviceFilterAction_Hold)
            {
                /*
                 * A device held by the proxy needs to be subjected
                 * to the machine filters.
                 */
                fHoldIt = true;
                break;
            }
            AssertMsgFailed(("action=%d\n", action));
        }
    }
    globalFilters.clear();

    /*
     * Run the per-machine filters.
     */
    for (SessionMachinesList::const_iterator it = llOpenedMachines.begin();
         it != llOpenedMachines.end();
         ++it)
    {
        ComObjPtr<SessionMachine> pMachine = *it;

        /* Skip the machine the device was just detached from. */
        if (    aIgnoreMachine
            &&  pMachine == aIgnoreMachine)
            continue;

        /* runMachineFilters takes care of checking the machine state. */
        devLock.release();
        alock.release();
        if (runMachineFilters(pMachine, aDevice))
        {
            LogFlowThisFunc(("{%s} attached to %p\n", aDevice->i_getName().c_str(), (void *)pMachine));
            return S_OK;
        }
        alock.acquire();
        devLock.acquire();
    }

    /*
     * No matching machine, so request hold or release depending
     * on global filter match.
     */
    devLock.release();
    alock.release();
    if (fHoldIt)
        aDevice->i_requestHold();
    else
        aDevice->i_requestReleaseToHost();
    return S_OK;
}


/**
 * Runs the USB filters of the machine on the device.
 *
 * If a match is found we will request capture for VM. This may cause
 * us to temporary abandon locks while doing IPC.
 *
 * @param   aMachine    Machine whose filters are to be run.
 * @param   aDevice     The USB device in question.
 * @returns @c true if the device has been or is being attached to the VM, @c false otherwise.
 *
 * @note    Locks several objects temporarily for reading or writing.
 */
bool USBProxyBackend::runMachineFilters(SessionMachine *aMachine, ComObjPtr<HostUSBDevice> &aDevice)
{
    LogFlowThisFunc(("{%s} aMachine=%p \n", aDevice->i_getName().c_str(), aMachine));

    /*
     * Validate preconditions.
     */
    AssertReturn(aMachine, false);
    AssertReturn(!isWriteLockOnCurrentThread(), false);
    AssertReturn(!aMachine->isWriteLockOnCurrentThread(), false);
    AssertReturn(!aDevice->isWriteLockOnCurrentThread(), false);
    /* Let HostUSBDevice::requestCaptureToVM() validate the state. */

    /*
     * Do the job.
     */
    ULONG ulMaskedIfs;
    if (aMachine->i_hasMatchingUSBFilter(aDevice, &ulMaskedIfs))
    {
        /* try to capture the device */
        HRESULT hrc = aDevice->i_requestCaptureForVM(aMachine, false /* aSetError */, Utf8Str(), ulMaskedIfs);
        return SUCCEEDED(hrc)
            || hrc == E_UNEXPECTED /* bad device state, give up */;
    }

    return false;
}


/**
 * A filter was inserted / loaded.
 *
 * @param   aFilter         Pointer to the inserted filter.
 * @return  ID of the inserted filter
 */
void *USBProxyBackend::insertFilter(PCUSBFILTER aFilter)
{
    // return non-NULL to fake success.
    NOREF(aFilter);
    return (void *)1;
}


/**
 * A filter was removed.
 *
 * @param   aId             ID of the filter to remove
 */
void USBProxyBackend::removeFilter(void *aId)
{
    NOREF(aId);
}


/**
 * A VM is trying to capture a device, do necessary preparations.
 *
 * @returns VBox status code.
 * @param   aDevice     The device in question.
 */
int USBProxyBackend::captureDevice(HostUSBDevice *aDevice)
{
    NOREF(aDevice);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Notification that an async captureDevice() operation completed.
 *
 * This is used by the proxy to release temporary filters.
 *
 * @returns VBox status code.
 * @param   aDevice     The device in question.
 * @param   aSuccess    Whether it succeeded or failed.
 */
void USBProxyBackend::captureDeviceCompleted(HostUSBDevice *aDevice, bool aSuccess)
{
    NOREF(aDevice);
    NOREF(aSuccess);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    incRef();
}


/**
 * The device is going to be detached from a VM.
 *
 * @param   aDevice     The device in question.
 *
 * @todo unused
 */
void USBProxyBackend::detachingDevice(HostUSBDevice *aDevice)
{
    NOREF(aDevice);
}


/**
 * A VM is releasing a device back to the host.
 *
 * @returns VBox status code.
 * @param   aDevice     The device in question.
 */
int USBProxyBackend::releaseDevice(HostUSBDevice *aDevice)
{
    NOREF(aDevice);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Notification that an async releaseDevice() operation completed.
 *
 * This is used by the proxy to release temporary filters.
 *
 * @returns VBox status code.
 * @param   aDevice     The device in question.
 * @param   aSuccess    Whether it succeeded or failed.
 */
void USBProxyBackend::releaseDeviceCompleted(HostUSBDevice *aDevice, bool aSuccess)
{
    NOREF(aDevice);
    NOREF(aSuccess);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    decRef();
}


// Internals
/////////////////////////////////////////////////////////////////////////////


/**
 * Starts the service.
 *
 * @returns VBox status code.
 */
int USBProxyBackend::start(void)
{
    int rc = VINF_SUCCESS;
    if (mThread == NIL_RTTHREAD)
    {
        /*
         * Force update before starting the poller thread.
         */
        rc = wait(0);
        if (rc == VERR_TIMEOUT || rc == VERR_INTERRUPTED || RT_SUCCESS(rc))
        {
            PUSBDEVICE pDevices = getDevices();
            m_pUsbProxyService->i_updateDeviceList(this, pDevices);

            /*
             * Create the poller thread which will look for changes.
             */
            mTerminate = false;
            rc = RTThreadCreate(&mThread, USBProxyBackend::serviceThread, this,
                                0, RTTHREADTYPE_INFREQUENT_POLLER, RTTHREADFLAGS_WAITABLE, "USBPROXY");
            AssertRC(rc);
            if (RT_SUCCESS(rc))
                LogFlowThisFunc(("started mThread=%RTthrd\n", mThread));
            else
                mThread = NIL_RTTHREAD;
        }
    }
    else
        LogFlowThisFunc(("already running, mThread=%RTthrd\n", mThread));
    return rc;
}


/**
 * Stops the service.
 *
 * @returns VBox status code.
 */
int USBProxyBackend::stop(void)
{
    int rc = VINF_SUCCESS;
    if (mThread != NIL_RTTHREAD)
    {
        /*
         * Mark the thread for termination and kick it.
         */
        ASMAtomicXchgSize(&mTerminate, true);
        rc = interruptWait();
        AssertRC(rc);

        /*
         * Wait for the thread to finish and then update the state.
         */
        rc = RTThreadWait(mThread, 60000, NULL);
        if (rc == VERR_INVALID_HANDLE)
            rc = VINF_SUCCESS;
        if (RT_SUCCESS(rc))
        {
            LogFlowThisFunc(("stopped mThread=%RTthrd\n", mThread));
            mThread = NIL_RTTHREAD;
            mTerminate = false;
        }
        else
            AssertRC(rc);
    }
    else
        LogFlowThisFunc(("not active\n"));

    /* Make sure there is no device from us in the list anymore. */
    m_pUsbProxyService->i_updateDeviceList(this, NULL);

    return rc;
}


/**
 * The service thread created by start().
 *
 * @param   Thread      The thread handle.
 * @param   pvUser      Pointer to the USBProxyBackend instance.
 */
/*static*/ DECLCALLBACK(int) USBProxyBackend::serviceThread(RTTHREAD /* Thread */, void *pvUser)
{
    USBProxyBackend *pThis = (USBProxyBackend *)pvUser;
    LogFlowFunc(("pThis=%p\n", pThis));
    pThis->serviceThreadInit();
    int rc = VINF_SUCCESS;

    /*
     * Processing loop.
     */
    for (;;)
    {
        rc = pThis->wait(RT_INDEFINITE_WAIT);
        if (RT_FAILURE(rc) && rc != VERR_INTERRUPTED && rc != VERR_TIMEOUT)
            break;
        if (pThis->mTerminate)
            break;

        PUSBDEVICE pDevices = pThis->getDevices();
        pThis->m_pUsbProxyService->i_updateDeviceList(pThis, pDevices);
    }

    pThis->serviceThreadTerm();
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/**
 * First call made on the service thread, use it to do
 * thread initialization.
 *
 * The default implementation in USBProxyBackend just a dummy stub.
 */
void USBProxyBackend::serviceThreadInit(void)
{
}


/**
 * Last call made on the service thread, use it to do
 * thread termination.
 */
void USBProxyBackend::serviceThreadTerm(void)
{
}


/**
 * Wait for a change in the USB devices attached to the host.
 *
 * The default implementation in USBProxyBackend just a dummy stub.
 *
 * @returns VBox status code.  VERR_INTERRUPTED and VERR_TIMEOUT are considered
 *          harmless, while all other error status are fatal.
 * @param   aMillies    Number of milliseconds to wait.
 */
int USBProxyBackend::wait(RTMSINTERVAL aMillies)
{
    return RTThreadSleep(RT_MIN(aMillies, 250));
}


/**
 * Interrupt any wait() call in progress.
 *
 * The default implementation in USBProxyBackend just a dummy stub.
 *
 * @returns VBox status code.
 */
int USBProxyBackend::interruptWait(void)
{
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Get a list of USB device currently attached to the host.
 *
 * The default implementation in USBProxyBackend just a dummy stub.
 *
 * @returns Pointer to a list of USB devices.
 *          The list nodes are freed individually by calling freeDevice().
 */
PUSBDEVICE USBProxyBackend::getDevices(void)
{
    return NULL;
}


/**
 * Performs the required actions when a device has been added.
 *
 * This means things like running filters and subsequent capturing and
 * VM attaching. This may result in IPC and temporary lock abandonment.
 *
 * @param   aDevice     The device in question.
 * @param   aUSBDevice  The USB device structure.
 */
void USBProxyBackend::deviceAdded(ComObjPtr<HostUSBDevice> &aDevice,
                                  SessionMachinesList &llOpenedMachines,
                                  PUSBDEVICE aUSBDevice)
{
    /*
     * Validate preconditions.
     */
    AssertReturnVoid(!isWriteLockOnCurrentThread());
    AssertReturnVoid(!aDevice->isWriteLockOnCurrentThread());
    AutoReadLock devLock(aDevice COMMA_LOCKVAL_SRC_POS);
    LogFlowThisFunc(("aDevice=%p name={%s} state=%s id={%RTuuid}\n",
                     (HostUSBDevice *)aDevice,
                     aDevice->i_getName().c_str(),
                     aDevice->i_getStateName(),
                     aDevice->i_getId().raw()));

    /*
     * Run filters on the device.
     */
    if (aDevice->i_isCapturableOrHeld())
    {
        devLock.release();
        HRESULT rc = runAllFiltersOnDevice(aDevice, llOpenedMachines, NULL /* aIgnoreMachine */);
        AssertComRC(rc);
    }

    NOREF(aUSBDevice);
}


/**
 * Remove device notification hook for the OS specific code.
 *
 * This is means things like
 *
 * @param   aDevice     The device in question.
 */
void USBProxyBackend::deviceRemoved(ComObjPtr<HostUSBDevice> &aDevice)
{
    /*
     * Validate preconditions.
     */
    AssertReturnVoid(!isWriteLockOnCurrentThread());
    AssertReturnVoid(!aDevice->isWriteLockOnCurrentThread());
    AutoWriteLock devLock(aDevice COMMA_LOCKVAL_SRC_POS);
    LogFlowThisFunc(("aDevice=%p name={%s} state=%s id={%RTuuid}\n",
                     (HostUSBDevice *)aDevice,
                     aDevice->i_getName().c_str(),
                     aDevice->i_getStateName(),
                     aDevice->i_getId().raw()));

    /*
     * Detach the device from any machine currently using it,
     * reset all data and uninitialize the device object.
     */
    devLock.release();
    aDevice->i_onPhysicalDetached();
}


/**
 * Implement fake capture, ++.
 *
 * @returns true if there is a state change.
 * @param   pDevice     The device in question.
 * @param   pUSBDevice  The USB device structure for the last enumeration.
 * @param   aRunFilters Whether or not to run filters.
 */
bool USBProxyBackend::updateDeviceStateFake(HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice, bool *aRunFilters,
                                            SessionMachine **aIgnoreMachine)
{
    *aRunFilters = false;
    *aIgnoreMachine = NULL;
    AssertReturn(aDevice, false);
    AssertReturn(!aDevice->isWriteLockOnCurrentThread(), false);

    /*
     * Just hand it to the device, it knows best what needs to be done.
     */
    return aDevice->i_updateStateFake(aUSBDevice, aRunFilters, aIgnoreMachine);
}

/**
 * Increments the reference counter.
 *
 * @returns New reference count value.
 */
uint32_t USBProxyBackend::incRef()
{
    Assert(isWriteLockOnCurrentThread());

    return ++m_cRefs;
}

/**
 * Decrements the reference counter.
 *
 * @returns New reference count value.
 */
uint32_t USBProxyBackend::decRef()
{
    Assert(isWriteLockOnCurrentThread());

    return --m_cRefs;
}

/**
 * Updates the device state.
 *
 * This is responsible for calling HostUSBDevice::updateState().
 *
 * @returns true if there is a state change.
 * @param   aDevice         The device in question.
 * @param   aUSBDevice      The USB device structure for the last enumeration.
 * @param   aRunFilters     Whether or not to run filters.
 * @param   aIgnoreMachine  Machine to ignore when running filters.
 */
bool USBProxyBackend::updateDeviceState(HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice, bool *aRunFilters,
                                        SessionMachine **aIgnoreMachine)
{
    AssertReturn(aDevice, false);
    AssertReturn(!aDevice->isWriteLockOnCurrentThread(), false);

    return aDevice->i_updateState(aUSBDevice, aRunFilters, aIgnoreMachine);
}


/**
 * Handle a device which state changed in some significant way.
 *
 * This means things like running filters and subsequent capturing and
 * VM attaching. This may result in IPC and temporary lock abandonment.
 *
 * @param   aDevice         The device.
 * @param   pllOpenedMachines list of running session machines (VirtualBox::getOpenedMachines()); if NULL, we don't run filters
 * @param   aIgnoreMachine  Machine to ignore when running filters.
 */
void USBProxyBackend::deviceChanged(ComObjPtr<HostUSBDevice> &aDevice, SessionMachinesList *pllOpenedMachines,
                                    SessionMachine *aIgnoreMachine)
{
    /*
     * Validate preconditions.
     */
    AssertReturnVoid(!isWriteLockOnCurrentThread());
    AssertReturnVoid(!aDevice->isWriteLockOnCurrentThread());
    AutoReadLock devLock(aDevice COMMA_LOCKVAL_SRC_POS);
    LogFlowThisFunc(("aDevice=%p name={%s} state=%s id={%RTuuid} aRunFilters=%RTbool aIgnoreMachine=%p\n",
                     (HostUSBDevice *)aDevice,
                     aDevice->i_getName().c_str(),
                     aDevice->i_getStateName(),
                     aDevice->i_getId().raw(),
                     (pllOpenedMachines != NULL),       // used to be "bool aRunFilters"
                     aIgnoreMachine));
    devLock.release();

    /*
     * Run filters if requested to do so.
     */
    if (pllOpenedMachines)
    {
        HRESULT rc = runAllFiltersOnDevice(aDevice, *pllOpenedMachines, aIgnoreMachine);
        AssertComRC(rc);
    }
}



/**
 * Free all the members of a USB device returned by getDevice().
 *
 * @param   pDevice     Pointer to the device.
 */
/*static*/ void
USBProxyBackend::freeDeviceMembers(PUSBDEVICE pDevice)
{
    RTStrFree((char *)pDevice->pszManufacturer);
    pDevice->pszManufacturer = NULL;
    RTStrFree((char *)pDevice->pszProduct);
    pDevice->pszProduct = NULL;
    RTStrFree((char *)pDevice->pszSerialNumber);
    pDevice->pszSerialNumber = NULL;

    RTStrFree((char *)pDevice->pszAddress);
    pDevice->pszAddress = NULL;
    RTStrFree((char *)pDevice->pszBackend);
    pDevice->pszBackend = NULL;
#ifdef RT_OS_WINDOWS
    RTStrFree(pDevice->pszAltAddress);
    pDevice->pszAltAddress = NULL;
    RTStrFree(pDevice->pszHubName);
    pDevice->pszHubName = NULL;
#elif defined(RT_OS_SOLARIS)
    RTStrFree(pDevice->pszDevicePath);
    pDevice->pszDevicePath = NULL;
#endif
}


/**
 * Free one USB device returned by getDevice().
 *
 * @param   pDevice     Pointer to the device.
 */
/*static*/ void
USBProxyBackend::freeDevice(PUSBDEVICE pDevice)
{
    freeDeviceMembers(pDevice);
    RTMemFree(pDevice);
}


/**
 * Initializes a filter with the data from the specified device.
 *
 * @param   aFilter     The filter to fill.
 * @param   aDevice     The device to fill it with.
 */
/*static*/ void
USBProxyBackend::initFilterFromDevice(PUSBFILTER aFilter, HostUSBDevice *aDevice)
{
    PCUSBDEVICE pDev = aDevice->i_getUsbData();
    int vrc;

    vrc = USBFilterSetNumExact(aFilter, USBFILTERIDX_VENDOR_ID,         pDev->idVendor,         true); AssertRC(vrc);
    vrc = USBFilterSetNumExact(aFilter, USBFILTERIDX_PRODUCT_ID,        pDev->idProduct,        true); AssertRC(vrc);
    vrc = USBFilterSetNumExact(aFilter, USBFILTERIDX_DEVICE_REV,        pDev->bcdDevice,        true); AssertRC(vrc);
    vrc = USBFilterSetNumExact(aFilter, USBFILTERIDX_DEVICE_CLASS,      pDev->bDeviceClass,     true); AssertRC(vrc);
    vrc = USBFilterSetNumExact(aFilter, USBFILTERIDX_DEVICE_SUB_CLASS,  pDev->bDeviceSubClass,  true); AssertRC(vrc);
    vrc = USBFilterSetNumExact(aFilter, USBFILTERIDX_DEVICE_PROTOCOL,   pDev->bDeviceProtocol,  true); AssertRC(vrc);
    vrc = USBFilterSetNumExact(aFilter, USBFILTERIDX_PORT,              pDev->bPort,            false); AssertRC(vrc);
    vrc = USBFilterSetNumExact(aFilter, USBFILTERIDX_BUS,               pDev->bBus,             false); AssertRC(vrc);
    if (pDev->pszSerialNumber)
    {
        vrc = USBFilterSetStringExact(aFilter, USBFILTERIDX_SERIAL_NUMBER_STR, pDev->pszSerialNumber, true);
        AssertRC(vrc);
    }
    if (pDev->pszProduct)
    {
        vrc = USBFilterSetStringExact(aFilter, USBFILTERIDX_PRODUCT_STR, pDev->pszProduct, true);
        AssertRC(vrc);
    }
    if (pDev->pszManufacturer)
    {
        vrc = USBFilterSetStringExact(aFilter, USBFILTERIDX_MANUFACTURER_STR, pDev->pszManufacturer, true);
        AssertRC(vrc);
    }
}

HRESULT USBProxyBackend::getName(com::Utf8Str &aName)
{
    /* strId is constant during life time, no need to lock */
    aName = m_strId;
    return S_OK;
}

HRESULT USBProxyBackend::getType(com::Utf8Str &aType)
{
    aType = Utf8Str("");
    return S_OK;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
