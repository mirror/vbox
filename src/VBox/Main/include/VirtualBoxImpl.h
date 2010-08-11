/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
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

#ifndef ____H_VIRTUALBOXIMPL
#define ____H_VIRTUALBOXIMPL

#include "VirtualBoxBase.h"

#ifdef RT_OS_WINDOWS
# include "win/resource.h"
#endif

namespace com
{
    class Event;
    class EventQueue;
}

class SessionMachine;
class GuestOSType;
class SharedFolder;
class Progress;
class Host;
class SystemProperties;
class DHCPServer;
class PerformanceCollector;
class VirtualBoxCallbackRegistration; /* see VirtualBoxImpl.cpp */

typedef std::list< ComObjPtr<SessionMachine> > SessionMachinesList;

#ifdef RT_OS_WINDOWS
class SVCHlpClient;
#endif

struct VMClientWatcherData;

namespace settings
{
    class MainConfigFile;
}

class ATL_NO_VTABLE VirtualBox :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IVirtualBox)
#ifdef RT_OS_WINDOWS
    , public CComCoClass<VirtualBox, &CLSID_VirtualBox>
#endif
{

public:

    typedef std::list< ComPtr<IInternalSessionControl> > InternalControlList;

    class CallbackEvent;
    friend class CallbackEvent;

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(VirtualBox, IVirtualBox)

    DECLARE_CLASSFACTORY_SINGLETON(VirtualBox)

    DECLARE_REGISTRY_RESOURCEID(IDR_VIRTUALBOX)
    DECLARE_NOT_AGGREGATABLE(VirtualBox)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(VirtualBox)
        COM_INTERFACE_ENTRY2(IDispatch, IVirtualBox)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IVirtualBox)
    END_COM_MAP()

    // to postpone generation of the default ctor/dtor
    VirtualBox();
    ~VirtualBox();

    HRESULT FinalConstruct();
    void FinalRelease();

    /* public initializer/uninitializer for internal purposes only */
    HRESULT init();
    HRESULT initMachines();
    HRESULT initMedia();
    void uninit();

    /* IVirtualBox properties */
    STDMETHOD(COMGETTER(Version))               (BSTR *aVersion);
    STDMETHOD(COMGETTER(Revision))              (ULONG *aRevision);
    STDMETHOD(COMGETTER(PackageType))           (BSTR *aPackageType);
    STDMETHOD(COMGETTER(HomeFolder))            (BSTR *aHomeFolder);
    STDMETHOD(COMGETTER(SettingsFilePath))      (BSTR *aSettingsFilePath);
    STDMETHOD(COMGETTER(Host))                  (IHost **aHost);
    STDMETHOD(COMGETTER(SystemProperties))      (ISystemProperties **aSystemProperties);
    STDMETHOD(COMGETTER(Machines))              (ComSafeArrayOut(IMachine *, aMachines));
    STDMETHOD(COMGETTER(HardDisks))             (ComSafeArrayOut(IMedium *, aHardDisks));
    STDMETHOD(COMGETTER(DVDImages))             (ComSafeArrayOut(IMedium *, aDVDImages));
    STDMETHOD(COMGETTER(FloppyImages))          (ComSafeArrayOut(IMedium *, aFloppyImages));
    STDMETHOD(COMGETTER(ProgressOperations))    (ComSafeArrayOut(IProgress *, aOperations));
    STDMETHOD(COMGETTER(GuestOSTypes))          (ComSafeArrayOut(IGuestOSType *, aGuestOSTypes));
    STDMETHOD(COMGETTER(SharedFolders))         (ComSafeArrayOut(ISharedFolder *, aSharedFolders));
    STDMETHOD(COMGETTER(PerformanceCollector))  (IPerformanceCollector **aPerformanceCollector);
    STDMETHOD(COMGETTER(DHCPServers))           (ComSafeArrayOut(IDHCPServer *, aDHCPServers));
    STDMETHOD(COMGETTER(EventSource))           (IEventSource ** aEventSource);

    /* IVirtualBox methods */

    STDMETHOD(CreateMachine) (IN_BSTR aName, IN_BSTR aOsTypeId, IN_BSTR aBaseFolder,
                              IN_BSTR aId, BOOL aOverride, IMachine **aMachine);
    STDMETHOD(CreateLegacyMachine) (IN_BSTR aName, IN_BSTR aOsTypeId, IN_BSTR aSettingsFile,
                                    IN_BSTR aId, IMachine **aMachine);
    STDMETHOD(OpenMachine) (IN_BSTR aSettingsFile, IMachine **aMachine);
    STDMETHOD(RegisterMachine) (IMachine *aMachine);
    STDMETHOD(GetMachine) (IN_BSTR aId, IMachine **aMachine);
    STDMETHOD(FindMachine) (IN_BSTR aName, IMachine **aMachine);
    STDMETHOD(CreateAppliance) (IAppliance **anAppliance);

    STDMETHOD(CreateHardDisk)(IN_BSTR aFormat, IN_BSTR aLocation,
                               IMedium **aHardDisk);
    STDMETHOD(OpenMedium)(IN_BSTR aLocation,
                          DeviceType_T deviceType,
                          AccessMode_T accessMode,
                          IMedium **aMedium);
    STDMETHOD(FindMedium)(IN_BSTR aLocation,
                          DeviceType_T deviceType,
                          IMedium **aMedium);

    STDMETHOD(GetGuestOSType) (IN_BSTR aId, IGuestOSType **aType);
    STDMETHOD(CreateSharedFolder) (IN_BSTR aName, IN_BSTR aHostPath, BOOL aWritable, BOOL aAutoMount);
    STDMETHOD(RemoveSharedFolder) (IN_BSTR aName);
    STDMETHOD(GetExtraDataKeys) (ComSafeArrayOut(BSTR, aKeys));
    STDMETHOD(GetExtraData) (IN_BSTR aKey, BSTR *aValue);
    STDMETHOD(SetExtraData) (IN_BSTR aKey, IN_BSTR aValue);

    STDMETHOD(WaitForPropertyChange) (IN_BSTR aWhat, ULONG aTimeout,
                                      BSTR *aChanged, BSTR *aValues);

    STDMETHOD(CreateDHCPServer) (IN_BSTR aName, IDHCPServer ** aServer);
    STDMETHOD(FindDHCPServerByNetworkName) (IN_BSTR aName, IDHCPServer ** aServer);
    STDMETHOD(RemoveDHCPServer) (IDHCPServer * aServer);
    STDMETHOD(CheckFirmwarePresent)(FirmwareType_T aFirmwareType, IN_BSTR aVersion,
                                    BSTR * aUrl, BSTR * aFile, BOOL * aResult);

    /* public methods only for internal purposes */

    /**
     * Override of the default locking class to be used for validating lock
     * order with the standard member lock handle.
     */
    virtual VBoxLockingClass getLockingClass() const
    {
        return LOCKCLASS_VIRTUALBOXOBJECT;
    }

#ifdef DEBUG
    void dumpAllBackRefs();
#endif

    HRESULT postEvent(Event *event);

    HRESULT addProgress(IProgress *aProgress);
    HRESULT removeProgress(IN_GUID aId);

#ifdef RT_OS_WINDOWS
    typedef DECLCALLBACKPTR (HRESULT, SVCHelperClientFunc)
        (SVCHlpClient *aClient, Progress *aProgress, void *aUser, int *aVrc);
    HRESULT startSVCHelperClient(bool aPrivileged,
                                 SVCHelperClientFunc aFunc,
                                 void *aUser, Progress *aProgress);
#endif

    void addProcessToReap (RTPROCESS pid);
    void updateClientWatcher();

    void onMachineStateChange(const Guid &aId, MachineState_T aState);
    void onMachineDataChange(const Guid &aId);
    BOOL onExtraDataCanChange(const Guid &aId, IN_BSTR aKey, IN_BSTR aValue,
                              Bstr &aError);
    void onExtraDataChange(const Guid &aId, IN_BSTR aKey, IN_BSTR aValue);
    void onMachineRegistered(const Guid &aId, BOOL aRegistered);
    void onSessionStateChange(const Guid &aId, SessionState_T aState);

    void onSnapshotTaken(const Guid &aMachineId, const Guid &aSnapshotId);
    void onSnapshotDeleted(const Guid &aMachineId, const Guid &aSnapshotId);
    void onSnapshotChange(const Guid &aMachineId, const Guid &aSnapshotId);
    void onGuestPropertyChange(const Guid &aMachineId, IN_BSTR aName, IN_BSTR aValue,
                               IN_BSTR aFlags);
    void onMachineUninit(Machine *aMachine);

    ComObjPtr<GuestOSType> getUnknownOSType();

    void getOpenedMachines(SessionMachinesList &aMachines,
                           InternalControlList *aControls = NULL);

    HRESULT findMachine (const Guid &aId,
                         bool fPermitInaccessible,
                         bool aSetError,
                         ComObjPtr<Machine> *machine = NULL);

    HRESULT findHardDisk(const Guid *aId,
                         const Utf8Str &strLocation,
                         bool aSetError,
                         ComObjPtr<Medium> *aHardDisk = NULL);
    HRESULT findDVDOrFloppyImage(DeviceType_T mediumType,
                                 const Guid *aId,
                                 const Utf8Str &aLocation,
                                 bool aSetError,
                                 ComObjPtr<Medium> *aImage = NULL);
    HRESULT findRemoveableMedium(DeviceType_T mediumType,
                                 const Guid &uuid,
                                 bool fRefresh,
                                 ComObjPtr<Medium> &pMedium);

    HRESULT findGuestOSType(const Bstr &bstrOSType,
                            GuestOSType*& pGuestOSType);

    const ComObjPtr<Host>& host() const;
    SystemProperties* getSystemProperties() const;
#ifdef VBOX_WITH_RESOURCE_USAGE_API
    const ComObjPtr<PerformanceCollector>& performanceCollector() const;
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

    void getDefaultMachineFolder(Utf8Str &str) const;
    void getDefaultHardDiskFolder(Utf8Str &str) const;
    void getDefaultHardDiskFormat(Utf8Str &str) const;

    /** Returns the VirtualBox home directory */
    const Utf8Str& homeDir() const;

    int calculateFullPath(const Utf8Str &strPath, Utf8Str &aResult);
    void copyPathRelativeToConfig(const Utf8Str &strSource, Utf8Str &strTarget);

    HRESULT registerHardDisk(Medium *aHardDisk, bool *pfNeedsSaveSettings);
    HRESULT unregisterHardDisk(Medium *aHardDisk, bool *pfNeedsSaveSettings);

    HRESULT registerImage(Medium *aImage, DeviceType_T argType, bool *pfNeedsSaveSettings);
    HRESULT unregisterImage(Medium *aImage, DeviceType_T argType, bool *pfNeedsSaveSettings);

    HRESULT unregisterMachine(Machine *pMachine);

    void rememberMachineNameChangeForMedia(const Utf8Str &strOldConfigDir,
                                           const Utf8Str &strNewConfigDir);

    HRESULT saveSettings();

    static HRESULT ensureFilePathExists(const Utf8Str &strFileName);

    static HRESULT handleUnexpectedExceptions (RT_SRC_POS_DECL);

    const Utf8Str& settingsFilePath();

    RWLockHandle& getMediaTreeLockHandle();

private:

    static HRESULT setErrorStatic(HRESULT aResultCode,
                                  const Utf8Str &aText)
    {
        return setErrorInternal(aResultCode, getStaticClassIID(), getStaticComponentName(), aText, false, true);
    }

    HRESULT checkMediaForConflicts2(const Guid &aId, const Utf8Str &aLocation,
                                    Utf8Str &aConflictType);

    HRESULT registerMachine(Machine *aMachine);

    HRESULT registerDHCPServer(DHCPServer *aDHCPServer,
                               bool aSaveRegistry = true);
    HRESULT unregisterDHCPServer(DHCPServer *aDHCPServer,
                                 bool aSaveRegistry = true);

    struct Data;            // opaque data structure, defined in VirtualBoxImpl.cpp
    Data *m;

    /* static variables (defined in VirtualBoxImpl.cpp) */
    static Bstr sVersion;
    static ULONG sRevision;
    static Bstr sPackageType;

    static DECLCALLBACK(int) ClientWatcher (RTTHREAD thread, void *pvUser);
    static DECLCALLBACK(int) AsyncEventHandler (RTTHREAD thread, void *pvUser);

#ifdef RT_OS_WINDOWS
    static DECLCALLBACK(int) SVCHelperClientThread (RTTHREAD aThread, void *aUser);
#endif
};

////////////////////////////////////////////////////////////////////////////////

#endif // !____H_VIRTUALBOXIMPL
