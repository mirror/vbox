/** @file
 *
 * Implementation of IUSBController.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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
#include <VBox/err.h>

#include <algorithm>

// defines
/////////////////////////////////////////////////////////////////////////////

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HRESULT USBController::FinalConstruct()
{
    return S_OK;
}

void USBController::FinalRelease()
{
    if (isReady())
        uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the USB controller object.
 *
 * @returns COM result indicator.
 * @param a_pParent     Pointer to our parent object.
 */
HRESULT USBController::init(Machine *a_pParent)
{
    LogFlowMember(("USBController::init (%p)\n", a_pParent));

    ComAssertRet (a_pParent, E_INVALIDARG);

    AutoLock alock(this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    m_Parent = a_pParent;
    // m_Peer is left null

    m_Data.allocate();
    m_DeviceFilters.allocate();

    setReady(true);
    return S_OK;
}


/**
 * Initializes the USB controller object given another USB controller object
 * (a kind of copy constructor). This object shares data with
 * the object passed as an argument.
 *
 * @returns COM result indicator.
 * @param a_pParent     Pointer to our parent object.
 * @param a_pPeer       The object to share.
 *
 * @remark This object must be destroyed before the original object
 * it shares data with is destroyed.
 */
HRESULT USBController::init(Machine *a_pParent, USBController *a_pPeer)
{
    LogFlowMember(("USBController::init (%p, %p)\n", a_pParent, a_pPeer));

    ComAssertRet (a_pParent && a_pPeer, E_INVALIDARG);

    AutoLock alock(this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    m_Parent = a_pParent;
    m_Peer = a_pPeer;

    AutoLock thatlock(a_pPeer);
    m_Data.share(a_pPeer->m_Data);

    // create copies of all filters
    m_DeviceFilters.allocate();
    DeviceFilterList::const_iterator it = a_pPeer->m_DeviceFilters->begin();
    while (it != a_pPeer->m_DeviceFilters->end())
    {
        ComObjPtr <USBDeviceFilter> filter;
        filter.createObject();
        filter->init (this, *it);
        m_DeviceFilters->push_back (filter);
        ++ it;
    }

    setReady(true);
    return S_OK;
}


/**
 *  Initializes the USB controller object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 */
HRESULT USBController::initCopy (Machine *a_pParent, USBController *a_pPeer)
{
    LogFlowMember(("USBController::initCopy (%p, %p)\n", a_pParent, a_pPeer));

    ComAssertRet (a_pParent && a_pPeer, E_INVALIDARG);

    AutoLock alock(this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    m_Parent = a_pParent;
    // m_Peer is left null

    AutoLock thatlock(a_pPeer);
    m_Data.attachCopy (a_pPeer->m_Data);

    // create private copies of all filters
    m_DeviceFilters.allocate();
    DeviceFilterList::const_iterator it = a_pPeer->m_DeviceFilters->begin();
    while (it != a_pPeer->m_DeviceFilters->end())
    {
        ComObjPtr <USBDeviceFilter> filter;
        filter.createObject();
        filter->initCopy (this, *it);
        m_DeviceFilters->push_back (filter);
        ++ it;
    }

    setReady(true);
    return S_OK;
}


/**
 * Uninitializes the instance and sets the ready flag to FALSE.
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void USBController::uninit()
{
    LogFlowMember(("USBController::uninit()\n"));

    AutoLock alock(this);
    AssertReturn (isReady(), (void) 0);

    // uninit all filters (including those still referenced by clients)
    uninitDependentChildren();

    m_DeviceFilters.free();
    m_Data.free();

    m_Peer.setNull();
    m_Parent.setNull();

    setReady(false);
}



// IUSBController properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP USBController::COMGETTER(Enabled)(BOOL *a_pfEnabled)
{
    if (!a_pfEnabled)
        return E_POINTER;

    AutoLock alock(this);
    CHECK_READY();

    *a_pfEnabled = m_Data->m_fEnabled;
    LogFlowMember(("USBController::GetEnabled: returns %p\n", *a_pfEnabled));
    return S_OK;
}


STDMETHODIMP USBController::COMSETTER(Enabled)(BOOL a_fEnabled)
{
    LogFlowMember(("USBController::SetEnabled: %s\n", a_fEnabled ? "enable" : "disable"));

    AutoLock alock(this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (m_Parent);

    if (m_Data->m_fEnabled != a_fEnabled)
    {
        m_Data.backup();
        m_Data->m_fEnabled = a_fEnabled;

        alock.unlock();
        m_Parent->onUSBControllerChange();
    }

    return S_OK;
}

/**
 * Get the USB standard version.
 *
 * @param   a_pusUSBStandard    Where to store the BCD version word.
 */
STDMETHODIMP USBController::COMGETTER(USBStandard)(USHORT *a_pusUSBStandard)
{
    if (!a_pusUSBStandard)
        return E_POINTER;

    AutoLock alock(this);
    CHECK_READY();

    *a_pusUSBStandard = 0x0101;
    LogFlowMember(("USBController::GetUSBStandard: returns 0x0101\n"));
    return S_OK;
}

STDMETHODIMP USBController::COMGETTER(DeviceFilters) (IUSBDeviceFilterCollection **aDevicesFilters)
{
    if (!aDevicesFilters)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    ComObjPtr <USBDeviceFilterCollection> collection;
    collection.createObject();
    collection->init (*m_DeviceFilters.data());
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

    AutoLock lock (this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (m_Parent);

    ComObjPtr <USBDeviceFilter> filter;
    filter.createObject();
    HRESULT rc = filter->init (this, aName);
    ComAssertComRCRet (rc, rc);
    rc = filter.queryInterfaceTo (aFilter);
    AssertComRCReturn (rc, rc);
    return S_OK;
}

STDMETHODIMP USBController::InsertDeviceFilter (ULONG aPosition,
                                                IUSBDeviceFilter *aFilter)
{
    if (!aFilter)
        return E_INVALIDARG;

    AutoLock lock (this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (m_Parent);

    ComObjPtr <USBDeviceFilter> filter = getDependentChild (aFilter);
    if (!filter)
        return setError (E_INVALIDARG,
            tr ("The given USB device filter is not created within "
                "this VirtualBox instance"));

    if (filter->mInList)
        return setError (E_INVALIDARG,
            tr ("The given USB device filter is already in the list"));

    // backup the list before modification
    m_DeviceFilters.backup();

    // iterate to the position...
    DeviceFilterList::iterator it;
    if (aPosition < m_DeviceFilters->size())
    {
        it = m_DeviceFilters->begin();
        std::advance (it, aPosition);
    }
    else
        it = m_DeviceFilters->end();
    // ...and insert
    m_DeviceFilters->insert (it, filter);
    filter->mInList = true;

    // notify the proxy (only when the filter is active)
    if (filter->data().mActive)
    {
        USBProxyService *service = m_Parent->virtualBox()->host()->usbProxyService();
        ComAssertRet (service, E_FAIL);

        ComAssertRet (filter->id() == NULL, E_FAIL);
        filter->id() = service->insertFilter (ComPtr <IUSBDeviceFilter> (aFilter));
    }

    return S_OK;
}

STDMETHODIMP USBController::RemoveDeviceFilter (ULONG aPosition,
                                                IUSBDeviceFilter **aFilter)
{
    if (!aFilter)
        return E_POINTER;

    AutoLock lock (this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (m_Parent);

    if (!m_DeviceFilters->size())
        return setError (E_INVALIDARG,
            tr ("The USB device filter list is empty"));

    if (aPosition >= m_DeviceFilters->size())
        return setError (E_INVALIDARG,
            tr ("Invalid position: %lu (must be in range [0, %lu])"),
            aPosition, m_DeviceFilters->size() - 1);

    // backup the list before modification
    m_DeviceFilters.backup();

    ComObjPtr <USBDeviceFilter> filter;
    {
        // iterate to the position...
        DeviceFilterList::iterator it = m_DeviceFilters->begin();
        std::advance (it, aPosition);
        // ...get an element from there...
        filter = *it;
        // ...and remove
        filter->mInList = false;
        m_DeviceFilters->erase (it);
    }

    // cancel sharing (make an independent copy of data)
    filter->unshare();

    filter.queryInterfaceTo (aFilter);

    // notify the proxy (only when the filter is active)
    if (filter->data().mActive)
    {
        USBProxyService *service = m_Parent->virtualBox()->host()->usbProxyService();
        ComAssertRet (service, E_FAIL);

        ComAssertRet (filter->id() != NULL, E_FAIL);
        service->removeFilter (filter->id());
        filter->id() = NULL;
    }

    return S_OK;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/** Loads settings from the configuration node */
HRESULT USBController::loadSettings (CFGNODE aMachine)
{
    AutoLock lock (this);
    CHECK_READY();

    ComAssertRet (aMachine, E_FAIL);

    CFGNODE controller = NULL;
    CFGLDRGetChildNode (aMachine, "USBController", 0, &controller);
    Assert (controller);

    // enabled
    bool enabled;
    CFGLDRQueryBool (controller, "enabled", &enabled);
    m_Data->m_fEnabled = enabled;

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
        // error info is set by init() when appropriate
        if (SUCCEEDED (rc))
        {
            m_DeviceFilters->push_back (filterObj);
            filterObj->mInList = true;
        }

        CFGLDRReleaseNode (filter);
    }

    CFGLDRReleaseNode (controller);

    return rc;
}

/** Saves settings to the configuration node */
HRESULT USBController::saveSettings (CFGNODE aMachine)
{
    AutoLock lock (this);
    CHECK_READY();

    ComAssertRet (aMachine, E_FAIL);

    // first, delete the entry
    CFGNODE controller = NULL;
    int vrc = CFGLDRGetChildNode (aMachine, "USBController", 0, &controller);
    if (VBOX_SUCCESS (vrc))
    {
        vrc = CFGLDRDeleteNode (controller);
        ComAssertRCRet (vrc, E_FAIL);
    }
    // then, recreate it
    vrc = CFGLDRCreateChildNode (aMachine, "USBController", &controller);
    ComAssertRCRet (vrc, E_FAIL);

    // enabled
    CFGLDRSetBool (controller, "enabled", !!m_Data->m_fEnabled);

    DeviceFilterList::const_iterator it = m_DeviceFilters->begin();
    while (it != m_DeviceFilters->end())
    {
        AutoLock filterLock (*it);
        const USBDeviceFilter::Data &data = (*it)->data();

        CFGNODE filter = NULL;
        CFGLDRAppendChildNode (controller, "DeviceFilter",  &filter);

        CFGLDRSetBSTR (filter, "name", data.mName);
        CFGLDRSetBool (filter, "active", !!data.mActive);

        // all are optional
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

        CFGLDRReleaseNode (filter);

        ++ it;
    }

    CFGLDRReleaseNode (controller);

    return S_OK;
}

bool USBController::isModified()
{
    AutoLock alock(this);

    if (m_Data.isBackedUp() || m_DeviceFilters.isBackedUp())
        return true;

    /* see whether any of filters has changed its data */
    for (DeviceFilterList::const_iterator
         it = m_DeviceFilters->begin();
         it != m_DeviceFilters->end();
         ++ it)
    {
        if ((*it)->isModified())
            return true;
    }

    return false;
}

bool USBController::isReallyModified()
{
    AutoLock alock(this);

    if (m_Data.hasActualChanges())
        return true;

    if (!m_DeviceFilters.isBackedUp())
    {
        /* see whether any of filters has changed its data */
        for (DeviceFilterList::const_iterator
             it = m_DeviceFilters->begin();
             it != m_DeviceFilters->end();
             ++ it)
        {
            if ((*it)->isReallyModified())
                return true;
        }

        return false;
    }

    if (m_DeviceFilters->size() != m_DeviceFilters.backedUpData()->size())
        return true;

    if (m_DeviceFilters->size() == 0)
        return false;

    /* Make copies to speed up comparison */
    DeviceFilterList devices = *m_DeviceFilters.data();
    DeviceFilterList backDevices = *m_DeviceFilters.backedUpData();

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

bool USBController::rollback()
{
    AutoLock alock(this);

    bool dataChanged = false;

    if (m_Data.isBackedUp())
    {
        // we need to check all data to see whether anything will be changed
        // after rollback
        dataChanged = m_Data.hasActualChanges();
        m_Data.rollback();
    }

    if (m_DeviceFilters.isBackedUp())
    {
        USBProxyService *service = m_Parent->virtualBox()->host()->usbProxyService();
        ComAssertRet (service, false);

        // uninitialize all new filters (absent in the backed up list)
        DeviceFilterList::const_iterator it = m_DeviceFilters->begin();
        DeviceFilterList *backedList = m_DeviceFilters.backedUpData();
        while (it != m_DeviceFilters->end())
        {
            if (std::find (backedList->begin(), backedList->end(), *it) ==
                backedList->end())
            {
                // notify the proxy (only when the filter is active)
                if ((*it)->data().mActive)
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

        // find all removed old filters (absent in the new list)
        // and insert them back to the USB proxy
        it = backedList->begin();
        while (it != backedList->end())
        {
            if (std::find (m_DeviceFilters->begin(), m_DeviceFilters->end(), *it) ==
                m_DeviceFilters->end())
            {
                // notify the proxy (only when the filter is active)
                if ((*it)->data().mActive)
                {
                    USBDeviceFilter *flt = *it; // resolve ambiguity
                    ComAssertRet (flt->id() == NULL, false);
                    flt->id() = service->insertFilter (ComPtr <IUSBDeviceFilter> (flt));
                }
            }
            ++ it;
        }

        // restore the list
        m_DeviceFilters.rollback();
    }

    // rollback any changes to filters after restoring the list
    DeviceFilterList::const_iterator it = m_DeviceFilters->begin();
    while (it != m_DeviceFilters->end())
    {
        if ((*it)->isModified())
        {
            (*it)->rollback();
            // call this to notify the USB proxy about changes
            onDeviceFilterChange (*it);
        }
        ++ it;
    }

    return dataChanged;
}

void USBController::commit()
{
    AutoLock alock (this);

    if (m_Data.isBackedUp())
    {
        m_Data.commit();
        if (m_Peer)
        {
            // attach new data to the peer and reshare it
            AutoLock peerlock (m_Peer);
            m_Peer->m_Data.attach (m_Data);
        }
    }

    bool commitFilters = false;

    if (m_DeviceFilters.isBackedUp())
    {
        m_DeviceFilters.commit();

        // apply changes to peer
        if (m_Peer)
        {
            AutoLock peerlock (m_Peer);
            // commit all changes to new filters (this will reshare data with
            // peers for those who have peers)
            DeviceFilterList *newList = new DeviceFilterList();
            DeviceFilterList::const_iterator it = m_DeviceFilters->begin();
            while (it != m_DeviceFilters->end())
            {
                (*it)->commit();

                // look if this filter has a peer filter
                ComObjPtr <USBDeviceFilter> peer = (*it)->peer();
                if (!peer)
                {
                    // no peer means the filter is a newly created one;
                    // create a peer owning data this filter share it with
                    peer.createObject();
                    peer->init (m_Peer, *it, true /* aReshare */);
                }
                else
                {
                    // remove peer from the old list
                    m_Peer->m_DeviceFilters->remove (peer);
                }
                // and add it to the new list
                newList->push_back (peer);

                ++ it;
            }

            // uninit old peer's filters that are left
            it = m_Peer->m_DeviceFilters->begin();
            while (it != m_Peer->m_DeviceFilters->end())
            {
                (*it)->uninit();
                ++ it;
            }

            // attach new list of filters to our peer
            m_Peer->m_DeviceFilters.attach (newList);
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
        DeviceFilterList::const_iterator it = m_DeviceFilters->begin();
        while (it != m_DeviceFilters->end())
        {
            (*it)->commit();
            ++ it;
        }
    }
}

void USBController::copyFrom (USBController *aThat)
{
    AutoLock alock (this);
    AutoLock thatLock (aThat);

    if (m_Parent->isRegistered())
    {
        // reuse onMachineRegistered to tell USB proxy to remove all current filters
        HRESULT rc = onMachineRegistered (FALSE);
        AssertComRCReturn (rc, (void) 0);
    }

    // this will back up current data
    m_Data.assignCopy (aThat->m_Data);

    // create private copies of all filters
    m_DeviceFilters.backup();
    m_DeviceFilters->clear();
    for (DeviceFilterList::const_iterator it = aThat->m_DeviceFilters->begin();
        it != aThat->m_DeviceFilters->end();
        ++ it)
    {
        ComObjPtr <USBDeviceFilter> filter;
        filter.createObject();
        filter->initCopy (this, *it);
        m_DeviceFilters->push_back (filter);
    }

    if (m_Parent->isRegistered())
    {
        // reuse onMachineRegistered to tell USB proxy to insert all current filters
        HRESULT rc = onMachineRegistered (TRUE);
        AssertComRCReturn (rc, (void) 0);
    }
}

/**
 *  Called by VirtualBox when it changes the registered state
 *  of the machine this USB controller belongs to.
 *
 *  @param aRegistered  new registered state of the machine
 */
HRESULT USBController::onMachineRegistered (BOOL aRegistered)
{
    AutoLock alock (this);
    CHECK_READY();

    USBProxyService *service = m_Parent->virtualBox()->host()->usbProxyService();
    ComAssertRet (service, E_FAIL);

    // iterate over the filter list and notify the proxy accordingly

    DeviceFilterList::const_iterator it = m_DeviceFilters->begin();
    while (it != m_DeviceFilters->end())
    {
        USBDeviceFilter *flt = *it; // resolve ambiguity (for ComPtr below)

        // notify the proxy (only if the filter is active)
        if (flt->data().mActive)
        {
            if (aRegistered)
            {
                ComAssertRet (flt->id() == NULL, E_FAIL);
                flt->id() = service->insertFilter (ComPtr <IUSBDeviceFilter> (flt));
            }
            else
            {
                ComAssertRet (flt->id() != NULL, E_FAIL);
                service->removeFilter (flt->id());
                flt->id() = NULL;
            }
        }
        ++ it;
    }

    return S_OK;
}

/**
 *  Called by setter methods of all USB device filters.
 */
HRESULT USBController::onDeviceFilterChange (USBDeviceFilter *aFilter,
                                             BOOL aActiveChanged /* = FALSE */)
{
    AutoLock alock (this);
    CHECK_READY();

    if (aFilter->mInList && m_Parent->isRegistered())
    {
        USBProxyService *service = m_Parent->virtualBox()->host()->usbProxyService();
        ComAssertRet (service, E_FAIL);

        if (aActiveChanged)
        {
            // insert/remove the filter from the proxy
            if (aFilter->data().mActive)
            {
                ComAssertRet (aFilter->id() == NULL, E_FAIL);
                aFilter->id() =
                    service->insertFilter (ComPtr <IUSBDeviceFilter> (aFilter));
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
                // update the filter in the proxy
                ComAssertRet (aFilter->id() != NULL, E_FAIL);
                service->removeFilter (aFilter->id());
                aFilter->id() =
                    service->insertFilter (ComPtr <IUSBDeviceFilter> (aFilter));
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
 */
bool USBController::hasMatchingFilter (ComObjPtr <HostUSBDevice> &aDevice)
{
    AutoLock alock (this);
    if (!isReady())
        return false;

    bool match = false;

    // apply self filters
    for (DeviceFilterList::const_iterator it = m_DeviceFilters->begin();
         !match && it != m_DeviceFilters->end();
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
 */
bool USBController::hasMatchingFilter (IUSBDevice *aUSBDevice)
{
    LogFlowMember (("USBController::hasMatchingFilter()\n"));

    AutoLock alock (this);
    if (!isReady())
        return false;

    HRESULT rc = S_OK;

    // query fields

    USHORT vendorId = 0;
    rc = aUSBDevice->COMGETTER(VendorId) (&vendorId);
    ComAssertComRCRet (rc, false);
    ComAssertRet (vendorId, false);

    USHORT productId = 0;
    rc = aUSBDevice->COMGETTER(ProductId) (&productId);
    ComAssertComRCRet (rc, false);
    ComAssertRet (productId, false);

    USHORT revision;
    rc = aUSBDevice->COMGETTER(Revision) (&revision);
    ComAssertComRCRet (rc, false);

    Bstr manufacturer;
    rc = aUSBDevice->COMGETTER(Manufacturer) (manufacturer.asOutParam());
    ComAssertComRCRet (rc, false);

    Bstr product;
    rc = aUSBDevice->COMGETTER(Product) (product.asOutParam());
    ComAssertComRCRet (rc, false);

    Bstr serialNumber;
    rc = aUSBDevice->COMGETTER(SerialNumber) (serialNumber.asOutParam());
    ComAssertComRCRet (rc, false);

    Bstr address;
    rc = aUSBDevice->COMGETTER(Address) (address.asOutParam());
    ComAssertComRCRet (rc, false);

    USHORT port = 0;
    rc = aUSBDevice->COMGETTER(Port)(&port);
    ComAssertComRCRet (rc, false);

    BOOL remote = FALSE;
    rc = aUSBDevice->COMGETTER(Remote)(&remote);
    ComAssertComRCRet (rc, false);
    ComAssertRet (remote == TRUE, false);

    bool match = false;

    // apply self filters
    for (DeviceFilterList::const_iterator it = m_DeviceFilters->begin();
         it != m_DeviceFilters->end();
         ++ it)
    {
        AutoLock filterLock (*it);
        const USBDeviceFilter::Data &aData = (*it)->data();

        if (!aData.mActive)
            continue;
        if (!aData.mVendorId.isMatch (vendorId))
            continue;
        if (!aData.mProductId.isMatch (productId))
            continue;
        if (!aData.mRevision.isMatch (revision))
            continue;

#if !defined (__WIN__)
        // these filters are temporarily ignored on Win32
        if (!aData.mManufacturer.isMatch (manufacturer))
            continue;
        if (!aData.mProduct.isMatch (product))
            continue;
        if (!aData.mSerialNumber.isMatch (serialNumber))
            continue;
        if (!aData.mPort.isMatch (port))
            continue;
#endif

        if (!aData.mRemote.isMatch (remote))
            continue;

        match = true;
        break;
    }

    LogFlowMember (("USBController::hasMatchingFilter() returns: %d\n", match));
    return match;
}

// private methods
/////////////////////////////////////////////////////////////////////////////

