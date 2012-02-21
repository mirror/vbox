/* $Id$ */
/** @file
 * Implementation of IVirtualBox in VBoxSVC.
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

#include <iprt/asm.h>
#include <iprt/buildconfig.h>
#include <iprt/cpp/utils.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/thread.h>
#include <iprt/uuid.h>
#include <iprt/cpp/xml.h>

#include <VBox/com/com.h>
#include <VBox/com/array.h>
#include "VBox/com/EventQueue.h"

#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/settings.h>
#include <VBox/version.h>

#include <package-generated.h>

#include <algorithm>
#include <set>
#include <vector>
#include <memory> // for auto_ptr

#include <typeinfo>

#include "VirtualBoxImpl.h"

#include "Global.h"
#include "MachineImpl.h"
#include "MediumImpl.h"
#include "SharedFolderImpl.h"
#include "ProgressImpl.h"
#include "ProgressProxyImpl.h"
#include "HostImpl.h"
#include "USBControllerImpl.h"
#include "SystemPropertiesImpl.h"
#include "GuestOSTypeImpl.h"
#include "DHCPServerRunner.h"
#include "DHCPServerImpl.h"
#ifdef VBOX_WITH_RESOURCE_USAGE_API
# include "PerformanceImpl.h"
#endif /* VBOX_WITH_RESOURCE_USAGE_API */
#include "EventImpl.h"
#include "VBoxEvents.h"
#ifdef VBOX_WITH_EXTPACK
# include "ExtPackManagerImpl.h"
#endif

#include "AutoCaller.h"
#include "Logging.h"
#include "objectslist.h"

#ifdef RT_OS_WINDOWS
# include "win/svchlp.h"
# include "win/VBoxComEvents.h"
#endif

////////////////////////////////////////////////////////////////////////////////
//
// Definitions
//
////////////////////////////////////////////////////////////////////////////////

#define VBOX_GLOBAL_SETTINGS_FILE "VirtualBox.xml"

////////////////////////////////////////////////////////////////////////////////
//
// Global variables
//
////////////////////////////////////////////////////////////////////////////////

// static
Bstr VirtualBox::sVersion;

// static
ULONG VirtualBox::sRevision;

// static
Bstr VirtualBox::sPackageType;

// static
Bstr VirtualBox::sAPIVersion;

////////////////////////////////////////////////////////////////////////////////
//
// CallbackEvent class
//
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

    CallbackEvent(VirtualBox *aVirtualBox, VBoxEventType_T aWhat)
        : mVirtualBox(aVirtualBox), mWhat(aWhat)
    {
        Assert(aVirtualBox);
    }

    void *handler();

    virtual HRESULT prepareEventDesc(IEventSource* aSource, VBoxEventDesc& aEvDesc) = 0;

private:

    /**
     *  Note that this is a weak ref -- the CallbackEvent handler thread
     *  is bound to the lifetime of the VirtualBox instance, so it's safe.
     */
    VirtualBox         *mVirtualBox;
protected:
    VBoxEventType_T     mWhat;
};

////////////////////////////////////////////////////////////////////////////////
//
// VirtualBox private member data definition
//
////////////////////////////////////////////////////////////////////////////////

#if defined(RT_OS_WINDOWS)
    #define UPDATEREQARG NULL
    #define UPDATEREQTYPE HANDLE
#elif defined(RT_OS_OS2)
    #define UPDATEREQARG NIL_RTSEMEVENT
    #define UPDATEREQTYPE RTSEMEVENT
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
    #define UPDATEREQARG
    #define UPDATEREQTYPE RTSEMEVENT
#else
# error "Port me!"
#endif

typedef ObjectsList<Machine> MachinesOList;
typedef ObjectsList<Medium> MediaOList;
typedef ObjectsList<GuestOSType> GuestOSTypesOList;
typedef ObjectsList<SharedFolder> SharedFoldersOList;
typedef ObjectsList<DHCPServer> DHCPServersOList;

typedef std::map<Guid, ComPtr<IProgress> > ProgressMap;
typedef std::map<Guid, ComObjPtr<Medium> > HardDiskMap;

/**
 *  Main VirtualBox data structure.
 *  @note |const| members are persistent during lifetime so can be accessed
 *  without locking.
 */
struct VirtualBox::Data
{
    Data()
        : pMainConfigFile(NULL),
          uuidMediaRegistry("48024e5c-fdd9-470f-93af-ec29f7ea518c"),
          lockMachines(LOCKCLASS_LISTOFMACHINES),
          allMachines(lockMachines),
          lockGuestOSTypes(LOCKCLASS_LISTOFOTHEROBJECTS),
          allGuestOSTypes(lockGuestOSTypes),
          lockMedia(LOCKCLASS_LISTOFMEDIA),
          allHardDisks(lockMedia),
          allDVDImages(lockMedia),
          allFloppyImages(lockMedia),
          lockSharedFolders(LOCKCLASS_LISTOFOTHEROBJECTS),
          allSharedFolders(lockSharedFolders),
          lockDHCPServers(LOCKCLASS_LISTOFOTHEROBJECTS),
          allDHCPServers(lockDHCPServers),
          mtxProgressOperations(LOCKCLASS_PROGRESSLIST),
          updateReq(UPDATEREQARG),
          threadClientWatcher(NIL_RTTHREAD),
          threadAsyncEvent(NIL_RTTHREAD),
          pAsyncEventQ(NULL)
    {
    }

    ~Data()
    {
        if (pMainConfigFile)
        {
            delete pMainConfigFile;
            pMainConfigFile = NULL;
        }
    };

    // const data members not requiring locking
    const Utf8Str                       strHomeDir;

    // VirtualBox main settings file
    const Utf8Str                       strSettingsFilePath;
    settings::MainConfigFile            *pMainConfigFile;

    // constant pseudo-machine ID for global media registry
    const Guid                          uuidMediaRegistry;

    // const objects not requiring locking
    const ComObjPtr<Host>               pHost;
    const ComObjPtr<SystemProperties>   pSystemProperties;
#ifdef VBOX_WITH_RESOURCE_USAGE_API
    const ComObjPtr<PerformanceCollector> pPerformanceCollector;
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

    // Each of the following lists use a particular lock handle that protects the
    // list as a whole. As opposed to version 3.1 and earlier, these lists no
    // longer need the main VirtualBox object lock, but only the respective list
    // lock. In each case, the locking order is defined that the list must be
    // requested before object locks of members of the lists (see the order definitions
    // in AutoLock.h; e.g. LOCKCLASS_LISTOFMACHINES before LOCKCLASS_MACHINEOBJECT).
    RWLockHandle                        lockMachines;
    MachinesOList                       allMachines;

    RWLockHandle                        lockGuestOSTypes;
    GuestOSTypesOList                   allGuestOSTypes;

    // All the media lists are protected by the following locking handle:
    RWLockHandle                        lockMedia;
    MediaOList                          allHardDisks,           // base images only!
                                        allDVDImages,
                                        allFloppyImages;
    // the hard disks map is an additional map sorted by UUID for quick lookup
    // and contains ALL hard disks (base and differencing); it is protected by
    // the same lock as the other media lists above
    HardDiskMap                         mapHardDisks;

    // list of pending machine renames (also protected by media tree lock;
    // see VirtualBox::rememberMachineNameChangeForMedia())
    struct PendingMachineRename
    {
        Utf8Str     strConfigDirOld;
        Utf8Str     strConfigDirNew;
    };
    typedef std::list<PendingMachineRename> PendingMachineRenamesList;
    PendingMachineRenamesList           llPendingMachineRenames;

    RWLockHandle                        lockSharedFolders;
    SharedFoldersOList                  allSharedFolders;

    RWLockHandle                        lockDHCPServers;
    DHCPServersOList                    allDHCPServers;

    RWLockHandle                        mtxProgressOperations;
    ProgressMap                         mapProgressOperations;

    // the following are data for the client watcher thread
    const UPDATEREQTYPE                 updateReq;
    const RTTHREAD                      threadClientWatcher;
    typedef std::list<RTPROCESS> ProcessList;
    ProcessList                         llProcesses;

    // the following are data for the async event thread
    const RTTHREAD                      threadAsyncEvent;
    EventQueue * const                  pAsyncEventQ;
    const ComObjPtr<EventSource>        pEventSource;

#ifdef VBOX_WITH_EXTPACK
    /** The extension pack manager object lives here. */
    const ComObjPtr<ExtPackManager>     ptrExtPackManager;
#endif
};

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

VirtualBox::VirtualBox()
{}

VirtualBox::~VirtualBox()
{}

HRESULT VirtualBox::FinalConstruct()
{
    LogFlowThisFunc(("\n"));

    HRESULT rc = init();

    BaseFinalConstruct();

    return rc;
}

void VirtualBox::FinalRelease()
{
    LogFlowThisFunc(("\n"));

    uninit();

    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the VirtualBox object.
 *
 *  @return COM result code
 */
HRESULT VirtualBox::init()
{
    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* Locking this object for writing during init sounds a bit paradoxical,
     * but in the current locking mess this avoids that some code gets a
     * read lock and later calls code which wants the same write lock. */
    AutoWriteLock lock(this COMMA_LOCKVAL_SRC_POS);

    // allocate our instance data
    m = new Data;

    LogFlow(("===========================================================\n"));
    LogFlowThisFuncEnter();

    if (sVersion.isEmpty())
        sVersion = VBOX_VERSION_STRING;
    sRevision = RTBldCfgRevision();
    if (sPackageType.isEmpty())
        sPackageType = VBOX_PACKAGE_STRING;
    if (sAPIVersion.isEmpty())
        sAPIVersion = VBOX_API_VERSION_STRING;
    LogFlowThisFunc(("Version: %ls, Package: %ls, API Version: %ls\n", sVersion.raw(), sPackageType.raw(), sAPIVersion.raw()));

    /* Get the VirtualBox home directory. */
    {
        char szHomeDir[RTPATH_MAX];
        int vrc = com::GetVBoxUserHomeDirectory(szHomeDir, sizeof(szHomeDir));
        if (RT_FAILURE(vrc))
            return setError(E_FAIL,
                            tr("Could not create the VirtualBox home directory '%s' (%Rrc)"),
                            szHomeDir, vrc);

        unconst(m->strHomeDir) = szHomeDir;
    }

    /* compose the VirtualBox.xml file name */
    unconst(m->strSettingsFilePath) = Utf8StrFmt("%s%c%s",
                                                 m->strHomeDir.c_str(),
                                                 RTPATH_DELIMITER,
                                                 VBOX_GLOBAL_SETTINGS_FILE);
    HRESULT rc = S_OK;
    bool fCreate = false;
    try
    {
        // load and parse VirtualBox.xml; this will throw on XML or logic errors
        try
        {
            m->pMainConfigFile = new settings::MainConfigFile(&m->strSettingsFilePath);
        }
        catch (xml::EIPRTFailure &e)
        {
            // this is thrown by the XML backend if the RTOpen() call fails;
            // only if the main settings file does not exist, create it,
            // if there's something more serious, then do fail!
            if (e.rc() == VERR_FILE_NOT_FOUND)
                fCreate = true;
            else
                throw;
        }

        if (fCreate)
            m->pMainConfigFile = new settings::MainConfigFile(NULL);

#ifdef VBOX_WITH_RESOURCE_USAGE_API
        /* create the performance collector object BEFORE host */
        unconst(m->pPerformanceCollector).createObject();
        rc = m->pPerformanceCollector->init();
        ComAssertComRCThrowRC(rc);
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

        /* create the host object early, machines will need it */
        unconst(m->pHost).createObject();
        rc = m->pHost->init(this);
        ComAssertComRCThrowRC(rc);

        rc = m->pHost->loadSettings(m->pMainConfigFile->host);
        if (FAILED(rc)) throw rc;

        /* create the system properties object, someone may need it too */
        unconst(m->pSystemProperties).createObject();
        rc = m->pSystemProperties->init(this);
        ComAssertComRCThrowRC(rc);

        rc = m->pSystemProperties->loadSettings(m->pMainConfigFile->systemProperties);
        if (FAILED(rc)) throw rc;

        /* guest OS type objects, needed by machines */
        for (size_t i = 0; i < Global::cOSTypes; ++i)
        {
            ComObjPtr<GuestOSType> guestOSTypeObj;
            rc = guestOSTypeObj.createObject();
            if (SUCCEEDED(rc))
            {
                rc = guestOSTypeObj->init(Global::sOSTypes[i]);
                if (SUCCEEDED(rc))
                    m->allGuestOSTypes.addChild(guestOSTypeObj);
            }
            ComAssertComRCThrowRC(rc);
        }

        /* all registered media, needed by machines */
        if (FAILED(rc = initMedia(m->uuidMediaRegistry,
                                  m->pMainConfigFile->mediaRegistry,
                                  Utf8Str::Empty)))     // const Utf8Str &machineFolder
            throw rc;

        /* machines */
        if (FAILED(rc = initMachines()))
            throw rc;


#ifdef DEBUG
        LogFlowThisFunc(("Dumping media backreferences\n"));
        dumpAllBackRefs();
#endif

        /* net services */
        for (settings::DHCPServersList::const_iterator it = m->pMainConfigFile->llDhcpServers.begin();
             it != m->pMainConfigFile->llDhcpServers.end();
             ++it)
        {
            const settings::DHCPServer &data = *it;

            ComObjPtr<DHCPServer> pDhcpServer;
            if (SUCCEEDED(rc = pDhcpServer.createObject()))
                rc = pDhcpServer->init(this, data);
            if (FAILED(rc)) throw rc;

            rc = registerDHCPServer(pDhcpServer, false /* aSaveRegistry */);
            if (FAILED(rc)) throw rc;
        }

        /* events */
        if (SUCCEEDED(rc = unconst(m->pEventSource).createObject()))
            rc = m->pEventSource->init(static_cast<IVirtualBox*>(this));
        if (FAILED(rc)) throw rc;

#ifdef VBOX_WITH_EXTPACK
        /* extension manager */
        rc = unconst(m->ptrExtPackManager).createObject();
        if (SUCCEEDED(rc))
            rc = m->ptrExtPackManager->initExtPackManager(this, VBOXEXTPACKCTX_PER_USER_DAEMON);
        if (FAILED(rc))
            throw rc;
#endif
    }
    catch (HRESULT err)
    {
        /* we assume that error info is set by the thrower */
        rc = err;
    }
    catch (...)
    {
        rc = VirtualBox::handleUnexpectedExceptions(RT_SRC_POS);
    }

    if (SUCCEEDED(rc))
    {
        /* start the client watcher thread */
#if defined(RT_OS_WINDOWS)
        unconst(m->updateReq) = ::CreateEvent(NULL, FALSE, FALSE, NULL);
#elif defined(RT_OS_OS2)
        RTSemEventCreate(&unconst(m->updateReq));
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
        RTSemEventCreate(&unconst(m->updateReq));
#else
# error "Port me!"
#endif
        int vrc = RTThreadCreate(&unconst(m->threadClientWatcher),
                                 ClientWatcher,
                                 (void *)this,
                                 0,
                                 RTTHREADTYPE_MAIN_WORKER,
                                 RTTHREADFLAGS_WAITABLE,
                                 "Watcher");
        ComAssertRC(vrc);
        if (RT_FAILURE(vrc))
            rc = E_FAIL;
    }

    if (SUCCEEDED(rc))
    {
        try
        {
            /* start the async event handler thread */
            int vrc = RTThreadCreate(&unconst(m->threadAsyncEvent),
                                     AsyncEventHandler,
                                     &unconst(m->pAsyncEventQ),
                                     0,
                                     RTTHREADTYPE_MAIN_WORKER,
                                     RTTHREADFLAGS_WAITABLE,
                                     "EventHandler");
            ComAssertRCThrow(vrc, E_FAIL);

            /* wait until the thread sets m->pAsyncEventQ */
            RTThreadUserWait(m->threadAsyncEvent, RT_INDEFINITE_WAIT);
            ComAssertThrow(m->pAsyncEventQ, E_FAIL);
        }
        catch (HRESULT aRC)
        {
            rc = aRC;
        }
    }

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED(rc))
        autoInitSpan.setSucceeded();

#ifdef VBOX_WITH_EXTPACK
    /* Let the extension packs have a go at things. */
    if (SUCCEEDED(rc))
    {
        lock.release();
        m->ptrExtPackManager->callAllVirtualBoxReadyHooks();
    }
#endif

    LogFlowThisFunc(("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    LogFlow(("===========================================================\n"));
    return rc;
}

HRESULT VirtualBox::initMachines()
{
    for (settings::MachinesRegistry::const_iterator it = m->pMainConfigFile->llMachines.begin();
         it != m->pMainConfigFile->llMachines.end();
         ++it)
    {
        HRESULT rc = S_OK;
        const settings::MachineRegistryEntry &xmlMachine = *it;
        Guid uuid = xmlMachine.uuid;

        ComObjPtr<Machine> pMachine;
        if (SUCCEEDED(rc = pMachine.createObject()))
        {
            rc = pMachine->init(this,
                                xmlMachine.strSettingsFile,
                                &uuid);
            if (SUCCEEDED(rc))
                rc = registerMachine(pMachine);
            if (FAILED(rc))
                return rc;
        }
    }

    return S_OK;
}

/**
 * Loads a media registry from XML and adds the media contained therein to
 * the global lists of known media.
 *
 * This now (4.0) gets called from two locations:
 *
 *  --  VirtualBox::init(), to load the global media registry from VirtualBox.xml;
 *
 *  --  Machine::loadMachineDataFromSettings(), to load the per-machine registry
 *      from machine XML, for machines created with VirtualBox 4.0 or later.
 *
 * In both cases, the media found are added to the global lists so the
 * global arrays of media (including the GUI's virtual media manager)
 * continue to work as before.
 *
 * @param uuidMachineRegistry The UUID of the media registry. This is either the
 *       transient UUID created at VirtualBox startup for the global registry or
 *       a machine ID.
 * @param mediaRegistry The XML settings structure to load, either from VirtualBox.xml
 *       or a machine XML.
 * @return
 */
HRESULT VirtualBox::initMedia(const Guid &uuidRegistry,
                              const settings::MediaRegistry mediaRegistry,
                              const Utf8Str &strMachineFolder)
{
    LogFlow(("VirtualBox::initMedia ENTERING, uuidRegistry=%s, strMachineFolder=%s\n",
             uuidRegistry.toString().c_str(),
             strMachineFolder.c_str()));

    AutoWriteLock treeLock(getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;
    settings::MediaList::const_iterator it;
    for (it = mediaRegistry.llHardDisks.begin();
         it != mediaRegistry.llHardDisks.end();
         ++it)
    {
        const settings::Medium &xmlHD = *it;

        ComObjPtr<Medium> pHardDisk;
        if (SUCCEEDED(rc = pHardDisk.createObject()))
            rc = pHardDisk->init(this,
                                 NULL,          // parent
                                 DeviceType_HardDisk,
                                 uuidRegistry,
                                 xmlHD,         // XML data; this recurses to processes the children
                                 strMachineFolder);
        if (FAILED(rc)) return rc;

        rc = registerHardDisk(pHardDisk, NULL /* pllRegistriesThatNeedSaving */);
        if (FAILED(rc)) return rc;
    }

    for (it = mediaRegistry.llDvdImages.begin();
         it != mediaRegistry.llDvdImages.end();
         ++it)
    {
        const settings::Medium &xmlDvd = *it;

        ComObjPtr<Medium> pImage;
        if (SUCCEEDED(pImage.createObject()))
            rc = pImage->init(this,
                              NULL,
                              DeviceType_DVD,
                              uuidRegistry,
                              xmlDvd,
                              strMachineFolder);
        if (FAILED(rc)) return rc;

        rc = registerImage(pImage,
                           DeviceType_DVD,
                           NULL /* pllRegistriesThatNeedSaving */);
        if (FAILED(rc)) return rc;
    }

    for (it = mediaRegistry.llFloppyImages.begin();
         it != mediaRegistry.llFloppyImages.end();
         ++it)
    {
        const settings::Medium &xmlFloppy = *it;

        ComObjPtr<Medium> pImage;
        if (SUCCEEDED(pImage.createObject()))
            rc = pImage->init(this,
                              NULL,
                              DeviceType_Floppy,
                              uuidRegistry,
                              xmlFloppy,
                              strMachineFolder);
        if (FAILED(rc)) return rc;

        rc = registerImage(pImage,
                           DeviceType_Floppy,
                           NULL /* pllRegistriesThatNeedSaving */);
        if (FAILED(rc)) return rc;
    }

    LogFlow(("VirtualBox::initMedia LEAVING\n"));

    return S_OK;
}

void VirtualBox::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    LogFlow(("===========================================================\n"));
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("initFailed()=%d\n", autoUninitSpan.initFailed()));

    /* tell all our child objects we've been uninitialized */

    LogFlowThisFunc(("Uninitializing machines (%d)...\n", m->allMachines.size()));
    if (m->pHost)
    {
        /* It is necessary to hold the VirtualBox and Host locks here because
           we may have to uninitialize SessionMachines. */
        AutoMultiWriteLock2 multilock(this, m->pHost COMMA_LOCKVAL_SRC_POS);
        m->allMachines.uninitAll();
    }
    else
        m->allMachines.uninitAll();
    m->allFloppyImages.uninitAll();
    m->allDVDImages.uninitAll();
    m->allHardDisks.uninitAll();
    m->allDHCPServers.uninitAll();

    m->mapProgressOperations.clear();

    m->allGuestOSTypes.uninitAll();

    /* Note that we release singleton children after we've all other children.
     * In some cases this is important because these other children may use
     * some resources of the singletons which would prevent them from
     * uninitializing (as for example, mSystemProperties which owns
     * MediumFormat objects which Medium objects refer to) */
    if (m->pSystemProperties)
    {
        m->pSystemProperties->uninit();
        unconst(m->pSystemProperties).setNull();
    }

    if (m->pHost)
    {
        m->pHost->uninit();
        unconst(m->pHost).setNull();
    }

#ifdef VBOX_WITH_RESOURCE_USAGE_API
    if (m->pPerformanceCollector)
    {
        m->pPerformanceCollector->uninit();
        unconst(m->pPerformanceCollector).setNull();
    }
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

    LogFlowThisFunc(("Terminating the async event handler...\n"));
    if (m->threadAsyncEvent != NIL_RTTHREAD)
    {
        /* signal to exit the event loop */
        if (RT_SUCCESS(m->pAsyncEventQ->interruptEventQueueProcessing()))
        {
            /*
             *  Wait for thread termination (only after we've successfully
             *  interrupted the event queue processing!)
             */
            int vrc = RTThreadWait(m->threadAsyncEvent, 60000, NULL);
            if (RT_FAILURE(vrc))
                LogWarningFunc(("RTThreadWait(%RTthrd) -> %Rrc\n",
                                m->threadAsyncEvent, vrc));
        }
        else
        {
            AssertMsgFailed(("interruptEventQueueProcessing() failed\n"));
            RTThreadWait(m->threadAsyncEvent, 0, NULL);
        }

        unconst(m->threadAsyncEvent) = NIL_RTTHREAD;
        unconst(m->pAsyncEventQ) = NULL;
    }

    LogFlowThisFunc(("Releasing event source...\n"));
    if (m->pEventSource)
    {
        // we don't perform uninit() as it's possible that some pending event refers to this source
        unconst(m->pEventSource).setNull();
    }

    LogFlowThisFunc(("Terminating the client watcher...\n"));
    if (m->threadClientWatcher != NIL_RTTHREAD)
    {
        /* signal the client watcher thread */
        updateClientWatcher();
        /* wait for the termination */
        RTThreadWait(m->threadClientWatcher, RT_INDEFINITE_WAIT, NULL);
        unconst(m->threadClientWatcher) = NIL_RTTHREAD;
    }
    m->llProcesses.clear();
#if defined(RT_OS_WINDOWS)
    if (m->updateReq != NULL)
    {
        ::CloseHandle(m->updateReq);
        unconst(m->updateReq) = NULL;
    }
#elif defined(RT_OS_OS2)
    if (m->updateReq != NIL_RTSEMEVENT)
    {
        RTSemEventDestroy(m->updateReq);
        unconst(m->updateReq) = NIL_RTSEMEVENT;
    }
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
    if (m->updateReq != NIL_RTSEMEVENT)
    {
        RTSemEventDestroy(m->updateReq);
        unconst(m->updateReq) = NIL_RTSEMEVENT;
    }
#else
# error "Port me!"
#endif

    // clean up our instance data
    delete m;

    /* Unload hard disk plugin backends. */
    VDShutdown();

    LogFlowThisFuncLeave();
    LogFlow(("===========================================================\n"));
}

// IVirtualBox properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP VirtualBox::COMGETTER(Version)(BSTR *aVersion)
{
    CheckComArgNotNull(aVersion);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    sVersion.cloneTo(aVersion);
    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(Revision)(ULONG *aRevision)
{
    CheckComArgNotNull(aRevision);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    *aRevision = sRevision;
    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(PackageType)(BSTR *aPackageType)
{
    CheckComArgNotNull(aPackageType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    sPackageType.cloneTo(aPackageType);
    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(APIVersion)(BSTR *aAPIVersion)
{
    CheckComArgNotNull(aAPIVersion);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    sAPIVersion.cloneTo(aAPIVersion);
    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(HomeFolder)(BSTR *aHomeFolder)
{
    CheckComArgNotNull(aHomeFolder);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mHomeDir is const and doesn't need a lock */
    m->strHomeDir.cloneTo(aHomeFolder);
    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(SettingsFilePath)(BSTR *aSettingsFilePath)
{
    CheckComArgNotNull(aSettingsFilePath);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mCfgFile.mName is const and doesn't need a lock */
    m->strSettingsFilePath.cloneTo(aSettingsFilePath);
    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(Host)(IHost **aHost)
{
    CheckComArgOutSafeArrayPointerValid(aHost);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mHost is const, no need to lock */
    m->pHost.queryInterfaceTo(aHost);
    return S_OK;
}

STDMETHODIMP
VirtualBox::COMGETTER(SystemProperties)(ISystemProperties **aSystemProperties)
{
    CheckComArgOutSafeArrayPointerValid(aSystemProperties);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mSystemProperties is const, no need to lock */
    m->pSystemProperties.queryInterfaceTo(aSystemProperties);
    return S_OK;
}

STDMETHODIMP
VirtualBox::COMGETTER(Machines)(ComSafeArrayOut(IMachine *, aMachines))
{
    if (ComSafeArrayOutIsNull(aMachines))
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock al(m->allMachines.getLockHandle() COMMA_LOCKVAL_SRC_POS);
    SafeIfaceArray<IMachine> machines(m->allMachines.getList());
    machines.detachTo(ComSafeArrayOutArg(aMachines));

    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(HardDisks)(ComSafeArrayOut(IMedium *, aHardDisks))
{
    if (ComSafeArrayOutIsNull(aHardDisks))
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock al(m->allHardDisks.getLockHandle() COMMA_LOCKVAL_SRC_POS);
    SafeIfaceArray<IMedium> hardDisks(m->allHardDisks.getList());
    hardDisks.detachTo(ComSafeArrayOutArg(aHardDisks));

    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(DVDImages)(ComSafeArrayOut(IMedium *, aDVDImages))
{
    if (ComSafeArrayOutIsNull(aDVDImages))
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock al(m->allDVDImages.getLockHandle() COMMA_LOCKVAL_SRC_POS);
    SafeIfaceArray<IMedium> images(m->allDVDImages.getList());
    images.detachTo(ComSafeArrayOutArg(aDVDImages));

    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(FloppyImages)(ComSafeArrayOut(IMedium *, aFloppyImages))
{
    if (ComSafeArrayOutIsNull(aFloppyImages))
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock al(m->allFloppyImages.getLockHandle() COMMA_LOCKVAL_SRC_POS);
    SafeIfaceArray<IMedium> images(m->allFloppyImages.getList());
    images.detachTo(ComSafeArrayOutArg(aFloppyImages));

    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(ProgressOperations)(ComSafeArrayOut(IProgress *, aOperations))
{
    CheckComArgOutSafeArrayPointerValid(aOperations);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* protect mProgressOperations */
    AutoReadLock safeLock(m->mtxProgressOperations COMMA_LOCKVAL_SRC_POS);
    SafeIfaceArray<IProgress> progress(m->mapProgressOperations);
    progress.detachTo(ComSafeArrayOutArg(aOperations));

    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(GuestOSTypes)(ComSafeArrayOut(IGuestOSType *, aGuestOSTypes))
{
    CheckComArgOutSafeArrayPointerValid(aGuestOSTypes);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock al(m->allGuestOSTypes.getLockHandle() COMMA_LOCKVAL_SRC_POS);
    SafeIfaceArray<IGuestOSType> ostypes(m->allGuestOSTypes.getList());
    ostypes.detachTo(ComSafeArrayOutArg(aGuestOSTypes));

    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(SharedFolders)(ComSafeArrayOut(ISharedFolder *, aSharedFolders))
{
#ifndef RT_OS_WINDOWS
    NOREF(aSharedFoldersSize);
#endif /* RT_OS_WINDOWS */

    CheckComArgOutSafeArrayPointerValid(aSharedFolders);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    return setError(E_NOTIMPL, "Not yet implemented");
}

STDMETHODIMP
VirtualBox::COMGETTER(PerformanceCollector)(IPerformanceCollector **aPerformanceCollector)
{
#ifdef VBOX_WITH_RESOURCE_USAGE_API
    CheckComArgOutSafeArrayPointerValid(aPerformanceCollector);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mPerformanceCollector is const, no need to lock */
    m->pPerformanceCollector.queryInterfaceTo(aPerformanceCollector);

    return S_OK;
#else /* !VBOX_WITH_RESOURCE_USAGE_API */
    ReturnComNotImplemented();
#endif /* !VBOX_WITH_RESOURCE_USAGE_API */
}

STDMETHODIMP
VirtualBox::COMGETTER(DHCPServers)(ComSafeArrayOut(IDHCPServer *, aDHCPServers))
{
    if (ComSafeArrayOutIsNull(aDHCPServers))
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock al(m->allDHCPServers.getLockHandle() COMMA_LOCKVAL_SRC_POS);
    SafeIfaceArray<IDHCPServer> svrs(m->allDHCPServers.getList());
    svrs.detachTo(ComSafeArrayOutArg(aDHCPServers));

    return S_OK;
}

STDMETHODIMP
VirtualBox::COMGETTER(EventSource)(IEventSource ** aEventSource)
{
    CheckComArgOutPointerValid(aEventSource);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* event source is const, no need to lock */
    m->pEventSource.queryInterfaceTo(aEventSource);

    return S_OK;
}

STDMETHODIMP
VirtualBox::COMGETTER(ExtensionPackManager)(IExtPackManager **aExtPackManager)
{
    CheckComArgOutPointerValid(aExtPackManager);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
#ifdef VBOX_WITH_EXTPACK
        /* The extension pack manager is const, no need to lock. */
        hrc = m->ptrExtPackManager.queryInterfaceTo(aExtPackManager);
#else
        hrc = E_NOTIMPL;
#endif
    }

    return hrc;
}

STDMETHODIMP VirtualBox::COMGETTER(InternalNetworks)(ComSafeArrayOut(BSTR, aInternalNetworks))
{
    if (ComSafeArrayOutIsNull(aInternalNetworks))
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    std::list<Bstr> allInternalNetworks;

    /* get copy of all machine references, to avoid holding the list lock */
    MachinesOList::MyList allMachines;
    {
        AutoReadLock al(m->allMachines.getLockHandle() COMMA_LOCKVAL_SRC_POS);
        allMachines = m->allMachines.getList();
    }
    for (MachinesOList::MyList::const_iterator it = allMachines.begin();
         it != allMachines.end();
         ++it)
    {
        const ComObjPtr<Machine> &pMachine = *it;
        AutoCaller autoMachineCaller(pMachine);
        if (FAILED(autoMachineCaller.rc()))
            continue;
        AutoReadLock mlock(pMachine COMMA_LOCKVAL_SRC_POS);

        if (pMachine->isAccessible())
        {
            uint32_t cNetworkAdapters = Global::getMaxNetworkAdapters(pMachine->getChipsetType());
            for (ULONG i = 0; i < cNetworkAdapters; i++)
            {
                ComPtr<INetworkAdapter> pNet;
                HRESULT rc = pMachine->GetNetworkAdapter(i, pNet.asOutParam());
                if (FAILED(rc) || pNet.isNull())
                    continue;
                Bstr strInternalNetwork;
                rc = pNet->COMGETTER(InternalNetwork)(strInternalNetwork.asOutParam());
                if (FAILED(rc) || strInternalNetwork.isEmpty())
                    continue;

                allInternalNetworks.push_back(strInternalNetwork);
            }
        }
    }

    /* throw out any duplicates */
    allInternalNetworks.sort();
    allInternalNetworks.unique();
    com::SafeArray<BSTR> internalNetworks(allInternalNetworks.size());
    size_t i = 0;
    for (std::list<Bstr>::const_iterator it = allInternalNetworks.begin();
         it != allInternalNetworks.end();
         ++it, i++)
    {
        const Bstr &tmp = *it;
        tmp.cloneTo(&internalNetworks[i]);
    }
    internalNetworks.detachTo(ComSafeArrayOutArg(aInternalNetworks));

    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(GenericNetworkDrivers)(ComSafeArrayOut(BSTR, aGenericNetworkDrivers))
{
    if (ComSafeArrayOutIsNull(aGenericNetworkDrivers))
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    std::list<Bstr> allGenericNetworkDrivers;

    /* get copy of all machine references, to avoid holding the list lock */
    MachinesOList::MyList allMachines;
    {
        AutoReadLock al(m->allMachines.getLockHandle() COMMA_LOCKVAL_SRC_POS);
        allMachines = m->allMachines.getList();
    }
    for (MachinesOList::MyList::const_iterator it = allMachines.begin();
         it != allMachines.end();
         ++it)
    {
        const ComObjPtr<Machine> &pMachine = *it;
        AutoCaller autoMachineCaller(pMachine);
        if (FAILED(autoMachineCaller.rc()))
            continue;
        AutoReadLock mlock(pMachine COMMA_LOCKVAL_SRC_POS);

        if (pMachine->isAccessible())
        {
            uint32_t cNetworkAdapters = Global::getMaxNetworkAdapters(pMachine->getChipsetType());
            for (ULONG i = 0; i < cNetworkAdapters; i++)
            {
                ComPtr<INetworkAdapter> pNet;
                HRESULT rc = pMachine->GetNetworkAdapter(i, pNet.asOutParam());
                if (FAILED(rc) || pNet.isNull())
                    continue;
                Bstr strGenericNetworkDriver;
                rc = pNet->COMGETTER(GenericDriver)(strGenericNetworkDriver.asOutParam());
                if (FAILED(rc) || strGenericNetworkDriver.isEmpty())
                    continue;

                allGenericNetworkDrivers.push_back(strGenericNetworkDriver);
            }
        }
    }

    /* throw out any duplicates */
    allGenericNetworkDrivers.sort();
    allGenericNetworkDrivers.unique();
    com::SafeArray<BSTR> genericNetworks(allGenericNetworkDrivers.size());
    size_t i = 0;
    for (std::list<Bstr>::const_iterator it = allGenericNetworkDrivers.begin();
         it != allGenericNetworkDrivers.end();
         ++it, i++)
    {
        const Bstr &tmp = *it;
        tmp.cloneTo(&genericNetworks[i]);
    }
    genericNetworks.detachTo(ComSafeArrayOutArg(aGenericNetworkDrivers));

    return S_OK;
}

STDMETHODIMP
VirtualBox::CheckFirmwarePresent(FirmwareType_T aFirmwareType,
                                 IN_BSTR        aVersion,
                                 BSTR           *aUrl,
                                 BSTR           *aFile,
                                 BOOL           *aResult)
{
    CheckComArgNotNull(aResult);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    NOREF(aVersion);

    static const struct
    {
        FirmwareType_T type;
        const char*    fileName;
        const char*    url;
    }
    firmwareDesc[] =
    {
        {
            /* compiled-in firmware */
            FirmwareType_BIOS,    NULL,             NULL
        },
        {
            FirmwareType_EFI32,   "VBoxEFI32.fd",   "http://virtualbox.org/firmware/VBoxEFI32.fd"
        },
        {
            FirmwareType_EFI64,   "VBoxEFI64.fd",   "http://virtualbox.org/firmware/VBoxEFI64.fd"
        },
        {
            FirmwareType_EFIDUAL, "VBoxEFIDual.fd", "http://virtualbox.org/firmware/VBoxEFIDual.fd"
        }
    };

    for (size_t i = 0; i < sizeof(firmwareDesc) / sizeof(firmwareDesc[0]); i++)
    {
        if (aFirmwareType != firmwareDesc[i].type)
            continue;

        /* compiled-in firmware */
        if (firmwareDesc[i].fileName == NULL)
        {
            *aResult = TRUE;
            break;
        }

        Utf8Str shortName, fullName;

        shortName = Utf8StrFmt("Firmware%c%s",
                               RTPATH_DELIMITER,
                               firmwareDesc[i].fileName);
        int rc = calculateFullPath(shortName, fullName);
        AssertRCReturn(rc, rc);
        if (RTFileExists(fullName.c_str()))
        {
            *aResult = TRUE;
            if (aFile)
                Utf8Str(fullName).cloneTo(aFile);
            break;
        }

        char pszVBoxPath[RTPATH_MAX];
        rc = RTPathExecDir(pszVBoxPath, RTPATH_MAX);
        AssertRCReturn(rc, rc);
        fullName = Utf8StrFmt("%s%c%s",
                              pszVBoxPath,
                              RTPATH_DELIMITER,
                              firmwareDesc[i].fileName);
        if (RTFileExists(fullName.c_str()))
        {
            *aResult = TRUE;
            if (aFile)
                Utf8Str(fullName).cloneTo(aFile);
            break;
        }

        /** @todo: account for version in the URL */
        if (aUrl != NULL)
        {
            Utf8Str strUrl(firmwareDesc[i].url);
            strUrl.cloneTo(aUrl);
        }
        *aResult = FALSE;

        /* Assume single record per firmware type */
        break;
    }

    return S_OK;
}
// IVirtualBox methods
/////////////////////////////////////////////////////////////////////////////

/* Helper for VirtualBox::ComposeMachineFilename */
static void sanitiseMachineFilename(Utf8Str &aName);

STDMETHODIMP VirtualBox::ComposeMachineFilename(IN_BSTR aName,
                                                IN_BSTR aBaseFolder,
                                                BSTR *aFilename)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aName=\"%ls\",aBaseFolder=\"%ls\"\n", aName, aBaseFolder));

    CheckComArgStrNotEmptyOrNull(aName);
    CheckComArgOutPointerValid(aFilename);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Compose the settings file name using the following scheme:
     *
     *     <base_folder>/<machine_name>/<machine_name>.xml
     *
     * If a non-null and non-empty base folder is specified, the default
     * machine folder will be used as a base folder.
     * We sanitise the machine name to a safe white list of characters before
     * using it.
     */
    Utf8Str strBase = aBaseFolder;
    Utf8Str strName = aName;
    sanitiseMachineFilename(strName);

    if (strBase.isEmpty())
        /* we use the non-full folder value below to keep the path relative */
        getDefaultMachineFolder(strBase);

    calculateFullPath(strBase, strBase);

    Bstr bstrSettingsFile = BstrFmt("%s%c%s%c%s.vbox",
                                    strBase.c_str(),
                                    RTPATH_DELIMITER,
                                    strName.c_str(),
                                    RTPATH_DELIMITER,
                                    strName.c_str());

#if 0  /* Try to get a unique name. */
    for (unsigned i = 1; RTFileExists(bstrSettingsFile.c_str() && i < 100; ++i)
        bstrSettingsFile = BstrFmt("%s%c%s%u%c%s%u.vbox",
                                   strBase.c_str(),
                                   RTPATH_DELIMITER,
                                   strName.c_str(), i,
                                   RTPATH_DELIMITER,
                                   strName.c_str());
#endif

    bstrSettingsFile.detachTo(aFilename);

    return S_OK;
}

/**
 * Remove characters from a machine file name which can be problematic on
 * particular systems.
 * @param  strName  The file name to sanitise.
 */
void sanitiseMachineFilename(Utf8Str &strName)
{
    /** Set of characters which should be safe for use in filenames: some basic
     * ASCII, Unicode from Latin-1 alphabetic to the end of Hangul.  We try to
     * skip anything that could count as a control character in Windows or
     * *nix, or be otherwise difficult for shells to handle (I would have
     * preferred to remove the space and brackets too).  We also remove all
     * characters which need UTF-16 surrogate pairs for Windows's benefit. */
    RTUNICP aCpSet[] =
        { ' ', ' ', '(', ')', '-', '.', '0', '9', 'A', 'Z', 'a', 'z', '_', '_',
          0xa0, 0xd7af, '\0' };
    char *pszName = strName.mutableRaw();
    Assert(RTStrPurgeComplementSet(pszName, aCpSet, '_') >= 0);
    /* No leading dot or dash. */
    if (pszName[0] == '.' || pszName[0] == '-')
        pszName[0] = '_';
    /* No trailing dot. */
    if (pszName[strName.length() - 1] == '.')
        pszName[strName.length() - 1] = '_';
    /* Mangle leading and trailing spaces. */
    for (size_t i = 0; pszName[i] == ' '; ++i)
       pszName[i] = '_';
    for (size_t i = strName.length() - 1; i && pszName[i] == ' '; --i)
       pszName[i] = '_';
}

/** Simple unit test/operation examples for sanitiseMachineFilename(). */
static unsigned testSanitiseMachineFilename(void (*pfnPrintf)(const char *, ...))
{
    unsigned cErrors = 0;

    /** Expected results of sanitising given file names. */
    static struct
    {
        /** The test file name to be sanitised (Utf-8). */
        const char *pcszIn;
        /** The expected sanitised output (Utf-8). */
        const char *pcszOutExpected;
    } aTest[] =
    {
        { "OS/2 2.1", "OS_2 2.1" },
        { "-!My VM!-", "__My VM_-" },
        { "\xF0\x90\x8C\xB0", "____" },
        { "  My VM  ", "__My VM__" },
        { ".My VM.", "_My VM_" },
        { "My VM", "My VM" }
    };
    for (unsigned i = 0; i < RT_ELEMENTS(aTest); ++i)
    {
        Utf8Str str(aTest[i].pcszIn);
        sanitiseMachineFilename(str);
        if (str.compare(aTest[i].pcszOutExpected))
        {
            ++cErrors;
            pfnPrintf("%s: line %d, expected %s, actual %s\n",
                      __PRETTY_FUNCTION__, i, aTest[i].pcszOutExpected,
                      str.c_str());
        }
    }
    return cErrors;
}

#ifdef DEBUG
/** @todo Proper testcase. */
/** @todo Do we have a better method of doing init functions? */
namespace
{
    class TestSanitiseMachineFilename
    {
    public:
        TestSanitiseMachineFilename(void)
        {
            Assert(!testSanitiseMachineFilename(RTAssertMsg2));
        }
    };
    TestSanitiseMachineFilename s_TestSanitiseMachineFilename;
}
#endif

/** @note Locks mSystemProperties object for reading. */
STDMETHODIMP VirtualBox::CreateMachine(IN_BSTR aSettingsFile,
                                       IN_BSTR aName,
                                       IN_BSTR aOsTypeId,
                                       IN_BSTR aId,
                                       BOOL forceOverwrite,
                                       IMachine **aMachine)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aSettingsFile=\"%ls\", aName=\"%ls\", aOsTypeId =\"%ls\"\n", aSettingsFile, aName, aOsTypeId));

    CheckComArgStrNotEmptyOrNull(aName);
    /** @todo tighten checks on aId? */
    CheckComArgOutPointerValid(aMachine);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* NULL settings file means compose automatically */
    HRESULT rc;
    Bstr bstrSettingsFile(aSettingsFile);
    if (bstrSettingsFile.isEmpty())
    {
        rc = ComposeMachineFilename(aName,
                                    NULL,
                                    bstrSettingsFile.asOutParam());
        if (FAILED(rc)) return rc;
    }

    /* create a new object */
    ComObjPtr<Machine> machine;
    rc = machine.createObject();
    if (FAILED(rc)) return rc;

    /* Create UUID if an empty one was specified. */
    Guid id(aId);
    if (id.isEmpty())
        id.create();

    GuestOSType *osType = NULL;
    rc = findGuestOSType(Bstr(aOsTypeId), osType);
    if (FAILED(rc)) return rc;

    /* initialize the machine object */
    rc = machine->init(this,
                       Utf8Str(bstrSettingsFile),
                       Utf8Str(aName),
                       osType,
                       id,
                       !!forceOverwrite);
    if (SUCCEEDED(rc))
    {
        /* set the return value */
        rc = machine.queryInterfaceTo(aMachine);
        AssertComRC(rc);

#ifdef VBOX_WITH_EXTPACK
        /* call the extension pack hooks */
        m->ptrExtPackManager->callAllVmCreatedHooks(machine);
#endif
    }

    LogFlowThisFuncLeave();

    return rc;
}

STDMETHODIMP VirtualBox::OpenMachine(IN_BSTR aSettingsFile,
                                     IMachine **aMachine)
{
    CheckComArgStrNotEmptyOrNull(aSettingsFile);
    CheckComArgOutSafeArrayPointerValid(aMachine);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = E_FAIL;

    /* create a new object */
    ComObjPtr<Machine> machine;
    rc = machine.createObject();
    if (SUCCEEDED(rc))
    {
        /* initialize the machine object */
        rc = machine->init(this,
                           aSettingsFile,
                           NULL);       /* const Guid *aId */
        if (SUCCEEDED(rc))
        {
            /* set the return value */
            rc = machine.queryInterfaceTo(aMachine);
            ComAssertComRC(rc);
        }
    }

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::RegisterMachine(IMachine *aMachine)
{
    CheckComArgNotNull(aMachine);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc;

    Bstr name;
    rc = aMachine->COMGETTER(Name)(name.asOutParam());
    if (FAILED(rc)) return rc;

    /* We can safely cast child to Machine * here because only Machine
     * implementations of IMachine can be among our children. */
    Machine *pMachine = static_cast<Machine*>(aMachine);

    AutoCaller machCaller(pMachine);
    ComAssertComRCRetRC(machCaller.rc());

    rc = registerMachine(pMachine);
    /* fire an event */
    if (SUCCEEDED(rc))
        onMachineRegistered(pMachine->getId(), TRUE);

    return rc;
}

/** @note Locks this object for reading, then some machine objects for reading. */
STDMETHODIMP VirtualBox::FindMachine(IN_BSTR aNameOrId, IMachine **aMachine)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aName=\"%ls\", aMachine={%p}\n", aNameOrId, aMachine));

    CheckComArgStrNotEmptyOrNull(aNameOrId);
    CheckComArgOutSafeArrayPointerValid(aMachine);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* start with not found */
    HRESULT rc = S_OK;
    ComObjPtr<Machine> pMachineFound;

    Guid id(aNameOrId);
    if (!id.isEmpty())
        rc = findMachine(id,
                         true /* fPermitInaccessible */,
                         true /* setError */,
                         &pMachineFound);
                // returns VBOX_E_OBJECT_NOT_FOUND if not found and sets error
    else
    {
        Utf8Str strName(aNameOrId);
        AutoReadLock al(m->allMachines.getLockHandle() COMMA_LOCKVAL_SRC_POS);
        for (MachinesOList::iterator it = m->allMachines.begin();
             it != m->allMachines.end();
             ++it)
        {
            ComObjPtr<Machine> &pMachine2 = *it;
            AutoCaller machCaller(pMachine2);
            if (machCaller.rc())
                continue;       // we can't ask inaccessible machines for their names

            AutoReadLock machLock(pMachine2 COMMA_LOCKVAL_SRC_POS);
            if (pMachine2->getName() == strName)
            {
                pMachineFound = pMachine2;
                break;
            }
            if (!RTPathCompare(pMachine2->getSettingsFileFull().c_str(), strName.c_str()))
            {
                pMachineFound = pMachine2;
                break;
            }
        }

        if (!pMachineFound)
            rc = setError(VBOX_E_OBJECT_NOT_FOUND,
                          tr("Could not find a registered machine named '%ls'"), aNameOrId);
    }

    /* this will set (*machine) to NULL if machineObj is null */
    pMachineFound.queryInterfaceTo(aMachine);

    LogFlowThisFunc(("aName=\"%ls\", aMachine=%p, rc=%08X\n", aNameOrId, *aMachine, rc));
    LogFlowThisFuncLeave();

    return rc;
}

STDMETHODIMP VirtualBox::CreateHardDisk(IN_BSTR aFormat,
                                        IN_BSTR aLocation,
                                        IMedium **aHardDisk)
{
    CheckComArgOutPointerValid(aHardDisk);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* we don't access non-const data members so no need to lock */

    Utf8Str format(aFormat);
    if (format.isEmpty())
        getDefaultHardDiskFormat(format);

    ComObjPtr<Medium> hardDisk;
    hardDisk.createObject();
    HRESULT rc = hardDisk->init(this,
                                format,
                                aLocation,
                                Guid::Empty,   // media registry: none yet
                                NULL /* pllRegistriesThatNeedSaving */);

    if (SUCCEEDED(rc))
        hardDisk.queryInterfaceTo(aHardDisk);

    return rc;
}

STDMETHODIMP VirtualBox::OpenMedium(IN_BSTR aLocation,
                                    DeviceType_T deviceType,
                                    AccessMode_T accessMode,
                                    BOOL fForceNewUuid,
                                    IMedium **aMedium)
{
    CheckComArgStrNotEmptyOrNull(aLocation);
    CheckComArgOutSafeArrayPointerValid(aMedium);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ComObjPtr<Medium> pMedium;

    // have to get write lock as the whole find/update sequence must be done
    // in one critical section, otherwise there are races which can lead to
    // multiple Medium objects with the same content
    AutoWriteLock treeLock(getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

    // check if the device type is correct, and see if a medium for the
    // given path has already initialized; if so, return that
    switch (deviceType)
    {
        case DeviceType_HardDisk:
            findHardDiskByLocation(aLocation,
                                   false, /* aSetError */
                                   &pMedium);
        break;

        case DeviceType_Floppy:
        case DeviceType_DVD:
            findDVDOrFloppyImage(deviceType,
                                 NULL, /* guid */
                                 aLocation,
                                 false, /* aSetError */
                                 &pMedium);

            // enforce read-only for DVDs even if caller specified ReadWrite
            if (deviceType == DeviceType_DVD)
                accessMode = AccessMode_ReadOnly;
        break;

        default:
            return setError(E_INVALIDARG, "Device type must be HardDisk, DVD or Floppy");
    }

    HRESULT rc = S_OK;

    if (pMedium.isNull())
    {
        pMedium.createObject();
        rc = pMedium->init(this,
                           aLocation,
                           (accessMode == AccessMode_ReadWrite) ? Medium::OpenReadWrite : Medium::OpenReadOnly,
                           fForceNewUuid,
                           deviceType);

        if (SUCCEEDED(rc))
        {
            switch (deviceType)
            {
                case DeviceType_HardDisk:
                    rc = registerHardDisk(pMedium, NULL /* pllRegistriesThatNeedSaving */);
                break;

                case DeviceType_DVD:
                case DeviceType_Floppy:
                    rc = registerImage(pMedium,
                                       deviceType,
                                       NULL /* pllRegistriesThatNeedSaving */);
                break;
            }

            treeLock.release();

            /* Note that it's important to call uninit() on failure to register
             * because the differencing hard disk would have been already associated
             * with the parent and this association needs to be broken. */

            if (FAILED(rc))
                pMedium->uninit();
        }
    }

    if (SUCCEEDED(rc))
        pMedium.queryInterfaceTo(aMedium);

    return rc;
}

STDMETHODIMP VirtualBox::FindMedium(IN_BSTR   aLocation,
                                    DeviceType_T aDeviceType,
                                    IMedium **aMedium)
{
    CheckComArgStrNotEmptyOrNull(aLocation);
    CheckComArgOutSafeArrayPointerValid(aMedium);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    Guid id(aLocation);
    Utf8Str strLocation(aLocation);

    HRESULT rc;
    ComObjPtr<Medium> pMedium;

    switch (aDeviceType)
    {
        case DeviceType_HardDisk:
            if (!id.isEmpty())
                rc = findHardDiskById(id, true /* setError */, &pMedium);
            else
                rc = findHardDiskByLocation(strLocation, true /* setError */, &pMedium);
        break;

        case DeviceType_Floppy:
        case DeviceType_DVD:
            if (!id.isEmpty())
                rc = findDVDOrFloppyImage(aDeviceType, &id, Utf8Str::Empty, true /* setError */, &pMedium);
            else
                rc = findDVDOrFloppyImage(aDeviceType, NULL, strLocation, true /* setError */, &pMedium);
        break;

        default:
            return setError(E_INVALIDARG,
                            tr("Invalid device type %d"), aDeviceType);
    }

    /* the below will set *aHardDisk to NULL if hardDisk is null */
    pMedium.queryInterfaceTo(aMedium);

    return rc;
}

/** @note Locks this object for reading. */
STDMETHODIMP VirtualBox::GetGuestOSType(IN_BSTR aId, IGuestOSType **aType)
{
    CheckComArgNotNull(aType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    *aType = NULL;

    AutoReadLock alock(m->allGuestOSTypes.getLockHandle() COMMA_LOCKVAL_SRC_POS);
    for (GuestOSTypesOList::iterator it = m->allGuestOSTypes.begin();
         it != m->allGuestOSTypes.end();
         ++it)
    {
        const Bstr &typeId = (*it)->id();
        AssertMsg(!typeId.isEmpty(), ("ID must not be NULL"));
        if (typeId.compare(aId, Bstr::CaseInsensitive) == 0)
        {
            (*it).queryInterfaceTo(aType);
            break;
        }
    }

    return (*aType) ? S_OK :
        setError(E_INVALIDARG,
                 tr("'%ls' is not a valid Guest OS type"),
                 aId);
}

STDMETHODIMP VirtualBox::CreateSharedFolder(IN_BSTR aName,        IN_BSTR aHostPath,
                                            BOOL /* aWritable */, BOOL /* aAutoMount */)
{
    CheckComArgStrNotEmptyOrNull(aName);
    CheckComArgStrNotEmptyOrNull(aHostPath);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    return setError(E_NOTIMPL, "Not yet implemented");
}

STDMETHODIMP VirtualBox::RemoveSharedFolder(IN_BSTR aName)
{
    CheckComArgStrNotEmptyOrNull(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    return setError(E_NOTIMPL, "Not yet implemented");
}

/**
 *  @note Locks this object for reading.
 */
STDMETHODIMP VirtualBox::GetExtraDataKeys(ComSafeArrayOut(BSTR, aKeys))
{
    using namespace settings;

    if (ComSafeArrayOutIsNull(aKeys))
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    com::SafeArray<BSTR> saKeys(m->pMainConfigFile->mapExtraDataItems.size());
    int i = 0;
    for (StringsMap::const_iterator it = m->pMainConfigFile->mapExtraDataItems.begin();
         it != m->pMainConfigFile->mapExtraDataItems.end();
         ++it, ++i)
    {
        const Utf8Str &strName = it->first;     // the key
        strName.cloneTo(&saKeys[i]);
    }
    saKeys.detachTo(ComSafeArrayOutArg(aKeys));

    return S_OK;
}

/**
 *  @note Locks this object for reading.
 */
STDMETHODIMP VirtualBox::GetExtraData(IN_BSTR aKey,
                                      BSTR *aValue)
{
    CheckComArgStrNotEmptyOrNull(aKey);
    CheckComArgNotNull(aValue);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* start with nothing found */
    Utf8Str strKey(aKey);
    Bstr bstrResult;

    settings::StringsMap::const_iterator it = m->pMainConfigFile->mapExtraDataItems.find(strKey);
    if (it != m->pMainConfigFile->mapExtraDataItems.end())
        // found:
        bstrResult = it->second; // source is a Utf8Str

    /* return the result to caller (may be empty) */
    bstrResult.cloneTo(aValue);

    return S_OK;
}

/**
 *  @note Locks this object for writing.
 */
STDMETHODIMP VirtualBox::SetExtraData(IN_BSTR aKey,
                                      IN_BSTR aValue)
{
    CheckComArgStrNotEmptyOrNull(aKey);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    Utf8Str strKey(aKey);
    Utf8Str strValue(aValue);
    Utf8Str strOldValue;            // empty

    // locking note: we only hold the read lock briefly to look up the old value,
    // then release it and call the onExtraCanChange callbacks. There is a small
    // chance of a race insofar as the callback might be called twice if two callers
    // change the same key at the same time, but that's a much better solution
    // than the deadlock we had here before. The actual changing of the extradata
    // is then performed under the write lock and race-free.

    // look up the old value first; if nothing has changed then we need not do anything
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS); // hold read lock only while looking up
        settings::StringsMap::const_iterator it = m->pMainConfigFile->mapExtraDataItems.find(strKey);
        if (it != m->pMainConfigFile->mapExtraDataItems.end())
            strOldValue = it->second;
    }

    bool fChanged;
    if ((fChanged = (strOldValue != strValue)))
    {
        // ask for permission from all listeners outside the locks;
        // onExtraDataCanChange() only briefly requests the VirtualBox
        // lock to copy the list of callbacks to invoke
        Bstr error;
        Bstr bstrValue(aValue);

        if (!onExtraDataCanChange(Guid::Empty, aKey, bstrValue.raw(), error))
        {
            const char *sep = error.isEmpty() ? "" : ": ";
            CBSTR err = error.raw();
            LogWarningFunc(("Someone vetoed! Change refused%s%ls\n",
                            sep, err));
            return setError(E_ACCESSDENIED,
                            tr("Could not set extra data because someone refused the requested change of '%ls' to '%ls'%s%ls"),
                            aKey,
                            bstrValue.raw(),
                            sep,
                            err);
        }

        // data is changing and change not vetoed: then write it out under the lock

        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        if (strValue.isEmpty())
            m->pMainConfigFile->mapExtraDataItems.erase(strKey);
        else
            m->pMainConfigFile->mapExtraDataItems[strKey] = strValue;
                // creates a new key if needed

        /* save settings on success */
        HRESULT rc = saveSettings();
        if (FAILED(rc)) return rc;
    }

    // fire notification outside the lock
    if (fChanged)
        onExtraDataChange(Guid::Empty, aKey, aValue);

    return S_OK;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG
void VirtualBox::dumpAllBackRefs()
{
    {
        AutoReadLock al(m->allHardDisks.getLockHandle() COMMA_LOCKVAL_SRC_POS);
        for (MediaList::const_iterator mt = m->allHardDisks.begin();
             mt != m->allHardDisks.end();
             ++mt)
        {
            ComObjPtr<Medium> pMedium = *mt;
            pMedium->dumpBackRefs();
        }
    }
    {
        AutoReadLock al(m->allDVDImages.getLockHandle() COMMA_LOCKVAL_SRC_POS);
        for (MediaList::const_iterator mt = m->allDVDImages.begin();
             mt != m->allDVDImages.end();
             ++mt)
        {
            ComObjPtr<Medium> pMedium = *mt;
            pMedium->dumpBackRefs();
        }
    }
}
#endif

/**
 *  Posts an event to the event queue that is processed asynchronously
 *  on a dedicated thread.
 *
 *  Posting events to the dedicated event queue is useful to perform secondary
 *  actions outside any object locks -- for example, to iterate over a list
 *  of callbacks and inform them about some change caused by some object's
 *  method call.
 *
 *  @param event    event to post; must have been allocated using |new|, will
 *                  be deleted automatically by the event thread after processing
 *
 *  @note Doesn't lock any object.
 */
HRESULT VirtualBox::postEvent(Event *event)
{
    AssertReturn(event, E_FAIL);

    HRESULT rc;
    AutoCaller autoCaller(this);
    if (SUCCEEDED((rc = autoCaller.rc())))
    {
        if (autoCaller.state() != Ready)
            LogWarningFunc(("VirtualBox has been uninitialized (state=%d), the event is discarded!\n",
                            autoCaller.state()));
            // return S_OK
        else if (    (m->pAsyncEventQ)
                  && (m->pAsyncEventQ->postEvent(event))
                )
            return S_OK;
        else
            rc = E_FAIL;
    }

    // in any event of failure, we must clean up here, or we'll leak;
    // the caller has allocated the object using new()
    delete event;
    return rc;
}

/**
 * Adds a progress to the global collection of pending operations.
 * Usually gets called upon progress object initialization.
 *
 * @param aProgress Operation to add to the collection.
 *
 * @note Doesn't lock objects.
 */
HRESULT VirtualBox::addProgress(IProgress *aProgress)
{
    CheckComArgNotNull(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    Bstr id;
    HRESULT rc = aProgress->COMGETTER(Id)(id.asOutParam());
    AssertComRCReturnRC(rc);

    /* protect mProgressOperations */
    AutoWriteLock safeLock(m->mtxProgressOperations COMMA_LOCKVAL_SRC_POS);

    m->mapProgressOperations.insert(ProgressMap::value_type(Guid(id), aProgress));
    return S_OK;
}

/**
 * Removes the progress from the global collection of pending operations.
 * Usually gets called upon progress completion.
 *
 * @param aId   UUID of the progress operation to remove
 *
 * @note Doesn't lock objects.
 */
HRESULT VirtualBox::removeProgress(IN_GUID aId)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ComPtr<IProgress> progress;

    /* protect mProgressOperations */
    AutoWriteLock safeLock(m->mtxProgressOperations COMMA_LOCKVAL_SRC_POS);

    size_t cnt = m->mapProgressOperations.erase(aId);
    Assert(cnt == 1);
    NOREF(cnt);

    return S_OK;
}

#ifdef RT_OS_WINDOWS

struct StartSVCHelperClientData
{
    ComObjPtr<VirtualBox> that;
    ComObjPtr<Progress> progress;
    bool privileged;
    VirtualBox::SVCHelperClientFunc func;
    void *user;
};

/**
 *  Helper method that starts a worker thread that:
 *  - creates a pipe communication channel using SVCHlpClient;
 *  - starts an SVC Helper process that will inherit this channel;
 *  - executes the supplied function by passing it the created SVCHlpClient
 *    and opened instance to communicate to the Helper process and the given
 *    Progress object.
 *
 *  The user function is supposed to communicate to the helper process
 *  using the \a aClient argument to do the requested job and optionally expose
 *  the progress through the \a aProgress object. The user function should never
 *  call notifyComplete() on it: this will be done automatically using the
 *  result code returned by the function.
 *
 *  Before the user function is started, the communication channel passed to
 *  the \a aClient argument is fully set up, the function should start using
 *  its write() and read() methods directly.
 *
 *  The \a aVrc parameter of the user function may be used to return an error
 *  code if it is related to communication errors (for example, returned by
 *  the SVCHlpClient members when they fail). In this case, the correct error
 *  message using this value will be reported to the caller. Note that the
 *  value of \a aVrc is inspected only if the user function itself returns
 *  success.
 *
 *  If a failure happens anywhere before the user function would be normally
 *  called, it will be called anyway in special "cleanup only" mode indicated
 *  by \a aClient, \a aProgress and \aVrc arguments set to NULL. In this mode,
 *  all the function is supposed to do is to cleanup its aUser argument if
 *  necessary (it's assumed that the ownership of this argument is passed to
 *  the user function once #startSVCHelperClient() returns a success, thus
 *  making it responsible for the cleanup).
 *
 *  After the user function returns, the thread will send the SVCHlpMsg::Null
 *  message to indicate a process termination.
 *
 *  @param  aPrivileged |true| to start the SVC Helper process as a privileged
 *                      user that can perform administrative tasks
 *  @param  aFunc       user function to run
 *  @param  aUser       argument to the user function
 *  @param  aProgress   progress object that will track operation completion
 *
 *  @note aPrivileged is currently ignored (due to some unsolved problems in
 *        Vista) and the process will be started as a normal (unprivileged)
 *        process.
 *
 *  @note Doesn't lock anything.
 */
HRESULT VirtualBox::startSVCHelperClient(bool aPrivileged,
                                         SVCHelperClientFunc aFunc,
                                         void *aUser, Progress *aProgress)
{
    AssertReturn(aFunc, E_POINTER);
    AssertReturn(aProgress, E_POINTER);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* create the SVCHelperClientThread() argument */
    std::auto_ptr <StartSVCHelperClientData>
        d(new StartSVCHelperClientData());
    AssertReturn(d.get(), E_OUTOFMEMORY);

    d->that = this;
    d->progress = aProgress;
    d->privileged = aPrivileged;
    d->func = aFunc;
    d->user = aUser;

    RTTHREAD tid = NIL_RTTHREAD;
    int vrc = RTThreadCreate(&tid, SVCHelperClientThread,
                             static_cast <void *>(d.get()),
                             0, RTTHREADTYPE_MAIN_WORKER,
                             RTTHREADFLAGS_WAITABLE, "SVCHelper");
    if (RT_FAILURE(vrc))
        return setError(E_FAIL, "Could not create SVCHelper thread (%Rrc)", vrc);

    /* d is now owned by SVCHelperClientThread(), so release it */
    d.release();

    return S_OK;
}

/**
 *  Worker thread for startSVCHelperClient().
 */
/* static */
DECLCALLBACK(int)
VirtualBox::SVCHelperClientThread(RTTHREAD aThread, void *aUser)
{
    LogFlowFuncEnter();

    std::auto_ptr<StartSVCHelperClientData>
        d(static_cast<StartSVCHelperClientData*>(aUser));

    HRESULT rc = S_OK;
    bool userFuncCalled = false;

    do
    {
        AssertBreakStmt(d.get(), rc = E_POINTER);
        AssertReturn(!d->progress.isNull(), E_POINTER);

        /* protect VirtualBox from uninitialization */
        AutoCaller autoCaller(d->that);
        if (!autoCaller.isOk())
        {
            /* it's too late */
            rc = autoCaller.rc();
            break;
        }

        int vrc = VINF_SUCCESS;

        Guid id;
        id.create();
        SVCHlpClient client;
        vrc = client.create(Utf8StrFmt("VirtualBox\\SVCHelper\\{%RTuuid}",
                                       id.raw()).c_str());
        if (RT_FAILURE(vrc))
        {
            rc = d->that->setError(E_FAIL,
                                   tr("Could not create the communication channel (%Rrc)"), vrc);
            break;
        }

        /* get the path to the executable */
        char exePathBuf[RTPATH_MAX];
        char *exePath = RTProcGetExecutablePath(exePathBuf, RTPATH_MAX);
        if (!exePath)
        {
            rc = d->that->setError(E_FAIL, tr("Cannot get executable name"));
            break;
        }

        Utf8Str argsStr = Utf8StrFmt("/Helper %s", client.name().c_str());

        LogFlowFunc(("Starting '\"%s\" %s'...\n", exePath, argsStr.c_str()));

        RTPROCESS pid = NIL_RTPROCESS;

        if (d->privileged)
        {
            /* Attempt to start a privileged process using the Run As dialog */

            Bstr file = exePath;
            Bstr parameters = argsStr;

            SHELLEXECUTEINFO shExecInfo;

            shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);

            shExecInfo.fMask = NULL;
            shExecInfo.hwnd = NULL;
            shExecInfo.lpVerb = L"runas";
            shExecInfo.lpFile = file.raw();
            shExecInfo.lpParameters = parameters.raw();
            shExecInfo.lpDirectory = NULL;
            shExecInfo.nShow = SW_NORMAL;
            shExecInfo.hInstApp = NULL;

            if (!ShellExecuteEx(&shExecInfo))
            {
                int vrc2 = RTErrConvertFromWin32(GetLastError());
                /* hide excessive details in case of a frequent error
                 * (pressing the Cancel button to close the Run As dialog) */
                if (vrc2 == VERR_CANCELLED)
                    rc = d->that->setError(E_FAIL,
                                           tr("Operation canceled by the user"));
                else
                    rc = d->that->setError(E_FAIL,
                                           tr("Could not launch a privileged process '%s' (%Rrc)"),
                                           exePath, vrc2);
                break;
            }
        }
        else
        {
            const char *args[] = { exePath, "/Helper", client.name().c_str(), 0 };
            vrc = RTProcCreate(exePath, args, RTENV_DEFAULT, 0, &pid);
            if (RT_FAILURE(vrc))
            {
                rc = d->that->setError(E_FAIL,
                                       tr("Could not launch a process '%s' (%Rrc)"), exePath, vrc);
                break;
            }
        }

        /* wait for the client to connect */
        vrc = client.connect();
        if (RT_SUCCESS(vrc))
        {
            /* start the user supplied function */
            rc = d->func(&client, d->progress, d->user, &vrc);
            userFuncCalled = true;
        }

        /* send the termination signal to the process anyway */
        {
            int vrc2 = client.write(SVCHlpMsg::Null);
            if (RT_SUCCESS(vrc))
                vrc = vrc2;
        }

        if (SUCCEEDED(rc) && RT_FAILURE(vrc))
        {
            rc = d->that->setError(E_FAIL,
                                   tr("Could not operate the communication channel (%Rrc)"), vrc);
            break;
        }
    }
    while (0);

    if (FAILED(rc) && !userFuncCalled)
    {
        /* call the user function in the "cleanup only" mode
         * to let it free resources passed to in aUser */
        d->func(NULL, NULL, d->user, NULL);
    }

    d->progress->notifyComplete(rc);

    LogFlowFuncLeave();
    return 0;
}

#endif /* RT_OS_WINDOWS */

/**
 *  Sends a signal to the client watcher thread to rescan the set of machines
 *  that have open sessions.
 *
 *  @note Doesn't lock anything.
 */
void VirtualBox::updateClientWatcher()
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AssertReturnVoid(m->threadClientWatcher != NIL_RTTHREAD);

    /* sent an update request */
#if defined(RT_OS_WINDOWS)
    ::SetEvent(m->updateReq);
#elif defined(RT_OS_OS2)
    RTSemEventSignal(m->updateReq);
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
    RTSemEventSignal(m->updateReq);
#else
# error "Port me!"
#endif
}

/**
 *  Adds the given child process ID to the list of processes to be reaped.
 *  This call should be followed by #updateClientWatcher() to take the effect.
 */
void VirtualBox::addProcessToReap(RTPROCESS pid)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /// @todo (dmik) Win32?
#ifndef RT_OS_WINDOWS
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    m->llProcesses.push_back(pid);
#endif
}

/** Event for onMachineStateChange(), onMachineDataChange(), onMachineRegistered() */
struct MachineEvent : public VirtualBox::CallbackEvent
{
    MachineEvent(VirtualBox *aVB, VBoxEventType_T aWhat, const Guid &aId, BOOL aBool)
        : CallbackEvent(aVB, aWhat), id(aId.toUtf16())
        , mBool(aBool)
        { }

    MachineEvent(VirtualBox *aVB, VBoxEventType_T aWhat, const Guid &aId, MachineState_T aState)
        : CallbackEvent(aVB, aWhat), id(aId.toUtf16())
        , mState(aState)
        {}

    virtual HRESULT prepareEventDesc(IEventSource* aSource, VBoxEventDesc& aEvDesc)
    {
        switch (mWhat)
        {
            case VBoxEventType_OnMachineDataChanged:
                aEvDesc.init(aSource, mWhat, id.raw(), mBool);
                break;

            case VBoxEventType_OnMachineStateChanged:
                aEvDesc.init(aSource, mWhat, id.raw(), mState);
                break;

            case VBoxEventType_OnMachineRegistered:
                aEvDesc.init(aSource, mWhat, id.raw(), mBool);
                break;

            default:
                AssertFailedReturn(S_OK);
         }
         return S_OK;
    }

    Bstr id;
    MachineState_T mState;
    BOOL mBool;
};

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onMachineStateChange(const Guid &aId, MachineState_T aState)
{
    postEvent(new MachineEvent(this, VBoxEventType_OnMachineStateChanged, aId, aState));
}

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onMachineDataChange(const Guid &aId, BOOL aTemporary)
{
    postEvent(new MachineEvent(this, VBoxEventType_OnMachineDataChanged, aId, aTemporary));
}

/**
 *  @note Locks this object for reading.
 */
BOOL VirtualBox::onExtraDataCanChange(const Guid &aId, IN_BSTR aKey, IN_BSTR aValue,
                                       Bstr &aError)
{
    LogFlowThisFunc(("machine={%s} aKey={%ls} aValue={%ls}\n",
                      aId.toString().c_str(), aKey, aValue));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    BOOL allowChange = TRUE;
    Bstr id = aId.toUtf16();

    VBoxEventDesc evDesc;
    evDesc.init(m->pEventSource, VBoxEventType_OnExtraDataCanChange, id.raw(), aKey, aValue);
    BOOL fDelivered = evDesc.fire(3000); /* Wait up to 3 secs for delivery */
    //Assert(fDelivered);
    if (fDelivered)
    {
        ComPtr<IEvent> aEvent;
        evDesc.getEvent(aEvent.asOutParam());
        ComPtr<IExtraDataCanChangeEvent> aCanChangeEvent = aEvent;
        Assert(aCanChangeEvent);
        BOOL fVetoed = FALSE;
        aCanChangeEvent->IsVetoed(&fVetoed);
        allowChange = !fVetoed;

        if (!allowChange)
        {
            SafeArray<BSTR> aVetos;
            aCanChangeEvent->GetVetos(ComSafeArrayAsOutParam(aVetos));
            if (aVetos.size() > 0)
                aError = aVetos[0];
        }
    }
    else
        allowChange = TRUE;

    LogFlowThisFunc(("allowChange=%RTbool\n", allowChange));
    return allowChange;
}

/** Event for onExtraDataChange() */
struct ExtraDataEvent : public VirtualBox::CallbackEvent
{
    ExtraDataEvent(VirtualBox *aVB, const Guid &aMachineId,
                   IN_BSTR aKey, IN_BSTR aVal)
        : CallbackEvent(aVB, VBoxEventType_OnExtraDataChanged)
        , machineId(aMachineId.toUtf16()), key(aKey), val(aVal)
    {}

    virtual HRESULT prepareEventDesc(IEventSource* aSource, VBoxEventDesc& aEvDesc)
    {
        return aEvDesc.init(aSource, VBoxEventType_OnExtraDataChanged, machineId.raw(), key.raw(), val.raw());
    }

    Bstr machineId, key, val;
};

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onExtraDataChange(const Guid &aId, IN_BSTR aKey, IN_BSTR aValue)
{
    postEvent(new ExtraDataEvent(this, aId, aKey, aValue));
}

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onMachineRegistered(const Guid &aId, BOOL aRegistered)
{
    postEvent(new MachineEvent(this, VBoxEventType_OnMachineRegistered, aId, aRegistered));
}

/** Event for onSessionStateChange() */
struct SessionEvent : public VirtualBox::CallbackEvent
{
    SessionEvent(VirtualBox *aVB, const Guid &aMachineId, SessionState_T aState)
        : CallbackEvent(aVB, VBoxEventType_OnSessionStateChanged)
        , machineId(aMachineId.toUtf16()), sessionState(aState)
    {}

    virtual HRESULT prepareEventDesc(IEventSource* aSource, VBoxEventDesc& aEvDesc)
    {
        return aEvDesc.init(aSource, VBoxEventType_OnSessionStateChanged, machineId.raw(), sessionState);
    }
    Bstr machineId;
    SessionState_T sessionState;
};

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onSessionStateChange(const Guid &aId, SessionState_T aState)
{
    postEvent(new SessionEvent(this, aId, aState));
}

/** Event for onSnapshotTaken(), onSnapshotDeleted() and onSnapshotChange() */
struct SnapshotEvent : public VirtualBox::CallbackEvent
{
    SnapshotEvent(VirtualBox *aVB, const Guid &aMachineId, const Guid &aSnapshotId,
                  VBoxEventType_T aWhat)
        : CallbackEvent(aVB, aWhat)
        , machineId(aMachineId), snapshotId(aSnapshotId)
        {}

    virtual HRESULT prepareEventDesc(IEventSource* aSource, VBoxEventDesc& aEvDesc)
    {
        return aEvDesc.init(aSource, VBoxEventType_OnSnapshotTaken,
                            machineId.toUtf16().raw(), snapshotId.toUtf16().raw());
    }

    Guid machineId;
    Guid snapshotId;
};

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onSnapshotTaken(const Guid &aMachineId, const Guid &aSnapshotId)
{
    postEvent(new SnapshotEvent(this, aMachineId, aSnapshotId,
                                VBoxEventType_OnSnapshotTaken));
}

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onSnapshotDeleted(const Guid &aMachineId, const Guid &aSnapshotId)
{
    postEvent(new SnapshotEvent(this, aMachineId, aSnapshotId,
                                VBoxEventType_OnSnapshotDeleted));
}

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onSnapshotChange(const Guid &aMachineId, const Guid &aSnapshotId)
{
    postEvent(new SnapshotEvent(this, aMachineId, aSnapshotId,
                                VBoxEventType_OnSnapshotChanged));
}

/** Event for onGuestPropertyChange() */
struct GuestPropertyEvent : public VirtualBox::CallbackEvent
{
    GuestPropertyEvent(VirtualBox *aVBox, const Guid &aMachineId,
                       IN_BSTR aName, IN_BSTR aValue, IN_BSTR aFlags)
        : CallbackEvent(aVBox, VBoxEventType_OnGuestPropertyChanged),
          machineId(aMachineId),
          name(aName),
          value(aValue),
          flags(aFlags)
    {}

    virtual HRESULT prepareEventDesc(IEventSource* aSource, VBoxEventDesc& aEvDesc)
    {
        return aEvDesc.init(aSource, VBoxEventType_OnGuestPropertyChanged,
                            machineId.toUtf16().raw(), name.raw(), value.raw(), flags.raw());
    }

    Guid machineId;
    Bstr name, value, flags;
};

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onGuestPropertyChange(const Guid &aMachineId, IN_BSTR aName,
                                       IN_BSTR aValue, IN_BSTR aFlags)
{
    postEvent(new GuestPropertyEvent(this, aMachineId, aName, aValue, aFlags));
}

/** Event for onMachineUninit(), this is not a CallbackEvent */
class MachineUninitEvent : public Event
{
public:

    MachineUninitEvent(VirtualBox *aVirtualBox, Machine *aMachine)
        : mVirtualBox(aVirtualBox), mMachine(aMachine)
    {
        Assert(aVirtualBox);
        Assert(aMachine);
    }

    void *handler()
    {
#ifdef VBOX_WITH_RESOURCE_USAGE_API
        /* Handle unregistering metrics here, as it is not vital to get
         * it done immediately. It reduces the number of locks needed and
         * the lock contention in SessionMachine::uninit. */
        {
            AutoWriteLock mLock(mMachine COMMA_LOCKVAL_SRC_POS);
            mMachine->unregisterMetrics(mVirtualBox->performanceCollector(), mMachine);
        }
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

        return NULL;
    }

private:

    /**
     *  Note that this is a weak ref -- the CallbackEvent handler thread
     *  is bound to the lifetime of the VirtualBox instance, so it's safe.
     */
    VirtualBox        *mVirtualBox;

    /** Reference to the machine object. */
    ComObjPtr<Machine> mMachine;
};

/**
 *  Trigger internal event. This isn't meant to be signalled to clients.
 *  @note Doesn't lock any object.
 */
void VirtualBox::onMachineUninit(Machine *aMachine)
{
    postEvent(new MachineUninitEvent(this, aMachine));
}

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onNatRedirectChange(const Guid &aMachineId, ULONG ulSlot, bool fRemove, IN_BSTR aName,
                               NATProtocol_T aProto, IN_BSTR aHostIp, uint16_t aHostPort,
                               IN_BSTR aGuestIp, uint16_t aGuestPort)
{
    fireNATRedirectEvent(m->pEventSource, aMachineId.toUtf16().raw(), ulSlot, fRemove, aName, aProto, aHostIp,
                         aHostPort, aGuestIp, aGuestPort);
}

/**
 *  @note Locks this object for reading.
 */
ComObjPtr<GuestOSType> VirtualBox::getUnknownOSType()
{
    ComObjPtr<GuestOSType> type;
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), type);

    /* unknown type must always be the first */
    ComAssertRet(m->allGuestOSTypes.size() > 0, type);

    return m->allGuestOSTypes.front();
}

/**
 * Returns the list of opened machines (machines having direct sessions opened
 * by client processes) and optionally the list of direct session controls.
 *
 * @param aMachines     Where to put opened machines (will be empty if none).
 * @param aControls     Where to put direct session controls (optional).
 *
 * @note The returned lists contain smart pointers. So, clear it as soon as
 * it becomes no more necessary to release instances.
 *
 * @note It can be possible that a session machine from the list has been
 * already uninitialized, so do a usual AutoCaller/AutoReadLock sequence
 * when accessing unprotected data directly.
 *
 * @note Locks objects for reading.
 */
void VirtualBox::getOpenedMachines(SessionMachinesList &aMachines,
                                   InternalControlList *aControls /*= NULL*/)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    aMachines.clear();
    if (aControls)
        aControls->clear();

    AutoReadLock alock(m->allMachines.getLockHandle() COMMA_LOCKVAL_SRC_POS);

    for (MachinesOList::iterator it = m->allMachines.begin();
         it != m->allMachines.end();
         ++it)
    {
        ComObjPtr<SessionMachine> sm;
        ComPtr<IInternalSessionControl> ctl;
        if ((*it)->isSessionOpen(sm, &ctl))
        {
            aMachines.push_back(sm);
            if (aControls)
                aControls->push_back(ctl);
        }
    }
}

/**
 *  Searches for a machine object with the given ID in the collection
 *  of registered machines.
 *
 * @param aId Machine UUID to look for.
 * @param aPermitInaccessible If true, inaccessible machines will be found;
 *                  if false, this will fail if the given machine is inaccessible.
 * @param aSetError If true, set errorinfo if the machine is not found.
 * @param aMachine Returned machine, if found.
 * @return
 */
HRESULT VirtualBox::findMachine(const Guid &aId,
                                bool fPermitInaccessible,
                                bool aSetError,
                                ComObjPtr<Machine> *aMachine /* = NULL */)
{
    HRESULT rc = VBOX_E_OBJECT_NOT_FOUND;

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    {
        AutoReadLock al(m->allMachines.getLockHandle() COMMA_LOCKVAL_SRC_POS);

        for (MachinesOList::iterator it = m->allMachines.begin();
             it != m->allMachines.end();
             ++it)
        {
            ComObjPtr<Machine> pMachine2 = *it;

            if (!fPermitInaccessible)
            {
                // skip inaccessible machines
                AutoCaller machCaller(pMachine2);
                if (FAILED(machCaller.rc()))
                    continue;
            }

            if (pMachine2->getId() == aId)
            {
                rc = S_OK;
                if (aMachine)
                    *aMachine = pMachine2;
                break;
            }
        }
    }

    if (aSetError && FAILED(rc))
        rc = setError(rc,
                      tr("Could not find a registered machine with UUID {%RTuuid}"),
                      aId.raw());

    return rc;
}

/**
 * Searches for a Medium object with the given ID in the list of registered
 * hard disks.
 *
 * @param aId           ID of the hard disk. Must not be empty.
 * @param aSetError     If @c true , the appropriate error info is set in case
 *                      when the hard disk is not found.
 * @param aHardDisk     Where to store the found hard disk object (can be NULL).
 *
 * @return S_OK, E_INVALIDARG or VBOX_E_OBJECT_NOT_FOUND when not found.
 *
 * @note Locks the media tree for reading.
 */
HRESULT VirtualBox::findHardDiskById(const Guid &id,
                                     bool aSetError,
                                     ComObjPtr<Medium> *aHardDisk /*= NULL*/)
{
    AssertReturn(!id.isEmpty(), E_INVALIDARG);

    // we use the hard disks map, but it is protected by the
    // hard disk _list_ lock handle
    AutoReadLock alock(m->allHardDisks.getLockHandle() COMMA_LOCKVAL_SRC_POS);

    HardDiskMap::const_iterator it = m->mapHardDisks.find(id);
    if (it != m->mapHardDisks.end())
    {
        if (aHardDisk)
            *aHardDisk = (*it).second;
        return S_OK;
    }

    if (aSetError)
        return setError(VBOX_E_OBJECT_NOT_FOUND,
                        tr("Could not find an open hard disk with UUID {%RTuuid}"),
                        id.raw());

    return VBOX_E_OBJECT_NOT_FOUND;
}

/**
 * Searches for a Medium object with the given ID or location in the list of
 * registered hard disks. If both ID and location are specified, the first
 * object that matches either of them (not necessarily both) is returned.
 *
 * @param aLocation     Full location specification. Must not be empty.
 * @param aSetError     If @c true , the appropriate error info is set in case
 *                      when the hard disk is not found.
 * @param aHardDisk     Where to store the found hard disk object (can be NULL).
 *
 * @return S_OK, E_INVALIDARG or VBOX_E_OBJECT_NOT_FOUND when not found.
 *
 * @note Locks the media tree for reading.
 */
HRESULT VirtualBox::findHardDiskByLocation(const Utf8Str &strLocation,
                                           bool aSetError,
                                           ComObjPtr<Medium> *aHardDisk /*= NULL*/)
{
    AssertReturn(!strLocation.isEmpty(), E_INVALIDARG);

    // we use the hard disks map, but it is protected by the
    // hard disk _list_ lock handle
    AutoReadLock alock(m->allHardDisks.getLockHandle() COMMA_LOCKVAL_SRC_POS);

    for (HardDiskMap::const_iterator it = m->mapHardDisks.begin();
         it != m->mapHardDisks.end();
         ++it)
    {
        const ComObjPtr<Medium> &pHD = (*it).second;

        AutoCaller autoCaller(pHD);
        if (FAILED(autoCaller.rc())) return autoCaller.rc();
        AutoWriteLock mlock(pHD COMMA_LOCKVAL_SRC_POS);

        Utf8Str strLocationFull = pHD->getLocationFull();

        if (0 == RTPathCompare(strLocationFull.c_str(), strLocation.c_str()))
        {
            if (aHardDisk)
                *aHardDisk = pHD;
            return S_OK;
        }
    }

    if (aSetError)
        return setError(VBOX_E_OBJECT_NOT_FOUND,
                        tr("Could not find an open hard disk with location '%s'"),
                        strLocation.c_str());

    return VBOX_E_OBJECT_NOT_FOUND;
}

/**
 * Searches for a Medium object with the given ID or location in the list of
 * registered DVD or floppy images, depending on the @a mediumType argument.
 * If both ID and file path are specified, the first object that matches either
 * of them (not necessarily both) is returned.
 *
 * @param mediumType Must be either DeviceType_DVD or DeviceType_Floppy.
 * @param aId       ID of the image file (unused when NULL).
 * @param aLocation Full path to the image file (unused when NULL).
 * @param aSetError If @c true, the appropriate error info is set in case when
 *                  the image is not found.
 * @param aImage    Where to store the found image object (can be NULL).
 *
 * @return S_OK when found or E_INVALIDARG or VBOX_E_OBJECT_NOT_FOUND when not found.
 *
 * @note Locks the media tree for reading.
 */
HRESULT VirtualBox::findDVDOrFloppyImage(DeviceType_T mediumType,
                                         const Guid *aId,
                                         const Utf8Str &aLocation,
                                         bool aSetError,
                                         ComObjPtr<Medium> *aImage /* = NULL */)
{
    AssertReturn(aId || !aLocation.isEmpty(), E_INVALIDARG);

    Utf8Str location;
    if (!aLocation.isEmpty())
    {
        int vrc = calculateFullPath(aLocation, location);
        if (RT_FAILURE(vrc))
            return setError(VBOX_E_FILE_ERROR,
                            tr("Invalid image file location '%s' (%Rrc)"),
                            aLocation.c_str(),
                            vrc);
    }

    MediaOList *pMediaList;

    switch (mediumType)
    {
        case DeviceType_DVD:
            pMediaList = &m->allDVDImages;
        break;

        case DeviceType_Floppy:
            pMediaList = &m->allFloppyImages;
        break;

        default:
            return E_INVALIDARG;
    }

    AutoReadLock alock(pMediaList->getLockHandle() COMMA_LOCKVAL_SRC_POS);

    bool found = false;

    for (MediaList::const_iterator it = pMediaList->begin();
         it != pMediaList->end();
         ++it)
    {
        // no AutoCaller, registered image life time is bound to this
        Medium *pMedium = *it;
        AutoReadLock imageLock(pMedium COMMA_LOCKVAL_SRC_POS);
        const Utf8Str &strLocationFull = pMedium->getLocationFull();

        found =     (    aId
                      && pMedium->getId() == *aId)
                 || (    !aLocation.isEmpty()
                      && RTPathCompare(location.c_str(),
                                       strLocationFull.c_str()) == 0);
        if (found)
        {
            if (pMedium->getDeviceType() != mediumType)
            {
                if (mediumType == DeviceType_DVD)
                    return setError(E_INVALIDARG,
                                    "Cannot mount DVD medium '%s' as floppy", strLocationFull.c_str());
                else
                    return setError(E_INVALIDARG,
                                    "Cannot mount floppy medium '%s' as DVD", strLocationFull.c_str());
            }

            if (aImage)
                *aImage = pMedium;
            break;
        }
    }

    HRESULT rc = found ? S_OK : VBOX_E_OBJECT_NOT_FOUND;

    if (aSetError && !found)
    {
        if (aId)
            setError(rc,
                     tr("Could not find an image file with UUID {%RTuuid} in the media registry ('%s')"),
                     aId->raw(),
                     m->strSettingsFilePath.c_str());
        else
            setError(rc,
                     tr("Could not find an image file with location '%s' in the media registry ('%s')"),
                     aLocation.c_str(),
                     m->strSettingsFilePath.c_str());
    }

    return rc;
}

/**
 * Searches for an IMedium object that represents the given UUID.
 *
 * If the UUID is empty (indicating an empty drive), this sets pMedium
 * to NULL and returns S_OK.
 *
 * If the UUID refers to a host drive of the given device type, this
 * sets pMedium to the object from the list in IHost and returns S_OK.
 *
 * If the UUID is an image file, this sets pMedium to the object that
 * findDVDOrFloppyImage() returned.
 *
 * If none of the above apply, this returns VBOX_E_OBJECT_NOT_FOUND.
 *
 * @param mediumType Must be DeviceType_DVD or DeviceType_Floppy.
 * @param uuid UUID to search for; must refer to a host drive or an image file or be null.
 * @param fRefresh Whether to refresh the list of host drives in IHost (see Host::getDrives())
 * @param pMedium out: IMedium object found.
 * @return
 */
HRESULT VirtualBox::findRemoveableMedium(DeviceType_T mediumType,
                                         const Guid &uuid,
                                         bool fRefresh,
                                         bool aSetError,
                                         ComObjPtr<Medium> &pMedium)
{
    if (uuid.isEmpty())
    {
        // that's easy
        pMedium.setNull();
        return S_OK;
    }

    // first search for host drive with that UUID
    HRESULT rc = m->pHost->findHostDriveById(mediumType,
                                             uuid,
                                             fRefresh,
                                             pMedium);
    if (rc == VBOX_E_OBJECT_NOT_FOUND)
                // then search for an image with that UUID
        rc = findDVDOrFloppyImage(mediumType, &uuid, Utf8Str::Empty, aSetError, &pMedium);

    return rc;
}

HRESULT VirtualBox::findGuestOSType(const Bstr &bstrOSType,
                                    GuestOSType*& pGuestOSType)
{
    /* Look for a GuestOSType object */
    AssertMsg(m->allGuestOSTypes.size() != 0,
              ("Guest OS types array must be filled"));

    if (bstrOSType.isEmpty())
    {
        pGuestOSType = NULL;
        return S_OK;
    }

    AutoReadLock alock(m->allGuestOSTypes.getLockHandle() COMMA_LOCKVAL_SRC_POS);
    for (GuestOSTypesOList::const_iterator it = m->allGuestOSTypes.begin();
         it != m->allGuestOSTypes.end();
         ++it)
    {
        if ((*it)->id() == bstrOSType)
        {
            pGuestOSType = *it;
            return S_OK;
        }
    }

    return setError(VBOX_E_OBJECT_NOT_FOUND,
                    tr("Guest OS type '%ls' is invalid"),
                    bstrOSType.raw());
}

/**
 * Returns the constant pseudo-machine UUID that is used to identify the
 * global media registry.
 *
 * Starting with VirtualBox 4.0 each medium remembers in its instance data
 * in which media registry it is saved (if any): this can either be a machine
 * UUID, if it's in a per-machine media registry, or this global ID.
 *
 * This UUID is only used to identify the VirtualBox object while VirtualBox
 * is running. It is a compile-time constant and not saved anywhere.
 *
 * @return
 */
const Guid& VirtualBox::getGlobalRegistryId() const
{
    return m->uuidMediaRegistry;
}

const ComObjPtr<Host>& VirtualBox::host() const
{
    return m->pHost;
}

SystemProperties* VirtualBox::getSystemProperties() const
{
    return m->pSystemProperties;
}

#ifdef VBOX_WITH_EXTPACK
/**
 * Getter that SystemProperties and others can use to talk to the extension
 * pack manager.
 */
ExtPackManager* VirtualBox::getExtPackManager() const
{
    return m->ptrExtPackManager;
}
#endif

#ifdef VBOX_WITH_RESOURCE_USAGE_API
const ComObjPtr<PerformanceCollector>& VirtualBox::performanceCollector() const
{
    return m->pPerformanceCollector;
}
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

/**
 * Returns the default machine folder from the system properties
 * with proper locking.
 * @return
 */
void VirtualBox::getDefaultMachineFolder(Utf8Str &str) const
{
    AutoReadLock propsLock(m->pSystemProperties COMMA_LOCKVAL_SRC_POS);
    str = m->pSystemProperties->m->strDefaultMachineFolder;
}

/**
 * Returns the default hard disk format from the system properties
 * with proper locking.
 * @return
 */
void VirtualBox::getDefaultHardDiskFormat(Utf8Str &str) const
{
    AutoReadLock propsLock(m->pSystemProperties COMMA_LOCKVAL_SRC_POS);
    str = m->pSystemProperties->m->strDefaultHardDiskFormat;
}

const Utf8Str& VirtualBox::homeDir() const
{
    return m->strHomeDir;
}

/**
 * Calculates the absolute path of the given path taking the VirtualBox home
 * directory as the current directory.
 *
 * @param  aPath    Path to calculate the absolute path for.
 * @param  aResult  Where to put the result (used only on success, can be the
 *                  same Utf8Str instance as passed in @a aPath).
 * @return IPRT result.
 *
 * @note Doesn't lock any object.
 */
int VirtualBox::calculateFullPath(const Utf8Str &strPath, Utf8Str &aResult)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), VERR_GENERAL_FAILURE);

    /* no need to lock since mHomeDir is const */

    char folder[RTPATH_MAX];
    int vrc = RTPathAbsEx(m->strHomeDir.c_str(),
                          strPath.c_str(),
                          folder,
                          sizeof(folder));
    if (RT_SUCCESS(vrc))
        aResult = folder;

    return vrc;
}

/**
 * Copies strSource to strTarget, making it relative to the VirtualBox config folder
 * if it is a subdirectory thereof, or simply copying it otherwise.
 *
 * @param strSource Path to evalue and copy.
 * @param strTarget Buffer to receive target path.
 */
void VirtualBox::copyPathRelativeToConfig(const Utf8Str &strSource,
                                          Utf8Str &strTarget)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    // no need to lock since mHomeDir is const

    // use strTarget as a temporary buffer to hold the machine settings dir
    strTarget = m->strHomeDir;
    if (RTPathStartsWith(strSource.c_str(), strTarget.c_str()))
        // is relative: then append what's left
        strTarget.append(strSource.c_str() + strTarget.length());     // include '/'
    else
        // is not relative: then overwrite
        strTarget = strSource;
}

// private methods
/////////////////////////////////////////////////////////////////////////////

/**
 * Checks if there is a hard disk, DVD or floppy image with the given ID or
 * location already registered.
 *
 * On return, sets @a aConflict to the string describing the conflicting medium,
 * or sets it to @c Null if no conflicting media is found. Returns S_OK in
 * either case. A failure is unexpected.
 *
 * @param aId           UUID to check.
 * @param aLocation     Location to check.
 * @param aConflict     Where to return parameters of the conflicting medium.
 *
 * @note Locks the media tree and media objects for reading.
 */
HRESULT VirtualBox::checkMediaForConflicts(const Guid &aId,
                                           const Utf8Str &aLocation,
                                           Utf8Str &aConflict,
                                           bool &fIdentical)
{
    aConflict.setNull();

    AssertReturn(!aId.isEmpty() && !aLocation.isEmpty(), E_FAIL);

    AutoReadLock alock(getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    aConflict.setNull();
    fIdentical = false;

    ComObjPtr<Medium> pMediumFound;
    const char *pcszType = NULL;

    if (!aId.isEmpty())
        rc = findHardDiskById(aId, false /* aSetError */, &pMediumFound);
    if (FAILED(rc) && !aLocation.isEmpty())
        rc = findHardDiskByLocation(aLocation, false /* aSetError */, &pMediumFound);
    if (SUCCEEDED(rc))
        pcszType = tr("hard disk");

    if (!pcszType)
    {
        rc = findDVDOrFloppyImage(DeviceType_DVD, &aId, aLocation, false /* aSetError */, &pMediumFound);
        if (SUCCEEDED(rc))
            pcszType = tr("CD/DVD image");
    }

    if (!pcszType)
    {
        rc = findDVDOrFloppyImage(DeviceType_Floppy, &aId, aLocation, false /* aSetError */, &pMediumFound);
        if (SUCCEEDED(rc))
            pcszType = tr("floppy image");
    }

    if (pcszType && pMediumFound)
    {
        /* Note: no AutoCaller since bound to this */
        AutoReadLock mlock(pMediumFound COMMA_LOCKVAL_SRC_POS);

        Utf8Str strLocFound = pMediumFound->getLocationFull();
        Guid idFound = pMediumFound->getId();

        if (    (RTPathCompare(strLocFound.c_str(), aLocation.c_str()) == 0)
             && (idFound == aId)
           )
            fIdentical = true;

        aConflict = Utf8StrFmt(tr("%s '%s' with UUID {%RTuuid}"),
                               pcszType,
                               strLocFound.c_str(),
                               idFound.raw());
    }

    return S_OK;
}

/**
 * Called from Machine::prepareSaveSettings() when it has detected
 * that a machine has been renamed. Such renames will require
 * updating the global media registry during the
 * VirtualBox::saveSettings() that follows later.
*
 * When a machine is renamed, there may well be media (in particular,
 * diff images for snapshots) in the global registry that will need
 * to have their paths updated. Before 3.2, Machine::saveSettings
 * used to call VirtualBox::saveSettings implicitly, which was both
 * unintuitive and caused locking order problems. Now, we remember
 * such pending name changes with this method so that
 * VirtualBox::saveSettings() can process them properly.
 */
void VirtualBox::rememberMachineNameChangeForMedia(const Utf8Str &strOldConfigDir,
                                                   const Utf8Str &strNewConfigDir)
{
    AutoWriteLock mediaLock(getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

    Data::PendingMachineRename pmr;
    pmr.strConfigDirOld = strOldConfigDir;
    pmr.strConfigDirNew = strNewConfigDir;
    m->llPendingMachineRenames.push_back(pmr);
}

/**
 * Goes through all known media (hard disks, floppies and DVDs) and saves
 * those into the given settings::MediaRegistry structures whose registry
 * ID match the given UUID.
 *
 * Before actually writing to the structures, all media paths (not just the
 * ones for the given registry) are updated if machines have been renamed
 * since the last call.
 *
 * This gets called from two contexts:
 *
 *  -- VirtualBox::saveSettings() with the UUID of the global registry
 *     (VirtualBox::Data.uuidRegistry); this will save those media
 *     which had been loaded from the global registry or have been
 *     attached to a "legacy" machine which can't save its own registry;
 *
 *  -- Machine::saveSettings() with the UUID of a machine, if a medium
 *     has been attached to a machine created with VirtualBox 4.0 or later.
 *
 * Media which have only been temporarily opened without having been
 * attached to a machine have a NULL registry UUID and therefore don't
 * get saved.
 *
 * This locks the media tree. Throws HRESULT on errors!
 *
 * @param mediaRegistry Settings structure to fill.
 * @param uuidRegistry The UUID of the media registry; either a machine UUID (if machine registry) or the UUID of the global registry.
 * @param hardDiskFolder The machine folder for relative paths, if machine registry, or an empty string otherwise.
 */
void VirtualBox::saveMediaRegistry(settings::MediaRegistry &mediaRegistry,
                                   const Guid &uuidRegistry,
                                   const Utf8Str &strMachineFolder)
{
    // lock all media for the following; use a write lock because we're
    // modifying the PendingMachineRenamesList, which is protected by this
    AutoWriteLock mediaLock(getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

    // if a machine was renamed, then we'll need to refresh media paths
    if (m->llPendingMachineRenames.size())
    {
        // make a single list from the three media lists so we don't need three loops
        MediaList llAllMedia;
        // with hard disks, we must use the map, not the list, because the list only has base images
        for (HardDiskMap::iterator it = m->mapHardDisks.begin(); it != m->mapHardDisks.end(); ++it)
            llAllMedia.push_back(it->second);
        for (MediaList::iterator it = m->allDVDImages.begin(); it != m->allDVDImages.end(); ++it)
            llAllMedia.push_back(*it);
        for (MediaList::iterator it = m->allFloppyImages.begin(); it != m->allFloppyImages.end(); ++it)
            llAllMedia.push_back(*it);

        for (MediaList::iterator it = llAllMedia.begin();
             it != llAllMedia.end();
             ++it)
        {
            Medium *pMedium = *it;
            for (Data::PendingMachineRenamesList::iterator it2 = m->llPendingMachineRenames.begin();
                 it2 != m->llPendingMachineRenames.end();
                 ++it2)
            {
                const Data::PendingMachineRename &pmr = *it2;
                pMedium->updatePath(pmr.strConfigDirOld,
                                    pmr.strConfigDirNew);
            }
        }
        // done, don't do it again until we have more machine renames
        m->llPendingMachineRenames.clear();
    }

    struct {
        MediaOList &llSource;
        settings::MediaList &llTarget;
    } s[] =
    {
        // hard disks
        { m->allHardDisks, mediaRegistry.llHardDisks },
        // CD/DVD images
        { m->allDVDImages, mediaRegistry.llDvdImages },
        // floppy images
        { m->allFloppyImages, mediaRegistry.llFloppyImages }
    };

    HRESULT rc;

    for (size_t i = 0; i < RT_ELEMENTS(s); ++i)
    {
        MediaOList &llSource = s[i].llSource;
        settings::MediaList &llTarget = s[i].llTarget;
        llTarget.clear();
        for (MediaList::const_iterator it = llSource.begin();
             it != llSource.end();
             ++it)
        {
            Medium *pMedium = *it;
            AutoCaller autoCaller(pMedium);
            if (FAILED(autoCaller.rc())) throw autoCaller.rc();
            AutoReadLock mlock(pMedium COMMA_LOCKVAL_SRC_POS);

            if (pMedium->isInRegistry(uuidRegistry))
            {
                settings::Medium med;
                rc = pMedium->saveSettings(med, strMachineFolder);     // this recurses into child hard disks
                if (FAILED(rc)) throw rc;
                llTarget.push_back(med);
            }
        }
    }
}

/**
 *  Helper function which actually writes out VirtualBox.xml, the main configuration file.
 *  Gets called from the public VirtualBox::SaveSettings() as well as from various other
 *  places internally when settings need saving.
 *
 *  @note Caller must have locked the VirtualBox object for writing and must not hold any
 *    other locks since this locks all kinds of member objects and trees temporarily,
 *    which could cause conflicts.
 */
HRESULT VirtualBox::saveSettings()
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AssertReturn(isWriteLockOnCurrentThread(), E_FAIL);
    AssertReturn(!m->strSettingsFilePath.isEmpty(), E_FAIL);

    HRESULT rc = S_OK;

    try
    {
        // machines
        m->pMainConfigFile->llMachines.clear();
        {
            AutoReadLock machinesLock(m->allMachines.getLockHandle() COMMA_LOCKVAL_SRC_POS);
            for (MachinesOList::iterator it = m->allMachines.begin();
                 it != m->allMachines.end();
                 ++it)
            {
                Machine *pMachine = *it;
                // save actual machine registry entry
                settings::MachineRegistryEntry mre;
                rc = pMachine->saveRegistryEntry(mre);
                m->pMainConfigFile->llMachines.push_back(mre);
            }
        }

        saveMediaRegistry(m->pMainConfigFile->mediaRegistry,
                          m->uuidMediaRegistry,         // global media registry ID
                          Utf8Str::Empty);              // strMachineFolder

        m->pMainConfigFile->llDhcpServers.clear();
        {
            AutoReadLock dhcpLock(m->allDHCPServers.getLockHandle() COMMA_LOCKVAL_SRC_POS);
            for (DHCPServersOList::const_iterator it = m->allDHCPServers.begin();
                 it != m->allDHCPServers.end();
                 ++it)
            {
                settings::DHCPServer d;
                rc = (*it)->saveSettings(d);
                if (FAILED(rc)) throw rc;
                m->pMainConfigFile->llDhcpServers.push_back(d);
            }
        }

        // leave extra data alone, it's still in the config file

        // host data (USB filters)
        rc = m->pHost->saveSettings(m->pMainConfigFile->host);
        if (FAILED(rc)) throw rc;

        rc = m->pSystemProperties->saveSettings(m->pMainConfigFile->systemProperties);
        if (FAILED(rc)) throw rc;

        // and write out the XML, still under the lock
        m->pMainConfigFile->write(m->strSettingsFilePath);
    }
    catch (HRESULT err)
    {
        /* we assume that error info is set by the thrower */
        rc = err;
    }
    catch (...)
    {
        rc = VirtualBox::handleUnexpectedExceptions(RT_SRC_POS);
    }

    return rc;
}

/**
 *  Helper to register the machine.
 *
 *  When called during VirtualBox startup, adds the given machine to the
 *  collection of registered machines. Otherwise tries to mark the machine
 *  as registered, and, if succeeded, adds it to the collection and
 *  saves global settings.
 *
 *  @note The caller must have added itself as a caller of the @a aMachine
 *  object if calls this method not on VirtualBox startup.
 *
 *  @param aMachine     machine to register
 *
 *  @note Locks objects!
 */
HRESULT VirtualBox::registerMachine(Machine *aMachine)
{
    ComAssertRet(aMachine, E_INVALIDARG);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    {
        ComObjPtr<Machine> pMachine;
        rc = findMachine(aMachine->getId(),
                         true /* fPermitInaccessible */,
                         false /* aDoSetError */,
                         &pMachine);
        if (SUCCEEDED(rc))
        {
            /* sanity */
            AutoLimitedCaller machCaller(pMachine);
            AssertComRC(machCaller.rc());

            return setError(E_INVALIDARG,
                            tr("Registered machine with UUID {%RTuuid} ('%s') already exists"),
                            aMachine->getId().raw(),
                            pMachine->getSettingsFileFull().c_str());
        }

        ComAssertRet(rc == VBOX_E_OBJECT_NOT_FOUND, rc);
        rc = S_OK;
    }

    if (autoCaller.state() != InInit)
    {
        rc = aMachine->prepareRegister();
        if (FAILED(rc)) return rc;
    }

    /* add to the collection of registered machines */
    m->allMachines.addChild(aMachine);

    if (autoCaller.state() != InInit)
        rc = saveSettings();

    return rc;
}

/**
 * Remembers the given hard disk by storing it in either the global hard disk registry
 * or a machine one.
 *
 * @note Caller must hold the media tree lock for writing; in addition, this locks @a aHardDisk for reading
 *
 * @param aHardDisk Hard disk object to remember.
 * @param uuidMachineRegistry UUID of machine whose registry should be used, or a NULL UUID for the global registry.
 * @param pllRegistriesThatNeedSaving Optional pointer to a list of UUIDs of media registries that need saving.
 * @return
 */
HRESULT VirtualBox::registerHardDisk(Medium *pMedium,
                                     GuidList *pllRegistriesThatNeedSaving)
{
    AssertReturn(pMedium != NULL, E_INVALIDARG);

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoCaller hardDiskCaller(pMedium);
    AssertComRCReturn(hardDiskCaller.rc(), hardDiskCaller.rc());

    // caller must hold the media tree write lock
    Assert(getMediaTreeLockHandle().isWriteLockOnCurrentThread());

    Guid id;
    Utf8Str strLocationFull;
    ComObjPtr<Medium> pParent;
    {
        AutoReadLock hardDiskLock(pMedium COMMA_LOCKVAL_SRC_POS);
        id = pMedium->getId();
        strLocationFull = pMedium->getLocationFull();
        pParent = pMedium->getParent();
    }

    HRESULT rc;

    Utf8Str strConflict;
    bool fIdentical;
    rc = checkMediaForConflicts(id,
                                strLocationFull,
                                strConflict,
                                fIdentical);
    if (FAILED(rc)) return rc;

    if (!fIdentical)
    {
        if (strConflict.length())
            return setError(E_INVALIDARG,
                            tr("Cannot register the hard disk '%s' {%RTuuid} because a %s already exists"),
                            strLocationFull.c_str(),
                            id.raw(),
                            strConflict.c_str(),
                            m->strSettingsFilePath.c_str());

        // store base (root) hard disks in the list
        if (pParent.isNull())
            m->allHardDisks.getList().push_back(pMedium);
                    // access the list directly because we already locked the list above

        // store all hard disks (even differencing images) in the map
        m->mapHardDisks[id] = pMedium;

        if (pllRegistriesThatNeedSaving)
            pMedium->addToRegistryIDList(*pllRegistriesThatNeedSaving);
    }

    return rc;
}

/**
 * Removes the given hard disk from the hard disk registry.
 *
 * @param aHardDisk     Hard disk object to remove.
 * @param pfNeedsGlobalSaveSettings Optional pointer to a bool that must have been initialized to false and that will be set to true
 *                by this function if the caller should invoke VirtualBox::saveSettings() because the global settings have changed.
 *
 * @note Caller must hold the media tree lock for writing; in addition, this locks @a aHardDisk for reading
 */
HRESULT VirtualBox::unregisterHardDisk(Medium *aHardDisk,
                                       GuidList *pllRegistriesThatNeedSaving)
{
    AssertReturn(aHardDisk != NULL, E_INVALIDARG);

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoCaller hardDiskCaller(aHardDisk);
    AssertComRCReturn(hardDiskCaller.rc(), hardDiskCaller.rc());

    // caller must hold the media tree write lock
    Assert(getMediaTreeLockHandle().isWriteLockOnCurrentThread());

    Guid id;
    ComObjPtr<Medium> pParent;
    {
        AutoReadLock hardDiskLock(aHardDisk COMMA_LOCKVAL_SRC_POS);
        id = aHardDisk->getId();
        pParent = aHardDisk->getParent();
    }

    // remove base (root) hard disks from the list
    if (pParent.isNull())
        m->allHardDisks.getList().remove(aHardDisk);
                // access the list directly because caller must have locked the list

    // remove all hard disks (even differencing images) from map
    size_t cnt = m->mapHardDisks.erase(id);
    Assert(cnt == 1);
    NOREF(cnt);

    if (pllRegistriesThatNeedSaving)
        aHardDisk->addToRegistryIDList(*pllRegistriesThatNeedSaving);

    return S_OK;
}

/**
 * Remembers the given image by storing it in the CD/DVD or floppy image registry.
 *
 * @param argImage      Image object to remember.
 * @param argType       Either DeviceType_DVD or DeviceType_Floppy.
 * @param uuidMachineRegistry UUID of machine whose registry should be used, or a NULL UUID for the global registry.
 *
 * @note Caller must hold the media tree lock for writing; in addition, this locks @a argImage for reading
 */
HRESULT VirtualBox::registerImage(Medium *pMedium,
                                  DeviceType_T argType,
                                  GuidList *pllRegistriesThatNeedSaving)
{
    AssertReturn(pMedium != NULL, E_INVALIDARG);

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoCaller imageCaller(pMedium);
    AssertComRCReturn(imageCaller.rc(), imageCaller.rc());

    // caller must hold the media tree write lock
    Assert(getMediaTreeLockHandle().isWriteLockOnCurrentThread());

    Guid id;
    Utf8Str strLocationFull;
    ComObjPtr<Medium> pParent;
    {
        AutoReadLock al(pMedium COMMA_LOCKVAL_SRC_POS);
        id = pMedium->getId();
        strLocationFull = pMedium->getLocationFull();
        pParent = pMedium->getParent();
    }

    // work on DVDs or floppies list?
    ObjectsList<Medium> &all = (argType == DeviceType_DVD) ? m->allDVDImages : m->allFloppyImages;

    HRESULT rc;
    // lock the images lists (list + map) while checking for conflicts
    AutoWriteLock al(all.getLockHandle() COMMA_LOCKVAL_SRC_POS);

    Utf8Str strConflict;
    bool fIdentical;
    rc = checkMediaForConflicts(id,
                                strLocationFull,
                                strConflict,
                                fIdentical);
    if (FAILED(rc)) return rc;

    if (!fIdentical)
    {
        if (strConflict.length())
            return setError(VBOX_E_INVALID_OBJECT_STATE,
                            tr("Cannot register the image '%s' with UUID {%RTuuid} because a %s already exists"),
                            strLocationFull.c_str(),
                            id.raw(),
                            strConflict.c_str());

        // add to the collection
        all.getList().push_back(pMedium);
                // access the list directly because we already locked the list above

        if (pllRegistriesThatNeedSaving)
            pMedium->addToRegistryIDList(*pllRegistriesThatNeedSaving);
    }

    return rc;
}

/**
 * Removes the given image from the CD/DVD or floppy image registry.
 *
 * @param argImage        Image object to remove.
 * @param argType         Either DeviceType_DVD or DeviceType_Floppy.
 * @param pfNeedsGlobalSaveSettings Optional pointer to a bool that must have been initialized to false and that will be set to true
 *                by this function if the caller should invoke VirtualBox::saveSettings() because the global settings have changed.
 *
 * @note Caller must hold the media tree lock for writing; in addition, this locks @a argImage for reading
 */
HRESULT VirtualBox::unregisterImage(Medium *argImage,
                                    DeviceType_T argType,
                                    GuidList *pllRegistriesThatNeedSaving)
{
    AssertReturn(argImage != NULL, E_INVALIDARG);

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoCaller imageCaller(argImage);
    AssertComRCReturn(imageCaller.rc(), imageCaller.rc());

    // caller must hold the media tree write lock
    Assert(getMediaTreeLockHandle().isWriteLockOnCurrentThread());

    Guid id;
    ComObjPtr<Medium> pParent;
    {
        AutoReadLock al(argImage COMMA_LOCKVAL_SRC_POS);
        id = argImage->getId();
        pParent = argImage->getParent();
    }

    // work on DVDs or floppies list?
    ObjectsList<Medium> &all = (argType == DeviceType_DVD) ? m->allDVDImages : m->allFloppyImages;

    // access the list directly because the caller must have requested the lock
    all.getList().remove(argImage);

    HRESULT rc = S_OK;

    if (pllRegistriesThatNeedSaving)
        argImage->addToRegistryIDList(*pllRegistriesThatNeedSaving);

    return rc;
}

/**
 * Little helper called from unregisterMachineMedia() to recursively add media to the given list,
 * with children appearing before their parents.
 * @param llMedia
 * @param pMedium
 */
void VirtualBox::pushMediumToListWithChildren(MediaList &llMedia, Medium *pMedium)
{
    // recurse first, then add ourselves; this way children end up on the
    // list before their parents

    const MediaList &llChildren = pMedium->getChildren();
    for (MediaList::const_iterator it = llChildren.begin();
         it != llChildren.end();
         ++it)
    {
        Medium *pChild = *it;
        pushMediumToListWithChildren(llMedia, pChild);
    }

    Log(("Pushing medium %RTuuid\n", pMedium->getId().raw()));
    llMedia.push_back(pMedium);
}

/**
 * Unregisters all Medium objects which belong to the given machine registry.
 * Gets called from Machine::uninit() just before the machine object dies
 * and must only be called with a machine UUID as the registry ID.
 *
 * Locks the media tree.
 *
 * @param uuidMachine Medium registry ID (always a machine UUID)
 * @return
 */
HRESULT VirtualBox::unregisterMachineMedia(const Guid &uuidMachine)
{
    Assert(!uuidMachine.isEmpty());

    LogFlowFuncEnter();

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    MediaList llMedia2Close;

    {
        AutoWriteLock tlock(getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

        for (MediaOList::iterator it = m->allHardDisks.getList().begin();
             it != m->allHardDisks.getList().end();
             ++it)
        {
            ComObjPtr<Medium> pMedium = *it;
            AutoCaller medCaller(pMedium);
            if (FAILED(medCaller.rc())) return medCaller.rc();
            AutoReadLock medlock(pMedium COMMA_LOCKVAL_SRC_POS);

            if (pMedium->isInRegistry(uuidMachine))
                // recursively with children first
                pushMediumToListWithChildren(llMedia2Close, pMedium);
        }
    }

    for (MediaList::iterator it = llMedia2Close.begin();
         it != llMedia2Close.end();
         ++it)
    {
        ComObjPtr<Medium> pMedium = *it;
        Log(("Closing medium %RTuuid\n", pMedium->getId().raw()));
        AutoCaller mac(pMedium);
        pMedium->close(NULL /* pfNeedsGlobalSaveSettings*/, mac);
    }

    LogFlowFuncLeave();

    return S_OK;
}

/**
 * Removes the given machine object from the internal list of registered machines.
 * Called from Machine::Unregister().
 * @param pMachine
 * @param id  UUID of the machine. Must be passed by caller because machine may be dead by this time.
 * @return
 */
HRESULT VirtualBox::unregisterMachine(Machine *pMachine,
                                      const Guid &id)
{
    // remove from the collection of registered machines
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    m->allMachines.removeChild(pMachine);
    // save the global registry
    HRESULT rc = saveSettings();
    alock.release();

    /*
     * Now go over all known media and checks if they were registered in the
     * media registry of the given machine. Each such medium is then moved to
     * a different media registry to make sure it doesn't get lost since its
     * media registry is about to go away.
     *
     * This fixes the following use case: Image A.vdi of machine A is also used
     * by machine B, but registered in the media registry of machine A. If machine
     * A is deleted, A.vdi must be moved to the registry of B, or else B will
     * become inaccessible.
     */
    GuidList llRegistriesThatNeedSaving;
    {
        AutoReadLock tlock(getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);
        // iterate over the list of *base* images
        for (MediaOList::iterator it = m->allHardDisks.getList().begin();
             it != m->allHardDisks.getList().end();
             ++it)
        {
            ComObjPtr<Medium> &pMedium = *it;
            AutoCaller medCaller(pMedium);
            if (FAILED(medCaller.rc())) return medCaller.rc();
            AutoWriteLock mlock(pMedium COMMA_LOCKVAL_SRC_POS);

            if (pMedium->removeRegistry(id, true /* fRecurse */))
            {
                // machine ID was found in base medium's registry list:
                // move this base image and all its children to another registry then
                // 1) first, find a better registry to add things to
                const Guid *puuidBetter = pMedium->getAnyMachineBackref();
                if (puuidBetter)
                {
                    // 2) better registry found: then use that
                    pMedium->addRegistry(*puuidBetter, true /* fRecurse */);
                    // 3) and make sure the registry is saved below
                    VirtualBox::addGuidToListUniquely(llRegistriesThatNeedSaving, *puuidBetter);
                }
            }
        }
    }

    saveRegistries(llRegistriesThatNeedSaving);

    /* fire an event */
    onMachineRegistered(id, FALSE);

    return rc;
}

/**
 * Adds uuid to llRegistriesThatNeedSaving unless it's already on the list.
 *
 * @todo maybe there's something in libstdc++ for this
 *
 * @param llRegistriesThatNeedSaving
 * @param uuid
 */
/* static */
void VirtualBox::addGuidToListUniquely(GuidList &llRegistriesThatNeedSaving,
                                       const Guid &uuid)
{
    for (GuidList::const_iterator it = llRegistriesThatNeedSaving.begin();
         it != llRegistriesThatNeedSaving.end();
         ++it)
    {
        if (*it == uuid)
            // uuid is already in list:
            return;
    }

    llRegistriesThatNeedSaving.push_back(uuid);
}

/**
 * Saves all settings files according to the given list of UUIDs, which are
 * either machine IDs (in which case Machine::saveSettings is invoked) or
 * the global registry UUID (in which case VirtualBox::saveSettings is invoked).
 *
 * This locks machines and the VirtualBox object as necessary, so better not
 * hold any locks before calling this.
 *
 * @param llRegistriesThatNeedSaving
 * @return
 */
HRESULT VirtualBox::saveRegistries(const GuidList &llRegistriesThatNeedSaving)
{
    bool fNeedsGlobalSettings = false;
    HRESULT rc = S_OK;

    for (GuidList::const_iterator it = llRegistriesThatNeedSaving.begin();
         it != llRegistriesThatNeedSaving.end();
         ++it)
    {
        const Guid &uuid = *it;

        if (uuid == getGlobalRegistryId())
            fNeedsGlobalSettings = true;
        else
        {
            // should be machine ID then:
            ComObjPtr<Machine> pMachine;
            rc = findMachine(uuid,
                             false /* fPermitInaccessible */,
                             false /* aSetError */,
                             &pMachine);
            if (SUCCEEDED(rc))
            {
                AutoCaller autoCaller(pMachine);
                if (FAILED(autoCaller.rc())) return autoCaller.rc();
                AutoWriteLock mlock(pMachine COMMA_LOCKVAL_SRC_POS);
                rc = pMachine->saveSettings(&fNeedsGlobalSettings,
                                            Machine::SaveS_Force);           // caller said save, so stop arguing
            }

            if (FAILED(rc))
                return rc;
        }
    }

    if (fNeedsGlobalSettings)
    {
        AutoWriteLock vlock(this COMMA_LOCKVAL_SRC_POS);
        rc = saveSettings();
    }

    return S_OK;
}

/**
 * Checks if the path to the specified file exists, according to the path
 * information present in the file name. Optionally the path is created.
 *
 * Note that the given file name must contain the full path otherwise the
 * extracted relative path will be created based on the current working
 * directory which is normally unknown.
 *
 * @param aFileName     Full file name which path is checked/created.
 * @param aCreate       Flag if the path should be created if it doesn't exist.
 *
 * @return Extended error information on failure to check/create the path.
 */
/* static */
HRESULT VirtualBox::ensureFilePathExists(const Utf8Str &strFileName, bool fCreate)
{
    Utf8Str strDir(strFileName);
    strDir.stripFilename();
    if (!RTDirExists(strDir.c_str()))
    {
        if (fCreate)
        {
            int vrc = RTDirCreateFullPath(strDir.c_str(), 0700);
            if (RT_FAILURE(vrc))
                return setErrorStatic(VBOX_E_IPRT_ERROR,
                                      Utf8StrFmt(tr("Could not create the directory '%s' (%Rrc)"),
                                                 strDir.c_str(),
                                                 vrc));
        }
        else
            return setErrorStatic(VBOX_E_IPRT_ERROR,
                                  Utf8StrFmt(tr("Directory '%s' does not exist"),
                                             strDir.c_str()));
    }

    return S_OK;
}

/**
 * Handles unexpected exceptions by turning them into COM errors in release
 * builds or by hitting a breakpoint in the release builds.
 *
 * Usage pattern:
 * @code
        try
        {
            // ...
        }
        catch (LaLalA)
        {
            // ...
        }
        catch (...)
        {
            rc = VirtualBox::handleUnexpectedExceptions (RT_SRC_POS);
        }
 * @endcode
 *
 * @param RT_SRC_POS_DECL "RT_SRC_POS" macro instantiation.
 */
/* static */
HRESULT VirtualBox::handleUnexpectedExceptions(RT_SRC_POS_DECL)
{
    try
    {
        /* re-throw the current exception */
        throw;
    }
    catch (const RTCError &err)      // includes all XML exceptions
    {
        return setErrorStatic(E_FAIL,
                              Utf8StrFmt(tr("%s.\n%s[%d] (%s)"),
                                         err.what(),
                                         pszFile, iLine, pszFunction).c_str());
    }
    catch (const std::exception &err)
    {
        return setErrorStatic(E_FAIL,
                              Utf8StrFmt(tr("Unexpected exception: %s [%s]\n%s[%d] (%s)"),
                                         err.what(), typeid(err).name(),
                                         pszFile, iLine, pszFunction).c_str());
    }
    catch (...)
    {
        return setErrorStatic(E_FAIL,
                              Utf8StrFmt(tr("Unknown exception\n%s[%d] (%s)"),
                                         pszFile, iLine, pszFunction).c_str());
    }

    /* should not get here */
    AssertFailed();
    return E_FAIL;
}

const Utf8Str& VirtualBox::settingsFilePath()
{
    return m->strSettingsFilePath;
}

/**
 * Returns the lock handle which protects the media trees (hard disks,
 * DVDs, floppies). As opposed to version 3.1 and earlier, these lists
 * are no longer protected by the VirtualBox lock, but by this more
 * specialized lock. Mind the locking order: always request this lock
 * after the VirtualBox object lock but before the locks of the media
 * objects contained in these lists. See AutoLock.h.
 */
RWLockHandle& VirtualBox::getMediaTreeLockHandle()
{
    return m->lockMedia;
}

/**
 *  Thread function that watches the termination of all client processes
 *  that have opened sessions using IMachine::LockMachine()
 */
// static
DECLCALLBACK(int) VirtualBox::ClientWatcher(RTTHREAD /* thread */, void *pvUser)
{
    LogFlowFuncEnter();

    VirtualBox *that = (VirtualBox*)pvUser;
    Assert(that);

    typedef std::vector< ComObjPtr<Machine> > MachineVector;
    typedef std::vector< ComObjPtr<SessionMachine> > SessionMachineVector;

    SessionMachineVector machines;
    MachineVector spawnedMachines;

    size_t cnt = 0;
    size_t cntSpawned = 0;

    VirtualBoxBase::initializeComForThread();

#if defined(RT_OS_WINDOWS)

    HRESULT hrc;

    /// @todo (dmik) processes reaping!

    HANDLE handles[MAXIMUM_WAIT_OBJECTS];
    handles[0] = that->m->updateReq;

    do
    {
        AutoCaller autoCaller(that);
        /* VirtualBox has been early uninitialized, terminate */
        if (!autoCaller.isOk())
            break;

        do
        {
            /* release the caller to let uninit() ever proceed */
            autoCaller.release();

            DWORD rc = ::WaitForMultipleObjects((DWORD)(1 + cnt + cntSpawned),
                                                handles,
                                                FALSE,
                                                INFINITE);

            /* Restore the caller before using VirtualBox. If it fails, this
             * means VirtualBox is being uninitialized and we must terminate. */
            autoCaller.add();
            if (!autoCaller.isOk())
                break;

            bool update = false;

            if (rc == WAIT_OBJECT_0)
            {
                /* update event is signaled */
                update = true;
            }
            else if (rc > WAIT_OBJECT_0 && rc <= (WAIT_OBJECT_0 + cnt))
            {
                /* machine mutex is released */
                (machines[rc - WAIT_OBJECT_0 - 1])->checkForDeath();
                update = true;
            }
            else if (rc > WAIT_ABANDONED_0 && rc <= (WAIT_ABANDONED_0 + cnt))
            {
                /* machine mutex is abandoned due to client process termination */
                (machines[rc - WAIT_ABANDONED_0 - 1])->checkForDeath();
                update = true;
            }
            else if (rc > WAIT_OBJECT_0 + cnt && rc <= (WAIT_OBJECT_0 + cntSpawned))
            {
                /* spawned VM process has terminated (normally or abnormally) */
                (spawnedMachines[rc - WAIT_OBJECT_0 - cnt - 1])->
                    checkForSpawnFailure();
                update = true;
            }

            if (update)
            {
                /* close old process handles */
                for (size_t i = 1 + cnt; i < 1 + cnt + cntSpawned; ++i)
                    CloseHandle(handles[i]);

                // lock the machines list for reading
                AutoReadLock thatLock(that->m->allMachines.getLockHandle() COMMA_LOCKVAL_SRC_POS);

                /* obtain a new set of opened machines */
                cnt = 0;
                machines.clear();

                for (MachinesOList::iterator it = that->m->allMachines.begin();
                     it != that->m->allMachines.end();
                     ++it)
                {
                    /// @todo handle situations with more than 64 objects
                    AssertMsgBreak((1 + cnt) <= MAXIMUM_WAIT_OBJECTS,
                                   ("MAXIMUM_WAIT_OBJECTS reached"));

                    ComObjPtr<SessionMachine> sm;
                    HANDLE ipcSem;
                    if ((*it)->isSessionOpenOrClosing(sm, NULL, &ipcSem))
                    {
                        machines.push_back(sm);
                        handles[1 + cnt] = ipcSem;
                        ++cnt;
                    }
                }

                LogFlowFunc(("UPDATE: direct session count = %d\n", cnt));

                /* obtain a new set of spawned machines */
                cntSpawned = 0;
                spawnedMachines.clear();

                for (MachinesOList::iterator it = that->m->allMachines.begin();
                     it != that->m->allMachines.end();
                     ++it)
                {
                    /// @todo handle situations with more than 64 objects
                    AssertMsgBreak((1 + cnt + cntSpawned) <= MAXIMUM_WAIT_OBJECTS,
                                   ("MAXIMUM_WAIT_OBJECTS reached"));

                    RTPROCESS pid;
                    if ((*it)->isSessionSpawning(&pid))
                    {
                        HANDLE ph = OpenProcess(SYNCHRONIZE, FALSE, pid);
                        AssertMsg(ph != NULL, ("OpenProcess (pid=%d) failed with %d\n",
                                               pid, GetLastError()));
                        if (rc == 0)
                        {
                            spawnedMachines.push_back(*it);
                            handles[1 + cnt + cntSpawned] = ph;
                            ++cntSpawned;
                        }
                    }
                }

                LogFlowFunc(("UPDATE: spawned session count = %d\n", cntSpawned));

                // machines lock unwinds here
            }
        }
        while (true);
    }
    while (0);

    /* close old process handles */
    for (size_t i = 1 + cnt; i < 1 + cnt + cntSpawned; ++ i)
        CloseHandle(handles[i]);

    /* release sets of machines if any */
    machines.clear();
    spawnedMachines.clear();

    ::CoUninitialize();

#elif defined(RT_OS_OS2)

    /// @todo (dmik) processes reaping!

    /* according to PMREF, 64 is the maximum for the muxwait list */
    SEMRECORD handles[64];

    HMUX muxSem = NULLHANDLE;

    do
    {
        AutoCaller autoCaller(that);
        /* VirtualBox has been early uninitialized, terminate */
        if (!autoCaller.isOk())
            break;

        do
        {
            /* release the caller to let uninit() ever proceed */
            autoCaller.release();

            int vrc = RTSemEventWait(that->m->updateReq, 500);

            /* Restore the caller before using VirtualBox. If it fails, this
             * means VirtualBox is being uninitialized and we must terminate. */
            autoCaller.add();
            if (!autoCaller.isOk())
                break;

            bool update = false;
            bool updateSpawned = false;

            if (RT_SUCCESS(vrc))
            {
                /* update event is signaled */
                update = true;
                updateSpawned = true;
            }
            else
            {
                AssertMsg(vrc == VERR_TIMEOUT || vrc == VERR_INTERRUPTED,
                          ("RTSemEventWait returned %Rrc\n", vrc));

                /* are there any mutexes? */
                if (cnt > 0)
                {
                    /* figure out what's going on with machines */

                    unsigned long semId = 0;
                    APIRET arc = ::DosWaitMuxWaitSem(muxSem,
                                                     SEM_IMMEDIATE_RETURN, &semId);

                    if (arc == NO_ERROR)
                    {
                        /* machine mutex is normally released */
                        Assert(semId >= 0 && semId < cnt);
                        if (semId >= 0 && semId < cnt)
                        {
#if 0//def DEBUG
                            {
                                AutoReadLock machineLock(machines[semId] COMMA_LOCKVAL_SRC_POS);
                                LogFlowFunc(("released mutex: machine='%ls'\n",
                                             machines[semId]->name().raw()));
                            }
#endif
                            machines[semId]->checkForDeath();
                        }
                        update = true;
                    }
                    else if (arc == ERROR_SEM_OWNER_DIED)
                    {
                        /* machine mutex is abandoned due to client process
                         * termination; find which mutex is in the Owner Died
                         * state */
                        for (size_t i = 0; i < cnt; ++ i)
                        {
                            PID pid; TID tid;
                            unsigned long reqCnt;
                            arc = DosQueryMutexSem((HMTX)handles[i].hsemCur, &pid, &tid, &reqCnt);
                            if (arc == ERROR_SEM_OWNER_DIED)
                            {
                                /* close the dead mutex as asked by PMREF */
                                ::DosCloseMutexSem((HMTX)handles[i].hsemCur);

                                Assert(i >= 0 && i < cnt);
                                if (i >= 0 && i < cnt)
                                {
#if 0//def DEBUG
                                    {
                                        AutoReadLock machineLock(machines[semId] COMMA_LOCKVAL_SRC_POS);
                                        LogFlowFunc(("mutex owner dead: machine='%ls'\n",
                                                     machines[i]->name().raw()));
                                    }
#endif
                                    machines[i]->checkForDeath();
                                }
                            }
                        }
                        update = true;
                    }
                    else
                        AssertMsg(arc == ERROR_INTERRUPT || arc == ERROR_TIMEOUT,
                                  ("DosWaitMuxWaitSem returned %d\n", arc));
                }

                /* are there any spawning sessions? */
                if (cntSpawned > 0)
                {
                    for (size_t i = 0; i < cntSpawned; ++ i)
                        updateSpawned |= (spawnedMachines[i])->
                            checkForSpawnFailure();
                }
            }

            if (update || updateSpawned)
            {
                AutoReadLock thatLock(that COMMA_LOCKVAL_SRC_POS);

                if (update)
                {
                    /* close the old muxsem */
                    if (muxSem != NULLHANDLE)
                        ::DosCloseMuxWaitSem(muxSem);

                    /* obtain a new set of opened machines */
                    cnt = 0;
                    machines.clear();

                    for (MachinesOList::iterator it = that->m->allMachines.begin();
                         it != that->m->allMachines.end(); ++ it)
                    {
                        /// @todo handle situations with more than 64 objects
                        AssertMsg(cnt <= 64 /* according to PMREF */,
                                  ("maximum of 64 mutex semaphores reached (%d)",
                                   cnt));

                        ComObjPtr<SessionMachine> sm;
                        HMTX ipcSem;
                        if ((*it)->isSessionOpenOrClosing(sm, NULL, &ipcSem))
                        {
                            machines.push_back(sm);
                            handles[cnt].hsemCur = (HSEM)ipcSem;
                            handles[cnt].ulUser = cnt;
                            ++ cnt;
                        }
                    }

                    LogFlowFunc(("UPDATE: direct session count = %d\n", cnt));

                    if (cnt > 0)
                    {
                        /* create a new muxsem */
                        APIRET arc = ::DosCreateMuxWaitSem(NULL, &muxSem, cnt,
                                                           handles,
                                                           DCMW_WAIT_ANY);
                        AssertMsg(arc == NO_ERROR,
                                  ("DosCreateMuxWaitSem returned %d\n", arc));
                        NOREF(arc);
                    }
                }

                if (updateSpawned)
                {
                    /* obtain a new set of spawned machines */
                    spawnedMachines.clear();

                    for (MachinesOList::iterator it = that->m->allMachines.begin();
                         it != that->m->allMachines.end(); ++ it)
                    {
                        if ((*it)->isSessionSpawning())
                            spawnedMachines.push_back(*it);
                    }

                    cntSpawned = spawnedMachines.size();
                    LogFlowFunc(("UPDATE: spawned session count = %d\n", cntSpawned));
                }
            }
        }
        while (true);
    }
    while (0);

    /* close the muxsem */
    if (muxSem != NULLHANDLE)
        ::DosCloseMuxWaitSem(muxSem);

    /* release sets of machines if any */
    machines.clear();
    spawnedMachines.clear();

#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)

    bool update = false;
    bool updateSpawned = false;

    do
    {
        AutoCaller autoCaller(that);
        if (!autoCaller.isOk())
            break;

        do
        {
            /* release the caller to let uninit() ever proceed */
            autoCaller.release();

            int rc = RTSemEventWait(that->m->updateReq, 500);

            /*
             *  Restore the caller before using VirtualBox. If it fails, this
             *  means VirtualBox is being uninitialized and we must terminate.
             */
            autoCaller.add();
            if (!autoCaller.isOk())
                break;

            if (RT_SUCCESS(rc) || update || updateSpawned)
            {
                /* RT_SUCCESS(rc) means an update event is signaled */

                // lock the machines list for reading
                AutoReadLock thatLock(that->m->allMachines.getLockHandle() COMMA_LOCKVAL_SRC_POS);

                if (RT_SUCCESS(rc) || update)
                {
                    /* obtain a new set of opened machines */
                    machines.clear();

                    for (MachinesOList::iterator it = that->m->allMachines.begin();
                         it != that->m->allMachines.end();
                         ++it)
                    {
                        ComObjPtr<SessionMachine> sm;
                        if ((*it)->isSessionOpenOrClosing(sm))
                            machines.push_back(sm);
                    }

                    cnt = machines.size();
                    LogFlowFunc(("UPDATE: direct session count = %d\n", cnt));
                }

                if (RT_SUCCESS(rc) || updateSpawned)
                {
                    /* obtain a new set of spawned machines */
                    spawnedMachines.clear();

                    for (MachinesOList::iterator it = that->m->allMachines.begin();
                         it != that->m->allMachines.end();
                         ++it)
                    {
                        if ((*it)->isSessionSpawning())
                            spawnedMachines.push_back(*it);
                    }

                    cntSpawned = spawnedMachines.size();
                    LogFlowFunc(("UPDATE: spawned session count = %d\n", cntSpawned));
                }

                // machines lock unwinds here
            }

            update = false;
            for (size_t i = 0; i < cnt; ++ i)
                update |= (machines[i])->checkForDeath();

            updateSpawned = false;
            for (size_t i = 0; i < cntSpawned; ++ i)
                updateSpawned |= (spawnedMachines[i])->checkForSpawnFailure();

            /* reap child processes */
            {
                AutoWriteLock alock(that COMMA_LOCKVAL_SRC_POS);
                if (that->m->llProcesses.size())
                {
                    LogFlowFunc(("UPDATE: child process count = %d\n",
                                 that->m->llProcesses.size()));
                    VirtualBox::Data::ProcessList::iterator it = that->m->llProcesses.begin();
                    while (it != that->m->llProcesses.end())
                    {
                        RTPROCESS pid = *it;
                        RTPROCSTATUS status;
                        int vrc = ::RTProcWait(pid, RTPROCWAIT_FLAGS_NOBLOCK, &status);
                        if (vrc == VINF_SUCCESS)
                        {
                            LogFlowFunc(("pid %d (%x) was reaped, status=%d, reason=%d\n",
                                         pid, pid, status.iStatus,
                                         status.enmReason));
                            it = that->m->llProcesses.erase(it);
                        }
                        else
                        {
                            LogFlowFunc(("pid %d (%x) was NOT reaped, vrc=%Rrc\n",
                                         pid, pid, vrc));
                            if (vrc != VERR_PROCESS_RUNNING)
                            {
                                /* remove the process if it is not already running */
                                it = that->m->llProcesses.erase(it);
                            }
                            else
                                ++ it;
                        }
                    }
                }
            }
        }
        while (true);
    }
    while (0);

    /* release sets of machines if any */
    machines.clear();
    spawnedMachines.clear();

#else
# error "Port me!"
#endif

    VirtualBoxBase::uninitializeComForThread();
    LogFlowFuncLeave();
    return 0;
}

/**
 *  Thread function that handles custom events posted using #postEvent().
 */
// static
DECLCALLBACK(int) VirtualBox::AsyncEventHandler(RTTHREAD thread, void *pvUser)
{
    LogFlowFuncEnter();

    AssertReturn(pvUser, VERR_INVALID_POINTER);

    com::Initialize();

    // create an event queue for the current thread
    EventQueue *eventQ = new EventQueue();
    AssertReturn(eventQ, VERR_NO_MEMORY);

    // return the queue to the one who created this thread
    *(static_cast <EventQueue **>(pvUser)) = eventQ;
    // signal that we're ready
    RTThreadUserSignal(thread);

    /*
     * In case of spurious wakeups causing VERR_TIMEOUTs and/or other return codes
     * we must not stop processing events and delete the "eventQ" object. This must
     * be done ONLY when we stop this loop via interruptEventQueueProcessing().
     * See #5724.
     */
    while (eventQ->processEventQueue(RT_INDEFINITE_WAIT) != VERR_INTERRUPTED)
        /* nothing */ ;

    delete eventQ;

    com::Shutdown();


    LogFlowFuncLeave();

    return 0;
}


////////////////////////////////////////////////////////////////////////////////

/**
 *  Takes the current list of registered callbacks of the managed VirtualBox
 *  instance, and calls #handleCallback() for every callback item from the
 *  list, passing the item as an argument.
 *
 *  @note Locks the managed VirtualBox object for reading but leaves the lock
 *        before iterating over callbacks and calling their methods.
 */
void *VirtualBox::CallbackEvent::handler()
{
    if (!mVirtualBox)
        return NULL;

    AutoCaller autoCaller(mVirtualBox);
    if (!autoCaller.isOk())
    {
        LogWarningFunc(("VirtualBox has been uninitialized (state=%d), the callback event is discarded!\n",
                        autoCaller.state()));
        /* We don't need mVirtualBox any more, so release it */
        mVirtualBox = NULL;
        return NULL;
    }

    {
        VBoxEventDesc evDesc;
        prepareEventDesc(mVirtualBox->m->pEventSource, evDesc);

        evDesc.fire(/* don't wait for delivery */0);
    }

    mVirtualBox = NULL; /* Not needed any longer. Still make sense to do this? */
    return NULL;
}

//STDMETHODIMP VirtualBox::CreateDHCPServerForInterface(/*IHostNetworkInterface * aIinterface,*/ IDHCPServer ** aServer)
//{
//    return E_NOTIMPL;
//}

STDMETHODIMP VirtualBox::CreateDHCPServer(IN_BSTR aName, IDHCPServer ** aServer)
{
    CheckComArgStrNotEmptyOrNull(aName);
    CheckComArgNotNull(aServer);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ComObjPtr<DHCPServer> dhcpServer;
    dhcpServer.createObject();
    HRESULT rc = dhcpServer->init(this, aName);
    if (FAILED(rc)) return rc;

    rc = registerDHCPServer(dhcpServer, true);
    if (FAILED(rc)) return rc;

    dhcpServer.queryInterfaceTo(aServer);

    return rc;
}

STDMETHODIMP VirtualBox::FindDHCPServerByNetworkName(IN_BSTR aName, IDHCPServer ** aServer)
{
    CheckComArgStrNotEmptyOrNull(aName);
    CheckComArgNotNull(aServer);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc;
    Bstr bstr;
    ComPtr<DHCPServer> found;

    AutoReadLock alock(m->allDHCPServers.getLockHandle() COMMA_LOCKVAL_SRC_POS);

    for (DHCPServersOList::const_iterator it = m->allDHCPServers.begin();
         it != m->allDHCPServers.end();
         ++it)
    {
        rc = (*it)->COMGETTER(NetworkName)(bstr.asOutParam());
        if (FAILED(rc)) return rc;

        if (bstr == aName)
        {
            found = *it;
            break;
        }
    }

    if (!found)
        return E_INVALIDARG;

    return found.queryInterfaceTo(aServer);
}

STDMETHODIMP VirtualBox::RemoveDHCPServer(IDHCPServer * aServer)
{
    CheckComArgNotNull(aServer);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = unregisterDHCPServer(static_cast<DHCPServer *>(aServer), true);

    return rc;
}

/**
 * Remembers the given dhcp server by storing it in the hard disk registry.
 *
 * @param aDHCPServer     Dhcp Server object to remember.
 * @param aSaveRegistry @c true to save hard disk registry to disk (default).
 *
 * When @a aSaveRegistry is @c true, this operation may fail because of the
 * failed #saveSettings() method it calls. In this case, the dhcp server object
 * will not be remembered. It is therefore the responsibility of the caller to
 * call this method as the last step of some action that requires registration
 * in order to make sure that only fully functional dhcp server objects get
 * registered.
 *
 * @note Locks this object for writing and @a aDHCPServer for reading.
 */
HRESULT VirtualBox::registerDHCPServer(DHCPServer *aDHCPServer,
                                       bool aSaveRegistry /*= true*/)
{
    AssertReturn(aDHCPServer != NULL, E_INVALIDARG);

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoCaller dhcpServerCaller(aDHCPServer);
    AssertComRCReturn(dhcpServerCaller.rc(), dhcpServerCaller.rc());

    Bstr name;
    HRESULT rc;
    rc = aDHCPServer->COMGETTER(NetworkName)(name.asOutParam());
    if (FAILED(rc)) return rc;

    ComPtr<IDHCPServer> existing;
    rc = FindDHCPServerByNetworkName(name.raw(), existing.asOutParam());
    if (SUCCEEDED(rc))
        return E_INVALIDARG;

    rc = S_OK;

    m->allDHCPServers.addChild(aDHCPServer);

    if (aSaveRegistry)
    {
        AutoWriteLock vboxLock(this COMMA_LOCKVAL_SRC_POS);
        rc = saveSettings();
        vboxLock.release();

        if (FAILED(rc))
            unregisterDHCPServer(aDHCPServer, false /* aSaveRegistry */);
    }

    return rc;
}

/**
 * Removes the given hard disk from the hard disk registry.
 *
 * @param aHardDisk     Hard disk object to remove.
 * @param aSaveRegistry @c true to save hard disk registry to disk (default).
 *
 * When @a aSaveRegistry is @c true, this operation may fail because of the
 * failed #saveSettings() method it calls. In this case, the hard disk object
 * will NOT be removed from the registry when this method returns. It is
 * therefore the responsibility of the caller to call this method as the first
 * step of some action that requires unregistration, before calling uninit() on
 * @a aHardDisk.
 *
 * @note Locks this object for writing and @a aHardDisk for reading.
 */
HRESULT VirtualBox::unregisterDHCPServer(DHCPServer *aDHCPServer,
                                         bool aSaveRegistry /*= true*/)
{
    AssertReturn(aDHCPServer != NULL, E_INVALIDARG);

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoCaller dhcpServerCaller(aDHCPServer);
    AssertComRCReturn(dhcpServerCaller.rc(), dhcpServerCaller.rc());

    m->allDHCPServers.removeChild(aDHCPServer);

    HRESULT rc = S_OK;

    if (aSaveRegistry)
    {
        AutoWriteLock vboxLock(this COMMA_LOCKVAL_SRC_POS);
        rc = saveSettings();
        vboxLock.release();

        if (FAILED(rc))
            registerDHCPServer(aDHCPServer, false /* aSaveRegistry */);
    }

    return rc;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
