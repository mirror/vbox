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

#include "VBox/com/EventQueue.h"

#include <list>
#include <vector>
#include <map>

#ifdef RT_OS_WINDOWS
# include "win/resource.h"
#endif

#ifdef VBOX_WITH_RESOURCE_USAGE_API
#include "PerformanceImpl.h"
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

class Machine;
class SessionMachine;
class HardDisk;
class DVDImage;
class FloppyImage;
class MachineCollection;
class GuestOSType;
class GuestOSTypeCollection;
class SharedFolder;
class Progress;
class ProgressCollection;
class Host;
class SystemProperties;
class DHCPServer;

#ifdef RT_OS_WINDOWS
class SVCHlpClient;
#endif

struct VMClientWatcherData;

class ATL_NO_VTABLE VirtualBox :
    public VirtualBoxBaseWithChildrenNEXT,
    public VirtualBoxSupportErrorInfoImpl <VirtualBox, IVirtualBox>,
    public VirtualBoxSupportTranslation <VirtualBox>,
#ifdef RT_OS_WINDOWS
    public IDispatchImpl<IVirtualBox, &IID_IVirtualBox, &LIBID_VirtualBox,
                         kTypeLibraryMajorVersion, kTypeLibraryMinorVersion>,
    public CComCoClass<VirtualBox, &CLSID_VirtualBox>
#else
    public IVirtualBox
#endif
{

public:

    typedef std::list <ComPtr <IVirtualBoxCallback> > CallbackList;
    typedef std::vector <ComPtr <IVirtualBoxCallback> > CallbackVector;

    typedef std::vector <ComObjPtr <SessionMachine> > SessionMachineVector;
    typedef std::vector <ComObjPtr <Machine> > MachineVector;

    typedef std::vector <ComPtr <IInternalSessionControl> > InternalControlVector;

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

    NS_DECL_ISUPPORTS

    /* to postpone generation of the default ctor/dtor */
    VirtualBox();
    ~VirtualBox();

    HRESULT FinalConstruct();
    void FinalRelease();

    /* public initializer/uninitializer for internal purposes only */
    HRESULT init();
    void uninit();

    /* IVirtualBox properties */
    STDMETHOD(COMGETTER(Version)) (BSTR *aVersion);
    STDMETHOD(COMGETTER(Revision)) (ULONG *aRevision);
    STDMETHOD(COMGETTER(PackageType)) (BSTR *aPackageType);
    STDMETHOD(COMGETTER(HomeFolder)) (BSTR *aHomeFolder);
    STDMETHOD(COMGETTER(SettingsFilePath)) (BSTR *aSettingsFilePath);
    STDMETHOD(COMGETTER(SettingsFileVersion)) (BSTR *aSettingsFileVersion);
    STDMETHOD(COMGETTER(SettingsFormatVersion)) (BSTR *aSettingsFormatVersion);
    STDMETHOD(COMGETTER(Host)) (IHost **aHost);
    STDMETHOD(COMGETTER(SystemProperties)) (ISystemProperties **aSystemProperties);
    STDMETHOD(COMGETTER(Machines)) (ComSafeArrayOut (IMachine *, aMachines));
    STDMETHOD(COMGETTER(HardDisks)) (ComSafeArrayOut (IHardDisk *, aHardDisks));
    STDMETHOD(COMGETTER(DVDImages)) (ComSafeArrayOut (IDVDImage *, aDVDImages));
    STDMETHOD(COMGETTER(FloppyImages)) (ComSafeArrayOut (IFloppyImage *, aFloppyImages));
    STDMETHOD(COMGETTER(ProgressOperations)) (ComSafeArrayOut (IProgress *, aOperations));
    STDMETHOD(COMGETTER(GuestOSTypes)) (ComSafeArrayOut (IGuestOSType *, aGuestOSTypes));
    STDMETHOD(COMGETTER(SharedFolders)) (ComSafeArrayOut (ISharedFolder *, aSharedFolders));
    STDMETHOD(COMGETTER(PerformanceCollector)) (IPerformanceCollector **aPerformanceCollector);
    STDMETHOD(COMGETTER(DHCPServers)) (ComSafeArrayOut (IDHCPServer *, aDHCPServers));

    /* IVirtualBox methods */

    STDMETHOD(CreateMachine) (IN_BSTR aName, IN_BSTR aOsTypeId, IN_BSTR aBaseFolder,
                              IN_GUID aId, IMachine **aMachine);
    STDMETHOD(CreateLegacyMachine) (IN_BSTR aName, IN_BSTR aOsTypeId, IN_BSTR aSettingsFile,
                                    IN_GUID aId, IMachine **aMachine);
    STDMETHOD(OpenMachine) (IN_BSTR aSettingsFile, IMachine **aMachine);
    STDMETHOD(RegisterMachine) (IMachine *aMachine);
    STDMETHOD(GetMachine) (IN_GUID aId, IMachine **aMachine);
    STDMETHOD(FindMachine) (IN_BSTR aName, IMachine **aMachine);
    STDMETHOD(UnregisterMachine) (IN_GUID aId, IMachine **aMachine);
    STDMETHOD(CreateAppliance) (IAppliance **anAppliance);

    STDMETHOD(CreateHardDisk)(IN_BSTR aFormat, IN_BSTR aLocation,
                               IHardDisk **aHardDisk);
    STDMETHOD(OpenHardDisk) (IN_BSTR aLocation, AccessMode_T accessMode, IHardDisk **aHardDisk);
    STDMETHOD(GetHardDisk) (IN_GUID aId, IHardDisk **aHardDisk);
    STDMETHOD(FindHardDisk) (IN_BSTR aLocation, IHardDisk **aHardDisk);

    STDMETHOD(OpenDVDImage) (IN_BSTR aLocation, IN_GUID aId,
                             IDVDImage **aDVDImage);
    STDMETHOD(GetDVDImage) (IN_GUID aId, IDVDImage **aDVDImage);
    STDMETHOD(FindDVDImage) (IN_BSTR aLocation, IDVDImage **aDVDImage);

    STDMETHOD(OpenFloppyImage) (IN_BSTR aLocation, IN_GUID aId,
                                IFloppyImage **aFloppyImage);
    STDMETHOD(GetFloppyImage) (IN_GUID aId, IFloppyImage **aFloppyImage);
    STDMETHOD(FindFloppyImage) (IN_BSTR aLocation, IFloppyImage **aFloppyImage);

    STDMETHOD(GetGuestOSType) (IN_BSTR aId, IGuestOSType **aType);
    STDMETHOD(CreateSharedFolder) (IN_BSTR aName, IN_BSTR aHostPath, BOOL aWritable);
    STDMETHOD(RemoveSharedFolder) (IN_BSTR aName);
    STDMETHOD(GetNextExtraDataKey) (IN_BSTR aKey, BSTR *aNextKey, BSTR *aNextValue);
    STDMETHOD(GetExtraData) (IN_BSTR aKey, BSTR *aValue);
    STDMETHOD(SetExtraData) (IN_BSTR aKey, IN_BSTR aValue);
    STDMETHOD(OpenSession) (ISession *aSession, IN_GUID aMachineId);
    STDMETHOD(OpenRemoteSession) (ISession *aSession, IN_GUID aMachineId,
                                  IN_BSTR aType, IN_BSTR aEnvironment,
                                  IProgress **aProgress);
    STDMETHOD(OpenExistingSession) (ISession *aSession, IN_GUID aMachineId);

    STDMETHOD(RegisterCallback) (IVirtualBoxCallback *aCallback);
    STDMETHOD(UnregisterCallback) (IVirtualBoxCallback *aCallback);

    STDMETHOD(WaitForPropertyChange) (IN_BSTR aWhat, ULONG aTimeout,
                                      BSTR *aChanged, BSTR *aValues);

    STDMETHOD(SaveSettings)();
    STDMETHOD(SaveSettingsWithBackup) (BSTR *aBakFileName);

//    STDMETHOD(CreateDHCPServerForInterface) (/*IHostNetworkInterface * aIinterface, */IDHCPServer ** aServer);
    STDMETHOD(CreateDHCPServer) (IN_BSTR aName, IDHCPServer ** aServer);
//    STDMETHOD(FindDHCPServerForInterface) (IHostNetworkInterface * aIinterface, IDHCPServer ** aServer);
    STDMETHOD(FindDHCPServerByNetworkName) (IN_BSTR aName, IDHCPServer ** aServer);
    STDMETHOD(RemoveDHCPServer) (IDHCPServer * aServer);

    /* public methods only for internal purposes */

    HRESULT postEvent (Event *event);

    HRESULT addProgress (IProgress *aProgress);
    HRESULT removeProgress (IN_GUID aId);

#ifdef RT_OS_WINDOWS
    typedef DECLCALLBACKPTR (HRESULT, SVCHelperClientFunc)
        (SVCHlpClient *aClient, Progress *aProgress, void *aUser, int *aVrc);
    HRESULT startSVCHelperClient (bool aPrivileged,
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

    ComObjPtr <GuestOSType> getUnknownOSType();

    void getOpenedMachines (SessionMachineVector &aMachines,
                            InternalControlVector *aControls = NULL);

    /** Shortcut to #getOpenedMachines (aMachines, &aControls). */
    void getOpenedMachinesAndControls (SessionMachineVector &aMachines,
                                       InternalControlVector &aControls)
    { getOpenedMachines (aMachines, &aControls); }

    bool isMachineIdValid (const Guid &aId)
    {
        return SUCCEEDED (findMachine (aId, false /* aSetError */, NULL));
    }

    HRESULT findMachine (const Guid &aId, bool aSetError,
                         ComObjPtr <Machine> *machine = NULL);

    HRESULT findHardDisk(const Guid *aId, CBSTR aLocation,
                          bool aSetError, ComObjPtr<HardDisk> *aHardDisk = NULL);
    HRESULT findDVDImage(const Guid *aId, CBSTR aLocation,
                         bool aSetError, ComObjPtr<DVDImage> *aImage = NULL);
    HRESULT findFloppyImage(const Guid *aId, CBSTR aLocation,
                            bool aSetError, ComObjPtr<FloppyImage> *aImage = NULL);

    const ComObjPtr <Host> &host() { return mData.mHost; }
    const ComObjPtr <SystemProperties> &systemProperties()
        { return mData.mSystemProperties; }
#ifdef VBOX_WITH_RESOURCE_USAGE_API
    const ComObjPtr <PerformanceCollector> &performanceCollector()
        { return mData.mPerformanceCollector; }
#endif /* VBOX_WITH_RESOURCE_USAGE_API */


    /** Returns the VirtualBox home directory */
    const Utf8Str &homeDir() { return mData.mHomeDir; }

    int calculateFullPath (const char *aPath, Utf8Str &aResult);
    void calculateRelativePath (const char *aPath, Utf8Str &aResult);

    HRESULT registerHardDisk(HardDisk *aHardDisk, bool aSaveRegistry = true);
    HRESULT unregisterHardDisk(HardDisk *aHardDisk, bool aSaveRegistry = true);

    HRESULT registerDVDImage(DVDImage *aImage, bool aSaveRegistry = true);
    HRESULT unregisterDVDImage(DVDImage *aImage, bool aSaveRegistry = true);

    HRESULT registerFloppyImage (FloppyImage *aImage, bool aSaveRegistry = true);
    HRESULT unregisterFloppyImage (FloppyImage *aImage, bool aSaveRegistry = true);

    HRESULT cast (IHardDisk *aFrom, ComObjPtr<HardDisk> &aTo);

    HRESULT saveSettings();
    HRESULT updateSettings (const char *aOldPath, const char *aNewPath);

    const Bstr &settingsFileName() { return mData.mCfgFile.mName; }

    static HRESULT ensureFilePathExists (const char *aFileName);

    static HRESULT loadSettingsTree (settings::XmlTreeBackend &aTree,
                                     xml::File &aFile,
                                     bool aValidate,
                                     bool aCatchLoadErrors,
                                     bool aAddDefaults,
                                     Utf8Str *aFormatVersion = NULL);

    /**
     * Shortcut to loadSettingsTree (aTree, aFile, true, true, true).
     *
     * Used when the settings file is to be loaded for the first time for the
     * given object in order to recreate it from the stored settings.
     *
     * @param aFormatVersion Where to store the current format version of the
     *                       loaded settings tree.
     */
    static HRESULT loadSettingsTree_FirstTime (settings::XmlTreeBackend &aTree,
                                               xml::File &aFile,
                                               Utf8Str &aFormatVersion)
    {
        return loadSettingsTree (aTree, aFile, true, true, true,
                                 &aFormatVersion);
    }

    /**
     * Shortcut to loadSettingsTree (aTree, aFile, true, false, true).
     *
     * Used when the settings file is loaded again (after it has been fully
     * checked and validated by #loadSettingsTree_FirstTime()) in order to
     * look at settings that don't have any representation within object's
     * data fields.
     */
    static HRESULT loadSettingsTree_Again (settings::XmlTreeBackend &aTree,
                                           xml::File &aFile)
    {
        return loadSettingsTree (aTree, aFile, true, false, true);
    }

    /**
     * Shortcut to loadSettingsTree (aTree, aFile, true, false, false).
     *
     * Used when the settings file is loaded again (after it has been fully
     * checked and validated by #loadSettingsTree_FirstTime()) in order to
     * update some settings and then save them back.
     */
    static HRESULT loadSettingsTree_ForUpdate (settings::XmlTreeBackend &aTree,
                                               xml::File &aFile)
    {
        return loadSettingsTree (aTree, aFile, true, false, false);
    }

    static HRESULT saveSettingsTree (settings::TreeBackend &aTree,
                                     xml::File &aFile,
                                     Utf8Str &aFormatVersion);

    static HRESULT backupSettingsFile (const Bstr &aFileName,
                                       const Utf8Str &aOldFormat,
                                       Bstr &aBakFileName);

    static HRESULT handleUnexpectedExceptions (RT_SRC_POS_DECL);

    /**
     * Returns a lock handle used to protect changes to the hard disk hierarchy
     * (e.g. serialize access to the HardDisk::mParent fields and methods
     * adding/removing children). When using this lock, the following rules must
     * be obeyed:
     *
     * 1. The write lock on this handle must be either held alone on the thread
     *    or requested *after* the VirtualBox object lock. Mixing with other
     *    locks is prohibited.
     *
     * 2. The read lock on this handle may be intermixed with any other lock
     *    with the exception that it must be requested *after* the VirtualBox
     *    object lock.
     */
    RWLockHandle *hardDiskTreeLockHandle() { return &mHardDiskTreeLockHandle; }

    /* for VirtualBoxSupportErrorInfoImpl */
    static const wchar_t *getComponentName() { return L"VirtualBox"; }

private:

    typedef std::list <ComObjPtr <Machine> > MachineList;
    typedef std::list <ComObjPtr <GuestOSType> > GuestOSTypeList;

    typedef std::map <Guid, ComPtr <IProgress> > ProgressMap;

    typedef std::list <ComObjPtr <HardDisk> > HardDiskList;
    typedef std::list <ComObjPtr <DVDImage> > DVDImageList;
    typedef std::list <ComObjPtr <FloppyImage> > FloppyImageList;
    typedef std::list <ComObjPtr <SharedFolder> > SharedFolderList;
    typedef std::list <ComObjPtr <DHCPServer> > DHCPServerList;

    typedef std::map <Guid, ComObjPtr<HardDisk> > HardDiskMap;

    /**
     * Reimplements VirtualBoxWithTypedChildren::childrenLock() to return a
     * dedicated lock instead of the main object lock. The dedicated lock for
     * child map operations frees callers of init() methods of these children
     * from acquiring a write parent (VirtualBox) lock (which would be mandatory
     * otherwise). Since VirtualBox has a lot of heterogenous children which
     * init() methods are called here and there, it definitely makes sense.
     */
    RWLockHandle *childrenLock() { return &mChildrenMapLockHandle; }

    HRESULT checkMediaForConflicts2 (const Guid &aId, const Bstr &aLocation,
                                     Utf8Str &aConflictType);

    HRESULT loadMachines (const settings::Key &aGlobal);
    HRESULT loadMedia (const settings::Key &aGlobal);
    HRESULT loadNetservices (const settings::Key &aGlobal);

    HRESULT registerMachine (Machine *aMachine);

    HRESULT registerDHCPServer(DHCPServer *aDHCPServer,
                                         bool aSaveRegistry = true);
    HRESULT unregisterDHCPServer(DHCPServer *aDHCPServer,
                                         bool aSaveRegistry = true);

    HRESULT lockConfig();
    HRESULT unlockConfig();

    /** @note This method is not thread safe */
    bool isConfigLocked() { return mData.mCfgFile.mHandle != NIL_RTFILE; }

    /**
     *  Main VirtualBox data structure.
     *  @note |const| members are persistent during lifetime so can be accessed
     *  without locking.
     */
    struct Data
    {
        Data();

        struct CfgFile
        {
            CfgFile() : mHandle (NIL_RTFILE) {}

            const Bstr mName;
            RTFILE mHandle;
        };

        // const data members not requiring locking
        const Utf8Str mHomeDir;

        // const objects not requiring locking
        const ComObjPtr <Host> mHost;
        const ComObjPtr <SystemProperties> mSystemProperties;
#ifdef VBOX_WITH_RESOURCE_USAGE_API
        const ComObjPtr <PerformanceCollector> mPerformanceCollector;
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

        CfgFile mCfgFile;

        Utf8Str mSettingsFileVersion;

        MachineList mMachines;
        GuestOSTypeList mGuestOSTypes;

        ProgressMap mProgressOperations;

        HardDiskList mHardDisks;
        DVDImageList mDVDImages;
        FloppyImageList mFloppyImages;
        SharedFolderList mSharedFolders;
        DHCPServerList mDHCPServers;

        /// @todo NEWMEDIA do we really need this map? Used only in
        /// find() it seems
        HardDiskMap mHardDiskMap;

        CallbackList mCallbacks;
    };

    Data mData;

    /** Client watcher thread data structure */
    struct ClientWatcherData
    {
        ClientWatcherData()
#if defined(RT_OS_WINDOWS)
            : mUpdateReq (NULL)
#elif defined(RT_OS_OS2)
            : mUpdateReq (NIL_RTSEMEVENT)
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
            : mUpdateReq (NIL_RTSEMEVENT)
#else
# error "Port me!"
#endif
            , mThread (NIL_RTTHREAD) {}

        // const objects not requiring locking
#if defined(RT_OS_WINDOWS)
        const HANDLE mUpdateReq;
#elif defined(RT_OS_OS2)
        const RTSEMEVENT mUpdateReq;
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
        const RTSEMEVENT mUpdateReq;
#else
# error "Port me!"
#endif
        const RTTHREAD mThread;

        typedef std::list <RTPROCESS> ProcessList;
        ProcessList mProcesses;
    };

    ClientWatcherData mWatcherData;

    const RTTHREAD mAsyncEventThread;
    EventQueue * const mAsyncEventQ;

    /**
     * "Safe" lock. May only be used if guaranteed that no other locks are
     * requested while holding it and no functions that may do so are called.
     * Currently, protects the following:
     *
     * - mProgressOperations
     */
    RWLockHandle mSafeLock;

    RWLockHandle mHardDiskTreeLockHandle;
    RWLockHandle mChildrenMapLockHandle;

    static Bstr sVersion;
    static ULONG sRevision;
    static Bstr sPackageType;
    static Bstr sSettingsFormatVersion;

    static DECLCALLBACK(int) ClientWatcher (RTTHREAD thread, void *pvUser);
    static DECLCALLBACK(int) AsyncEventHandler (RTTHREAD thread, void *pvUser);

#ifdef RT_OS_WINDOWS
    static DECLCALLBACK(int) SVCHelperClientThread (RTTHREAD aThread, void *aUser);
#endif
};

////////////////////////////////////////////////////////////////////////////////

/**
 *  Abstract callback event class to asynchronously call VirtualBox callbacks
 *  on a dedicated event thread. Subclasses reimplement #handleCallback()
 *  to call appropriate IVirtualBoxCallback methods depending on the event
 *  to be dispatched.
 *
 *  @note The VirtualBox instance passed to the constructor is strongly
 *  referenced, so that the VirtualBox singleton won't be released until the
 *  event gets handled by the event thread.
 */
class VirtualBox::CallbackEvent : public Event
{
public:

    CallbackEvent (VirtualBox *aVirtualBox) : mVirtualBox (aVirtualBox)
    {
        Assert (aVirtualBox);
    }

    void *handler();

    virtual void handleCallback (const ComPtr <IVirtualBoxCallback> &aCallback) = 0;

private:

    /*
     *  Note that this is a weak ref -- the CallbackEvent handler thread
     *  is bound to the lifetime of the VirtualBox instance, so it's safe.
     */
    ComObjPtr <VirtualBox, ComWeakRef> mVirtualBox;
};

#endif // ____H_VIRTUALBOXIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
