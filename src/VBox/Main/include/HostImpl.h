/** @file
 *
 * VirtualBox COM class implementation
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

#ifndef ____H_HOSTIMPL
#define ____H_HOSTIMPL

#include "VirtualBoxBase.h"
#include "HostUSBDeviceImpl.h"
#include "USBDeviceFilterImpl.h"

#ifdef RT_OS_WINDOWS
#include "win32/svchlp.h"
#endif

#include <VBox/cfgldr.h>

class VirtualBox;
class SessionMachine;
class HostDVDDrive;
class HostFloppyDrive;
class Progress;

#include <list>

class ATL_NO_VTABLE Host :
    public VirtualBoxBaseWithChildren,
    public VirtualBoxSupportErrorInfoImpl <Host, IHost>,
    public VirtualBoxSupportTranslation <Host>,
    public IHost
{
public:

    DECLARE_NOT_AGGREGATABLE(Host)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Host)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IHost)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (VirtualBox *parent);
    void uninit();

    // IHost properties
    STDMETHOD(COMGETTER(DVDDrives))(IHostDVDDriveCollection **drives);
    STDMETHOD(COMGETTER(FloppyDrives))(IHostFloppyDriveCollection **drives);
    STDMETHOD(COMGETTER(USBDevices))(IHostUSBDeviceCollection **aUSBDevices);
    STDMETHOD(COMGETTER(USBDeviceFilters))(IHostUSBDeviceFilterCollection ** aUSBDeviceFilters);
#ifdef RT_OS_WINDOWS
    STDMETHOD(COMGETTER(NetworkInterfaces))(IHostNetworkInterfaceCollection **networkInterfaces);
#endif
    STDMETHOD(COMGETTER(ProcessorCount))(ULONG *count);
    STDMETHOD(COMGETTER(ProcessorSpeed))(ULONG *speed);
    STDMETHOD(COMGETTER(ProcessorDescription))(BSTR *description);
    STDMETHOD(COMGETTER(MemorySize))(ULONG *size);
    STDMETHOD(COMGETTER(MemoryAvailable))(ULONG *available);
    STDMETHOD(COMGETTER(OperatingSystem))(BSTR *os);
    STDMETHOD(COMGETTER(OSVersion))(BSTR *version);
    STDMETHOD(COMGETTER(UTCTime))(LONG64 *aUTCTime);

    // IHost methods
#ifdef RT_OS_WINDOWS
    STDMETHOD(CreateHostNetworkInterface) (INPTR BSTR aName,
                                           IHostNetworkInterface **aHostNetworkInterface,
                                           IProgress **aProgress);
    STDMETHOD(RemoveHostNetworkInterface) (INPTR GUIDPARAM aId,
                                           IHostNetworkInterface **aHostNetworkInterface,
                                           IProgress **aProgress);
#endif
    STDMETHOD(CreateUSBDeviceFilter) (INPTR BSTR aName, IHostUSBDeviceFilter **aFilter);
    STDMETHOD(InsertUSBDeviceFilter) (ULONG aPosition, IHostUSBDeviceFilter *aFilter);
    STDMETHOD(RemoveUSBDeviceFilter) (ULONG aPosition, IHostUSBDeviceFilter **aFilter);

    // public methods only for internal purposes

    HRESULT onUSBDeviceFilterChange (HostUSBDeviceFilter *aFilter,
                                     BOOL aActiveChanged = FALSE);

    HRESULT loadSettings (CFGNODE aGlobal);
    HRESULT saveSettings (CFGNODE aGlobal);

    HRESULT captureUSBDevice (SessionMachine *aMachine, INPTR GUIDPARAM aId);
    HRESULT detachUSBDevice (SessionMachine *aMachine, INPTR GUIDPARAM aId, BOOL aDone);
    HRESULT autoCaptureUSBDevices (SessionMachine *aMachine);
    HRESULT detachAllUSBDevices (SessionMachine *aMachine, BOOL aDone);

    void onUSBDeviceAttached (HostUSBDevice *aDevice);
    void onUSBDeviceDetached (HostUSBDevice *aDevice);
    void onUSBDeviceStateChanged (HostUSBDevice *aDevice);

    HRESULT checkUSBProxyService();
    
    /* must be called from under this object's lock */ 
    USBProxyService *usbProxyService() { return mUSBProxyService; }

#ifdef RT_OS_WINDOWS
    static int networkInterfaceHelperServer (SVCHlpClient *aClient,
                                             SVCHlpMsg::Code aMsgCode);
#endif

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"Host"; }

private:

#if defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS)
# ifdef VBOX_USE_LIBHAL
    bool getDVDInfoFromHal(std::list <ComObjPtr <HostDVDDrive> > &list);
    bool getFloppyInfoFromHal(std::list <ComObjPtr <HostFloppyDrive> > &list);
# endif
    void parseMountTable(char *mountTable, std::list <ComObjPtr <HostDVDDrive> > &list);
    bool validateDevice(const char *deviceNode, bool isCDROM);
#endif

    /** specialization for IHostUSBDeviceFilter */
    ComObjPtr <HostUSBDeviceFilter> getDependentChild (IHostUSBDeviceFilter *aFilter)
    {
        VirtualBoxBase *child = VirtualBoxBaseWithChildren::
                                getDependentChild (ComPtr <IUnknown> (aFilter));
        return child ? dynamic_cast <HostUSBDeviceFilter *> (child)
                     : NULL;
    }

    HRESULT applyAllUSBFilters (ComObjPtr <HostUSBDevice> &aDevice,
                                SessionMachine *aMachine = NULL);

    bool applyMachineUSBFilters (SessionMachine *aMachine,
                                 ComObjPtr <HostUSBDevice> &aDevice);

#ifdef RT_OS_WINDOWS
    static int createNetworkInterface (SVCHlpClient *aClient,
                                       const Utf8Str &aName,
                                       Guid &aGUID, Utf8Str &aErrMsg);
    static int removeNetworkInterface (SVCHlpClient *aClient,
                                       const Guid &aGUID,
                                       Utf8Str &aErrMsg);
    static HRESULT networkInterfaceHelperClient (SVCHlpClient *aClient,
                                                 Progress *aProgress,
                                                 void *aUser, int *aVrc);
#endif

    ComObjPtr <VirtualBox, ComWeakRef> mParent;

    typedef std::list <ComObjPtr <HostUSBDevice> > USBDeviceList;
    USBDeviceList mUSBDevices;

    typedef std::list <ComObjPtr <HostUSBDeviceFilter> > USBDeviceFilterList;
    USBDeviceFilterList mUSBDeviceFilters;

    /** Pointer to the USBProxyService object. */
    USBProxyService *mUSBProxyService;
};

#endif // ____H_HOSTIMPL
