/* $Id$ */
/** @file
 * VirtualBox COM class implementation
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
#ifdef VBOX_WITH_EXTPACK
class ExtPackManager;
#endif

typedef std::list< ComObjPtr<SessionMachine> > SessionMachinesList;

#ifdef RT_OS_WINDOWS
class SVCHlpClient;
#endif

struct VMClientWatcherData;

namespace settings
{
    class MainConfigFile;
    struct MediaRegistry;
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
        VBOX_DEFAULT_INTERFACE_ENTRIES(IVirtualBox)
    END_COM_MAP()

    // to postpone generation of the default ctor/dtor
    VirtualBox();
    ~VirtualBox();

    HRESULT FinalConstruct();
    void FinalRelease();

    /* public initializer/uninitializer for internal purposes only */
    HRESULT init();
    HRESULT initMachines();
    HRESULT initMedia(const Guid &uuidMachineRegistry,
                      const settings::MediaRegistry mediaRegistry,
                      const Utf8Str &strMachineFolder);
    void uninit();

    /* IVirtualBox properties */
    STDMETHOD(COMGETTER(Version))               (BSTR *aVersion);
    STDMETHOD(COMGETTER(Revision))              (ULONG *aRevision);
    STDMETHOD(COMGETTER(PackageType))           (BSTR *aPackageType);
    STDMETHOD(COMGETTER(APIVersion))            (BSTR *aAPIVersion);
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
    STDMETHOD(COMGETTER(ExtensionPackManager))  (IExtPackManager **aExtPackManager);
    STDMETHOD(COMGETTER(InternalNetworks))      (ComSafeArrayOut(BSTR, aInternalNetworks));
    STDMETHOD(COMGETTER(GenericNetworkDrivers)) (ComSafeArrayOut(BSTR, aGenericNetworkDrivers));

    /* IVirtualBox methods */
    STDMETHOD(ComposeMachineFilename) (IN_BSTR aName, IN_BSTR aBaseFolder, BSTR *aFilename);
    STDMETHOD(CreateMachine) (IN_BSTR aSettingsFile,
                              IN_BSTR aName,
                              IN_BSTR aOsTypeId,
                              IN_BSTR aId,
                              BOOL forceOverwrite,
                              IMachine **aMachine);
    STDMETHOD(OpenMachine) (IN_BSTR aSettingsFile, IMachine **aMachine);
    STDMETHOD(RegisterMachine) (IMachine *aMachine);
    STDMETHOD(FindMachine) (IN_BSTR aNameOrId, IMachine **aMachine);
    STDMETHOD(CreateAppliance) (IAppliance **anAppliance);

    STDMETHOD(CreateHardDisk)(IN_BSTR aFormat,
                              IN_BSTR aLocation,
                              IMedium **aHardDisk);
    STDMETHOD(OpenMedium)(IN_BSTR aLocation,
                          DeviceType_T deviceType,
                          AccessMode_T accessMode,
                          BOOL fForceNewUuid,
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
    void onMachineDataChange(const Guid &aId, BOOL aTemporary = FALSE);
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
    void onNatRedirectChange(const Guid &aMachineId, ULONG ulSlot, bool fRemove, IN_BSTR aName,
                                   NATProtocol_T aProto, IN_BSTR aHostIp, uint16_t aHostPort,
                                   IN_BSTR aGuestIp, uint16_t aGuestPort);

    ComObjPtr<GuestOSType> getUnknownOSType();

    void getOpenedMachines(SessionMachinesList &aMachines,
                           InternalControlList *aControls = NULL);

    HRESULT findMachine(const Guid &aId,
                        bool fPermitInaccessible,
                        bool aSetError,
                        ComObjPtr<Machine> *machine = NULL);

    HRESULT findHardDiskById(const Guid &id,
                             bool aSetError,
                             ComObjPtr<Medium> *aHardDisk = NULL);
    HRESULT findHardDiskByLocation(const Utf8Str &strLocation,
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
                                 bool aSetError,
                                 ComObjPtr<Medium> &pMedium);

    HRESULT findGuestOSType(const Bstr &bstrOSType,
                            GuestOSType*& pGuestOSType);

    const Guid& getGlobalRegistryId() const;

    const ComObjPtr<Host>& host() const;
    SystemProperties* getSystemProperties() const;
#ifdef VBOX_WITH_EXTPACK
    ExtPackManager* getExtPackManager() const;
#endif
#ifdef VBOX_WITH_RESOURCE_USAGE_API
    const ComObjPtr<PerformanceCollector>& performanceCollector() const;
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

    void getDefaultMachineFolder(Utf8Str &str) const;
    void getDefaultHardDiskFormat(Utf8Str &str) const;

    /** Returns the VirtualBox home directory */
    const Utf8Str& homeDir() const;

    int calculateFullPath(const Utf8Str &strPath, Utf8Str &aResult);
    void copyPathRelativeToConfig(const Utf8Str &strSource, Utf8Str &strTarget);

    HRESULT registerMedium(const ComObjPtr<Medium> &pMedium, ComObjPtr<Medium> *ppMedium, DeviceType_T argType);
    HRESULT unregisterMedium(Medium *pMedium);

    void pushMediumToListWithChildren(MediaList &llMedia, Medium *pMedium);
    HRESULT unregisterMachineMedia(const Guid &id);

    HRESULT unregisterMachine(Machine *pMachine, const Guid &id);

    void rememberMachineNameChangeForMedia(const Utf8Str &strOldConfigDir,
                                           const Utf8Str &strNewConfigDir);

    void saveMediaRegistry(settings::MediaRegistry &mediaRegistry,
                           const Guid &uuidRegistry,
                           const Utf8Str &strMachineFolder);
    HRESULT saveSettings();

    void markRegistryModified(const Guid &uuid);
    void saveModifiedRegistries();

    static HRESULT ensureFilePathExists(const Utf8Str &strFileName, bool fCreate);

    static HRESULT handleUnexpectedExceptions (RT_SRC_POS_DECL);

    const Utf8Str& settingsFilePath();

    RWLockHandle& getMediaTreeLockHandle();

private:

    static HRESULT setErrorStatic(HRESULT aResultCode,
                                  const Utf8Str &aText)
    {
        return setErrorInternal(aResultCode, getStaticClassIID(), getStaticComponentName(), aText, false, true);
    }

    HRESULT checkMediaForConflicts(const Guid &aId,
                                   const Utf8Str &aLocation,
                                   Utf8Str &aConflictType,
                                   ComObjPtr<Medium> *pDupMedium);

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
    static Bstr sAPIVersion;

    static DECLCALLBACK(int) ClientWatcher (RTTHREAD thread, void *pvUser);
    static DECLCALLBACK(int) AsyncEventHandler (RTTHREAD thread, void *pvUser);

#ifdef RT_OS_WINDOWS
    static DECLCALLBACK(int) SVCHelperClientThread (RTTHREAD aThread, void *aUser);
#endif
};

////////////////////////////////////////////////////////////////////////////////

#endif // !____H_VIRTUALBOXIMPL

