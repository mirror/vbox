/* $Id$ */
/** @file
 * Implementation of IUSBController.
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

#include "USBControllerImpl.h"

#include "Global.h"
#include "MachineImpl.h"
#include "VirtualBoxImpl.h"
#include "HostImpl.h"
#ifdef VBOX_WITH_USB
# include "USBDeviceImpl.h"
# include "HostUSBDeviceImpl.h"
# include "USBProxyService.h"
# include "USBDeviceFilterImpl.h"
#endif

#include <iprt/string.h>
#include <iprt/cpp/utils.h>

#include <VBox/err.h>
#include <VBox/settings.h>

#include <algorithm>

#include "AutoStateDep.h"
#include "AutoCaller.h"
#include "Logging.h"

// defines
/////////////////////////////////////////////////////////////////////////////

typedef std::list< ComObjPtr<USBDeviceFilter> > DeviceFilterList;

struct BackupableUSBData
{
    BackupableUSBData()
        : fEnabled(false),
          fEnabledEHCI(false)
    { }

    BOOL fEnabled;
    BOOL fEnabledEHCI;
};

struct USBController::Data
{
    Data(Machine *pMachine)
        : pParent(pMachine),
          pHost(pMachine->getVirtualBox()->host())
    { }

    ~Data()
    {};

    Machine * const                 pParent;
    Host * const                    pHost;

    // peer machine's USB controller
    const ComObjPtr<USBController>  pPeer;

    Backupable<BackupableUSBData>   bd;
#ifdef VBOX_WITH_USB
    // the following fields need special backup/rollback/commit handling,
    // so they cannot be a part of BackupableData
    Backupable<DeviceFilterList>    llDeviceFilters;
#endif
};



// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (USBController)

HRESULT USBController::FinalConstruct()
{
    return BaseFinalConstruct();
}

void USBController::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the USB controller object.
 *
 * @returns COM result indicator.
 * @param aParent       Pointer to our parent object.
 */
HRESULT USBController::init(Machine *aParent)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    ComAssertRet(aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data(aParent);

    /* mPeer is left null */

    m->bd.allocate();
#ifdef VBOX_WITH_USB
    m->llDeviceFilters.allocate();
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
HRESULT USBController::init(Machine *aParent, USBController *aPeer)
{
    LogFlowThisFunc(("aParent=%p, aPeer=%p\n", aParent, aPeer));

    ComAssertRet(aParent && aPeer, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data(aParent);

    unconst(m->pPeer) = aPeer;

    AutoWriteLock thatlock(aPeer COMMA_LOCKVAL_SRC_POS);
    m->bd.share(aPeer->m->bd);

#ifdef VBOX_WITH_USB
    /* create copies of all filters */
    m->llDeviceFilters.allocate();
    DeviceFilterList::const_iterator it = aPeer->m->llDeviceFilters->begin();
    while (it != aPeer->m->llDeviceFilters->end())
    {
        ComObjPtr<USBDeviceFilter> filter;
        filter.createObject();
        filter->init(this, *it);
        m->llDeviceFilters->push_back(filter);
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
HRESULT USBController::initCopy(Machine *aParent, USBController *aPeer)
{
    LogFlowThisFunc(("aParent=%p, aPeer=%p\n", aParent, aPeer));

    ComAssertRet(aParent && aPeer, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data(aParent);

    /* mPeer is left null */

    AutoWriteLock thatlock(aPeer COMMA_LOCKVAL_SRC_POS);
    m->bd.attachCopy(aPeer->m->bd);

#ifdef VBOX_WITH_USB
    /* create private copies of all filters */
    m->llDeviceFilters.allocate();
    DeviceFilterList::const_iterator it = aPeer->m->llDeviceFilters->begin();
    while (it != aPeer->m->llDeviceFilters->end())
    {
        ComObjPtr<USBDeviceFilter> filter;
        filter.createObject();
        filter->initCopy(this, *it);
        m->llDeviceFilters->push_back(filter);
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
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

#ifdef VBOX_WITH_USB
    // uninit all device filters on the list (it's a standard std::list not an ObjectsList
    // so we must uninit() manually)
    for (DeviceFilterList::iterator it = m->llDeviceFilters->begin();
         it != m->llDeviceFilters->end();
         ++it)
        (*it)->uninit();

    m->llDeviceFilters.free();
#endif
    m->bd.free();

    unconst(m->pPeer) = NULL;
    unconst(m->pParent) = NULL;

    delete m;
    m = NULL;
}


// IUSBController properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP USBController::COMGETTER(Enabled) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aEnabled = m->bd->fEnabled;

    return S_OK;
}


STDMETHODIMP USBController::COMSETTER(Enabled) (BOOL aEnabled)
{
    LogFlowThisFunc(("aEnabled=%RTbool\n", aEnabled));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd->fEnabled != aEnabled)
    {
        m->bd.backup();
        m->bd->fEnabled = aEnabled;

        // leave the lock for safety
        alock.release();

        AutoWriteLock mlock(m->pParent COMMA_LOCKVAL_SRC_POS);
        m->pParent->setModified(Machine::IsModified_USB);
        mlock.release();

        m->pParent->onUSBControllerChange();
    }

    return S_OK;
}

STDMETHODIMP USBController::COMGETTER(EnabledEhci) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aEnabled = m->bd->fEnabledEHCI;

    return S_OK;
}

STDMETHODIMP USBController::COMSETTER(EnabledEhci) (BOOL aEnabled)
{
    LogFlowThisFunc(("aEnabled=%RTbool\n", aEnabled));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd->fEnabledEHCI != aEnabled)
    {
        m->bd.backup();
        m->bd->fEnabledEHCI = aEnabled;

        // leave the lock for safety
        alock.release();

        AutoWriteLock mlock(m->pParent COMMA_LOCKVAL_SRC_POS);
        m->pParent->setModified(Machine::IsModified_USB);
        mlock.release();

        m->pParent->onUSBControllerChange();
    }

    return S_OK;
}

STDMETHODIMP USBController::COMGETTER(ProxyAvailable) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

#ifdef VBOX_WITH_USB
    *aEnabled = true;
#else
    *aEnabled = false;
#endif

    return S_OK;
}

STDMETHODIMP USBController::COMGETTER(USBStandard) (USHORT *aUSBStandard)
{
    CheckComArgOutPointerValid(aUSBStandard);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

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
class ATL_NO_VTABLE USBDeviceFilter :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IUSBDeviceFilter)
{
public:
    DECLARE_NOT_AGGREGATABLE(USBDeviceFilter)
    DECLARE_PROTECT_FINAL_CONSTRUCT()
    BEGIN_COM_MAP(USBDeviceFilter)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IUSBDeviceFilter)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (USBDeviceFilter)

    // IUSBDeviceFilter properties
    STDMETHOD(COMGETTER(Name)) (BSTR *aName);
    STDMETHOD(COMSETTER(Name)) (IN_BSTR aName);
    STDMETHOD(COMGETTER(Active)) (BOOL *aActive);
    STDMETHOD(COMSETTER(Active)) (BOOL aActive);
    STDMETHOD(COMGETTER(VendorId)) (BSTR *aVendorId);
    STDMETHOD(COMSETTER(VendorId)) (IN_BSTR aVendorId);
    STDMETHOD(COMGETTER(ProductId)) (BSTR *aProductId);
    STDMETHOD(COMSETTER(ProductId)) (IN_BSTR aProductId);
    STDMETHOD(COMGETTER(Revision)) (BSTR *aRevision);
    STDMETHOD(COMSETTER(Revision)) (IN_BSTR aRevision);
    STDMETHOD(COMGETTER(Manufacturer)) (BSTR *aManufacturer);
    STDMETHOD(COMSETTER(Manufacturer)) (IN_BSTR aManufacturer);
    STDMETHOD(COMGETTER(Product)) (BSTR *aProduct);
    STDMETHOD(COMSETTER(Product)) (IN_BSTR aProduct);
    STDMETHOD(COMGETTER(SerialNumber)) (BSTR *aSerialNumber);
    STDMETHOD(COMSETTER(SerialNumber)) (IN_BSTR aSerialNumber);
    STDMETHOD(COMGETTER(Port)) (BSTR *aPort);
    STDMETHOD(COMSETTER(Port)) (IN_BSTR aPort);
    STDMETHOD(COMGETTER(Remote)) (BSTR *aRemote);
    STDMETHOD(COMSETTER(Remote)) (IN_BSTR aRemote);
    STDMETHOD(COMGETTER(MaskedInterfaces)) (ULONG *aMaskedIfs);
    STDMETHOD(COMSETTER(MaskedInterfaces)) (ULONG aMaskedIfs);
};
#endif /* !VBOX_WITH_USB */


STDMETHODIMP USBController::COMGETTER(DeviceFilters) (ComSafeArrayOut(IUSBDeviceFilter *, aDevicesFilters))
{
#ifdef VBOX_WITH_USB
    CheckComArgOutSafeArrayPointerValid(aDevicesFilters);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    SafeIfaceArray<IUSBDeviceFilter> collection (*m->llDeviceFilters.data());
    collection.detachTo(ComSafeArrayOutArg(aDevicesFilters));

    return S_OK;
#else
    NOREF(aDevicesFilters);
# ifndef RT_OS_WINDOWS
    NOREF(aDevicesFiltersSize);
# endif
    ReturnComNotImplemented();
#endif
}

// IUSBController methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP USBController::CreateDeviceFilter (IN_BSTR aName,
                                                IUSBDeviceFilter **aFilter)
{
#ifdef VBOX_WITH_USB
    CheckComArgOutPointerValid(aFilter);

    CheckComArgStrNotEmptyOrNull(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    ComObjPtr<USBDeviceFilter> filter;
    filter.createObject();
    HRESULT rc = filter->init (this, aName);
    ComAssertComRCRetRC (rc);
    rc = filter.queryInterfaceTo(aFilter);
    AssertComRCReturnRC(rc);

    return S_OK;
#else
    NOREF(aName);
    NOREF(aFilter);
    ReturnComNotImplemented();
#endif
}

STDMETHODIMP USBController::InsertDeviceFilter(ULONG aPosition,
                                               IUSBDeviceFilter *aFilter)
{
#ifdef VBOX_WITH_USB

    CheckComArgNotNull(aFilter);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    ComObjPtr<USBDeviceFilter> filter = static_cast<USBDeviceFilter*>(aFilter);
    // @todo r=dj make sure the input object is actually from us
//     if (!filter)
//         return setError (E_INVALIDARG,
//             tr ("The given USB device filter is not created within "
//                 "this VirtualBox instance"));

    if (filter->mInList)
        return setError(VBOX_E_INVALID_OBJECT_STATE,
                        tr("The given USB device filter is already in the list"));

    /* backup the list before modification */
    m->llDeviceFilters.backup();

    /* iterate to the position... */
    DeviceFilterList::iterator it;
    if (aPosition < m->llDeviceFilters->size())
    {
        it = m->llDeviceFilters->begin();
        std::advance (it, aPosition);
    }
    else
        it = m->llDeviceFilters->end();
    /* ...and insert */
    m->llDeviceFilters->insert (it, filter);
    filter->mInList = true;

    /* notify the proxy (only when it makes sense) */
    if (filter->getData().mActive && Global::IsOnline(adep.machineState())
        && filter->getData().mRemote.isMatch (false))
    {
        USBProxyService *service = m->pHost->usbProxyService();
        ComAssertRet(service, E_FAIL);

        ComAssertRet(filter->getId() == NULL, E_FAIL);
        filter->getId() = service->insertFilter (&filter->getData().mUSBFilter);
    }

    alock.release();
    AutoWriteLock mlock(m->pParent COMMA_LOCKVAL_SRC_POS);
    m->pParent->setModified(Machine::IsModified_USB);
    mlock.release();

    return S_OK;

#else /* VBOX_WITH_USB */

    NOREF(aPosition);
    NOREF(aFilter);
    ReturnComNotImplemented();

#endif /* VBOX_WITH_USB */
}

STDMETHODIMP USBController::RemoveDeviceFilter(ULONG aPosition,
                                               IUSBDeviceFilter **aFilter)
{
#ifdef VBOX_WITH_USB

    CheckComArgOutPointerValid(aFilter);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!m->llDeviceFilters->size())
        return setError(E_INVALIDARG,
                        tr("The USB device filter list is empty"));

    if (aPosition >= m->llDeviceFilters->size())
        return setError(E_INVALIDARG,
                        tr("Invalid position: %lu (must be in range [0, %lu])"),
                        aPosition, m->llDeviceFilters->size() - 1);

    /* backup the list before modification */
    m->llDeviceFilters.backup();

    ComObjPtr<USBDeviceFilter> filter;
    {
        /* iterate to the position... */
        DeviceFilterList::iterator it = m->llDeviceFilters->begin();
        std::advance (it, aPosition);
        /* ...get an element from there... */
        filter = *it;
        /* ...and remove */
        filter->mInList = false;
        m->llDeviceFilters->erase (it);
    }

    /* cancel sharing (make an independent copy of data) */
    filter->unshare();

    filter.queryInterfaceTo(aFilter);

    /* notify the proxy (only when it makes sense) */
    if (filter->getData().mActive && Global::IsOnline(adep.machineState())
        && filter->getData().mRemote.isMatch (false))
    {
        USBProxyService *service = m->pHost->usbProxyService();
        ComAssertRet(service, E_FAIL);

        ComAssertRet(filter->getId() != NULL, E_FAIL);
        service->removeFilter(filter->getId());
        filter->getId() = NULL;
    }

    alock.release();
    AutoWriteLock mlock(m->pParent COMMA_LOCKVAL_SRC_POS);
    m->pParent->setModified(Machine::IsModified_USB);
    mlock.release();

    return S_OK;

#else /* VBOX_WITH_USB */

    NOREF(aPosition);
    NOREF(aFilter);
    ReturnComNotImplemented();

#endif /* VBOX_WITH_USB */
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 *  Loads settings from the given machine node.
 *  May be called once right after this object creation.
 *
 *  @param aMachineNode <Machine> node.
 *
 *  @note Does not lock "this" as Machine::loadHardware, which calls this, does not lock either.
 */
HRESULT USBController::loadSettings(const settings::USBController &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

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

    m->bd->fEnabled = data.fEnabled;
    m->bd->fEnabledEHCI = data.fEnabledEHCI;

#ifdef VBOX_WITH_USB
    for (settings::USBDeviceFiltersList::const_iterator it = data.llDeviceFilters.begin();
         it != data.llDeviceFilters.end();
         ++it)
    {
        const settings::USBDeviceFilter &f = *it;
        ComObjPtr<USBDeviceFilter> pFilter;
        pFilter.createObject();
        HRESULT rc = pFilter->init(this,        // parent
                                   f);
        if (FAILED(rc)) return rc;

        m->llDeviceFilters->push_back(pFilter);
        pFilter->mInList = true;
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
HRESULT USBController::saveSettings(settings::USBController &data)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    data.fEnabled = !!m->bd->fEnabled;
    data.fEnabledEHCI = !!m->bd->fEnabledEHCI;

#ifdef VBOX_WITH_USB
    data.llDeviceFilters.clear();

    for (DeviceFilterList::const_iterator it = m->llDeviceFilters->begin();
         it != m->llDeviceFilters->end();
         ++it)
    {
        AutoWriteLock filterLock(*it COMMA_LOCKVAL_SRC_POS);
        const USBDeviceFilter::Data &filterData = (*it)->getData();

        Bstr str;

        settings::USBDeviceFilter f;
        f.strName = filterData.mName;
        f.fActive = !!filterData.mActive;
        (*it)->COMGETTER(VendorId)(str.asOutParam());
        f.strVendorId = str;
        (*it)->COMGETTER(ProductId)(str.asOutParam());
        f.strProductId = str;
        (*it)->COMGETTER (Revision) (str.asOutParam());
        f.strRevision = str;
        (*it)->COMGETTER (Manufacturer) (str.asOutParam());
        f.strManufacturer = str;
        (*it)->COMGETTER (Product) (str.asOutParam());
        f.strProduct = str;
        (*it)->COMGETTER (SerialNumber) (str.asOutParam());
        f.strSerialNumber = str;
        (*it)->COMGETTER (Port) (str.asOutParam());
        f.strPort = str;
        f.strRemote = filterData.mRemote.string();
        f.ulMaskedInterfaces = filterData.mMaskedIfs;

        data.llDeviceFilters.push_back(f);
    }
#endif /* VBOX_WITH_USB */

    return S_OK;
}

/** @note Locks objects for writing! */
void USBController::rollback()
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* we need the machine state */
    AutoAnyStateDependency adep(m->pParent);
    AssertComRCReturnVoid(adep.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.rollback();

#ifdef VBOX_WITH_USB

    if (m->llDeviceFilters.isBackedUp())
    {
        USBProxyService *service = m->pHost->usbProxyService();
        Assert(service);

        /* uninitialize all new filters (absent in the backed up list) */
        DeviceFilterList::const_iterator it = m->llDeviceFilters->begin();
        DeviceFilterList *backedList = m->llDeviceFilters.backedUpData();
        while (it != m->llDeviceFilters->end())
        {
            if (std::find (backedList->begin(), backedList->end(), *it) ==
                backedList->end())
            {
                /* notify the proxy (only when it makes sense) */
                if ((*it)->getData().mActive &&
                    Global::IsOnline (adep.machineState())
                    && (*it)->getData().mRemote.isMatch (false))
                {
                    USBDeviceFilter *filter = *it;
                    Assert(filter->getId() != NULL);
                    service->removeFilter(filter->getId());
                    filter->getId() = NULL;
                }

                (*it)->uninit();
            }
            ++ it;
        }

        if (Global::IsOnline (adep.machineState()))
        {
            /* find all removed old filters (absent in the new list)
             * and insert them back to the USB proxy */
            it = backedList->begin();
            while (it != backedList->end())
            {
                if (std::find (m->llDeviceFilters->begin(), m->llDeviceFilters->end(), *it) ==
                    m->llDeviceFilters->end())
                {
                    /* notify the proxy (only when necessary) */
                    if ((*it)->getData().mActive
                            && (*it)->getData().mRemote.isMatch (false))
                    {
                        USBDeviceFilter *flt = *it; /* resolve ambiguity */
                        Assert(flt->getId() == NULL);
                        flt->getId() = service->insertFilter(&flt->getData().mUSBFilter);
                    }
                }
                ++ it;
            }
        }

        /* restore the list */
        m->llDeviceFilters.rollback();
    }

    /* here we don't depend on the machine state any more */
    adep.release();

    /* rollback any changes to filters after restoring the list */
    DeviceFilterList::const_iterator it = m->llDeviceFilters->begin();
    while (it != m->llDeviceFilters->end())
    {
        if ((*it)->isModified())
        {
            (*it)->rollback();
            /* call this to notify the USB proxy about changes */
            onDeviceFilterChange(*it);
        }
        ++it;
    }

#endif /* VBOX_WITH_USB */
}

/**
 *  @note Locks this object for writing, together with the peer object (also
 *  for writing) if there is one.
 */
void USBController::commit()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller(m->pPeer);
    AssertComRCReturnVoid (peerCaller.rc());

    /* lock both for writing since we modify both (mPeer is "master" so locked
     * first) */
    AutoMultiWriteLock2 alock(m->pPeer, this COMMA_LOCKVAL_SRC_POS);

    if (m->bd.isBackedUp())
    {
        m->bd.commit();
        if (m->pPeer)
        {
            /* attach new data to the peer and reshare it */
            AutoWriteLock peerlock(m->pPeer COMMA_LOCKVAL_SRC_POS);
            m->pPeer->m->bd.attach(m->bd);
        }
    }

#ifdef VBOX_WITH_USB
    bool commitFilters = false;

    if (m->llDeviceFilters.isBackedUp())
    {
        m->llDeviceFilters.commit();

        /* apply changes to peer */
        if (m->pPeer)
        {
            AutoWriteLock peerlock(m->pPeer COMMA_LOCKVAL_SRC_POS);

            /* commit all changes to new filters (this will reshare data with
             * peers for those who have peers) */
            DeviceFilterList *newList = new DeviceFilterList();
            DeviceFilterList::const_iterator it = m->llDeviceFilters->begin();
            while (it != m->llDeviceFilters->end())
            {
                (*it)->commit();

                /* look if this filter has a peer filter */
                ComObjPtr<USBDeviceFilter> peer = (*it)->peer();
                if (!peer)
                {
                    /* no peer means the filter is a newly created one;
                     * create a peer owning data this filter share it with */
                    peer.createObject();
                    peer->init(m->pPeer, *it, true /* aReshare */);
                }
                else
                {
                    /* remove peer from the old list */
                    m->pPeer->m->llDeviceFilters->remove(peer);
                }
                /* and add it to the new list */
                newList->push_back (peer);

                ++ it;
            }

            /* uninit old peer's filters that are left */
            it = m->pPeer->m->llDeviceFilters->begin();
            while (it != m->pPeer->m->llDeviceFilters->end())
            {
                (*it)->uninit();
                ++ it;
            }

            /* attach new list of filters to our peer */
            m->pPeer->m->llDeviceFilters.attach(newList);
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
        DeviceFilterList::const_iterator it = m->llDeviceFilters->begin();
        while (it != m->llDeviceFilters->end())
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
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (aThat);
    AssertComRCReturnVoid (thatCaller.rc());

    /* even more sanity */
    AutoAnyStateDependency adep(m->pParent);
    AssertComRCReturnVoid (adep.rc());
    /* Machine::copyFrom() may not be called when the VM is running */
    AssertReturnVoid (!Global::IsOnline (adep.machineState()));

    /* peer is not modified, lock it for reading (aThat is "master" so locked
     * first) */
    AutoReadLock rl(aThat COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);

    /* this will back up current data */
    m->bd.assignCopy(aThat->m->bd);

#ifdef VBOX_WITH_USB

    /* Note that we won't inform the USB proxy about new filters since the VM is
     * not running when we are here and therefore no need to do so */

    /* create private copies of all filters */
    m->llDeviceFilters.backup();
    m->llDeviceFilters->clear();
    for (DeviceFilterList::const_iterator it = aThat->m->llDeviceFilters->begin();
        it != aThat->m->llDeviceFilters->end();
        ++ it)
    {
        ComObjPtr<USBDeviceFilter> filter;
        filter.createObject();
        filter->initCopy (this, *it);
        m->llDeviceFilters->push_back (filter);
    }

#endif /* VBOX_WITH_USB */
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
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    /* we need the machine state */
    AutoAnyStateDependency adep(m->pParent);
    AssertComRCReturnRC(adep.rc());

    /* nothing to do if the machine isn't running */
    if (!Global::IsOnline (adep.machineState()))
        return S_OK;

    /* we don't modify our data fields -- no need to lock */

    if (    aFilter->mInList
         && m->pParent->isRegistered())
    {
        USBProxyService *service = m->pHost->usbProxyService();
        ComAssertRet(service, E_FAIL);

        if (aActiveChanged)
        {
            if (aFilter->getData().mRemote.isMatch (false))
            {
                /* insert/remove the filter from the proxy */
                if (aFilter->getData().mActive)
                {
                    ComAssertRet(aFilter->getId() == NULL, E_FAIL);
                    aFilter->getId() = service->insertFilter(&aFilter->getData().mUSBFilter);
                }
                else
                {
                    ComAssertRet(aFilter->getId() != NULL, E_FAIL);
                    service->removeFilter(aFilter->getId());
                    aFilter->getId() = NULL;
                }
            }
        }
        else
        {
            if (aFilter->getData().mActive)
            {
                /* update the filter in the proxy */
                ComAssertRet(aFilter->getId() != NULL, E_FAIL);
                service->removeFilter(aFilter->getId());
                if (aFilter->getData().mRemote.isMatch (false))
                {
                    aFilter->getId() = service->insertFilter(&aFilter->getData().mUSBFilter);
                }
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
bool USBController::hasMatchingFilter (const ComObjPtr<HostUSBDevice> &aDevice, ULONG *aMaskedIfs)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Disabled USB controllers cannot actually work with USB devices */
    if (!m->bd->fEnabled)
        return false;

    /* apply self filters */
    for (DeviceFilterList::const_iterator it = m->llDeviceFilters->begin();
         it != m->llDeviceFilters->end();
         ++ it)
    {
        AutoWriteLock filterLock(*it COMMA_LOCKVAL_SRC_POS);
        if (aDevice->isMatch((*it)->getData()))
        {
            *aMaskedIfs = (*it)->getData().mMaskedIfs;
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

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Disabled USB controllers cannot actually work with USB devices */
    if (!m->bd->fEnabled)
        return false;

    HRESULT rc = S_OK;

    /* query fields */
    USBFILTER dev;
    USBFilterInit (&dev, USBFILTERTYPE_CAPTURE);

    USHORT vendorId = 0;
    rc = aUSBDevice->COMGETTER(VendorId) (&vendorId);
    ComAssertComRCRet(rc, false);
    ComAssertRet(vendorId, false);
    int vrc = USBFilterSetNumExact (&dev, USBFILTERIDX_VENDOR_ID, vendorId, true); AssertRC(vrc);

    USHORT productId = 0;
    rc = aUSBDevice->COMGETTER(ProductId) (&productId);
    ComAssertComRCRet(rc, false);
    vrc = USBFilterSetNumExact (&dev, USBFILTERIDX_PRODUCT_ID, productId, true); AssertRC(vrc);

    USHORT revision;
    rc = aUSBDevice->COMGETTER(Revision) (&revision);
    ComAssertComRCRet(rc, false);
    vrc = USBFilterSetNumExact (&dev, USBFILTERIDX_DEVICE, revision, true); AssertRC(vrc);

    Bstr manufacturer;
    rc = aUSBDevice->COMGETTER(Manufacturer) (manufacturer.asOutParam());
    ComAssertComRCRet(rc, false);
    if (!manufacturer.isEmpty())
        USBFilterSetStringExact (&dev, USBFILTERIDX_MANUFACTURER_STR, Utf8Str(manufacturer).c_str(), true);

    Bstr product;
    rc = aUSBDevice->COMGETTER(Product) (product.asOutParam());
    ComAssertComRCRet(rc, false);
    if (!product.isEmpty())
        USBFilterSetStringExact (&dev, USBFILTERIDX_PRODUCT_STR, Utf8Str(product).c_str(), true);

    Bstr serialNumber;
    rc = aUSBDevice->COMGETTER(SerialNumber) (serialNumber.asOutParam());
    ComAssertComRCRet(rc, false);
    if (!serialNumber.isEmpty())
        USBFilterSetStringExact (&dev, USBFILTERIDX_SERIAL_NUMBER_STR, Utf8Str(serialNumber).c_str(), true);

    Bstr address;
    rc = aUSBDevice->COMGETTER(Address) (address.asOutParam());
    ComAssertComRCRet(rc, false);

    USHORT port = 0;
    rc = aUSBDevice->COMGETTER(Port)(&port);
    ComAssertComRCRet(rc, false);
    USBFilterSetNumExact (&dev, USBFILTERIDX_PORT, port, true);

    BOOL remote = FALSE;
    rc = aUSBDevice->COMGETTER(Remote)(&remote);
    ComAssertComRCRet(rc, false);
    ComAssertRet(remote == TRUE, false);

    bool match = false;

    /* apply self filters */
    for (DeviceFilterList::const_iterator it = m->llDeviceFilters->begin();
         it != m->llDeviceFilters->end();
         ++ it)
    {
        AutoWriteLock filterLock(*it COMMA_LOCKVAL_SRC_POS);
        const USBDeviceFilter::Data &aData = (*it)->getData();

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

    LogFlowThisFunc(("returns: %d\n", match));
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
    LogFlowThisFunc(("aInsertFilters=%RTbool\n", aInsertFilters));

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    USBProxyService *service = m->pHost->usbProxyService();
    AssertReturn(service, E_FAIL);

    DeviceFilterList::const_iterator it = m->llDeviceFilters->begin();
    while (it != m->llDeviceFilters->end())
    {
        USBDeviceFilter *flt = *it; /* resolve ambiguity (for ComPtr below) */

        /* notify the proxy (only if the filter is active) */
        if (flt->getData().mActive
                && flt->getData().mRemote.isMatch (false) /* and if the filter is NOT remote */
                )
        {
            if (aInsertFilters)
            {
                AssertReturn(flt->getId() == NULL, E_FAIL);
                flt->getId() = service->insertFilter(&flt->getData().mUSBFilter);
            }
            else
            {
                /* It's possible that the given filter was not inserted the proxy
                 * when this method gets called (as a result of an early VM
                 * process crash for example. So, don't assert that ID != NULL. */
                if (flt->getId() != NULL)
                {
                    service->removeFilter(flt->getId());
                    flt->getId() = NULL;
                }
            }
        }
        ++ it;
    }

    return S_OK;
}

Machine* USBController::getMachine()
{
    return m->pParent;
}

#endif /* VBOX_WITH_USB */

// private methods
/////////////////////////////////////////////////////////////////////////////
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
