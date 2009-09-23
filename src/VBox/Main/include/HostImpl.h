/* $Id$ */
/** @file
 * Implemenation of IHost.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ____H_HOSTIMPL
#define ____H_HOSTIMPL

#include "VirtualBoxBase.h"

class HostUSBDeviceFilter;
class USBProxyService;
class VirtualBox;
class SessionMachine;
class Progress;
class PerformanceCollector;
class Medium;

namespace settings
{
    struct Host;
}

#include <list>

class ATL_NO_VTABLE Host :
    public VirtualBoxBaseWithChildren,
    public VirtualBoxSupportErrorInfoImpl<Host, IHost>,
    public VirtualBoxSupportTranslation<Host>,
    VBOX_SCRIPTABLE_IMPL(IHost)
{
public:

    DECLARE_NOT_AGGREGATABLE(Host)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Host)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IHost)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(VirtualBox *aParent);
    void uninit();

    // IHost properties
    STDMETHOD(COMGETTER(DVDDrives))(ComSafeArrayOut (IMedium *, drives));
    STDMETHOD(COMGETTER(FloppyDrives))(ComSafeArrayOut (IMedium *, drives));
    STDMETHOD(COMGETTER(USBDevices))(ComSafeArrayOut (IHostUSBDevice *, aUSBDevices));
    STDMETHOD(COMGETTER(USBDeviceFilters))(ComSafeArrayOut (IHostUSBDeviceFilter *, aUSBDeviceFilters));
    STDMETHOD(COMGETTER(NetworkInterfaces))(ComSafeArrayOut (IHostNetworkInterface *, aNetworkInterfaces));
    STDMETHOD(COMGETTER(ProcessorCount))(ULONG *count);
    STDMETHOD(COMGETTER(ProcessorOnlineCount))(ULONG *count);
    STDMETHOD(GetProcessorSpeed)(ULONG cpuId, ULONG *speed);
    STDMETHOD(GetProcessorDescription)(ULONG cpuId, BSTR *description);
    STDMETHOD(GetProcessorFeature) (ProcessorFeature_T feature, BOOL *supported);
    STDMETHOD(COMGETTER(MemorySize))(ULONG *size);
    STDMETHOD(COMGETTER(MemoryAvailable))(ULONG *available);
    STDMETHOD(COMGETTER(OperatingSystem))(BSTR *os);
    STDMETHOD(COMGETTER(OSVersion))(BSTR *version);
    STDMETHOD(COMGETTER(UTCTime))(LONG64 *aUTCTime);
    STDMETHOD(COMGETTER(Acceleration3DAvailable))(BOOL *aSupported);

    // IHost methods
    STDMETHOD(CreateHostOnlyNetworkInterface) (IHostNetworkInterface **aHostNetworkInterface,
                                               IProgress **aProgress);
    STDMETHOD(RemoveHostOnlyNetworkInterface) (IN_BSTR aId, IProgress **aProgress);
    STDMETHOD(CreateUSBDeviceFilter) (IN_BSTR aName, IHostUSBDeviceFilter **aFilter);
    STDMETHOD(InsertUSBDeviceFilter) (ULONG aPosition, IHostUSBDeviceFilter *aFilter);
    STDMETHOD(RemoveUSBDeviceFilter) (ULONG aPosition);

    STDMETHOD(FindHostDVDDrive) (IN_BSTR aName, IMedium **aDrive);
    STDMETHOD(FindHostFloppyDrive) (IN_BSTR aName, IMedium **aDrive);
    STDMETHOD(FindHostNetworkInterfaceByName) (IN_BSTR aName, IHostNetworkInterface **networkInterface);
    STDMETHOD(FindHostNetworkInterfaceById) (IN_BSTR id, IHostNetworkInterface **networkInterface);
    STDMETHOD(FindHostNetworkInterfacesOfType) (HostNetworkInterfaceType_T type, ComSafeArrayOut (IHostNetworkInterface *, aNetworkInterfaces));
    STDMETHOD(FindUSBDeviceByAddress) (IN_BSTR aAddress, IHostUSBDevice **aDevice);
    STDMETHOD(FindUSBDeviceById) (IN_BSTR aId, IHostUSBDevice **aDevice);

    // public methods only for internal purposes
    HRESULT loadSettings(const settings::Host &data);
    HRESULT saveSettings(settings::Host &data);

#ifdef VBOX_WITH_USB
    typedef std::list< ComObjPtr<HostUSBDeviceFilter> > USBDeviceFilterList;

    /** Must be called from under this object's lock. */
    USBProxyService* usbProxyService();

    VirtualBox* parent();

    HRESULT onUSBDeviceFilterChange (HostUSBDeviceFilter *aFilter, BOOL aActiveChanged = FALSE);
    void getUSBFilters(USBDeviceFilterList *aGlobalFiltes);
    HRESULT checkUSBProxyService();
#endif /* !VBOX_WITH_USB */

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"Host"; }

private:

#if (defined(RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)) && defined(VBOX_USE_LIBHAL)
    bool getDVDInfoFromHal(std::list< ComObjPtr<Medium> > &list);
    bool getFloppyInfoFromHal(std::list< ComObjPtr<Medium> > &list);
#endif

#if defined(RT_OS_SOLARIS)
    void parseMountTable(char *mountTable, std::list< ComObjPtr<Medium> > &list);
    bool validateDevice(const char *deviceNode, bool isCDROM);
#endif

#ifdef VBOX_WITH_USB
    /** specialization for IHostUSBDeviceFilter */
    ComObjPtr<HostUSBDeviceFilter> getDependentChild(IHostUSBDeviceFilter *aFilter);
#endif /* VBOX_WITH_USB */

#ifdef VBOX_WITH_RESOURCE_USAGE_API
    void registerMetrics (PerformanceCollector *aCollector);
    void unregisterMetrics (PerformanceCollector *aCollector);
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

    struct Data;        // opaque data structure, defined in HostImpl.cpp
    Data *m;
};

#endif // ____H_HOSTIMPL

