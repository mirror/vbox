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

/** Pair of a USB proxy backend and the opaque filter data assigned by the backend. */
typedef std::pair<USBProxyBackend *, void *> USBFilterPair;
/** List of USB filter pairs. */
typedef std::list<USBFilterPair> USBFilterList;

/**
 * Data for a USB device filter.
 */
struct USBFilterData
{
    USBFilterData()
        : llUsbFilters()
    { }

    USBFilterList llUsbFilters;
};

/**
 * Initialize data members.
 */
USBProxyService::USBProxyService(Host *aHost)
    : mHost(aHost), mDevices(), mBackends()
{
    LogFlowThisFunc(("aHost=%p\n", aHost));
}


/**
 * Stub needed as long as the class isn't virtual
 */
HRESULT USBProxyService::init(void)
{
    USBProxyBackend *pUsbProxyBackendHost;
# if defined(RT_OS_DARWIN)
    pUsbProxyBackendHost = new USBProxyBackendDarwin(this, Utf8Str("host"));
# elif defined(RT_OS_LINUX)
    pUsbProxyBackendHost = new USBProxyBackendLinux(this, Utf8Str("host"));
# elif defined(RT_OS_OS2)
    pUsbProxyBackendHost = new USBProxyBackendOs2(this, Utf8Str("host"));
# elif defined(RT_OS_SOLARIS)
    pUsbProxyBackendHost = new USBProxyBackendSolaris(this, Utf8Str("host"));
# elif defined(RT_OS_WINDOWS)
    pUsbProxyBackendHost = new USBProxyBackendWindows(this, Utf8Str("host"));
# elif defined(RT_OS_FREEBSD)
    pUsbProxyBackendHost = new USBProxyBackendFreeBSD(this, Utf8Str("host"));
# else
    pUsbProxyBackendHost = new USBProxyBackend(this, Utf8Str("host"));
# endif
    int vrc = pUsbProxyBackendHost->init(Utf8Str(""));
    if (RT_FAILURE(vrc))
    {
        delete pUsbProxyBackendHost;
        mLastError = vrc;
    }
    else
        mBackends.push_back(pUsbProxyBackendHost);

#if 0 /** @todo: Pass in the config. */
    pUsbProxyBackendHost = new USBProxyBackendUsbIp(this);
    hrc = pUsbProxyBackendHost->init();
    if (FAILED(hrc))
    {
        delete pUsbProxyBackendHost;
        return hrc;
    }
#endif

    return S_OK;
}


/**
 * Empty destructor.
 */
USBProxyService::~USBProxyService()
{
    LogFlowThisFunc(("\n"));
    while (!mBackends.empty())
    {
        USBProxyBackend *pUsbProxyBackend = mBackends.front();
        mBackends.pop_front();
        delete pUsbProxyBackend;
    }

    mDevices.clear();
    mBackends.clear();
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
    return mBackends.size() > 0;
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


void *USBProxyService::insertFilter(PCUSBFILTER aFilter)
{
    USBFilterData *pFilterData = new USBFilterData();

    for (USBProxyBackendList::iterator it = mBackends.begin();
         it != mBackends.end();
         ++it)
    {
        USBProxyBackend *pUsbProxyBackend = *it;
        void *pvId = pUsbProxyBackend->insertFilter(aFilter);

        pFilterData->llUsbFilters.push_back(USBFilterPair(pUsbProxyBackend, pvId));
    }

    return pFilterData;
}

void USBProxyService::removeFilter(void *aId)
{
    USBFilterData *pFilterData = (USBFilterData *)aId;

    for (USBFilterList::iterator it = pFilterData->llUsbFilters.begin();
         it != pFilterData->llUsbFilters.end();
         ++it)
    {
        USBProxyBackend *pUsbProxyBackend = it->first;
        pUsbProxyBackend->removeFilter(it->second);
    }

    pFilterData->llUsbFilters.clear();
    delete pFilterData;
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
HRESULT USBProxyService::getDeviceCollection(std::vector<ComPtr<IHostUSBDevice> > &aUSBDevices)
{
    AssertReturn(isWriteLockOnCurrentThread(), E_FAIL);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    aUSBDevices.resize(mDevices.size());
    size_t i = 0;
    for (HostUSBDeviceList::const_iterator it = mDevices.begin(); it != mDevices.end(); ++it, ++i)
        aUSBDevices[i] = *it;

    return S_OK;
}


HRESULT USBProxyService::addUSBDeviceSource(const com::Utf8Str &aBackend, const com::Utf8Str &aId, const com::Utf8Str &aAddress,
                                            const std::vector<com::Utf8Str> &aPropertyNames, const std::vector<com::Utf8Str> &aPropertyValues)
{
    HRESULT hrc = S_OK;

    /* Check whether the ID is used first. */
    for (USBProxyBackendList::iterator it = mBackends.begin();
         it != mBackends.end();
         ++it)
    {
        USBProxyBackend *pUsbProxyBackend = *it;

        if (aId.equals(pUsbProxyBackend->i_getId()))
            return setError(VBOX_E_OBJECT_IN_USE,
                            tr("The USB device source \"%s\" exists already"), aId.c_str());
    }

    /* Create appropriate proxy backend. */
    if (aBackend.equalsIgnoreCase("USBIP"))
    {
        USBProxyBackend *pUsbProxyBackend = new USBProxyBackendUsbIp(this, aId);

        int vrc = pUsbProxyBackend->init(aAddress);
        if (RT_FAILURE(vrc))
        {
            delete pUsbProxyBackend;
            mLastError = vrc;
        }
        else
            mBackends.push_back(pUsbProxyBackend);
    }
    else
        hrc = setError(VBOX_E_OBJECT_NOT_FOUND,
                       tr("The USB backend \"%s\" is not supported"), aBackend.c_str());

    return hrc;
}

HRESULT USBProxyService::removeUSBDeviceSource(const com::Utf8Str &aId)
{
    for (USBProxyBackendList::iterator it = mBackends.begin();
         it != mBackends.end();
         ++it)
    {
        USBProxyBackend *pUsbProxyBackend = *it;

        if (aId.equals(pUsbProxyBackend->i_getId()))
        {
            mBackends.erase(it);
            delete pUsbProxyBackend;
            return S_OK;
        }
    }

    return setError(VBOX_E_OBJECT_NOT_FOUND,
                    tr("The USB device source \"%s\" could not be found"), aId.c_str());
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
HRESULT USBProxyService::captureDeviceForVM(SessionMachine *aMachine, IN_GUID aId, const com::Utf8Str &aCaptureFilename)
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
    alock.release();
    return pHostDevice->i_requestCaptureForVM(aMachine, true /* aSetError */, aCaptureFilename);
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
                     aMachine->i_getName().c_str(),
                     Guid(aId).raw(),
                     aDone));

    // get a list of all running machines while we're outside the lock
    // (getOpenedMachines requests locks which are incompatible with the lock of the machines list)
    SessionMachinesList llOpenedMachines;
    mHost->i_parent()->i_getOpenedMachines(llOpenedMachines);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    ComObjPtr<HostUSBDevice> pHostDevice = findDeviceById(aId);
    ComAssertRet(!pHostDevice.isNull(), E_FAIL);
    AutoWriteLock devLock(pHostDevice COMMA_LOCKVAL_SRC_POS);

    /*
     * Work the state machine.
     */
    LogFlowThisFunc(("id={%RTuuid} state=%s aDone=%RTbool name={%s}\n",
                     pHostDevice->i_getId().raw(), pHostDevice->i_getStateName(), aDone, pHostDevice->i_getName().c_str()));
    bool fRunFilters = false;
    HRESULT hrc = pHostDevice->i_onDetachFromVM(aMachine, aDone, &fRunFilters);

    /*
     * Run filters if necessary.
     */
    if (    SUCCEEDED(hrc)
        &&  fRunFilters)
    {
        USBProxyBackend *pUsbProxyBackend = pHostDevice->i_getUsbProxyBackend();
        Assert(aDone && pHostDevice->i_getUnistate() == kHostUSBDeviceState_HeldByProxy && pHostDevice->i_getMachine().isNull());
        devLock.release();
        alock.release();
        HRESULT hrc2 = pUsbProxyBackend->runAllFiltersOnDevice(pHostDevice, llOpenedMachines, aMachine);
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
 * @remarks Temporarily locks this object, the machine object and some USB
 *          device, and the called methods will lock similar objects.
 */
HRESULT USBProxyService::autoCaptureDevicesForVM(SessionMachine *aMachine)
{
    LogFlowThisFunc(("aMachine=%p{%s}\n",
                     aMachine,
                     aMachine->i_getName().c_str()));

    /*
     * Make a copy of the list because we cannot hold the lock protecting it.
     * (This will not make copies of any HostUSBDevice objects, only reference them.)
     */
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    HostUSBDeviceList ListCopy = mDevices;
    alock.release();

    for (HostUSBDeviceList::iterator it = ListCopy.begin();
         it != ListCopy.end();
         ++it)
    {
        ComObjPtr<HostUSBDevice> pHostDevice = *it;
        AutoReadLock devLock(pHostDevice COMMA_LOCKVAL_SRC_POS);
        if (   pHostDevice->i_getUnistate() == kHostUSBDeviceState_HeldByProxy
            || pHostDevice->i_getUnistate() == kHostUSBDeviceState_Unused
            || pHostDevice->i_getUnistate() == kHostUSBDeviceState_Capturable)
        {
            USBProxyBackend *pUsbProxyBackend = pHostDevice->i_getUsbProxyBackend();
            devLock.release();
            pUsbProxyBackend->runMachineFilters(aMachine, pHostDevice);
        }
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
    // (getOpenedMachines requests locks which are incompatible with the host object lock)
    SessionMachinesList llOpenedMachines;
    mHost->i_parent()->i_getOpenedMachines(llOpenedMachines);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /*
     * Make a copy of the device list (not the HostUSBDevice objects, just
     * the list) since we may end up performing IPC and temporarily have
     * to abandon locks when applying filters.
     */
    HostUSBDeviceList ListCopy = mDevices;

    for (HostUSBDeviceList::iterator it = ListCopy.begin();
         it != ListCopy.end();
         ++it)
    {
        ComObjPtr<HostUSBDevice> pHostDevice = *it;
        AutoWriteLock devLock(pHostDevice COMMA_LOCKVAL_SRC_POS);
        if (pHostDevice->i_getMachine() == aMachine)
        {
            /*
             * Same procedure as in detachUSBDevice().
             */
            bool fRunFilters = false;
            HRESULT hrc = pHostDevice->i_onDetachFromVM(aMachine, aDone, &fRunFilters, aAbnormal);
            if (    SUCCEEDED(hrc)
                &&  fRunFilters)
            {
                USBProxyBackend *pUsbProxyBackend = pHostDevice->i_getUsbProxyBackend();
                Assert(   aDone
                       && pHostDevice->i_getUnistate() == kHostUSBDeviceState_HeldByProxy
                       && pHostDevice->i_getMachine().isNull());
                devLock.release();
                alock.release();
                HRESULT hrc2 = pUsbProxyBackend->runAllFiltersOnDevice(pHostDevice, llOpenedMachines, aMachine);
                ComAssertComRC(hrc2);
                alock.acquire();
            }
        }
    }

    return S_OK;
}


// Internals
/////////////////////////////////////////////////////////////////////////////


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
               &&   HostUSBDevice::i_compare(pCur, pDev) > 0)
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

    LogFlowFuncLeave();
    return pHead;
}


/**
 * Process any relevant changes in the attached USB devices.
 *
 * This is called from any available USB proxy backends service thread when they discover
 * a change.
 */
void USBProxyService::i_updateDeviceList(USBProxyBackend *pUsbProxyBackend, PUSBDEVICE pDevices)
{
    LogFlowThisFunc(("\n"));

    pDevices = sortDevices(pDevices);

    // get a list of all running machines while we're outside the lock
    // (getOpenedMachines requests higher priority locks)
    SessionMachinesList llOpenedMachines;
    mHost->i_parent()->i_getOpenedMachines(llOpenedMachines);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /*
     * Compare previous list with the new list of devices
     * and merge in any changes while notifying Host.
     */
    HostUSBDeviceList::iterator it = this->mDevices.begin();
    while (    it != mDevices.end()
            || pDevices)
    {
        ComObjPtr<HostUSBDevice> pHostDevice;

        if (it != mDevices.end())
            pHostDevice = *it;

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

        /* Skip all devices not belonging to the same backend. */
        if (   !pHostDevice.isNull()
            && pHostDevice->i_getUsbProxyBackend() != pUsbProxyBackend)
        {
            ++it;
            continue;
        }

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
                iDiff = pHostDevice->i_compare(pDevices);
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
            devLock.release();
            alock.release();
            if (pUsbProxyBackend->updateDeviceState(pHostDevice, pCur, &fRunFilters, &pIgnoreMachine))
                pUsbProxyBackend->deviceChanged(pHostDevice,
                                                (fRunFilters ? &llOpenedMachines : NULL),
                                                pIgnoreMachine);
            alock.acquire();
            ++it;
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
                NewObj->init(pNew, pUsbProxyBackend);
                Log(("USBProxyService::processChanges: attached %p {%s} %s / %p:{.idVendor=%#06x, .idProduct=%#06x, .pszProduct=\"%s\", .pszManufacturer=\"%s\"}\n",
                     (HostUSBDevice *)NewObj,
                     NewObj->i_getName().c_str(),
                     NewObj->i_getStateName(),
                     pNew,
                     pNew->idVendor,
                     pNew->idProduct,
                     pNew->pszProduct,
                     pNew->pszManufacturer));

                mDevices.insert(it, NewObj);

                devLock.release();
                alock.release();
                pUsbProxyBackend->deviceAdded(NewObj, llOpenedMachines, pNew);
                alock.acquire();
            }
            else
            {
                /*
                 * Check if the device was actually detached or logically detached
                 * as the result of a re-enumeration.
                 */
                if (!pHostDevice->i_wasActuallyDetached())
                    ++it;
                else
                {
                    it = mDevices.erase(it);
                    devLock.release();
                    alock.release();
                    pUsbProxyBackend->deviceRemoved(pHostDevice);
                    Log(("USBProxyService::processChanges: detached %p {%s}\n",
                         (HostUSBDevice *)pHostDevice,
                         pHostDevice->i_getName().c_str()));

                    /* from now on, the object is no more valid,
                     * uninitialize to avoid abuse */
                    devCaller.release();
                    pHostDevice->uninit();
                    alock.acquire();
                }
            }
        }
    } /* while */

    LogFlowThisFunc(("returns void\n"));
}


/**
 * Returns the global USB filter list stored in the Host object.
 *
 * @returns nothing.
 * @param   pGlobalFilters    Where to store the global filter list on success.
 */
void USBProxyService::i_getUSBFilters(USBDeviceFilterList *pGlobalFilters)
{
    mHost->i_getUSBFilters(pGlobalFilters);
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
    for (HostUSBDeviceList::iterator it = mDevices.begin();
         it != mDevices.end();
         ++it)
        if ((*it)->i_getId() == Id)
        {
            Dev = (*it);
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
