/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
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

class Machine;
class SessionMachine;
class Medium;
class GuestOSType;
class SharedFolder;
class Progress;
class Host;
class SystemProperties;
class DHCPServer;
class PerformanceCollector;

#ifdef RT_OS_WINDOWS
class SVCHlpClient;
#endif

struct VMClientWatcherData;

namespace settings
{
    class MainConfigFile;
}

class ATL_NO_VTABLE VirtualBox :
    public VirtualBoxBaseWithChildrenNEXT,
    public VirtualBoxSupportErrorInfoImpl<VirtualBox, IVirtualBox>,
    public VirtualBoxSupportTranslation<VirtualBox>,
    VBOX_SCRIPTABLE_IMPL(IVirtualBox)
#ifdef RT_OS_WINDOWS
    , public CComCoClass<VirtualBox, &CLSID_VirtualBox>
#endif
{

public:

    typedef std::list< ComPtr<IVirtualBoxCallback> > CallbackList;
    typedef std::list< ComObjPtr<SessionMachine> > SessionMachineList;
    typedef std::list< ComPtr<IInternalSessionControl> > InternalControlList;

    class CallbackEvent;
    friend class CallbackEvent;

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (VirtualBox)

    DECLARE_CLASSFACTORY_SINGLETON(VirtualBox)

    DECLARE_REGISTRY_RESOURCEID(IDR_VIRTUALBOX)
    DECLARE_NOT_AGGREGATABLE(VirtualBox)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(VirtualBox)
        COM_INTERFACE_ENTRY(IDispatch)
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
    STDMETHOD(COMGETTER(Version)) (BSTR *aVersion);
    STDMETHOD(COMGETTER(Revision)) (ULONG *aRevision);
    STDMETHOD(COMGETTER(PackageType)) (BSTR *aPackageType);
    STDMETHOD(COMGETTER(HomeFolder)) (BSTR *aHomeFolder);
    STDMETHOD(COMGETTER(SettingsFilePath)) (BSTR *aSettingsFilePath);
    STDMETHOD(COMGETTER(Host)) (IHost **aHost);
    STDMETHOD(COMGETTER(SystemProperties)) (ISystemProperties **aSystemProperties);
    STDMETHOD(COMGETTER(Machines)) (ComSafeArrayOut (IMachine *, aMachines));
    STDMETHOD(COMGETTER(HardDisks)) (ComSafeArrayOut (IMedium *, aHardDisks));
    STDMETHOD(COMGETTER(DVDImages)) (ComSafeArrayOut (IMedium *, aDVDImages));
    STDMETHOD(COMGETTER(FloppyImages)) (ComSafeArrayOut (IMedium *, aFloppyImages));
    STDMETHOD(COMGETTER(ProgressOperations)) (ComSafeArrayOut (IProgress *, aOperations));
    STDMETHOD(COMGETTER(GuestOSTypes)) (ComSafeArrayOut (IGuestOSType *, aGuestOSTypes));
    STDMETHOD(COMGETTER(SharedFolders)) (ComSafeArrayOut (ISharedFolder *, aSharedFolders));
    STDMETHOD(COMGETTER(PerformanceCollector)) (IPerformanceCollector **aPerformanceCollector);
    STDMETHOD(COMGETTER(DHCPServers)) (ComSafeArrayOut (IDHCPServer *, aDHCPServers));

    /* IVirtualBox methods */

    STDMETHOD(CreateMachine) (IN_BSTR aName, IN_BSTR aOsTypeId, IN_BSTR aBaseFolder,
                              IN_BSTR aId, IMachine **aMachine);
    STDMETHOD(CreateLegacyMachine) (IN_BSTR aName, IN_BSTR aOsTypeId, IN_BSTR aSettingsFile,
                                    IN_BSTR aId, IMachine **aMachine);
    STDMETHOD(OpenMachine) (IN_BSTR aSettingsFile, IMachine **aMachine);
    STDMETHOD(RegisterMachine) (IMachine *aMachine);
    STDMETHOD(GetMachine) (IN_BSTR aId, IMachine **aMachine);
    STDMETHOD(FindMachine) (IN_BSTR aName, IMachine **aMachine);
    STDMETHOD(UnregisterMachine) (IN_BSTR aId, IMachine **aMachine);
    STDMETHOD(CreateAppliance) (IAppliance **anAppliance);

    STDMETHOD(CreateHardDisk)(IN_BSTR aFormat, IN_BSTR aLocation,
                               IMedium **aHardDisk);
    STDMETHOD(OpenHardDisk) (IN_BSTR aLocation, AccessMode_T accessMode,
                             BOOL aSetImageId, IN_BSTR aImageId,
                             BOOL aSetParentId, IN_BSTR aParentId,
                             IMedium **aHardDisk);
    STDMETHOD(GetHardDisk) (IN_BSTR aId, IMedium **aHardDisk);
    STDMETHOD(FindHardDisk) (IN_BSTR aLocation, IMedium **aHardDisk);

    STDMETHOD(OpenDVDImage) (IN_BSTR aLocation, IN_BSTR aId,
                             IMedium **aDVDImage);
    STDMETHOD(GetDVDImage) (IN_BSTR aId, IMedium **aDVDImage);
    STDMETHOD(FindDVDImage) (IN_BSTR aLocation, IMedium **aDVDImage);

    STDMETHOD(OpenFloppyImage) (IN_BSTR aLocation, IN_BSTR aId,
                                IMedium **aFloppyImage);
    STDMETHOD(GetFloppyImage) (IN_BSTR aId, IMedium **aFloppyImage);
    STDMETHOD(FindFloppyImage) (IN_BSTR aLocation, IMedium **aFloppyImage);

    STDMETHOD(GetGuestOSType) (IN_BSTR aId, IGuestOSType **aType);
    STDMETHOD(CreateSharedFolder) (IN_BSTR aName, IN_BSTR aHostPath, BOOL aWritable);
    STDMETHOD(RemoveSharedFolder) (IN_BSTR aName);
    STDMETHOD(GetExtraDataKeys) (ComSafeArrayOut(BSTR, aKeys));
    STDMETHOD(GetExtraData) (IN_BSTR aKey, BSTR *aValue);
    STDMETHOD(SetExtraData) (IN_BSTR aKey, IN_BSTR aValue);
    STDMETHOD(OpenSession) (ISession *aSession, IN_BSTR aMachineId);
    STDMETHOD(OpenRemoteSession) (ISession *aSession, IN_BSTR aMachineId,
                                  IN_BSTR aType, IN_BSTR aEnvironment,
                                  IProgress **aProgress);
    STDMETHOD(OpenExistingSession) (ISession *aSession, IN_BSTR aMachineId);

    STDMETHOD(RegisterCallback) (IVirtualBoxCallback *aCallback);
    STDMETHOD(UnregisterCallback) (IVirtualBoxCallback *aCallback);

    STDMETHOD(WaitForPropertyChange) (IN_BSTR aWhat, ULONG aTimeout,
                                      BSTR *aChanged, BSTR *aValues);

    STDMETHOD(CreateDHCPServer) (IN_BSTR aName, IDHCPServer ** aServer);
    STDMETHOD(FindDHCPServerByNetworkName) (IN_BSTR aName, IDHCPServer ** aServer);
    STDMETHOD(RemoveDHCPServer) (IDHCPServer * aServer);

    /* public methods only for internal purposes */

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

    void onMachineStateChange (const Guid &aId, MachineState_T aState);
    void onMachineDataChange (const Guid &aId);
    BOOL onExtraDataCanChange(const Guid &aId, IN_BSTR aKey, IN_BSTR aValue,
                              Bstr &aError);
    void onExtraDataChange(const Guid &aId, IN_BSTR aKey, IN_BSTR aValue);
    void onMachineRegistered (const Guid &aId, BOOL aRegistered);
    void onSessionStateChange (const Guid &aId, SessionState_T aState);

    void onSnapshotTaken (const Guid &aMachineId, const Guid &aSnapshotId);
    void onSnapshotDiscarded (const Guid &aMachineId, const Guid &aSnapshotId);
    void onSnapshotChange (const Guid &aMachineId, const Guid &aSnapshotId);
    void onGuestPropertyChange (const Guid &aMachineId, IN_BSTR aName, IN_BSTR aValue,
                                IN_BSTR aFlags);

    ComObjPtr<GuestOSType> getUnknownOSType();

    void getOpenedMachines(SessionMachineList &aMachines,
                           InternalControlList *aControls = NULL);

    bool isMachineIdValid(const Guid &aId)
    {
        return SUCCEEDED(findMachine(aId, false /* aSetError */, NULL));
    }

    HRESULT findMachine (const Guid &aId, bool aSetError,
                         ComObjPtr<Machine> *machine = NULL);

    HRESULT findHardDisk(const Guid *aId, CBSTR aLocation,
                          bool aSetError, ComObjPtr<Medium> *aHardDisk = NULL);
    HRESULT findDVDImage(const Guid *aId, CBSTR aLocation,
                         bool aSetError, ComObjPtr<Medium> *aImage = NULL);
    HRESULT findFloppyImage(const Guid *aId, CBSTR aLocation,
                            bool aSetError, ComObjPtr<Medium> *aImage = NULL);

    HRESULT findGuestOSType(CBSTR bstrOSType,
                            GuestOSType*& pGuestOSType);

    const ComObjPtr<Host>& host() const;
    const ComObjPtr<SystemProperties>& systemProperties() const;
#ifdef VBOX_WITH_RESOURCE_USAGE_API
    const ComObjPtr<PerformanceCollector>& performanceCollector() const;
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

    const Utf8Str& getDefaultMachineFolder() const;
    const Utf8Str& getDefaultHardDiskFolder() const;
    const Utf8Str& getDefaultHardDiskFormat() const;

    /** Returns the VirtualBox home directory */
    const Utf8Str& homeDir() const;

    int calculateFullPath(const Utf8Str &strPath, Utf8Str &aResult);
    void calculateRelativePath(const Utf8Str &strPath, Utf8Str &aResult);

    HRESULT registerHardDisk(Medium *aHardDisk, bool aSaveRegistry = true);
    HRESULT unregisterHardDisk(Medium *aHardDisk, bool aSaveRegistry = true);

    HRESULT registerDVDImage(Medium *aImage, bool aSaveRegistry = true);
    HRESULT unregisterDVDImage(Medium *aImage, bool aSaveRegistry = true);

    HRESULT registerFloppyImage (Medium *aImage, bool aSaveRegistry = true);
    HRESULT unregisterFloppyImage (Medium *aImage, bool aSaveRegistry = true);

    HRESULT cast (IMedium *aFrom, ComObjPtr<Medium> &aTo);

    HRESULT saveSettings();
    HRESULT updateSettings(const char *aOldPath, const char *aNewPath);

    static HRESULT ensureFilePathExists(const Utf8Str &strFileName);

    static HRESULT handleUnexpectedExceptions (RT_SRC_POS_DECL);

    const Utf8Str& settingsFilePath();

    RWLockHandle& hardDiskTreeLockHandle();
    RWLockHandle* childrenLock();

    /* for VirtualBoxSupportErrorInfoImpl */
    static const wchar_t *getComponentName() { return L"VirtualBox"; }

private:

    HRESULT checkMediaForConflicts2(const Guid &aId, const Bstr &aLocation,
                                    Utf8Str &aConflictType);

    HRESULT registerMachine (Machine *aMachine);

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

#endif // ____H_VIRTUALBOXIMPL

