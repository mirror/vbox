/* $Id$ */
/** @file
 * Implementation of IHost.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_HOSTIMPL
#define ____H_HOSTIMPL

#include "VirtualBoxBase.h"

class HostUSBDeviceFilter;
class USBProxyService;
class SessionMachine;
class Progress;
class PerformanceCollector;

namespace settings
{
    struct Host;
}

#include <list>

class ATL_NO_VTABLE Host :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IHost)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(Host, IHost)

    DECLARE_NOT_AGGREGATABLE(Host)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Host)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IHost)
    END_COM_MAP()

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(VirtualBox *aParent);
    void uninit();

    // IHost properties
    STDMETHOD(COMGETTER(DVDDrives))(ComSafeArrayOut(IMedium *, drives));
    STDMETHOD(COMGETTER(FloppyDrives))(ComSafeArrayOut(IMedium *, drives));
    STDMETHOD(COMGETTER(USBDevices))(ComSafeArrayOut(IHostUSBDevice *, aUSBDevices));
    STDMETHOD(COMGETTER(USBDeviceFilters))(ComSafeArrayOut(IHostUSBDeviceFilter *, aUSBDeviceFilters));
    STDMETHOD(COMGETTER(NetworkInterfaces))(ComSafeArrayOut(IHostNetworkInterface *, aNetworkInterfaces));
    STDMETHOD(COMGETTER(ProcessorCount))(ULONG *count);
    STDMETHOD(COMGETTER(ProcessorOnlineCount))(ULONG *count);
    STDMETHOD(COMGETTER(ProcessorCoreCount))(ULONG *count);
    STDMETHOD(GetProcessorSpeed)(ULONG cpuId, ULONG *speed);
    STDMETHOD(GetProcessorDescription)(ULONG cpuId, BSTR *description);
    STDMETHOD(GetProcessorFeature)(ProcessorFeature_T feature, BOOL *supported);
    STDMETHOD(GetProcessorCPUIDLeaf)(ULONG aCpuId, ULONG aLeaf, ULONG aSubLeaf, ULONG *aValEAX, ULONG *aValEBX, ULONG *aValECX, ULONG *aValEDX);
    STDMETHOD(COMGETTER(MemorySize))(ULONG *size);
    STDMETHOD(COMGETTER(MemoryAvailable))(ULONG *available);
    STDMETHOD(COMGETTER(OperatingSystem))(BSTR *os);
    STDMETHOD(COMGETTER(OSVersion))(BSTR *version);
    STDMETHOD(COMGETTER(UTCTime))(LONG64 *aUTCTime);
    STDMETHOD(COMGETTER(Acceleration3DAvailable))(BOOL *aSupported);

    // IHost methods
    STDMETHOD(CreateHostOnlyNetworkInterface)(IHostNetworkInterface **aHostNetworkInterface,
                                              IProgress **aProgress);
    STDMETHOD(RemoveHostOnlyNetworkInterface)(IN_BSTR aId, IProgress **aProgress);
    STDMETHOD(CreateUSBDeviceFilter)(IN_BSTR aName, IHostUSBDeviceFilter **aFilter);
    STDMETHOD(InsertUSBDeviceFilter)(ULONG aPosition, IHostUSBDeviceFilter *aFilter);
    STDMETHOD(RemoveUSBDeviceFilter)(ULONG aPosition);

    STDMETHOD(FindHostDVDDrive)(IN_BSTR aName, IMedium **aDrive);
    STDMETHOD(FindHostFloppyDrive)(IN_BSTR aName, IMedium **aDrive);
    STDMETHOD(FindHostNetworkInterfaceByName)(IN_BSTR aName, IHostNetworkInterface **networkInterface);
    STDMETHOD(FindHostNetworkInterfaceById)(IN_BSTR id, IHostNetworkInterface **networkInterface);
    STDMETHOD(FindHostNetworkInterfacesOfType)(HostNetworkInterfaceType_T type, ComSafeArrayOut(IHostNetworkInterface *, aNetworkInterfaces));
    STDMETHOD(FindUSBDeviceByAddress)(IN_BSTR aAddress, IHostUSBDevice **aDevice);
    STDMETHOD(FindUSBDeviceById)(IN_BSTR aId, IHostUSBDevice **aDevice);
    STDMETHOD(GenerateMACAddress)(BSTR *aAddress);

    // public methods only for internal purposes

    /**
     * Override of the default locking class to be used for validating lock
     * order with the standard member lock handle.
     */
    virtual VBoxLockingClass getLockingClass() const
    {
        return LOCKCLASS_HOSTOBJECT;
    }

    HRESULT loadSettings(const settings::Host &data);
    HRESULT saveSettings(settings::Host &data);

    HRESULT getDrives(DeviceType_T mediumType, bool fRefresh, MediaList *&pll);
    HRESULT findHostDriveById(DeviceType_T mediumType, const Guid &uuid, bool fRefresh, ComObjPtr<Medium> &pMedium);
    HRESULT findHostDriveByName(DeviceType_T mediumType, const Utf8Str &strLocationFull, bool fRefresh, ComObjPtr<Medium> &pMedium);

#ifdef VBOX_WITH_USB
    typedef std::list< ComObjPtr<HostUSBDeviceFilter> > USBDeviceFilterList;

    /** Must be called from under this object's lock. */
    USBProxyService* usbProxyService();

    HRESULT addChild(HostUSBDeviceFilter *pChild);
    HRESULT removeChild(HostUSBDeviceFilter *pChild);
    VirtualBox* parent();

    HRESULT onUSBDeviceFilterChange(HostUSBDeviceFilter *aFilter, BOOL aActiveChanged = FALSE);
    void getUSBFilters(USBDeviceFilterList *aGlobalFiltes);
    HRESULT checkUSBProxyService();
#endif /* !VBOX_WITH_USB */

    static void generateMACAddress(Utf8Str &mac);

private:

    HRESULT buildDVDDrivesList(MediaList &list);
    HRESULT buildFloppyDrivesList(MediaList &list);
    HRESULT findHostDriveByNameOrId(DeviceType_T mediumType, const Utf8Str &strNameOrId, ComObjPtr<Medium> &pMedium);

#if defined(RT_OS_SOLARIS) && defined(VBOX_USE_LIBHAL)
    bool getDVDInfoFromHal(std::list< ComObjPtr<Medium> > &list);
    bool getFloppyInfoFromHal(std::list< ComObjPtr<Medium> > &list);
#endif

#if defined(RT_OS_SOLARIS)
    void getDVDInfoFromDevTree(std::list< ComObjPtr<Medium> > &list);
    void parseMountTable(char *mountTable, std::list< ComObjPtr<Medium> > &list);
    bool validateDevice(const char *deviceNode, bool isCDROM);
#endif

    HRESULT updateNetIfList();

#ifdef VBOX_WITH_RESOURCE_USAGE_API
    void registerMetrics(PerformanceCollector *aCollector);
    void registerDiskMetrics(PerformanceCollector *aCollector);
    void unregisterMetrics(PerformanceCollector *aCollector);
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

    struct Data;        // opaque data structure, defined in HostImpl.cpp
    Data *m;
};

#endif // ____H_HOSTIMPL

