/** @file
 *
 * Implementation of IUSBController.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */



#include "USBControllerImpl.h"
#include "MachineImpl.h"
#include "VirtualBoxImpl.h"
#include "HostImpl.h"
#include "USBDeviceImpl.h"
#include "HostUSBDeviceImpl.h"
#include "Logging.h"

#include "USBProxyService.h"

#include <iprt/string.h>
#include <iprt/cpputils.h>
#include <VBox/err.h>

#include <algorithm>

// defines
/////////////////////////////////////////////////////////////////////////////

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (USBController)

HRESULT USBController::FinalConstruct()
{
    return S_OK;
}

void USBController::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the USB controller object.
 *
 * @returns COM result indicator.
 * @param aParent       Pointer to our parent object.
 */
HRESULT USBController::init (Machine *aParent)
{
    LogFlowThisFunc (("aParent=%p\n", aParent));

    ComAssertRet (aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;
    /* mPeer is left null */

    mData.allocate();
    mDeviceFilters.allocate();

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}


/**
 * Initializes the USB controller object given another USB controller object
 * (a kind of copy constructor). This object shares data with
 * the object passed as an argument.
 *
 * @returns COM result indicator.
 * @param aParent       Pointer to our parent object.
 * @param aPeer         The object to share.
 *
 * @note This object must be destroyed before the original object
 * it shares data with is destroyed.
 */
HRESULT USBController::init (Machine *aParent, USBController *aPeer)
{
    LogFlowThisFunc (("aParent=%p, aPeer=%p\n", aParent, aPeer));

    ComAssertRet (aParent && aPeer, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;
    unconst (mPeer) = aPeer;

    AutoLock thatlock (aPeer);
    mData.share (aPeer->mData);

    /* create copies of all filters */
    mDeviceFilters.allocate();
    DeviceFilterList::const_iterator it = aPeer->mDeviceFilters->begin();
    while (it != aPeer->mDeviceFilters->end())
    {
        ComObjPtr <USBDeviceFilter> filter;
        filter.createObject();
        filter->init (this, *it);
        mDeviceFilters->push_back (filter);
        ++ it;
    }

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}


/**
 *  Initializes the USB controller object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 */
HRESULT USBController::initCopy (Machine *aParent, USBController *aPeer)
{
    LogFlowThisFunc (("aParent=%p, aPeer=%p\n", aParent, aPeer));

    ComAssertRet (aParent && aPeer, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;
    /* mPeer is left null */

    AutoLock thatlock (aPeer);
    mData.attachCopy (aPeer->mData);

    /* create private copies of all filters */
    mDeviceFilters.allocate();
    DeviceFilterList::const_iterator it = aPeer->mDeviceFilters->begin();
    while (it != aPeer->mDeviceFilters->end())
    {
        ComObjPtr <USBDeviceFilter> filter;
        filter.createObject();
        filter->initCopy (this, *it);
        mDeviceFilters->push_back (filter);
        ++ it;
    }

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}


/**
 * Uninitializes the instance and sets the ready flag to FALSE.
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void USBController::uninit()
{
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    /* uninit all filters (including those still referenced by clients) */
    uninitDependentChildren();

    mDeviceFilters.free();
    mData.free();

    unconst (mPeer).setNull();
    unconst (mParent).setNull();
}


// IUSBController properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP USBController::COMGETTER(Enabled) (BOOL *aEnabled)
{
    if (!aEnabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aEnabled = mData->mEnabled;

    return S_OK;
}


STDMETHODIMP USBController::COMSETTER(Enabled) (BOOL aEnabled)
{
    LogFlowThisFunc (("aEnabled=%RTbool\n", aEnabled));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mEnabled != aEnabled)
    {
        mData.backup();
        mData->mEnabled = aEnabled;

        /* leave the lock for safety */
        alock.leave();

        mParent->onUSBControllerChange();
    }

    return S_OK;
}

STDMETHODIMP USBController::COMGETTER(USBStandard) (USHORT *aUSBStandard)
{
    if (!aUSBStandard)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* not accessing data -- no need to lock */

    *aUSBStandard = 0x0101;

    return S_OK;
}

STDMETHODIMP USBController::COMGETTER(DeviceFilters) (IUSBDeviceFilterCollection **aDevicesFilters)
{
    if (!aDevicesFilters)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    ComObjPtr <USBDeviceFilterCollection> collection;
    collection.createObject();
    collection->init (*mDeviceFilters.data());
    collection.queryInterfaceTo (aDevicesFilters);

    return S_OK;
}

// IUSBController methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP USBController::CreateDeviceFilter (INPTR BSTR aName,
                                                IUSBDeviceFilter **aFilter)
{
    if (!aFilter)
        return E_POINTER;

    if (!aName || *aName == 0)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    ComObjPtr <USBDeviceFilter> filter;
    filter.createObject();
    HRESULT rc = filter->init (this, aName);
    ComAssertComRCRetRC (rc);
    rc = filter.queryInterfaceTo (aFilter);
    AssertComRCReturnRC (rc);

    return S_OK;
}

STDMETHODIMP USBController::InsertDeviceFilter (ULONG aPosition,
                                                IUSBDeviceFilter *aFilter)
{
    if (!aFilter)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    ComObjPtr <USBDeviceFilter> filter = getDependentChild (aFilter);
    if (!filter)
        return setError (E_INVALIDARG,
            tr ("The given USB device filter is not created within "
                "this VirtualBox instance"));

    if (filter->mInList)
        return setError (E_INVALIDARG,
            tr ("The given USB device filter is already in the list"));

    /* backup the list before modification */
    mDeviceFilters.backup();

    /* iterate to the position... */
    DeviceFilterList::iterator it;
    if (aPosition < mDeviceFilters->size())
    {
        it = mDeviceFilters->begin();
        std::advance (it, aPosition);
    }
    else
        it = mDeviceFilters->end();
    /* ...and insert */
    mDeviceFilters->insert (it, filter);
    filter->mInList = true;

    /// @todo After rewriting Win32 USB support, no more necessary;
    //  a candidate for removal.
#if 0
    /* notify the proxy (only when the filter is active) */
    if (filter->data().mActive)
#else
    /* notify the proxy (only when it makes sense) */
    if (filter->data().mActive && adep.machineState() >= MachineState_Running)
#endif
    {
        USBProxyService *service = mParent->virtualBox()->host()->usbProxyService();
        ComAssertRet (service, E_FAIL);

        ComAssertRet (filter->id() == NULL, E_FAIL);
#ifndef VBOX_WITH_USBFILTER
        filter->id() = service->insertFilter (ComPtr <IUSBDeviceFilter> (aFilter));
#else
        filter->id() = service->insertFilter (&filter->data().mUSBFilter);
#endif
    }

    return S_OK;
}

STDMETHODIMP USBController::RemoveDeviceFilter (ULONG aPosition,
                                                IUSBDeviceFilter **aFilter)
{
    if (!aFilter)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (!mDeviceFilters->size())
        return setError (E_INVALIDARG,
            tr ("The USB device filter list is empty"));

    if (aPosition >= mDeviceFilters->size())
        return setError (E_INVALIDARG,
            tr ("Invalid position: %lu (must be in range [0, %lu])"),
            aPosition, mDeviceFilters->size() - 1);

    /* backup the list before modification */
    mDeviceFilters.backup();

    ComObjPtr <USBDeviceFilter> filter;
    {
        /* iterate to the position... */
        DeviceFilterList::iterator it = mDeviceFilters->begin();
        std::advance (it, aPosition);
        /* ...get an element from there... */
        filter = *it;
        /* ...and remove */
        filter->mInList = false;
        mDeviceFilters->erase (it);
    }

    /* cancel sharing (make an independent copy of data) */
    filter->unshare();

    filter.queryInterfaceTo (aFilter);

    /// @todo After rewriting Win32 USB support, no more necessary;
    //  a candidate for removal.
#if 0
    /* notify the proxy (only when the filter is active) */
    if (filter->data().mActive)
#else
    /* notify the proxy (only when it makes sense) */
    if (filter->data().mActive && adep.machineState() >= MachineState_Running)
#endif
    {
        USBProxyService *service = mParent->virtualBox()->host()->usbProxyService();
        ComAssertRet (service, E_FAIL);

        ComAssertRet (filter->id() != NULL, E_FAIL);
        service->removeFilter (filter->id());
        filter->id() = NULL;
    }

    return S_OK;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 *  Loads settings from the configuration node.
 *
 *  @note Locks objects for writing!
 */
HRESULT USBController::loadSettings (CFGNODE aMachine)
{
    AssertReturn (aMachine, E_FAIL);

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    CFGNODE controller = NULL;
    CFGLDRGetChildNode (aMachine, "USBController", 0, &controller);
    Assert (controller);

    /* enabled */
    bool enabled;
    CFGLDRQueryBool (controller, "enabled", &enabled);
    mData->mEnabled = enabled;

    HRESULT rc = S_OK;

    unsigned filterCount = 0;
    CFGLDRCountChildren (controller, "DeviceFilter", &filterCount);
    for (unsigned i = 0; i < filterCount && SUCCEEDED (rc); i++)
    {
        CFGNODE filter = NULL;
        CFGLDRGetChildNode (controller, "DeviceFilter", i, &filter);
        Assert (filter);

        Bstr name;
        CFGLDRQueryBSTR (filter, "name", name.asOutParam());
        bool active;
        CFGLDRQueryBool (filter, "active", &active);

        Bstr vendorId;
        CFGLDRQueryBSTR (filter, "vendorid", vendorId.asOutParam());
        Bstr productId;
        CFGLDRQueryBSTR (filter, "productid", productId.asOutParam());
        Bstr revision;
        CFGLDRQueryBSTR (filter, "revision", revision.asOutParam());
        Bstr manufacturer;
        CFGLDRQueryBSTR (filter, "manufacturer", manufacturer.asOutParam());
        Bstr product;
        CFGLDRQueryBSTR (filter, "product", product.asOutParam());
        Bstr serialNumber;
        CFGLDRQueryBSTR (filter, "serialnumber", serialNumber.asOutParam());
        Bstr port;
        CFGLDRQueryBSTR (filter, "port", port.asOutParam());
        Bstr remote;
        CFGLDRQueryBSTR (filter, "remote", remote.asOutParam());

        ComObjPtr <USBDeviceFilter> filterObj;
        filterObj.createObject();
        rc = filterObj->init (this,
                              name, active, vendorId, productId, revision,
                              manufacturer, product, serialNumber,
                              port, remote);
        /* error info is set by init() when appropriate */
        if (SUCCEEDED (rc))
        {
            mDeviceFilters->push_back (filterObj);
            filterObj->mInList = true;
        }

        CFGLDRReleaseNode (filter);
    }

    CFGLDRReleaseNode (controller);

    return rc;
}

/**
 *  Saves settings to the configuration node.
 *
 *  @note Locks objects for reading!
 */
HRESULT USBController::saveSettings (CFGNODE aMachine)
{
    AssertReturn (aMachine, E_FAIL);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    /* first, delete the entry */
    CFGNODE controller = NULL;
    int vrc = CFGLDRGetChildNode (aMachine, "USBController", 0, &controller);
    if (VBOX_SUCCESS (vrc))
    {
        vrc = CFGLDRDeleteNode (controller);
        ComAssertRCRet (vrc, E_FAIL);
    }
    /* then, recreate it */
    vrc = CFGLDRCreateChildNode (aMachine, "USBController", &controller);
    ComAssertRCRet (vrc, E_FAIL);

    /* enabled */
    CFGLDRSetBool (controller, "enabled", !!mData->mEnabled);

    DeviceFilterList::const_iterator it = mDeviceFilters->begin();
    while (it != mDeviceFilters->end())
    {
        AutoLock filterLock (*it);
        const USBDeviceFilter::Data &data = (*it)->data();

        CFGNODE filter = NULL;
        CFGLDRAppendChildNode (controller, "DeviceFilter",  &filter);

        CFGLDRSetBSTR (filter, "name", data.mName);
        CFGLDRSetBool (filter, "active", !!data.mActive);

        /* all are optional */
#ifndef VBOX_WITH_USBFILTER
        if (data.mVendorId.string())
            CFGLDRSetBSTR (filter, "vendorid", data.mVendorId.string());
        if (data.mProductId.string())
            CFGLDRSetBSTR (filter, "productid", data.mProductId.string());
        if (data.mRevision.string())
            CFGLDRSetBSTR (filter, "revision", data.mRevision.string());
        if (data.mManufacturer.string())
            CFGLDRSetBSTR (filter, "manufacturer", data.mManufacturer.string());
        if (data.mProduct.string())
            CFGLDRSetBSTR (filter, "product", data.mProduct.string());
        if (data.mSerialNumber.string())
            CFGLDRSetBSTR (filter, "serialnumber", data.mSerialNumber.string());
        if (data.mPort.string())
            CFGLDRSetBSTR (filter, "port", data.mPort.string());
        if (data.mRemote.string())
            CFGLDRSetBSTR (filter, "remote", data.mRemote.string());

#else  /* VBOX_WITH_USBFILTER */
        Bstr str;
        (*it)->COMGETTER (VendorId) (str.asOutParam());
        if (!str.isNull())
            CFGLDRSetBSTR (filter, "vendorid", str);

        (*it)->COMGETTER (ProductId) (str.asOutParam());
        if (!str.isNull())
            CFGLDRSetBSTR (filter, "productid", str);

        (*it)->COMGETTER (Revision) (str.asOutParam());
        if (!str.isNull())
            CFGLDRSetBSTR (filter, "revision", str);

        (*it)->COMGETTER (Manufacturer) (str.asOutParam());
        if (!str.isNull())
            CFGLDRSetBSTR (filter, "manufacturer", str);

        (*it)->COMGETTER (Product) (str.asOutParam());
        if (!str.isNull())
            CFGLDRSetBSTR (filter, "product", str);

        (*it)->COMGETTER (SerialNumber) (str.asOutParam());
        if (!str.isNull())
            CFGLDRSetBSTR (filter, "serialnumber", str);

        (*it)->COMGETTER (Port) (str.asOutParam());
        if (!str.isNull())
            CFGLDRSetBSTR (filter, "port", str);

        if (data.mRemote.string())
            CFGLDRSetBSTR (filter, "remote", data.mRemote.string());
#endif /* VBOX_WITH_USBFILTER */

        CFGLDRReleaseNode (filter);

        ++ it;
    }

    CFGLDRReleaseNode (controller);

    return S_OK;
}

/** @note Locks objects for reading! */
bool USBController::isModified()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReaderLock alock (this);

    if (mData.isBackedUp() || mDeviceFilters.isBackedUp())
        return true;

    /* see whether any of filters has changed its data */
    for (DeviceFilterList::const_iterator
         it = mDeviceFilters->begin();
         it != mDeviceFilters->end();
         ++ it)
    {
        if ((*it)->isModified())
            return true;
    }

    return false;
}

/** @note Locks objects for reading! */
bool USBController::isReallyModified()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReaderLock alock (this);

    if (mData.hasActualChanges())
        return true;

    if (!mDeviceFilters.isBackedUp())
    {
        /* see whether any of filters has changed its data */
        for (DeviceFilterList::const_iterator
             it = mDeviceFilters->begin();
             it != mDeviceFilters->end();
             ++ it)
        {
            if ((*it)->isReallyModified())
                return true;
        }

        return false;
    }

    if (mDeviceFilters->size() != mDeviceFilters.backedUpData()->size())
        return true;

    if (mDeviceFilters->size() == 0)
        return false;

    /* Make copies to speed up comparison */
    DeviceFilterList devices = *mDeviceFilters.data();
    DeviceFilterList backDevices = *mDeviceFilters.backedUpData();

    DeviceFilterList::iterator it = devices.begin();
    while (it != devices.end())
    {
        bool found = false;
        DeviceFilterList::iterator thatIt = backDevices.begin();
        while (thatIt != backDevices.end())
        {
            if ((*it)->data() == (*thatIt)->data())
            {
                backDevices.erase (thatIt);
                found = true;
                break;
            }
            else
                ++ thatIt;
        }
        if (found)
            it = devices.erase (it);
        else
            return false;
    }

    Assert (devices.size() == 0 && backDevices.size() == 0);

    return false;
}

/** @note Locks objects for writing! */
bool USBController::rollback()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    /* we need the machine state */
    Machine::AutoAnyStateDependency adep (mParent);
    AssertComRCReturn (adep.rc(), false);

    AutoLock alock (this);

    bool dataChanged = false;

    if (mData.isBackedUp())
    {
        /* we need to check all data to see whether anything will be changed
         * after rollback */
        dataChanged = mData.hasActualChanges();
        mData.rollback();
    }

    if (mDeviceFilters.isBackedUp())
    {
        USBProxyService *service = mParent->virtualBox()->host()->usbProxyService();
        ComAssertRet (service, false);

        /* uninitialize all new filters (absent in the backed up list) */
        DeviceFilterList::const_iterator it = mDeviceFilters->begin();
        DeviceFilterList *backedList = mDeviceFilters.backedUpData();
        while (it != mDeviceFilters->end())
        {
            if (std::find (backedList->begin(), backedList->end(), *it) ==
                backedList->end())
            {
    /// @todo After rewriting Win32 USB support, no more necessary;
    //  a candidate for removal.
#if 0
                /* notify the proxy (only when the filter is active) */
                if ((*it)->data().mActive)
#else
                /* notify the proxy (only when it makes sense) */
                if ((*it)->data().mActive &&
                    adep.machineState() >= MachineState_Running)
#endif
                {
                    USBDeviceFilter *filter = *it;
                    ComAssertRet (filter->id() != NULL, false);
                    service->removeFilter (filter->id());
                    filter->id() = NULL;
                }

                (*it)->uninit();
            }
            ++ it;
        }

    /// @todo After rewriting Win32 USB support, no more necessary;
    //  a candidate for removal.
#if 0
#else
        if (adep.machineState() >= MachineState_Running)
#endif
        {
            /* find all removed old filters (absent in the new list)
             * and insert them back to the USB proxy */
            it = backedList->begin();
            while (it != backedList->end())
            {
                if (std::find (mDeviceFilters->begin(), mDeviceFilters->end(), *it) ==
                    mDeviceFilters->end())
                {
                    /* notify the proxy (only when necessary) */
                    if ((*it)->data().mActive)
                    {
                        USBDeviceFilter *flt = *it; /* resolve ambiguity */
                        ComAssertRet (flt->id() == NULL, false);
#ifndef VBOX_WITH_USBFILTER
                        flt->id() = service->insertFilter
                            (ComPtr <IUSBDeviceFilter> (flt));
#else
                        flt->id() = service->insertFilter (&flt->data().mUSBFilter);
#endif
                    }
                }
                ++ it;
            }
        }

        /* restore the list */
        mDeviceFilters.rollback();
    }

    /* here we don't depend on the machine state any more */
    adep.release();

    /* rollback any changes to filters after restoring the list */
    DeviceFilterList::const_iterator it = mDeviceFilters->begin();
    while (it != mDeviceFilters->end())
    {
        if ((*it)->isModified())
        {
            (*it)->rollback();
            /* call this to notify the USB proxy about changes */
            onDeviceFilterChange (*it);
        }
        ++ it;
    }

    return dataChanged;
}

/** @note Locks objects for writing! */
void USBController::commit()
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoLock alock (this);

    if (mData.isBackedUp())
    {
        mData.commit();
        if (mPeer)
        {
            // attach new data to the peer and reshare it
            AutoLock peerlock (mPeer);
            mPeer->mData.attach (mData);
        }
    }

    bool commitFilters = false;

    if (mDeviceFilters.isBackedUp())
    {
        mDeviceFilters.commit();

        // apply changes to peer
        if (mPeer)
        {
            AutoLock peerlock (mPeer);
            // commit all changes to new filters (this will reshare data with
            // peers for those who have peers)
            DeviceFilterList *newList = new DeviceFilterList();
            DeviceFilterList::const_iterator it = mDeviceFilters->begin();
            while (it != mDeviceFilters->end())
            {
                (*it)->commit();

                // look if this filter has a peer filter
                ComObjPtr <USBDeviceFilter> peer = (*it)->peer();
                if (!peer)
                {
                    // no peer means the filter is a newly created one;
                    // create a peer owning data this filter share it with
                    peer.createObject();
                    peer->init (mPeer, *it, true /* aReshare */);
                }
                else
                {
                    // remove peer from the old list
                    mPeer->mDeviceFilters->remove (peer);
                }
                // and add it to the new list
                newList->push_back (peer);

                ++ it;
            }

            // uninit old peer's filters that are left
            it = mPeer->mDeviceFilters->begin();
            while (it != mPeer->mDeviceFilters->end())
            {
                (*it)->uninit();
                ++ it;
            }

            // attach new list of filters to our peer
            mPeer->mDeviceFilters.attach (newList);
        }
        else
        {
            // we have no peer (our parent is the newly created machine);
            // just commit changes to filters
            commitFilters = true;
        }
    }
    else
    {
        // the list of filters itself is not changed,
        // just commit changes to filters themselves
        commitFilters = true;
    }

    if (commitFilters)
    {
        DeviceFilterList::const_iterator it = mDeviceFilters->begin();
        while (it != mDeviceFilters->end())
        {
            (*it)->commit();
            ++ it;
        }
    }
}

/** @note Locks object for writing and that object for reading! */
void USBController::copyFrom (USBController *aThat)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoMultiLock <2> alock (this->wlock(), aThat->rlock());

    if (mParent->isRegistered())
    {
        /* reuse onMachineRegistered to tell USB proxy to remove all current
           filters */
        HRESULT rc = onMachineRegistered (FALSE);
        AssertComRCReturn (rc, (void) 0);
    }

    /* this will back up current data */
    mData.assignCopy (aThat->mData);

    /* create private copies of all filters */
    mDeviceFilters.backup();
    mDeviceFilters->clear();
    for (DeviceFilterList::const_iterator it = aThat->mDeviceFilters->begin();
        it != aThat->mDeviceFilters->end();
        ++ it)
    {
        ComObjPtr <USBDeviceFilter> filter;
        filter.createObject();
        filter->initCopy (this, *it);
        mDeviceFilters->push_back (filter);
    }

    if (mParent->isRegistered())
    {
        /* reuse onMachineRegistered to tell USB proxy to insert all current
           filters */
        HRESULT rc = onMachineRegistered (TRUE);
        AssertComRCReturn (rc, (void) 0);
    }
}

/**
 *  Called by VirtualBox when it changes the registered state
 *  of the machine this USB controller belongs to.
 *
 *  @param aRegistered  new registered state of the machine
 *
 *  @note Locks nothing.
 */
HRESULT USBController::onMachineRegistered (BOOL aRegistered)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    /// @todo After rewriting Win32 USB support, no more necessary;
    //  a candidate for removal.
#if 0
    notifyProxy (!!aRegistered);
#endif

    return S_OK;
}

/**
 *  Called by setter methods of all USB device filters.
 *
 *  @note Locks nothing.
 */
HRESULT USBController::onDeviceFilterChange (USBDeviceFilter *aFilter,
                                             BOOL aActiveChanged /* = FALSE */)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    /// @todo After rewriting Win32 USB support, no more necessary;
    //  a candidate for removal.
#if 0
#else
    /* we need the machine state */
    Machine::AutoAnyStateDependency adep (mParent);
    AssertComRCReturnRC (adep.rc());

    /* nothing to do if the machine isn't running */
    if (adep.machineState() < MachineState_Running)
        return S_OK;
#endif

    /* we don't modify our data fields -- no need to lock */

    if (aFilter->mInList && mParent->isRegistered())
    {
        USBProxyService *service = mParent->virtualBox()->host()->usbProxyService();
        ComAssertRet (service, E_FAIL);

        if (aActiveChanged)
        {
            /* insert/remove the filter from the proxy */
            if (aFilter->data().mActive)
            {
                ComAssertRet (aFilter->id() == NULL, E_FAIL);
#ifndef VBOX_WITH_USBFILTER
                aFilter->id() = service->insertFilter
                    (ComPtr <IUSBDeviceFilter> (aFilter));
#else
                aFilter->id() = service->insertFilter (&aFilter->data().mUSBFilter);
#endif
            }
            else
            {
                ComAssertRet (aFilter->id() != NULL, E_FAIL);
                service->removeFilter (aFilter->id());
                aFilter->id() = NULL;
            }
        }
        else
        {
            if (aFilter->data().mActive)
            {
                /* update the filter in the proxy */
                ComAssertRet (aFilter->id() != NULL, E_FAIL);
                service->removeFilter (aFilter->id());
#ifndef VBOX_WITH_USBFILTER
                aFilter->id() = service->insertFilter
                    (ComPtr <IUSBDeviceFilter> (aFilter));
#else
                aFilter->id() = service->insertFilter (&aFilter->data().mUSBFilter);
#endif
            }
        }
    }

    return S_OK;
}

/**
 *  Returns true if the given USB device matches to at least one of
 *  this controller's USB device filters.
 *
 *  A HostUSBDevice specific version.
 *
 *  @note Locks this object for reading.
 */
bool USBController::hasMatchingFilter (ComObjPtr <HostUSBDevice> &aDevice)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReaderLock alock (this);

    /* Disabled USB controllers cannot actually work with USB devices */
    if (!mData->mEnabled)
        return false;

    bool match = false;

    /* apply self filters */
    for (DeviceFilterList::const_iterator it = mDeviceFilters->begin();
         !match && it != mDeviceFilters->end();
         ++ it)
    {
        AutoLock filterLock (*it);
        match = aDevice->isMatch ((*it)->data());
    }

    return match;
}

/**
 *  Returns true if the given USB device matches to at least one of
 *  this controller's USB device filters.
 *
 *  A generic version that accepts any IUSBDevice on input.
 *
 *  @note
 *      This method MUST correlate with HostUSBDevice::isMatch()
 *      in the sense of the device matching logic.
 *
 *  @note Locks this object for reading.
 */
bool USBController::hasMatchingFilter (IUSBDevice *aUSBDevice)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReaderLock alock (this);

    /* Disabled USB controllers cannot actually work with USB devices */
    if (!mData->mEnabled)
        return false;

    HRESULT rc = S_OK;

    /* query fields */
#ifdef VBOX_WITH_USBFILTER
    USBFILTER dev;
    USBFilterInit (&dev, USBFILTERTYPE_CAPTURE);
#endif

    USHORT vendorId = 0;
    rc = aUSBDevice->COMGETTER(VendorId) (&vendorId);
    ComAssertComRCRet (rc, false);
    ComAssertRet (vendorId, false);
#ifdef VBOX_WITH_USBFILTER
    int vrc = USBFilterSetNumExact (&dev, USBFILTERIDX_VENDOR_ID, vendorId, true); AssertRC(vrc);
#endif

    USHORT productId = 0;
    rc = aUSBDevice->COMGETTER(ProductId) (&productId);
    ComAssertComRCRet (rc, false);
    ComAssertRet (productId, false);
#ifdef VBOX_WITH_USBFILTER
    vrc = USBFilterSetNumExact (&dev, USBFILTERIDX_PRODUCT_ID, productId, true); AssertRC(vrc);
#endif

    USHORT revision;
    rc = aUSBDevice->COMGETTER(Revision) (&revision);
    ComAssertComRCRet (rc, false);
#ifdef VBOX_WITH_USBFILTER
    vrc = USBFilterSetNumExact (&dev, USBFILTERIDX_DEVICE, revision, true); AssertRC(vrc);
#endif

    Bstr manufacturer;
    rc = aUSBDevice->COMGETTER(Manufacturer) (manufacturer.asOutParam());
    ComAssertComRCRet (rc, false);
#ifdef VBOX_WITH_USBFILTER
    if (!manufacturer.isNull())
        USBFilterSetStringExact (&dev, USBFILTERIDX_MANUFACTURER_STR, Utf8Str(manufacturer), true);
#endif

    Bstr product;
    rc = aUSBDevice->COMGETTER(Product) (product.asOutParam());
    ComAssertComRCRet (rc, false);
#ifdef VBOX_WITH_USBFILTER
    if (!product.isNull())
        USBFilterSetStringExact (&dev, USBFILTERIDX_PRODUCT_STR, Utf8Str(product), true);
#endif

    Bstr serialNumber;
    rc = aUSBDevice->COMGETTER(SerialNumber) (serialNumber.asOutParam());
    ComAssertComRCRet (rc, false);
#ifdef VBOX_WITH_USBFILTER
    if (!serialNumber.isNull())
        USBFilterSetStringExact (&dev, USBFILTERIDX_SERIAL_NUMBER_STR, Utf8Str(serialNumber), true);
#endif

    Bstr address;
    rc = aUSBDevice->COMGETTER(Address) (address.asOutParam());
    ComAssertComRCRet (rc, false);

    USHORT port = 0;
    rc = aUSBDevice->COMGETTER(Port)(&port);
    ComAssertComRCRet (rc, false);
#ifdef VBOX_WITH_USBFILTER
    USBFilterSetNumExact (&dev, USBFILTERIDX_PORT, port, true);
#endif

    BOOL remote = FALSE;
    rc = aUSBDevice->COMGETTER(Remote)(&remote);
    ComAssertComRCRet (rc, false);
    ComAssertRet (remote == TRUE, false);

    bool match = false;

    /* apply self filters */
    for (DeviceFilterList::const_iterator it = mDeviceFilters->begin();
         it != mDeviceFilters->end();
         ++ it)
    {
        AutoLock filterLock (*it);
        const USBDeviceFilter::Data &aData = (*it)->data();


        if (!aData.mActive)
            continue;
        if (!aData.mRemote.isMatch (remote))
            continue;

#ifndef VBOX_WITH_USBFILTER
        if (!aData.mVendorId.isMatch (vendorId))
            continue;
        if (!aData.mProductId.isMatch (productId))
            continue;
        if (!aData.mRevision.isMatch (revision))
            continue;

#if !defined (RT_OS_WINDOWS)
        /* these filters are temporarily ignored on Win32 */
        if (!aData.mManufacturer.isMatch (manufacturer))
            continue;
        if (!aData.mProduct.isMatch (product))
            continue;
        if (!aData.mSerialNumber.isMatch (serialNumber))
            continue;
        if (!aData.mPort.isMatch (port))
            continue;
#endif

#else  /* VBOX_WITH_USBFILTER */
        if (!USBFilterMatch (&aData.mUSBFilter, &dev))
            continue;
#endif /* VBOX_WITH_USBFILTER */

        match = true;
        break;
    }

    LogFlowThisFunc (("returns: %d\n", match));
    LogFlowThisFuncLeave();

    return match;
}

/**
 *  Notifies the proxy service about all filters as requested by the
 *  @a aInsertFilters argument.
 *
 *  @param aInsertFilters   @c true to insert filters, @c false to remove.
 *
 *  @note Locks this object for reading.
 */
HRESULT USBController::notifyProxy (bool aInsertFilters)
{
    LogFlowThisFunc (("aInsertFilters=%RTbool\n", aInsertFilters));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReaderLock alock (this);

    USBProxyService *service = mParent->virtualBox()->host()->usbProxyService();
    AssertReturn (service, E_FAIL);

    DeviceFilterList::const_iterator it = mDeviceFilters->begin();
    while (it != mDeviceFilters->end())
    {
        USBDeviceFilter *flt = *it; /* resolve ambiguity (for ComPtr below) */

        /* notify the proxy (only if the filter is active) */
        if (flt->data().mActive)
        {
            if (aInsertFilters)
            {
                AssertReturn (flt->id() == NULL, E_FAIL);
#ifndef VBOX_WITH_USBFILTER
                flt->id() = service->insertFilter
                    (ComPtr <IUSBDeviceFilter> (flt));
#else
                flt->id() = service->insertFilter (&flt->data().mUSBFilter);
#endif
            }
            else
            {
                /* It's possible that the given filter was not inserted the proxy
                 * when this method gets called (as a result of an early VM
                 * process crash for example. So, don't assert that ID != NULL. */
                if (flt->id() != NULL)
                {
                    service->removeFilter (flt->id());
                    flt->id() = NULL;
                }
            }
        }
        ++ it;
    }

    return S_OK;
}

// private methods
/////////////////////////////////////////////////////////////////////////////

