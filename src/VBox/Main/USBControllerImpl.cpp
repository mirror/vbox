/* $Id$ */
/** @file
 * Implementation of IUSBController.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */



#include "USBControllerImpl.h"
#include "MachineImpl.h"
#include "VirtualBoxImpl.h"
#include "HostImpl.h"
#ifdef VBOX_WITH_USB
# include "USBDeviceImpl.h"
# include "HostUSBDeviceImpl.h"
# include "USBProxyService.h"
#endif
#include "Logging.h"


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
#ifdef VBOX_WITH_USB
    mDeviceFilters.allocate();
#endif

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

    AutoWriteLock thatlock (aPeer);
    mData.share (aPeer->mData);

#ifdef VBOX_WITH_USB
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
#endif /* VBOX_WITH_USB */

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

    AutoWriteLock thatlock (aPeer);
    mData.attachCopy (aPeer->mData);

#ifdef VBOX_WITH_USB
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
#endif /* VBOX_WITH_USB */

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

#ifdef VBOX_WITH_USB
    mDeviceFilters.free();
#endif
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

    AutoReadLock alock (this);

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

    AutoWriteLock alock (this);

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

STDMETHODIMP USBController::COMGETTER(EnabledEhci) (BOOL *aEnabled)
{
    if (!aEnabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aEnabled = mData->mEnabledEhci;

    return S_OK;
}

STDMETHODIMP USBController::COMSETTER(EnabledEhci) (BOOL aEnabled)
{
    LogFlowThisFunc (("aEnabled=%RTbool\n", aEnabled));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    if (mData->mEnabledEhci != aEnabled)
    {
        mData.backup();
        mData->mEnabledEhci = aEnabled;

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

    /** @todo This is no longer correct */
    *aUSBStandard = 0x0101;

    return S_OK;
}

#ifndef VBOX_WITH_USB
/**
 * Fake class for build without USB.
 * We need an empty collection & enum for deviceFilters, that's all.
 */
class ATL_NO_VTABLE USBDeviceFilter : public VirtualBoxBaseNEXT, public IUSBDeviceFilter
{
public:
    DECLARE_NOT_AGGREGATABLE(USBDeviceFilter)
    DECLARE_PROTECT_FINAL_CONSTRUCT()
    BEGIN_COM_MAP(USBDeviceFilter)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IUSBDeviceFilter)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (USBDeviceFilter)

    // IUSBDeviceFilter properties
    STDMETHOD(COMGETTER(Name)) (BSTR *aName);
    STDMETHOD(COMSETTER(Name)) (INPTR BSTR aName);
    STDMETHOD(COMGETTER(Active)) (BOOL *aActive);
    STDMETHOD(COMSETTER(Active)) (BOOL aActive);
    STDMETHOD(COMGETTER(VendorId)) (BSTR *aVendorId);
    STDMETHOD(COMSETTER(VendorId)) (INPTR BSTR aVendorId);
    STDMETHOD(COMGETTER(ProductId)) (BSTR *aProductId);
    STDMETHOD(COMSETTER(ProductId)) (INPTR BSTR aProductId);
    STDMETHOD(COMGETTER(Revision)) (BSTR *aRevision);
    STDMETHOD(COMSETTER(Revision)) (INPTR BSTR aRevision);
    STDMETHOD(COMGETTER(Manufacturer)) (BSTR *aManufacturer);
    STDMETHOD(COMSETTER(Manufacturer)) (INPTR BSTR aManufacturer);
    STDMETHOD(COMGETTER(Product)) (BSTR *aProduct);
    STDMETHOD(COMSETTER(Product)) (INPTR BSTR aProduct);
    STDMETHOD(COMGETTER(SerialNumber)) (BSTR *aSerialNumber);
    STDMETHOD(COMSETTER(SerialNumber)) (INPTR BSTR aSerialNumber);
    STDMETHOD(COMGETTER(Port)) (BSTR *aPort);
    STDMETHOD(COMSETTER(Port)) (INPTR BSTR aPort);
    STDMETHOD(COMGETTER(Remote)) (BSTR *aRemote);
    STDMETHOD(COMSETTER(Remote)) (INPTR BSTR aRemote);
    STDMETHOD(COMGETTER(MaskedInterfaces)) (ULONG *aMaskedIfs);
    STDMETHOD(COMSETTER(MaskedInterfaces)) (ULONG aMaskedIfs);
};
COM_DECL_READONLY_ENUM_AND_COLLECTION (USBDeviceFilter);
COM_IMPL_READONLY_ENUM_AND_COLLECTION (USBDeviceFilter);
#endif /* !VBOX_WITH_USB */


STDMETHODIMP USBController::COMGETTER(DeviceFilters) (IUSBDeviceFilterCollection **aDevicesFilters)
{
    if (!aDevicesFilters)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    ComObjPtr <USBDeviceFilterCollection> collection;
    collection.createObject();
#ifdef VBOX_WITH_USB
    collection->init (*mDeviceFilters.data());
#endif
    collection.queryInterfaceTo (aDevicesFilters);

    return S_OK;
}

// IUSBController methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP USBController::CreateDeviceFilter (INPTR BSTR aName,
                                                IUSBDeviceFilter **aFilter)
{
#ifdef VBOX_WITH_USB
    if (!aFilter)
        return E_POINTER;

    if (!aName || *aName == 0)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    ComObjPtr <USBDeviceFilter> filter;
    filter.createObject();
    HRESULT rc = filter->init (this, aName);
    ComAssertComRCRetRC (rc);
    rc = filter.queryInterfaceTo (aFilter);
    AssertComRCReturnRC (rc);

    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

STDMETHODIMP USBController::InsertDeviceFilter (ULONG aPosition,
                                                IUSBDeviceFilter *aFilter)
{
#ifdef VBOX_WITH_USB
    if (!aFilter)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

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
        filter->id() = service->insertFilter (&filter->data().mUSBFilter);
    }

    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

STDMETHODIMP USBController::RemoveDeviceFilter (ULONG aPosition,
                                                IUSBDeviceFilter **aFilter)
{
#ifdef VBOX_WITH_USB
    if (!aFilter)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

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
#else
    return E_NOTIMPL;
#endif
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 *  Loads settings from the given machine node.
 *  May be called once right after this object creation.
 *
 *  @param aMachineNode <Machine> node.
 *
 *  @note Locks this object for writing.
 */
HRESULT USBController::loadSettings (const settings::Key &aMachineNode)
{
    using namespace settings;

    AssertReturn (!aMachineNode.isNull(), E_FAIL);

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /* Note: we assume that the default values for attributes of optional
     * nodes are assigned in the Data::Data() constructor and don't do it
     * here. It implies that this method may only be called after constructing
     * a new BIOSSettings object while all its data fields are in the default
     * values. Exceptions are fields whose creation time defaults don't match
     * values that should be applied when these fields are not explicitly set
     * in the settings file (for backwards compatibility reasons). This takes
     * place when a setting of a newly created object must default to A while
     * the same setting of an object loaded from the old settings file must
     * default to B. */

    /* USB Controller node (required) */
    Key controller = aMachineNode.key ("USBController");

    /* enabled (required) */
    mData->mEnabled = controller.value <bool> ("enabled");

    /* enabledEhci (optiona, defaults to false) */
    mData->mEnabledEhci = controller.value <bool> ("enabledEhci");

#ifdef VBOX_WITH_USB
    HRESULT rc = S_OK;

    Key::List children = controller.keys ("DeviceFilter");
    for (Key::List::const_iterator it = children.begin();
         it != children.end(); ++ it)
    {
        /* required */
        Bstr name = (*it).stringValue ("name");
        bool active = (*it).value <bool> ("active");

        /* optional */
        Bstr vendorId = (*it).stringValue ("vendorId");
        Bstr productId = (*it).stringValue ("productId");
        Bstr revision = (*it).stringValue ("revision");
        Bstr manufacturer = (*it).stringValue ("manufacturer");
        Bstr product = (*it).stringValue ("product");
        Bstr serialNumber = (*it).stringValue ("serialNumber");
        Bstr port = (*it).stringValue ("port");
        Bstr remote = (*it).stringValue ("remote");
        ULONG maskedIfs = (*it).value <ULONG> ("maskedInterfaces");

        ComObjPtr <USBDeviceFilter> filterObj;
        filterObj.createObject();
        rc = filterObj->init (this,
                              name, active, vendorId, productId, revision,
                              manufacturer, product, serialNumber,
                              port, remote, maskedIfs);
        /* error info is set by init() when appropriate */
        CheckComRCReturnRC (rc);

        mDeviceFilters->push_back (filterObj);
        filterObj->mInList = true;
    }
#endif /* VBOX_WITH_USB */

    return S_OK;
}

/**
 *  Saves settings to the given machine node.
 *
 *  @param aMachineNode <Machine> node.
 *
 *  @note Locks this object for reading.
 */
HRESULT USBController::saveSettings (settings::Key &aMachineNode)
{
    using namespace settings;

    AssertReturn (!aMachineNode.isNull(), E_FAIL);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    /* first, delete the entry */
    Key controller = aMachineNode.findKey ("USBController");
#ifdef VBOX_WITH_USB
    if (!controller.isNull())
        controller.zap();
    /* then, recreate it */
    controller = aMachineNode.createKey ("USBController");
#else
    /* don't zap it. */
    if (controller.isNull())
        controller = aMachineNode.createKey ("USBController");
#endif

    /* enabled */
    controller.setValue <bool> ("enabled", !!mData->mEnabled);

    /* enabledEhci */
    controller.setValue <bool> ("enabledEhci", !!mData->mEnabledEhci);

#ifdef VBOX_WITH_USB
    DeviceFilterList::const_iterator it = mDeviceFilters->begin();
    while (it != mDeviceFilters->end())
    {
        AutoWriteLock filterLock (*it);
        const USBDeviceFilter::Data &data = (*it)->data();

        Key filter = controller.appendKey ("DeviceFilter");

        filter.setValue <Bstr> ("name", data.mName);
        filter.setValue <bool> ("active", !!data.mActive);

        /* all are optional */
        Bstr str;
        (*it)->COMGETTER (VendorId) (str.asOutParam());
        if (!str.isNull())
            filter.setValue <Bstr> ("vendorId", str);

        (*it)->COMGETTER (ProductId) (str.asOutParam());
        if (!str.isNull())
            filter.setValue <Bstr> ("productId", str);

        (*it)->COMGETTER (Revision) (str.asOutParam());
        if (!str.isNull())
            filter.setValue <Bstr> ("revision", str);

        (*it)->COMGETTER (Manufacturer) (str.asOutParam());
        if (!str.isNull())
            filter.setValue <Bstr> ("manufacturer", str);

        (*it)->COMGETTER (Product) (str.asOutParam());
        if (!str.isNull())
            filter.setValue <Bstr> ("product", str);

        (*it)->COMGETTER (SerialNumber) (str.asOutParam());
        if (!str.isNull())
            filter.setValue <Bstr> ("serialNumber", str);

        (*it)->COMGETTER (Port) (str.asOutParam());
        if (!str.isNull())
            filter.setValue <Bstr> ("port", str);

        if (data.mRemote.string())
            filter.setValue <Bstr> ("remote", data.mRemote.string());

        if (data.mMaskedIfs)
            filter.setValue <ULONG> ("maskedInterfaces", data.mMaskedIfs);

        ++ it;
    }
#endif /* VBOX_WITH_USB */

    return S_OK;
}

/** @note Locks objects for reading! */
bool USBController::isModified()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReadLock alock (this);

    if (mData.isBackedUp()
#ifdef VBOX_WITH_USB
        || mDeviceFilters.isBackedUp()
#endif
        )
        return true;

#ifdef VBOX_WITH_USB
    /* see whether any of filters has changed its data */
    for (DeviceFilterList::const_iterator
         it = mDeviceFilters->begin();
         it != mDeviceFilters->end();
         ++ it)
    {
        if ((*it)->isModified())
            return true;
    }
#endif /* VBOX_WITH_USB */

    return false;
}

/** @note Locks objects for reading! */
bool USBController::isReallyModified()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReadLock alock (this);

    if (mData.hasActualChanges())
        return true;

#ifdef VBOX_WITH_USB
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
#endif /* VBOX_WITH_USB */

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

    AutoWriteLock alock (this);

    bool dataChanged = false;

    if (mData.isBackedUp())
    {
        /* we need to check all data to see whether anything will be changed
         * after rollback */
        dataChanged = mData.hasActualChanges();
        mData.rollback();
    }

#ifdef VBOX_WITH_USB
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
                        flt->id() = service->insertFilter (&flt->data().mUSBFilter);
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
#endif /* VBOX_WITH_USB */

    return dataChanged;
}

/**
 *  @note Locks this object for writing, together with the peer object (also
 *  for writing) if there is one.
 */
void USBController::commit()
{
    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller (mPeer);
    AssertComRCReturnVoid (peerCaller.rc());

    /* lock both for writing since we modify both (mPeer is "master" so locked
     * first) */
    AutoMultiWriteLock2 alock (mPeer, this);

    if (mData.isBackedUp())
    {
        mData.commit();
        if (mPeer)
        {
            /* attach new data to the peer and reshare it */
            AutoWriteLock peerlock (mPeer);
            mPeer->mData.attach (mData);
        }
    }

#ifdef VBOX_WITH_USB
    bool commitFilters = false;

    if (mDeviceFilters.isBackedUp())
    {
        mDeviceFilters.commit();

        /* apply changes to peer */
        if (mPeer)
        {
            AutoWriteLock peerlock (mPeer);
            /* commit all changes to new filters (this will reshare data with
             * peers for those who have peers) */
            DeviceFilterList *newList = new DeviceFilterList();
            DeviceFilterList::const_iterator it = mDeviceFilters->begin();
            while (it != mDeviceFilters->end())
            {
                (*it)->commit();

                /* look if this filter has a peer filter */
                ComObjPtr <USBDeviceFilter> peer = (*it)->peer();
                if (!peer)
                {
                    /* no peer means the filter is a newly created one;
                     * create a peer owning data this filter share it with */
                    peer.createObject();
                    peer->init (mPeer, *it, true /* aReshare */);
                }
                else
                {
                    /* remove peer from the old list */
                    mPeer->mDeviceFilters->remove (peer);
                }
                /* and add it to the new list */
                newList->push_back (peer);

                ++ it;
            }

            /* uninit old peer's filters that are left */
            it = mPeer->mDeviceFilters->begin();
            while (it != mPeer->mDeviceFilters->end())
            {
                (*it)->uninit();
                ++ it;
            }

            /* attach new list of filters to our peer */
            mPeer->mDeviceFilters.attach (newList);
        }
        else
        {
            /* we have no peer (our parent is the newly created machine);
             * just commit changes to filters */
            commitFilters = true;
        }
    }
    else
    {
        /* the list of filters itself is not changed,
         * just commit changes to filters themselves */
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
#endif /* VBOX_WITH_USB */
}

/**
 *  @note Locks this object for writing, together with the peer object
 *  represented by @a aThat (locked for reading).
 */
void USBController::copyFrom (USBController *aThat)
{
    AssertReturnVoid (aThat != NULL);

    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (aThat);
    AssertComRCReturnVoid (thatCaller.rc());

    /* peer is not modified, lock it for reading (aThat is "master" so locked
     * first) */
    AutoMultiLock2 alock (aThat->rlock(), this->wlock());

    if (mParent->isRegistered())
    {
        /* reuse onMachineRegistered to tell USB proxy to remove all current
           filters */
        HRESULT rc = onMachineRegistered (FALSE);
        AssertComRCReturn (rc, (void) 0);
    }

    /* this will back up current data */
    mData.assignCopy (aThat->mData);

#ifdef VBOX_WITH_USB
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
#endif /* VBOX_WITH_USB */

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

#ifdef VBOX_WITH_USB

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
                aFilter->id() = service->insertFilter (&aFilter->data().mUSBFilter);
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
                aFilter->id() = service->insertFilter (&aFilter->data().mUSBFilter);
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
bool USBController::hasMatchingFilter (const ComObjPtr <HostUSBDevice> &aDevice, ULONG *aMaskedIfs)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReadLock alock (this);

    /* Disabled USB controllers cannot actually work with USB devices */
    if (!mData->mEnabled)
        return false;

    /* apply self filters */
    for (DeviceFilterList::const_iterator it = mDeviceFilters->begin();
         it != mDeviceFilters->end();
         ++ it)
    {
        AutoWriteLock filterLock (*it);
        if (aDevice->isMatch ((*it)->data()))
        {
            *aMaskedIfs = (*it)->data().mMaskedIfs;
            return true;
        }
    }

    return false;
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
bool USBController::hasMatchingFilter (IUSBDevice *aUSBDevice, ULONG *aMaskedIfs)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReadLock alock (this);

    /* Disabled USB controllers cannot actually work with USB devices */
    if (!mData->mEnabled)
        return false;

    HRESULT rc = S_OK;

    /* query fields */
    USBFILTER dev;
    USBFilterInit (&dev, USBFILTERTYPE_CAPTURE);

    USHORT vendorId = 0;
    rc = aUSBDevice->COMGETTER(VendorId) (&vendorId);
    ComAssertComRCRet (rc, false);
    ComAssertRet (vendorId, false);
    int vrc = USBFilterSetNumExact (&dev, USBFILTERIDX_VENDOR_ID, vendorId, true); AssertRC(vrc);

    USHORT productId = 0;
    rc = aUSBDevice->COMGETTER(ProductId) (&productId);
    ComAssertComRCRet (rc, false);
    ComAssertRet (productId, false);
    vrc = USBFilterSetNumExact (&dev, USBFILTERIDX_PRODUCT_ID, productId, true); AssertRC(vrc);

    USHORT revision;
    rc = aUSBDevice->COMGETTER(Revision) (&revision);
    ComAssertComRCRet (rc, false);
    vrc = USBFilterSetNumExact (&dev, USBFILTERIDX_DEVICE, revision, true); AssertRC(vrc);

    Bstr manufacturer;
    rc = aUSBDevice->COMGETTER(Manufacturer) (manufacturer.asOutParam());
    ComAssertComRCRet (rc, false);
    if (!manufacturer.isNull())
        USBFilterSetStringExact (&dev, USBFILTERIDX_MANUFACTURER_STR, Utf8Str(manufacturer), true);

    Bstr product;
    rc = aUSBDevice->COMGETTER(Product) (product.asOutParam());
    ComAssertComRCRet (rc, false);
    if (!product.isNull())
        USBFilterSetStringExact (&dev, USBFILTERIDX_PRODUCT_STR, Utf8Str(product), true);

    Bstr serialNumber;
    rc = aUSBDevice->COMGETTER(SerialNumber) (serialNumber.asOutParam());
    ComAssertComRCRet (rc, false);
    if (!serialNumber.isNull())
        USBFilterSetStringExact (&dev, USBFILTERIDX_SERIAL_NUMBER_STR, Utf8Str(serialNumber), true);

    Bstr address;
    rc = aUSBDevice->COMGETTER(Address) (address.asOutParam());
    ComAssertComRCRet (rc, false);

    USHORT port = 0;
    rc = aUSBDevice->COMGETTER(Port)(&port);
    ComAssertComRCRet (rc, false);
    USBFilterSetNumExact (&dev, USBFILTERIDX_PORT, port, true);

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
        AutoWriteLock filterLock (*it);
        const USBDeviceFilter::Data &aData = (*it)->data();

        if (!aData.mActive)
            continue;
        if (!aData.mRemote.isMatch (remote))
            continue;
        if (!USBFilterMatch (&aData.mUSBFilter, &dev))
            continue;

        match = true;
        *aMaskedIfs = aData.mMaskedIfs;
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

    AutoReadLock alock (this);

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
                flt->id() = service->insertFilter (&flt->data().mUSBFilter);
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

#endif /* VBOX_WITH_USB */

// private methods
/////////////////////////////////////////////////////////////////////////////

