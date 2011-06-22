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


/**
 * Initialize data members.
 */
USBProxyService::USBProxyService(Host *aHost)
    : mHost(aHost), mThread(NIL_RTTHREAD), mTerminate(false), mLastError(VINF_SUCCESS), mDevices()
{
    LogFlowThisFunc(("aHost=%p\n", aHost));
}


/**
 * Stub needed as long as the class isn't virtual
 */
HRESULT USBProxyService::init(void)
{
    return S_OK;
}


/**
 * Empty destructor.
 */
USBProxyService::~USBProxyService()
{
    LogFlowThisFunc(("\n"));
    Assert(mThread == NIL_RTTHREAD);
    mDevices.clear();
    mTerminate = true;
    mHost = NULL;
}


/**
 * Query if the service is active and working.
 *
 * @returns true if the service is up running.
 * @returns false if the service isn't running.
 */
bool USBProxyService::isActive(void)
{
    return mThread != NIL_RTTHREAD;
}


/**
 * Get last error.
 * Can be used to check why the proxy !isActive() upon construction.
 *
 * @returns VBox status code.
 */
int USBProxyService::getLastError(void)
{
    return mLastError;
}


/**
 * Get last error message.
 * Can be used to check why the proxy !isActive() upon construction as an
 * extension to getLastError().  May return a NULL error.
 *
 * @param
 * @returns VBox status code.
 */
HRESULT USBProxyService::getLastErrorMessage(BSTR *aError)
{
    AssertPtrReturn(aError, E_POINTER);
    mLastErrorMessage.cloneTo(aError);
    return S_OK;
}


/**
 * We're using the Host object lock.
 *
 * This is just a temporary measure until all the USB refactoring is
 * done, probably... For now it help avoiding deadlocks we don't have
 * time to fix.
 *
 * @returns Lock handle.
 */
RWLockHandle *USBProxyService::lockHandle() const
{
    return mHost->lockHandle();
}


/**
 * Gets the collection of USB devices, slave of Host::USBDevices.
 *
 * This is an interface for the HostImpl::USBDevices property getter.
 *
 *
 * @param   aUSBDevices     Where to store the pointer to the collection.
 *
 * @returns COM status code.
 *
 * @remarks The caller must own the write lock of the host object.
 */
HRESULT USBProxyService::getDeviceCollection(ComSafeArrayOut(IHostUSBDevice *, aUSBDevices))
{
    AssertReturn(isWriteLockOnCurrentThread(), E_FAIL);
    CheckComArgOutSafeArrayPointerValid(aUSBDevices);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    SafeIfaceArray<IHostUSBDevice> Collection (mDevices);
    Collection.detachTo(ComSafeArrayOutArg(aUSBDevices));

    return S_OK;
}


/**
 * Request capture of a specific device.
 *
 * This is in an interface for SessionMachine::CaptureUSBDevice(), which is
 * an internal worker used by Console::AttachUSBDevice() from the VM process.
 *
 * When the request is completed, SessionMachine::onUSBDeviceAttach() will
 * be called for the given machine object.
 *
 *
 * @param   aMachine        The machine to attach the device to.
 * @param   aId             The UUID of the USB device to capture and attach.
 *
 * @returns COM status code and error info.
 *
 * @remarks This method may operate synchronously as well as asynchronously. In the
 *          former case it will temporarily abandon locks because of IPC.
 */
HRESULT USBProxyService::captureDeviceForVM(SessionMachine *aMachine, IN_GUID aId)
{
    ComAssertRet(aMachine, E_INVALIDARG);
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /*
     * Translate the device id into a device object.
     */
    ComObjPtr<HostUSBDevice> pHostDevice = findDeviceById(aId);
    if (pHostDevice.isNull())
        return setError(E_INVALIDARG,
                        tr("The USB device with UUID {%RTuuid} is not currently attached to the host"), Guid(aId).raw());

    /*
     * Try to capture the device
     */
    AutoWriteLock DevLock(pHostDevice COMMA_LOCKVAL_SRC_POS);
    return pHostDevice->requestCaptureForVM(aMachine, true /* aSetError */);
}


/**
 * Notification from VM process about USB device detaching progress.
 *
 * This is in an interface for SessionMachine::DetachUSBDevice(), which is
 * an internal worker used by Console::DetachUSBDevice() from the VM process.
 *
 * @param   aMachine        The machine which is sending the notification.
 * @param   aId             The UUID of the USB device is concerns.
 * @param   aDone           \a false for the pre-action notification (necessary
 *                          for advancing the device state to avoid confusing
 *                          the guest).
 *                          \a true for the post-action notification. The device
 *                          will be subjected to all filters except those of
 *                          of \a Machine.
 *
 * @returns COM status code.
 *
 * @remarks When \a aDone is \a true this method may end up doing IPC to other
 *          VMs when running filters. In these cases it will temporarily
 *          abandon its locks.
 */
HRESULT USBProxyService::detachDeviceFromVM(SessionMachine *aMachine, IN_GUID aId, bool aDone)
{
    LogFlowThisFunc(("aMachine=%p{%s} aId={%RTuuid} aDone=%RTbool\n",
                     aMachine,
                     aMachine->getName().c_str(),
                     Guid(aId).raw(),
                     aDone));

    // get a list of all running machines while we're outside the lock
    // (getOpenedMachines requests locks which are incompatible with the lock of the machines list)
    SessionMachinesList llOpenedMachines;
    mHost->parent()->getOpenedMachines(llOpenedMachines);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    ComObjPtr<HostUSBDevice> pHostDevice = findDeviceById(aId);
    ComAssertRet(!pHostDevice.isNull(), E_FAIL);
    AutoWriteLock DevLock(pHostDevice COMMA_LOCKVAL_SRC_POS);

    /*
     * Work the state machine.
     */
    LogFlowThisFunc(("id={%RTuuid} state=%s aDone=%RTbool name={%s}\n",
                     pHostDevice->getId().raw(), pHostDevice->getStateName(), aDone, pHostDevice->getName().c_str()));
    bool fRunFilters = false;
    HRESULT hrc = pHostDevice->onDetachFromVM(aMachine, aDone, &fRunFilters);

    /*
     * Run filters if necessary.
     */
    if (    SUCCEEDED(hrc)
        &&  fRunFilters)
    {
        Assert(aDone && pHostDevice->getUnistate() == kHostUSBDeviceState_HeldByProxy && pHostDevice->getMachine().isNull());
        HRESULT hrc2 = runAllFiltersOnDevice(pHostDevice, llOpenedMachines, aMachine);
        ComAssertComRC(hrc2);
    }
    return hrc;
}


/**
 * Apply filters for the machine to all eligible USB devices.
 *
 * This is in an interface for SessionMachine::CaptureUSBDevice(), which
 * is an internal worker used by Console::AutoCaptureUSBDevices() from the
 * VM process at VM startup.
 *
 * Matching devices will be attached to the VM and may result IPC back
 * to the VM process via SessionMachine::onUSBDeviceAttach() depending
 * on whether the device needs to be captured or not. If capture is
 * required, SessionMachine::onUSBDeviceAttach() will be called
 * asynchronously by the USB proxy service thread.
 *
 * @param   aMachine        The machine to capture devices for.
 *
 * @returns COM status code, perhaps with error info.
 *
 * @remarks Write locks the host object and may temporarily abandon
 *          its locks to perform IPC.
 */
HRESULT USBProxyService::autoCaptureDevicesForVM(SessionMachine *aMachine)
{
    LogFlowThisFunc(("aMachine=%p{%s}\n",
                     aMachine,
                     aMachine->getName().c_str()));
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock mlock(aMachine COMMA_LOCKVAL_SRC_POS);

    /*
     * Make a copy of the list because we might have to exit and
     * re-enter the lock protecting it. (This will not make copies
     * of any HostUSBDevice objects, only reference them.)
     */
    HostUSBDeviceList ListCopy = mDevices;

    for (HostUSBDeviceList::iterator it = ListCopy.begin();
         it != ListCopy.end();
         ++it)
    {
        ComObjPtr<HostUSBDevice> device = *it;
        AutoWriteLock devLock(device COMMA_LOCKVAL_SRC_POS);
        if (   device->getUnistate() == kHostUSBDeviceState_HeldByProxy
            || device->getUnistate() == kHostUSBDeviceState_Unused
            || device->getUnistate() == kHostUSBDeviceState_Capturable)
            runMachineFilters(aMachine, device);
    }

    return S_OK;
}


/**
 * Detach all USB devices currently attached to a VM.
 *
 * This is in an interface for SessionMachine::DetachAllUSBDevices(), which
 * is an internal worker used by Console::powerDown() from the VM process
 * at VM startup, and SessionMachine::uninit() at VM abend.
 *
 * This is, like #detachDeviceFromVM(), normally a two stage journey
 * where \a aDone indicates where we are. In addition we may be called
 * to clean up VMs that have abended, in which case there will be no
 * preparatory call. Filters will be applied to the devices in the final
 * call with the risk that we have to do some IPC when attaching them
 * to other VMs.
 *
 * @param   aMachine        The machine to detach devices from.
 *
 * @returns COM status code, perhaps with error info.
 *
 * @remarks Write locks the host object and may temporarily abandon
 *          its locks to perform IPC.
 */
HRESULT USBProxyService::detachAllDevicesFromVM(SessionMachine *aMachine, bool aDone, bool aAbnormal)
{
    // get a list of all running machines while we're outside the lock
    // (getOpenedMachines requests locks which are incompatible with the lock of the machines list)
    SessionMachinesList llOpenedMachines;
    mHost->parent()->getOpenedMachines(llOpenedMachines);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /*
     * Make a copy of the device list (not the HostUSBDevice objects, just
     * the list) since we may end up performing IPC and temporarily have
     * to abandon locks when applying filters.
     */
    HostUSBDeviceList ListCopy = mDevices;

    for (HostUSBDeviceList::iterator It = ListCopy.begin();
         It != ListCopy.end();
         ++It)
    {
        ComObjPtr<HostUSBDevice> pHostDevice = *It;
        AutoWriteLock devLock(pHostDevice COMMA_LOCKVAL_SRC_POS);
        if (pHostDevice->getMachine() == aMachine)
        {
            /*
             * Same procedure as in detachUSBDevice().
             */
            bool fRunFilters = false;
            HRESULT hrc = pHostDevice->onDetachFromVM(aMachine, aDone, &fRunFilters, aAbnormal);
            if (    SUCCEEDED(hrc)
                &&  fRunFilters)
            {
                Assert(aDone && pHostDevice->getUnistate() == kHostUSBDeviceState_HeldByProxy && pHostDevice->getMachine().isNull());
                HRESULT hrc2 = runAllFiltersOnDevice(pHostDevice, llOpenedMachines, aMachine);
                ComAssertComRC(hrc2);
            }
        }
    }

    return S_OK;
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
 * @note    The caller is expected to own both the device and Host write locks,
 *          and be prepared that these locks may be abandond temporarily.
 */
HRESULT USBProxyService::runAllFiltersOnDevice(ComObjPtr<HostUSBDevice> &aDevice,
                                               SessionMachinesList &llOpenedMachines,
                                               SessionMachine *aIgnoreMachine)
{
    LogFlowThisFunc(("{%s} ignoring=%p\n", aDevice->getName().c_str(), aIgnoreMachine));

    /*
     * Verify preconditions.
     */
    AssertReturn(isWriteLockOnCurrentThread(), E_FAIL);
    AssertReturn(aDevice->isWriteLockOnCurrentThread(), E_FAIL);
    AssertMsgReturn(aDevice->isCapturableOrHeld(), ("{%s} %s\n", aDevice->getName().c_str(), aDevice->getStateName()), E_FAIL);

    /*
     * Get the lists we'll iterate.
     */
    Host::USBDeviceFilterList globalFilters;

    mHost->getUSBFilters(&globalFilters);

    /*
     * Run global filters filerts first.
     */
    bool fHoldIt = false;
    for (Host::USBDeviceFilterList::const_iterator it = globalFilters.begin();
         it != globalFilters.end();
         ++it)
    {
        AutoWriteLock filterLock(*it COMMA_LOCKVAL_SRC_POS);
        const HostUSBDeviceFilter::Data &data = (*it)->getData();
        if (aDevice->isMatch(data))
        {
            USBDeviceFilterAction_T action = USBDeviceFilterAction_Null;
            (*it)->COMGETTER(Action)(&action);
            if (action == USBDeviceFilterAction_Ignore)
            {
                /*
                 * Release the device to the host and we're done.
                 */
                aDevice->requestReleaseToHost();
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
        if (runMachineFilters(pMachine, aDevice))
        {
            LogFlowThisFunc(("{%s} attached to %p\n", aDevice->getName().c_str(), (void *)pMachine));
            return S_OK;
        }
    }

    /*
     * No matching machine, so request hold or release depending
     * on global filter match.
     */
    if (fHoldIt)
        aDevice->requestHold();
    else
        aDevice->requestReleaseToHost();
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
 * @note    Caller must own the USB and device locks for writing.
 * @note    Locks aMachine for reading.
 */
bool USBProxyService::runMachineFilters(SessionMachine *aMachine, ComObjPtr<HostUSBDevice> &aDevice)
{
    LogFlowThisFunc(("{%s} aMachine=%p \n", aDevice->getName().c_str(), aMachine));

    /*
     * Validate preconditions.
     */
    AssertReturn(aMachine, false);
    AssertReturn(isWriteLockOnCurrentThread(), false);
    AssertReturn(aDevice->isWriteLockOnCurrentThread(), false);
    /* Let HostUSBDevice::requestCaptureToVM() validate the state. */

    /*
     * Do the job.
     */
    ULONG ulMaskedIfs;
    if (aMachine->hasMatchingUSBFilter(aDevice, &ulMaskedIfs))
    {
        /* try to capture the device */
        HRESULT hrc = aDevice->requestCaptureForVM(aMachine, false /* aSetError */, ulMaskedIfs);
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
void *USBProxyService::insertFilter(PCUSBFILTER aFilter)
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
void USBProxyService::removeFilter(void *aId)
{
    NOREF(aId);
}


/**
 * A VM is trying to capture a device, do necessary preparations.
 *
 * @returns VBox status code.
 * @param   aDevice     The device in question.
 */
int USBProxyService::captureDevice(HostUSBDevice *aDevice)
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
void USBProxyService::captureDeviceCompleted(HostUSBDevice *aDevice, bool aSuccess)
{
    NOREF(aDevice);
    NOREF(aSuccess);
}


/**
 * The device is going to be detached from a VM.
 *
 * @param   aDevice     The device in question.
 */
void USBProxyService::detachingDevice(HostUSBDevice *aDevice)
{
    NOREF(aDevice);
}


/**
 * A VM is releasing a device back to the host.
 *
 * @returns VBox status code.
 * @param   aDevice     The device in question.
 */
int USBProxyService::releaseDevice(HostUSBDevice *aDevice)
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
void USBProxyService::releaseDeviceCompleted(HostUSBDevice *aDevice, bool aSuccess)
{
    NOREF(aDevice);
    NOREF(aSuccess);
}


// Internals
/////////////////////////////////////////////////////////////////////////////


/**
 * Starts the service.
 *
 * @returns VBox status.
 */
int USBProxyService::start(void)
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
            processChanges();

            /*
             * Create the poller thread which will look for changes.
             */
            mTerminate = false;
            rc = RTThreadCreate(&mThread, USBProxyService::serviceThread, this,
                                0, RTTHREADTYPE_INFREQUENT_POLLER, RTTHREADFLAGS_WAITABLE, "USBPROXY");
            AssertRC(rc);
            if (RT_SUCCESS(rc))
                LogFlowThisFunc(("started mThread=%RTthrd\n", mThread));
            else
                mThread = NIL_RTTHREAD;
        }
        mLastError = rc;
    }
    else
        LogFlowThisFunc(("already running, mThread=%RTthrd\n", mThread));
    return rc;
}


/**
 * Stops the service.
 *
 * @returns VBox status.
 */
int USBProxyService::stop(void)
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
        {
            AssertRC(rc);
            mLastError = rc;
        }
    }
    else
        LogFlowThisFunc(("not active\n"));

    return rc;
}


/**
 * The service thread created by start().
 *
 * @param   Thread      The thread handle.
 * @param   pvUser      Pointer to the USBProxyService instance.
 */
/*static*/ DECLCALLBACK(int) USBProxyService::serviceThread(RTTHREAD /* Thread */, void *pvUser)
{
    USBProxyService *pThis = (USBProxyService *)pvUser;
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
        pThis->processChanges();
    }

    pThis->serviceThreadTerm();
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/**
 * First call made on the service thread, use it to do
 * thread initialization.
 *
 * The default implementation in USBProxyService just a dummy stub.
 */
void USBProxyService::serviceThreadInit(void)
{
}


/**
 * Last call made on the service thread, use it to do
 * thread termination.
 */
void USBProxyService::serviceThreadTerm(void)
{
}


/**
 * Wait for a change in the USB devices attached to the host.
 *
 * The default implementation in USBProxyService just a dummy stub.
 *
 * @returns VBox status code.  VERR_INTERRUPTED and VERR_TIMEOUT are considered
 *          harmless, while all other error status are fatal.
 * @param   aMillies    Number of milliseconds to wait.
 */
int USBProxyService::wait(RTMSINTERVAL aMillies)
{
    return RTThreadSleep(RT_MIN(aMillies, 250));
}


/**
 * Interrupt any wait() call in progress.
 *
 * The default implementation in USBProxyService just a dummy stub.
 *
 * @returns VBox status.
 */
int USBProxyService::interruptWait(void)
{
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Sort a list of USB devices.
 *
 * @returns Pointer to the head of the sorted doubly linked list.
 * @param   aDevices        Head pointer (can be both singly and doubly linked list).
 */
static PUSBDEVICE sortDevices(PUSBDEVICE pDevices)
{
    PUSBDEVICE pHead = NULL;
    PUSBDEVICE pTail = NULL;
    while (pDevices)
    {
        /* unlink head */
        PUSBDEVICE pDev = pDevices;
        pDevices = pDev->pNext;
        if (pDevices)
            pDevices->pPrev = NULL;

        /* find location. */
        PUSBDEVICE pCur = pTail;
        while (     pCur
               &&   HostUSBDevice::compare(pCur, pDev) > 0)
            pCur = pCur->pPrev;

        /* insert (after pCur) */
        pDev->pPrev = pCur;
        if (pCur)
        {
            pDev->pNext = pCur->pNext;
            pCur->pNext = pDev;
            if (pDev->pNext)
                pDev->pNext->pPrev = pDev;
            else
                pTail = pDev;
        }
        else
        {
            pDev->pNext = pHead;
            if (pHead)
                pHead->pPrev = pDev;
            else
                pTail = pDev;
            pHead = pDev;
        }
    }

    return pHead;
}


/**
 * Process any relevant changes in the attached USB devices.
 *
 * Except for the first call, this is always running on the service thread.
 */
void USBProxyService::processChanges(void)
{
    LogFlowThisFunc(("\n"));

    /*
     * Get the sorted list of USB devices.
     */
    PUSBDEVICE pDevices = getDevices();
    pDevices = sortDevices(pDevices);

    // get a list of all running machines while we're outside the lock
    // (getOpenedMachines requests locks which are incompatible with the lock of the machines list)
    SessionMachinesList llOpenedMachines;
    mHost->parent()->getOpenedMachines(llOpenedMachines);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /*
     * Compare previous list with the previous list of devices
     * and merge in any changes while notifying Host.
     */
    HostUSBDeviceList::iterator It = this->mDevices.begin();
    while (    It != mDevices.end()
            || pDevices)
    {
        ComObjPtr<HostUSBDevice> pHostDevice;

        if (It != mDevices.end())
            pHostDevice = *It;

        /*
         * Assert that the object is still alive (we still reference it in
         * the collection and we're the only one who calls uninit() on it.
         */
        AutoCaller devCaller(pHostDevice.isNull() ? NULL : pHostDevice);
        AssertComRC(devCaller.rc());

        /*
         * Lock the device object since we will read/write its
         * properties. All Host callbacks also imply the object is locked.
         */
        AutoWriteLock devLock(pHostDevice.isNull() ? NULL : pHostDevice
                              COMMA_LOCKVAL_SRC_POS);

        /*
         * Compare.
         */
        int iDiff;
        if (pHostDevice.isNull())
            iDiff = 1;
        else
        {
            if (!pDevices)
                iDiff = -1;
            else
                iDiff = pHostDevice->compare(pDevices);
        }
        if (!iDiff)
        {
            /*
             * The device still there, update the state and move on. The PUSBDEVICE
             * structure is eaten by updateDeviceState / HostUSBDevice::updateState().
             */
            PUSBDEVICE pCur = pDevices;
            pDevices = pDevices->pNext;
            pCur->pPrev = pCur->pNext = NULL;

            bool fRunFilters = false;
            SessionMachine *pIgnoreMachine = NULL;
            if (updateDeviceState(pHostDevice, pCur, &fRunFilters, &pIgnoreMachine))
                deviceChanged(pHostDevice,
                              (fRunFilters ? &llOpenedMachines : NULL),
                              pIgnoreMachine);
            It++;
        }
        else
        {
            if (iDiff > 0)
            {
                /*
                 * Head of pDevices was attached.
                 */
                PUSBDEVICE pNew = pDevices;
                pDevices = pDevices->pNext;
                pNew->pPrev = pNew->pNext = NULL;

                ComObjPtr<HostUSBDevice> NewObj;
                NewObj.createObject();
                NewObj->init(pNew, this);
                Log(("USBProxyService::processChanges: attached %p {%s} %s / %p:{.idVendor=%#06x, .idProduct=%#06x, .pszProduct=\"%s\", .pszManufacturer=\"%s\"}\n",
                     (HostUSBDevice *)NewObj,
                     NewObj->getName().c_str(),
                     NewObj->getStateName(),
                     pNew,
                     pNew->idVendor,
                     pNew->idProduct,
                     pNew->pszProduct,
                     pNew->pszManufacturer));

                mDevices.insert(It, NewObj);

                /* Not really necessary to lock here, but make Assert checks happy. */
                AutoWriteLock newDevLock(NewObj COMMA_LOCKVAL_SRC_POS);
                deviceAdded(NewObj, llOpenedMachines, pNew);
            }
            else
            {
                /*
                 * Check if the device was actually detached or logically detached
                 * as the result of a re-enumeration.
                 */
                if (!pHostDevice->wasActuallyDetached())
                    It++;
                else
                {
                    It = mDevices.erase(It);
                    deviceRemoved(pHostDevice);
                    Log(("USBProxyService::processChanges: detached %p {%s}\n",
                         (HostUSBDevice *)pHostDevice,
                         pHostDevice->getName().c_str()));

                    /* from now on, the object is no more valid,
                     * uninitialize to avoid abuse */
                    devCaller.release();
                    pHostDevice->uninit();
                }
            }
        }
    } /* while */

    LogFlowThisFunc(("returns void\n"));
}


/**
 * Get a list of USB device currently attached to the host.
 *
 * The default implementation in USBProxyService just a dummy stub.
 *
 * @returns Pointer to a list of USB devices.
 *          The list nodes are freed individually by calling freeDevice().
 */
PUSBDEVICE USBProxyService::getDevices(void)
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
void USBProxyService::deviceAdded(ComObjPtr<HostUSBDevice> &aDevice,
                                  SessionMachinesList &llOpenedMachines,
                                  PUSBDEVICE aUSBDevice)
{
    /*
     * Validate preconditions.
     */
    AssertReturnVoid(isWriteLockOnCurrentThread());
    AssertReturnVoid(aDevice->isWriteLockOnCurrentThread());
    LogFlowThisFunc(("aDevice=%p name={%s} state=%s id={%RTuuid}\n",
                     (HostUSBDevice *)aDevice,
                     aDevice->getName().c_str(),
                     aDevice->getStateName(),
                     aDevice->getId().raw()));

    /*
     * Run filters on the device.
     */
    if (aDevice->isCapturableOrHeld())
    {
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
void USBProxyService::deviceRemoved(ComObjPtr<HostUSBDevice> &aDevice)
{
    /*
     * Validate preconditions.
     */
    AssertReturnVoid(isWriteLockOnCurrentThread());
    AssertReturnVoid(aDevice->isWriteLockOnCurrentThread());
    LogFlowThisFunc(("aDevice=%p name={%s} state=%s id={%RTuuid}\n",
                     (HostUSBDevice *)aDevice,
                     aDevice->getName().c_str(),
                     aDevice->getStateName(),
                     aDevice->getId().raw()));

    /*
     * Detach the device from any machine currently using it,
     * reset all data and uninitialize the device object.
     */
    aDevice->onPhysicalDetached();
}


/**
 * Implement fake capture, ++.
 *
 * @returns true if there is a state change.
 * @param   pDevice     The device in question.
 * @param   pUSBDevice  The USB device structure for the last enumeration.
 * @param   aRunFilters Whether or not to run filters.
 */
bool USBProxyService::updateDeviceStateFake(HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice, bool *aRunFilters, SessionMachine **aIgnoreMachine)
{
    *aRunFilters = false;
    *aIgnoreMachine = NULL;
    AssertReturn(aDevice, false);
    AssertReturn(aDevice->isWriteLockOnCurrentThread(), false);

    /*
     * Just hand it to the device, it knows best what needs to be done.
     */
    return aDevice->updateStateFake(aUSBDevice, aRunFilters, aIgnoreMachine);
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
bool USBProxyService::updateDeviceState(HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice, bool *aRunFilters, SessionMachine **aIgnoreMachine)
{
    AssertReturn(aDevice, false);
    AssertReturn(aDevice->isWriteLockOnCurrentThread(), false);

    return aDevice->updateState(aUSBDevice, aRunFilters, aIgnoreMachine);
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
void USBProxyService::deviceChanged(ComObjPtr<HostUSBDevice> &aDevice, SessionMachinesList *pllOpenedMachines, SessionMachine *aIgnoreMachine)
{
    /*
     * Validate preconditions.
     */
    AssertReturnVoid(isWriteLockOnCurrentThread());
    AssertReturnVoid(aDevice->isWriteLockOnCurrentThread());
    LogFlowThisFunc(("aDevice=%p name={%s} state=%s id={%RTuuid} aRunFilters=%RTbool aIgnoreMachine=%p\n",
                     (HostUSBDevice *)aDevice,
                     aDevice->getName().c_str(),
                     aDevice->getStateName(),
                     aDevice->getId().raw(),
                     (pllOpenedMachines != NULL),       // used to be "bool aRunFilters"
                     aIgnoreMachine));

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
USBProxyService::freeDeviceMembers(PUSBDEVICE pDevice)
{
    RTStrFree((char *)pDevice->pszManufacturer);
    pDevice->pszManufacturer = NULL;
    RTStrFree((char *)pDevice->pszProduct);
    pDevice->pszProduct = NULL;
    RTStrFree((char *)pDevice->pszSerialNumber);
    pDevice->pszSerialNumber = NULL;

    RTStrFree((char *)pDevice->pszAddress);
    pDevice->pszAddress = NULL;
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
USBProxyService::freeDevice(PUSBDEVICE pDevice)
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
USBProxyService::initFilterFromDevice(PUSBFILTER aFilter, HostUSBDevice *aDevice)
{
    PCUSBDEVICE pDev = aDevice->mUsb;
    int vrc;

    vrc = USBFilterSetNumExact(aFilter, USBFILTERIDX_VENDOR_ID,         pDev->idVendor,         true); AssertRC(vrc);
    vrc = USBFilterSetNumExact(aFilter, USBFILTERIDX_PRODUCT_ID,        pDev->idProduct,        true); AssertRC(vrc);
    vrc = USBFilterSetNumExact(aFilter, USBFILTERIDX_DEVICE_REV,        pDev->bcdDevice,        true); AssertRC(vrc);
    vrc = USBFilterSetNumExact(aFilter, USBFILTERIDX_DEVICE_CLASS,      pDev->bDeviceClass,     true); AssertRC(vrc);
    vrc = USBFilterSetNumExact(aFilter, USBFILTERIDX_DEVICE_SUB_CLASS,  pDev->bDeviceSubClass,  true); AssertRC(vrc);
    vrc = USBFilterSetNumExact(aFilter, USBFILTERIDX_DEVICE_PROTOCOL,   pDev->bDeviceProtocol,  true); AssertRC(vrc);
    vrc = USBFilterSetNumExact(aFilter, USBFILTERIDX_PORT,              pDev->bPort,            true); AssertRC(vrc);
    vrc = USBFilterSetNumExact(aFilter, USBFILTERIDX_BUS,               pDev->bBus,             true); AssertRC(vrc);
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


/**
 * Searches the list of devices (mDevices) for the given device.
 *
 *
 * @returns Smart pointer to the device on success, NULL otherwise.
 * @param   aId             The UUID of the device we're looking for.
 */
ComObjPtr<HostUSBDevice> USBProxyService::findDeviceById(IN_GUID aId)
{
    Guid Id(aId);
    ComObjPtr<HostUSBDevice> Dev;
    for (HostUSBDeviceList::iterator It = mDevices.begin();
         It != mDevices.end();
         ++It)
        if ((*It)->getId() == Id)
        {
            Dev = (*It);
            break;
        }

    return Dev;
}

/*static*/
HRESULT USBProxyService::setError(HRESULT aResultCode, const char *aText, ...)
{
    va_list va;
    va_start(va, aText);
    HRESULT rc = VirtualBoxBase::setErrorInternal(aResultCode,
                                                    COM_IIDOF(IHost),
                                                    "USBProxyService",
                                                    Utf8StrFmt(aText, va),
                                                    false /* aWarning*/,
                                                    true /* aLogIt*/);
    va_end(va);
    return rc;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
