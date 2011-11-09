/* $Id$ */
/** @file
 * VBox Console COM Class implementation
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/** @todo Move the TAP mess back into the driver! */
#if defined(RT_OS_WINDOWS)
#elif defined(RT_OS_LINUX)
#   include <errno.h>
#   include <sys/ioctl.h>
#   include <sys/poll.h>
#   include <sys/fcntl.h>
#   include <sys/types.h>
#   include <sys/wait.h>
#   include <net/if.h>
#   include <linux/if_tun.h>
#   include <stdio.h>
#   include <stdlib.h>
#   include <string.h>
#elif defined(RT_OS_FREEBSD)
#   include <errno.h>
#   include <sys/ioctl.h>
#   include <sys/poll.h>
#   include <sys/fcntl.h>
#   include <sys/types.h>
#   include <sys/wait.h>
#   include <stdio.h>
#   include <stdlib.h>
#   include <string.h>
#elif defined(RT_OS_SOLARIS)
#   include <iprt/coredumper.h>
#endif

#include "ConsoleImpl.h"

#include "Global.h"
#include "VirtualBoxErrorInfoImpl.h"
#include "GuestImpl.h"
#include "KeyboardImpl.h"
#include "MouseImpl.h"
#include "DisplayImpl.h"
#include "MachineDebuggerImpl.h"
#include "USBDeviceImpl.h"
#include "RemoteUSBDeviceImpl.h"
#include "SharedFolderImpl.h"
#include "AudioSnifferInterface.h"
#ifdef VBOX_WITH_USB_VIDEO
# include "UsbWebcamInterface.h"
#endif
#include "ProgressCombinedImpl.h"
#include "ConsoleVRDPServer.h"
#include "VMMDev.h"
#include "package-generated.h"
#ifdef VBOX_WITH_EXTPACK
# include "ExtPackManagerImpl.h"
#endif
#include "BusAssignmentManager.h"

#include "VBoxEvents.h"
#include "AutoCaller.h"
#include "Logging.h"

#include <VBox/com/array.h>
#include "VBox/com/ErrorInfo.h"
#include <VBox/com/listeners.h>

#include <iprt/asm.h>
#include <iprt/buildconfig.h>
#include <iprt/cpp/utils.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/ldr.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/system.h>

#include <VBox/vmm/vmapi.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/vmm/pdmasynccompletion.h>
#include <VBox/vmm/pdmnetifs.h>
#ifdef VBOX_WITH_USB
# include <VBox/vmm/pdmusb.h>
#endif
#include <VBox/vmm/mm.h>
#include <VBox/vmm/ftm.h>
#include <VBox/vmm/ssm.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/vusb.h>
#include <VBox/version.h>

#include <VBox/VMMDev.h>

#include <VBox/HostServices/VBoxClipboardSvc.h>
#ifdef VBOX_WITH_GUEST_PROPS
# include <VBox/HostServices/GuestPropertySvc.h>
# include <VBox/com/array.h>
#endif

#include <set>
#include <algorithm>
#include <memory> // for auto_ptr
#include <vector>
#include <typeinfo>


// VMTask and friends
////////////////////////////////////////////////////////////////////////////////

/**
 * Task structure for asynchronous VM operations.
 *
 * Once created, the task structure adds itself as a Console caller. This means:
 *
 * 1. The user must check for #rc() before using the created structure
 *    (e.g. passing it as a thread function argument). If #rc() returns a
 *    failure, the Console object may not be used by the task (see
 *    Console::addCaller() for more details).
 * 2. On successful initialization, the structure keeps the Console caller
 *    until destruction (to ensure Console remains in the Ready state and won't
 *    be accidentally uninitialized). Forgetting to delete the created task
 *    will lead to Console::uninit() stuck waiting for releasing all added
 *    callers.
 *
 * If \a aUsesVMPtr parameter is true, the task structure will also add itself
 * as a Console::mpUVM caller with the same meaning as above. See
 * Console::addVMCaller() for more info.
 */
struct VMTask
{
    VMTask(Console *aConsole,
           Progress *aProgress,
           const ComPtr<IProgress> &aServerProgress,
           bool aUsesVMPtr)
        : mConsole(aConsole),
          mConsoleCaller(aConsole),
          mProgress(aProgress),
          mServerProgress(aServerProgress),
          mpVM(NULL),
          mRC(E_FAIL),
          mpSafeVMPtr(NULL)
    {
        AssertReturnVoid(aConsole);
        mRC = mConsoleCaller.rc();
        if (FAILED(mRC))
            return;
        if (aUsesVMPtr)
        {
            mpSafeVMPtr = new Console::SafeVMPtr(aConsole);
            if (mpSafeVMPtr->isOk())
                mpVM = mpSafeVMPtr->raw();
            else
                mRC = mpSafeVMPtr->rc();
        }
    }

    ~VMTask()
    {
        releaseVMCaller();
    }

    HRESULT rc() const { return mRC; }
    bool isOk() const { return SUCCEEDED(rc()); }

    /** Releases the VM caller before destruction. Not normally necessary. */
    void releaseVMCaller()
    {
        if (mpSafeVMPtr)
        {
            delete mpSafeVMPtr;
            mpSafeVMPtr = NULL;
        }
    }

    const ComObjPtr<Console>    mConsole;
    AutoCaller                  mConsoleCaller;
    const ComObjPtr<Progress>   mProgress;
    Utf8Str                     mErrorMsg;
    const ComPtr<IProgress>     mServerProgress;
    PVM                         mpVM;

private:
    HRESULT                     mRC;
    Console::SafeVMPtr         *mpSafeVMPtr;
};

struct VMTakeSnapshotTask : public VMTask
{
    VMTakeSnapshotTask(Console *aConsole,
                       Progress *aProgress,
                       IN_BSTR aName,
                       IN_BSTR aDescription)
        : VMTask(aConsole, aProgress, NULL /* aServerProgress */,
                 false /* aUsesVMPtr */),
          bstrName(aName),
          bstrDescription(aDescription),
          lastMachineState(MachineState_Null)
    {}

    Bstr                    bstrName,
                            bstrDescription;
    Bstr                    bstrSavedStateFile;         // received from BeginTakeSnapshot()
    MachineState_T          lastMachineState;
    bool                    fTakingSnapshotOnline;
    ULONG                   ulMemSize;
};

struct VMPowerUpTask : public VMTask
{
    VMPowerUpTask(Console *aConsole,
                  Progress *aProgress)
        : VMTask(aConsole, aProgress, NULL /* aServerProgress */,
                 false /* aUsesVMPtr */),
          mConfigConstructor(NULL),
          mStartPaused(false),
          mTeleporterEnabled(FALSE),
          mEnmFaultToleranceState(FaultToleranceState_Inactive)
    {}

    PFNCFGMCONSTRUCTOR mConfigConstructor;
    Utf8Str mSavedStateFile;
    Console::SharedFolderDataMap mSharedFolders;
    bool mStartPaused;
    BOOL mTeleporterEnabled;
    FaultToleranceState_T mEnmFaultToleranceState;

    /* array of progress objects for hard disk reset operations */
    typedef std::list<ComPtr<IProgress> > ProgressList;
    ProgressList hardDiskProgresses;
};

struct VMPowerDownTask : public VMTask
{
    VMPowerDownTask(Console *aConsole,
                    const ComPtr<IProgress> &aServerProgress)
        : VMTask(aConsole, NULL /* aProgress */, aServerProgress,
                 true /* aUsesVMPtr */)
    {}
};

struct VMSaveTask : public VMTask
{
    VMSaveTask(Console *aConsole,
               const ComPtr<IProgress> &aServerProgress,
               const Utf8Str &aSavedStateFile)
        : VMTask(aConsole, NULL /* aProgress */, aServerProgress,
                 true /* aUsesVMPtr */),
          mSavedStateFile(aSavedStateFile)
    {}

    Utf8Str mSavedStateFile;
};

// Handler for global events
////////////////////////////////////////////////////////////////////////////////
inline static const char *networkAdapterTypeToName(NetworkAdapterType_T adapterType);

class VmEventListener {
public:
    VmEventListener()
    {}


    HRESULT init(Console *aConsole)
    {
        mConsole = aConsole;
        return S_OK;
    }

    void uninit()
    {
    }

    virtual ~VmEventListener()
    {
    }

    STDMETHOD(HandleEvent)(VBoxEventType_T aType, IEvent * aEvent)
    {
        switch(aType)
        {
            case VBoxEventType_OnNATRedirect:
            {
                Bstr id;
                ComPtr<IMachine> pMachine = mConsole->machine();
                ComPtr<INATRedirectEvent> pNREv = aEvent;
                HRESULT rc = E_FAIL;
                Assert(pNREv);

                Bstr interestedId;
                rc = pMachine->COMGETTER(Id)(interestedId.asOutParam());
                AssertComRC(rc);
                rc = pNREv->COMGETTER(MachineId)(id.asOutParam());
                AssertComRC(rc);
                if (id != interestedId)
                    break;
                /* now we can operate with redirects */
                NATProtocol_T proto;
                pNREv->COMGETTER(Proto)(&proto);
                BOOL fRemove;
                pNREv->COMGETTER(Remove)(&fRemove);
                bool fUdp = (proto == NATProtocol_UDP);
                Bstr hostIp, guestIp;
                LONG hostPort, guestPort;
                pNREv->COMGETTER(HostIp)(hostIp.asOutParam());
                pNREv->COMGETTER(HostPort)(&hostPort);
                pNREv->COMGETTER(GuestIp)(guestIp.asOutParam());
                pNREv->COMGETTER(GuestPort)(&guestPort);
                ULONG ulSlot;
                rc = pNREv->COMGETTER(Slot)(&ulSlot);
                AssertComRC(rc);
                if (FAILED(rc))
                    break;
                mConsole->onNATRedirectRuleChange(ulSlot, fRemove, proto, hostIp.raw(), hostPort, guestIp.raw(), guestPort);
            }
            break;

            case VBoxEventType_OnHostPciDevicePlug:
            {
                // handle if needed
                break;
            }

            default:
              AssertFailed();
        }
        return S_OK;
    }
private:
    Console *mConsole;
};

typedef ListenerImpl<VmEventListener, Console*> VmEventListenerImpl;


VBOX_LISTENER_DECLARE(VmEventListenerImpl)


// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

Console::Console()
    : mSavedStateDataLoaded(false)
    , mConsoleVRDPServer(NULL)
    , mpUVM(NULL)
    , mVMCallers(0)
    , mVMZeroCallersSem(NIL_RTSEMEVENT)
    , mVMDestroying(false)
    , mVMPoweredOff(false)
    , mVMIsAlreadyPoweringOff(false)
    , mfSnapshotFolderSizeWarningShown(false)
    , mfSnapshotFolderExt4WarningShown(false)
    , mfSnapshotFolderDiskTypeShown(false)
    , mpVmm2UserMethods(NULL)
    , m_pVMMDev(NULL)
    , mAudioSniffer(NULL)
#ifdef VBOX_WITH_USB_VIDEO
    , mUsbWebcamInterface(NULL)
#endif
    , mBusMgr(NULL)
    , mVMStateChangeCallbackDisabled(false)
    , mfUseHostClipboard(true)
    , mMachineState(MachineState_PoweredOff)
{
}

Console::~Console()
{}

HRESULT Console::FinalConstruct()
{
    LogFlowThisFunc(("\n"));

    memset(mapStorageLeds, 0, sizeof(mapStorageLeds));
    memset(mapNetworkLeds, 0, sizeof(mapNetworkLeds));
    memset(&mapUSBLed, 0, sizeof(mapUSBLed));
    memset(&mapSharedFolderLed, 0, sizeof(mapSharedFolderLed));

    for (unsigned i = 0; i < RT_ELEMENTS(maStorageDevType); ++i)
        maStorageDevType[i] = DeviceType_Null;

    MYVMM2USERMETHODS *pVmm2UserMethods = (MYVMM2USERMETHODS *)RTMemAllocZ(sizeof(*mpVmm2UserMethods) + sizeof(Console *));
    if (!pVmm2UserMethods)
        return E_OUTOFMEMORY;
    pVmm2UserMethods->u32Magic          = VMM2USERMETHODS_MAGIC;
    pVmm2UserMethods->u32Version        = VMM2USERMETHODS_VERSION;
    pVmm2UserMethods->pfnSaveState      = Console::vmm2User_SaveState;
    pVmm2UserMethods->pfnNotifyEmtInit  = Console::vmm2User_NotifyEmtInit;
    pVmm2UserMethods->pfnNotifyEmtTerm  = Console::vmm2User_NotifyEmtTerm;
    pVmm2UserMethods->pfnNotifyPdmtInit = Console::vmm2User_NotifyPdmtInit;
    pVmm2UserMethods->pfnNotifyPdmtTerm = Console::vmm2User_NotifyPdmtTerm;
    pVmm2UserMethods->u32EndMagic       = VMM2USERMETHODS_MAGIC;
    pVmm2UserMethods->pConsole          = this;
    mpVmm2UserMethods = pVmm2UserMethods;

    return BaseFinalConstruct();
}

void Console::FinalRelease()
{
    LogFlowThisFunc(("\n"));

    uninit();

    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

HRESULT Console::init(IMachine *aMachine, IInternalMachineControl *aControl)
{
    AssertReturn(aMachine && aControl, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aMachine=%p, aControl=%p\n", aMachine, aControl));

    HRESULT rc = E_FAIL;

    unconst(mMachine) = aMachine;
    unconst(mControl) = aControl;

    /* Cache essential properties and objects */

    rc = mMachine->COMGETTER(State)(&mMachineState);
    AssertComRCReturnRC(rc);

    rc = mMachine->COMGETTER(VRDEServer)(unconst(mVRDEServer).asOutParam());
    AssertComRCReturnRC(rc);

    /* Create associated child COM objects */

    // Event source may be needed by other children
    unconst(mEventSource).createObject();
    rc = mEventSource->init(static_cast<IConsole*>(this));
    AssertComRCReturnRC(rc);

    unconst(mGuest).createObject();
    rc = mGuest->init(this);
    AssertComRCReturnRC(rc);

    unconst(mKeyboard).createObject();
    rc = mKeyboard->init(this);
    AssertComRCReturnRC(rc);

    unconst(mMouse).createObject();
    rc = mMouse->init(this);
    AssertComRCReturnRC(rc);

    unconst(mDisplay).createObject();
    rc = mDisplay->init(this);
    AssertComRCReturnRC(rc);

    unconst(mVRDEServerInfo).createObject();
    rc = mVRDEServerInfo->init(this);
    AssertComRCReturnRC(rc);

#ifdef VBOX_WITH_EXTPACK
    unconst(mptrExtPackManager).createObject();
    rc = mptrExtPackManager->initExtPackManager(NULL, VBOXEXTPACKCTX_VM_PROCESS);
    AssertComRCReturnRC(rc);
#endif

    /* Grab global and machine shared folder lists */

    rc = fetchSharedFolders(true /* aGlobal */);
    AssertComRCReturnRC(rc);
    rc = fetchSharedFolders(false /* aGlobal */);
    AssertComRCReturnRC(rc);

    /* Create other child objects */

    unconst(mConsoleVRDPServer) = new ConsoleVRDPServer(this);
    AssertReturn(mConsoleVRDPServer, E_FAIL);

    mcAudioRefs = 0;
    mcVRDPClients = 0;
    mu32SingleRDPClientId = 0;
    mcGuestCredentialsProvided = false;

    /* Figure out size of meAttachmentType vector */
    ComPtr<IVirtualBox> pVirtualBox;
    rc = aMachine->COMGETTER(Parent)(pVirtualBox.asOutParam());
    AssertComRC(rc);
    ComPtr<ISystemProperties> pSystemProperties;
    if (pVirtualBox)
        pVirtualBox->COMGETTER(SystemProperties)(pSystemProperties.asOutParam());
    ChipsetType_T chipsetType = ChipsetType_PIIX3;
    aMachine->COMGETTER(ChipsetType)(&chipsetType);
    ULONG maxNetworkAdapters = 0;
    if (pSystemProperties)
        pSystemProperties->GetMaxNetworkAdapters(chipsetType, &maxNetworkAdapters);
    meAttachmentType.resize(maxNetworkAdapters);
    for (ULONG slot = 0; slot < maxNetworkAdapters; ++slot)
        meAttachmentType[slot] = NetworkAttachmentType_Null;


    // VirtualBox 4.0: We no longer initialize the VMMDev instance here,
    // which starts the HGCM thread. Instead, this is now done in the
    // power-up thread when a VM is actually being powered up to avoid
    // having HGCM threads all over the place every time a session is
    // opened, even if that session will not run a VM.
    //     unconst(m_pVMMDev) = new VMMDev(this);
    //     AssertReturn(mVMMDev, E_FAIL);

    unconst(mAudioSniffer) = new AudioSniffer(this);
    AssertReturn(mAudioSniffer, E_FAIL);
#ifdef VBOX_WITH_USB_VIDEO
    unconst(mUsbWebcamInterface) = new UsbWebcamInterface(this);
    AssertReturn(mUsbWebcamInterface, E_FAIL);
#endif

    /* VirtualBox events registration. */
    {
        ComPtr<IEventSource> pES;
        rc = pVirtualBox->COMGETTER(EventSource)(pES.asOutParam());
        AssertComRC(rc);
        ComObjPtr<VmEventListenerImpl> aVmListener;
        aVmListener.createObject();
        aVmListener->init(new VmEventListener(), this);
        mVmListener = aVmListener;
        com::SafeArray<VBoxEventType_T> eventTypes;
        eventTypes.push_back(VBoxEventType_OnNATRedirect);
        eventTypes.push_back(VBoxEventType_OnHostPciDevicePlug);
        rc = pES->RegisterListener(aVmListener, ComSafeArrayAsInParam(eventTypes), true);
        AssertComRC(rc);
    }


    /* Confirm a successful initialization when it's the case */
    autoInitSpan.setSucceeded();

#ifdef VBOX_WITH_EXTPACK
    /* Let the extension packs have a go at things (hold no locks). */
    if (SUCCEEDED(rc))
        mptrExtPackManager->callAllConsoleReadyHooks(this);
#endif

    LogFlowThisFuncLeave();

    return S_OK;
}

/**
 * Uninitializes the Console object.
 */
void Console::uninit()
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
    {
        LogFlowThisFunc(("Already uninitialized.\n"));
        LogFlowThisFuncLeave();
        return;
    }

    LogFlowThisFunc(("initFailed()=%d\n", autoUninitSpan.initFailed()));
    if (mVmListener)
    {
        ComPtr<IEventSource> pES;
        ComPtr<IVirtualBox> pVirtualBox;
        HRESULT rc = mMachine->COMGETTER(Parent)(pVirtualBox.asOutParam());
        AssertComRC(rc);
        if (SUCCEEDED(rc) && !pVirtualBox.isNull())
        {
            rc = pVirtualBox->COMGETTER(EventSource)(pES.asOutParam());
            AssertComRC(rc);
            if (!pES.isNull())
            {
                rc = pES->UnregisterListener(mVmListener);
                AssertComRC(rc);
            }
        }
        mVmListener.setNull();
    }

    /* power down the VM if necessary */
    if (mpUVM)
    {
        powerDown();
        Assert(mpUVM == NULL);
    }

    if (mVMZeroCallersSem != NIL_RTSEMEVENT)
    {
        RTSemEventDestroy(mVMZeroCallersSem);
        mVMZeroCallersSem = NIL_RTSEMEVENT;
    }

    if (mpVmm2UserMethods)
    {
        RTMemFree((void *)mpVmm2UserMethods);
        mpVmm2UserMethods = NULL;
    }

#ifdef VBOX_WITH_USB_VIDEO
    if (mUsbWebcamInterface)
    {
        delete mUsbWebcamInterface;
        unconst(mUsbWebcamInterface) = NULL;
    }
#endif

    if (mAudioSniffer)
    {
        delete mAudioSniffer;
        unconst(mAudioSniffer) = NULL;
    }

    // if the VM had a VMMDev with an HGCM thread, then remove that here
    if (m_pVMMDev)
    {
        delete m_pVMMDev;
        unconst(m_pVMMDev) = NULL;
    }

    if (mBusMgr)
    {
        mBusMgr->Release();
        mBusMgr = NULL;
    }

    m_mapGlobalSharedFolders.clear();
    m_mapMachineSharedFolders.clear();
    m_mapSharedFolders.clear();             // console instances

    mRemoteUSBDevices.clear();
    mUSBDevices.clear();

    if (mVRDEServerInfo)
    {
        mVRDEServerInfo->uninit();
        unconst(mVRDEServerInfo).setNull();;
    }

    if (mDebugger)
    {
        mDebugger->uninit();
        unconst(mDebugger).setNull();
    }

    if (mDisplay)
    {
        mDisplay->uninit();
        unconst(mDisplay).setNull();
    }

    if (mMouse)
    {
        mMouse->uninit();
        unconst(mMouse).setNull();
    }

    if (mKeyboard)
    {
        mKeyboard->uninit();
        unconst(mKeyboard).setNull();;
    }

    if (mGuest)
    {
        mGuest->uninit();
        unconst(mGuest).setNull();;
    }

    if (mConsoleVRDPServer)
    {
        delete mConsoleVRDPServer;
        unconst(mConsoleVRDPServer) = NULL;
    }

    unconst(mVRDEServer).setNull();

    unconst(mControl).setNull();
    unconst(mMachine).setNull();

    // we don't perform uninit() as it's possible that some pending event refers to this source
    unconst(mEventSource).setNull();

    mCallbackData.clear();

    LogFlowThisFuncLeave();
}

#ifdef VBOX_WITH_GUEST_PROPS

/**
 * Handles guest properties on a VM reset.
 *
 * We must delete properties that are flagged TRANSRESET.
 *
 * @todo r=bird: Would be more efficient if we added a request to the HGCM
 *       service to do this instead of detouring thru VBoxSVC.
 *       (IMachine::SetGuestProperty ends up in VBoxSVC, which in turns calls
 *       back into the VM process and the HGCM service.)
 */
void Console::guestPropertiesHandleVMReset(void)
{
    com::SafeArray<BSTR>   arrNames;
    com::SafeArray<BSTR>   arrValues;
    com::SafeArray<LONG64> arrTimestamps;
    com::SafeArray<BSTR>   arrFlags;
    HRESULT hrc = enumerateGuestProperties(Bstr("*").raw(),
                                           ComSafeArrayAsOutParam(arrNames),
                                           ComSafeArrayAsOutParam(arrValues),
                                           ComSafeArrayAsOutParam(arrTimestamps),
                                           ComSafeArrayAsOutParam(arrFlags));
    if (SUCCEEDED(hrc))
    {
        for (size_t i = 0; i < arrFlags.size(); i++)
        {
            /* Delete all properties which have the flag "TRANSRESET". */
            if (Utf8Str(arrFlags[i]).contains("TRANSRESET", Utf8Str::CaseInsensitive))
            {
                hrc = mMachine->SetGuestProperty(arrNames[i], Bstr("").raw() /* Value */,
                                                 Bstr("").raw() /* Flags */);
                if (FAILED(hrc))
                    LogRel(("RESET: Could not delete transient property \"%ls\", rc=%Rhrc\n",
                            arrNames[i], hrc));
            }
        }
    }
    else
        LogRel(("RESET: Unable to enumerate guest properties, rc=%Rhrc\n", hrc));
}

bool Console::guestPropertiesVRDPEnabled(void)
{
    Bstr value;
    HRESULT hrc = mMachine->GetExtraData(Bstr("VBoxInternal2/EnableGuestPropertiesVRDP").raw(),
                                         value.asOutParam());
    if (   hrc   == S_OK
        && value == "1")
        return true;
    return false;
}

void Console::guestPropertiesVRDPUpdateLogon(uint32_t u32ClientId, const char *pszUser, const char *pszDomain)
{
    if (!guestPropertiesVRDPEnabled())
        return;

    char szPropNm[256];
    Bstr bstrReadOnlyGuest(L"RDONLYGUEST");

    RTStrPrintf(szPropNm, sizeof(szPropNm), "/VirtualBox/HostInfo/VRDP/Client/%u/Name", u32ClientId);
    Bstr clientName;
    mVRDEServerInfo->COMGETTER(ClientName)(clientName.asOutParam());

    mMachine->SetGuestProperty(Bstr(szPropNm).raw(),
                               clientName.raw(),
                               bstrReadOnlyGuest.raw());

    RTStrPrintf(szPropNm, sizeof(szPropNm), "/VirtualBox/HostInfo/VRDP/Client/%u/User", u32ClientId);
    mMachine->SetGuestProperty(Bstr(szPropNm).raw(),
                               Bstr(pszUser).raw(),
                               bstrReadOnlyGuest.raw());

    RTStrPrintf(szPropNm, sizeof(szPropNm), "/VirtualBox/HostInfo/VRDP/Client/%u/Domain", u32ClientId);
    mMachine->SetGuestProperty(Bstr(szPropNm).raw(),
                               Bstr(pszDomain).raw(),
                               bstrReadOnlyGuest.raw());

    char szClientId[64];
    RTStrPrintf(szClientId, sizeof(szClientId), "%d", u32ClientId);
    mMachine->SetGuestProperty(Bstr("/VirtualBox/HostInfo/VRDP/LastConnectedClient").raw(),
                               Bstr(szClientId).raw(),
                               bstrReadOnlyGuest.raw());

    return;
}

void Console::guestPropertiesVRDPUpdateDisconnect(uint32_t u32ClientId)
{
    if (!guestPropertiesVRDPEnabled())
        return;

    Bstr bstrReadOnlyGuest(L"RDONLYGUEST");

    char szPropNm[256];
    RTStrPrintf(szPropNm, sizeof(szPropNm), "/VirtualBox/HostInfo/VRDP/Client/%u/Name", u32ClientId);
    mMachine->SetGuestProperty(Bstr(szPropNm).raw(), Bstr("").raw(),
                               bstrReadOnlyGuest.raw());

    RTStrPrintf(szPropNm, sizeof(szPropNm), "/VirtualBox/HostInfo/VRDP/Client/%u/User", u32ClientId);
    mMachine->SetGuestProperty(Bstr(szPropNm).raw(), Bstr("").raw(),
                               bstrReadOnlyGuest.raw());

    RTStrPrintf(szPropNm, sizeof(szPropNm), "/VirtualBox/HostInfo/VRDP/Client/%u/Domain", u32ClientId);
    mMachine->SetGuestProperty(Bstr(szPropNm).raw(), Bstr("").raw(),
                               bstrReadOnlyGuest.raw());

    char szClientId[64];
    RTStrPrintf(szClientId, sizeof(szClientId), "%d", u32ClientId);
    mMachine->SetGuestProperty(Bstr("/VirtualBox/HostInfo/VRDP/LastDisconnectedClient").raw(),
                               Bstr(szClientId).raw(),
                               bstrReadOnlyGuest.raw());

    return;
}

#endif /* VBOX_WITH_GUEST_PROPS */

#ifdef VBOX_WITH_EXTPACK
/**
 * Used by VRDEServer and others to talke to the extension pack manager.
 *
 * @returns The extension pack manager.
 */
ExtPackManager *Console::getExtPackManager()
{
    return mptrExtPackManager;
}
#endif


int Console::VRDPClientLogon(uint32_t u32ClientId, const char *pszUser, const char *pszPassword, const char *pszDomain)
{
    LogFlowFuncEnter();
    LogFlowFunc(("%d, %s, %s, %s\n", u32ClientId, pszUser, pszPassword, pszDomain));

    AutoCaller autoCaller(this);
    if (!autoCaller.isOk())
    {
        /* Console has been already uninitialized, deny request */
        LogRel(("AUTH: Access denied (Console uninitialized).\n"));
        LogFlowFuncLeave();
        return VERR_ACCESS_DENIED;
    }

    Bstr id;
    HRESULT hrc = mMachine->COMGETTER(Id)(id.asOutParam());
    Guid uuid = Guid(id);

    AssertComRCReturn(hrc, VERR_ACCESS_DENIED);

    AuthType_T authType = AuthType_Null;
    hrc = mVRDEServer->COMGETTER(AuthType)(&authType);
    AssertComRCReturn(hrc, VERR_ACCESS_DENIED);

    ULONG authTimeout = 0;
    hrc = mVRDEServer->COMGETTER(AuthTimeout)(&authTimeout);
    AssertComRCReturn(hrc, VERR_ACCESS_DENIED);

    AuthResult result = AuthResultAccessDenied;
    AuthGuestJudgement guestJudgement = AuthGuestNotAsked;

    LogFlowFunc(("Auth type %d\n", authType));

    LogRel(("AUTH: User: [%s]. Domain: [%s]. Authentication type: [%s]\n",
                pszUser, pszDomain,
                authType == AuthType_Null?
                    "Null":
                    (authType == AuthType_External?
                        "External":
                        (authType == AuthType_Guest?
                            "Guest":
                            "INVALID"
                        )
                    )
            ));

    switch (authType)
    {
        case AuthType_Null:
        {
            result = AuthResultAccessGranted;
            break;
        }

        case AuthType_External:
        {
            /* Call the external library. */
            result = mConsoleVRDPServer->Authenticate(uuid, guestJudgement, pszUser, pszPassword, pszDomain, u32ClientId);

            if (result != AuthResultDelegateToGuest)
            {
                break;
            }

            LogRel(("AUTH: Delegated to guest.\n"));

            LogFlowFunc(("External auth asked for guest judgement\n"));
        } /* pass through */

        case AuthType_Guest:
        {
            guestJudgement = AuthGuestNotReacted;

            // @todo r=dj locking required here for m_pVMMDev?
            PPDMIVMMDEVPORT pDevPort;
            if (    (m_pVMMDev)
                 && ((pDevPort = m_pVMMDev->getVMMDevPort()))
               )
            {
                /* Issue the request to guest. Assume that the call does not require EMT. It should not. */

                /* Ask the guest to judge these credentials. */
                uint32_t u32GuestFlags = VMMDEV_SETCREDENTIALS_JUDGE;

                int rc = pDevPort->pfnSetCredentials(pDevPort, pszUser, pszPassword, pszDomain, u32GuestFlags);

                if (RT_SUCCESS(rc))
                {
                    /* Wait for guest. */
                    rc = m_pVMMDev->WaitCredentialsJudgement(authTimeout, &u32GuestFlags);

                    if (RT_SUCCESS(rc))
                    {
                        switch (u32GuestFlags & (VMMDEV_CREDENTIALS_JUDGE_OK | VMMDEV_CREDENTIALS_JUDGE_DENY | VMMDEV_CREDENTIALS_JUDGE_NOJUDGEMENT))
                        {
                            case VMMDEV_CREDENTIALS_JUDGE_DENY:        guestJudgement = AuthGuestAccessDenied;  break;
                            case VMMDEV_CREDENTIALS_JUDGE_NOJUDGEMENT: guestJudgement = AuthGuestNoJudgement;   break;
                            case VMMDEV_CREDENTIALS_JUDGE_OK:          guestJudgement = AuthGuestAccessGranted; break;
                            default:
                                LogFlowFunc(("Invalid guest flags %08X!!!\n", u32GuestFlags)); break;
                        }
                    }
                    else
                    {
                        LogFlowFunc(("Wait for credentials judgement rc = %Rrc!!!\n", rc));
                    }

                    LogFlowFunc(("Guest judgement %d\n", guestJudgement));
                }
                else
                {
                    LogFlowFunc(("Could not set credentials rc = %Rrc!!!\n", rc));
                }
            }

            if (authType == AuthType_External)
            {
                LogRel(("AUTH: Guest judgement %d.\n", guestJudgement));
                LogFlowFunc(("External auth called again with guest judgement = %d\n", guestJudgement));
                result = mConsoleVRDPServer->Authenticate(uuid, guestJudgement, pszUser, pszPassword, pszDomain, u32ClientId);
            }
            else
            {
                switch (guestJudgement)
                {
                    case AuthGuestAccessGranted:
                        result = AuthResultAccessGranted;
                        break;
                    default:
                        result = AuthResultAccessDenied;
                        break;
                }
            }
        } break;

        default:
            AssertFailed();
    }

    LogFlowFunc(("Result = %d\n", result));
    LogFlowFuncLeave();

    if (result != AuthResultAccessGranted)
    {
        /* Reject. */
        LogRel(("AUTH: Access denied.\n"));
        return VERR_ACCESS_DENIED;
    }

    LogRel(("AUTH: Access granted.\n"));

    /* Multiconnection check must be made after authentication, so bad clients would not interfere with a good one. */
    BOOL allowMultiConnection = FALSE;
    hrc = mVRDEServer->COMGETTER(AllowMultiConnection)(&allowMultiConnection);
    AssertComRCReturn(hrc, VERR_ACCESS_DENIED);

    BOOL reuseSingleConnection = FALSE;
    hrc = mVRDEServer->COMGETTER(ReuseSingleConnection)(&reuseSingleConnection);
    AssertComRCReturn(hrc, VERR_ACCESS_DENIED);

    LogFlowFunc(("allowMultiConnection %d, reuseSingleConnection = %d, mcVRDPClients = %d, mu32SingleRDPClientId = %d\n", allowMultiConnection, reuseSingleConnection, mcVRDPClients, mu32SingleRDPClientId));

    if (allowMultiConnection == FALSE)
    {
        /* Note: the 'mcVRDPClients' variable is incremented in ClientConnect callback, which is called when the client
         * is successfully connected, that is after the ClientLogon callback. Therefore the mcVRDPClients
         * value is 0 for first client.
         */
        if (mcVRDPClients != 0)
        {
            Assert(mcVRDPClients == 1);
            /* There is a client already.
             * If required drop the existing client connection and let the connecting one in.
             */
            if (reuseSingleConnection)
            {
                LogRel(("AUTH: Multiple connections are not enabled. Disconnecting existing client.\n"));
                mConsoleVRDPServer->DisconnectClient(mu32SingleRDPClientId, false);
            }
            else
            {
                /* Reject. */
                LogRel(("AUTH: Multiple connections are not enabled. Access denied.\n"));
                return VERR_ACCESS_DENIED;
            }
        }

        /* Save the connected client id. From now on it will be necessary to disconnect this one. */
        mu32SingleRDPClientId = u32ClientId;
    }

#ifdef VBOX_WITH_GUEST_PROPS
    guestPropertiesVRDPUpdateLogon(u32ClientId, pszUser, pszDomain);
#endif /* VBOX_WITH_GUEST_PROPS */

    /* Check if the successfully verified credentials are to be sent to the guest. */
    BOOL fProvideGuestCredentials = FALSE;

    Bstr value;
    hrc = mMachine->GetExtraData(Bstr("VRDP/ProvideGuestCredentials").raw(),
                                 value.asOutParam());
    if (SUCCEEDED(hrc) && value == "1")
    {
        /* Provide credentials only if there are no logged in users. */
        Bstr noLoggedInUsersValue;
        LONG64 ul64Timestamp = 0;
        Bstr flags;

        hrc = getGuestProperty(Bstr("/VirtualBox/GuestInfo/OS/NoLoggedInUsers").raw(),
                               noLoggedInUsersValue.asOutParam(), &ul64Timestamp, flags.asOutParam());

        if (SUCCEEDED(hrc) && noLoggedInUsersValue != Bstr("false"))
        {
            /* And only if there are no connected clients. */
            if (ASMAtomicCmpXchgBool(&mcGuestCredentialsProvided, true, false))
            {
                fProvideGuestCredentials = TRUE;
            }
        }
    }

    // @todo r=dj locking required here for m_pVMMDev?
    if (   fProvideGuestCredentials
        && m_pVMMDev)
    {
        uint32_t u32GuestFlags = VMMDEV_SETCREDENTIALS_GUESTLOGON;

        int rc = m_pVMMDev->getVMMDevPort()->pfnSetCredentials(m_pVMMDev->getVMMDevPort(),
                     pszUser, pszPassword, pszDomain, u32GuestFlags);
        AssertRC(rc);
    }

    return VINF_SUCCESS;
}

void Console::VRDPClientConnect(uint32_t u32ClientId)
{
    LogFlowFuncEnter();

    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    uint32_t u32Clients = ASMAtomicIncU32(&mcVRDPClients);
    VMMDev *pDev;
    PPDMIVMMDEVPORT pPort;
    if (    (u32Clients == 1)
         && ((pDev = getVMMDev()))
         && ((pPort = pDev->getVMMDevPort()))
       )
    {
        pPort->pfnVRDPChange(pPort,
                             true,
                             VRDP_EXPERIENCE_LEVEL_FULL); // @todo configurable
    }

    NOREF(u32ClientId);
    mDisplay->VideoAccelVRDP(true);

    LogFlowFuncLeave();
    return;
}

void Console::VRDPClientDisconnect(uint32_t u32ClientId,
                                   uint32_t fu32Intercepted)
{
    LogFlowFuncEnter();

    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AssertReturnVoid(mConsoleVRDPServer);

    uint32_t u32Clients = ASMAtomicDecU32(&mcVRDPClients);
    VMMDev *pDev;
    PPDMIVMMDEVPORT pPort;

    if (    (u32Clients == 0)
         && ((pDev = getVMMDev()))
         && ((pPort = pDev->getVMMDevPort()))
       )
    {
        pPort->pfnVRDPChange(pPort,
                             false,
                             0);
    }

    mDisplay->VideoAccelVRDP(false);

    if (fu32Intercepted & VRDE_CLIENT_INTERCEPT_USB)
    {
        mConsoleVRDPServer->USBBackendDelete(u32ClientId);
    }

    if (fu32Intercepted & VRDE_CLIENT_INTERCEPT_CLIPBOARD)
    {
        mConsoleVRDPServer->ClipboardDelete(u32ClientId);
    }

    if (fu32Intercepted & VRDE_CLIENT_INTERCEPT_AUDIO)
    {
        mcAudioRefs--;

        if (mcAudioRefs <= 0)
        {
            if (mAudioSniffer)
            {
                PPDMIAUDIOSNIFFERPORT port = mAudioSniffer->getAudioSnifferPort();
                if (port)
                {
                    port->pfnSetup(port, false, false);
                }
            }
        }
    }

    Bstr uuid;
    HRESULT hrc = mMachine->COMGETTER(Id)(uuid.asOutParam());
    AssertComRC(hrc);

    AuthType_T authType = AuthType_Null;
    hrc = mVRDEServer->COMGETTER(AuthType)(&authType);
    AssertComRC(hrc);

    if (authType == AuthType_External)
        mConsoleVRDPServer->AuthDisconnect(uuid, u32ClientId);

#ifdef VBOX_WITH_GUEST_PROPS
    guestPropertiesVRDPUpdateDisconnect(u32ClientId);
#endif /* VBOX_WITH_GUEST_PROPS */

    if (u32Clients == 0)
        mcGuestCredentialsProvided = false;

    LogFlowFuncLeave();
    return;
}

void Console::VRDPInterceptAudio(uint32_t u32ClientId)
{
    LogFlowFuncEnter();

    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    LogFlowFunc(("mAudioSniffer %p, u32ClientId %d.\n",
                 mAudioSniffer, u32ClientId));
    NOREF(u32ClientId);

    ++mcAudioRefs;

    if (mcAudioRefs == 1)
    {
        if (mAudioSniffer)
        {
            PPDMIAUDIOSNIFFERPORT port = mAudioSniffer->getAudioSnifferPort();
            if (port)
            {
                port->pfnSetup(port, true, true);
            }
        }
    }

    LogFlowFuncLeave();
    return;
}

void Console::VRDPInterceptUSB(uint32_t u32ClientId, void **ppvIntercept)
{
    LogFlowFuncEnter();

    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AssertReturnVoid(mConsoleVRDPServer);

    mConsoleVRDPServer->USBBackendCreate(u32ClientId, ppvIntercept);

    LogFlowFuncLeave();
    return;
}

void Console::VRDPInterceptClipboard(uint32_t u32ClientId)
{
    LogFlowFuncEnter();

    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AssertReturnVoid(mConsoleVRDPServer);

    mConsoleVRDPServer->ClipboardCreate(u32ClientId);

    LogFlowFuncLeave();
    return;
}


//static
const char *Console::sSSMConsoleUnit = "ConsoleData";
//static
uint32_t Console::sSSMConsoleVer = 0x00010001;

inline static const char *networkAdapterTypeToName(NetworkAdapterType_T adapterType)
{
    switch (adapterType)
    {
        case NetworkAdapterType_Am79C970A:
        case NetworkAdapterType_Am79C973:
            return "pcnet";
#ifdef VBOX_WITH_E1000
        case NetworkAdapterType_I82540EM:
        case NetworkAdapterType_I82543GC:
        case NetworkAdapterType_I82545EM:
            return "e1000";
#endif
#ifdef VBOX_WITH_VIRTIO
        case NetworkAdapterType_Virtio:
            return "virtio-net";
#endif
        default:
            AssertFailed();
            return "unknown";
    }
    return NULL;
}

/**
 * Loads various console data stored in the saved state file.
 * This method does validation of the state file and returns an error info
 * when appropriate.
 *
 * The method does nothing if the machine is not in the Saved file or if
 * console data from it has already been loaded.
 *
 * @note The caller must lock this object for writing.
 */
HRESULT Console::loadDataFromSavedState()
{
    if (mMachineState != MachineState_Saved || mSavedStateDataLoaded)
        return S_OK;

    Bstr savedStateFile;
    HRESULT rc = mMachine->COMGETTER(StateFilePath)(savedStateFile.asOutParam());
    if (FAILED(rc))
        return rc;

    PSSMHANDLE ssm;
    int vrc = SSMR3Open(Utf8Str(savedStateFile).c_str(), 0, &ssm);
    if (RT_SUCCESS(vrc))
    {
        uint32_t version = 0;
        vrc = SSMR3Seek(ssm, sSSMConsoleUnit, 0 /* iInstance */, &version);
        if (SSM_VERSION_MAJOR(version) == SSM_VERSION_MAJOR(sSSMConsoleVer))
        {
            if (RT_SUCCESS(vrc))
                vrc = loadStateFileExecInternal(ssm, version);
            else if (vrc == VERR_SSM_UNIT_NOT_FOUND)
                vrc = VINF_SUCCESS;
        }
        else
            vrc = VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

        SSMR3Close(ssm);
    }

    if (RT_FAILURE(vrc))
        rc = setError(VBOX_E_FILE_ERROR,
                      tr("The saved state file '%ls' is invalid (%Rrc). Delete the saved state and try again"),
                      savedStateFile.raw(), vrc);

    mSavedStateDataLoaded = true;

    return rc;
}

/**
 * Callback handler to save various console data to the state file,
 * called when the user saves the VM state.
 *
 * @param pvUser       pointer to Console
 *
 * @note Locks the Console object for reading.
 */
//static
DECLCALLBACK(void)
Console::saveStateFileExec(PSSMHANDLE pSSM, void *pvUser)
{
    LogFlowFunc(("\n"));

    Console *that = static_cast<Console *>(pvUser);
    AssertReturnVoid(that);

    AutoCaller autoCaller(that);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoReadLock alock(that COMMA_LOCKVAL_SRC_POS);

    int vrc = SSMR3PutU32(pSSM, (uint32_t)that->m_mapSharedFolders.size());
    AssertRC(vrc);

    for (SharedFolderMap::const_iterator it = that->m_mapSharedFolders.begin();
         it != that->m_mapSharedFolders.end();
         ++it)
    {
        SharedFolder *pSF = (*it).second;
        AutoCaller sfCaller(pSF);
        AutoReadLock sfLock(pSF COMMA_LOCKVAL_SRC_POS);

        Utf8Str name = pSF->getName();
        vrc = SSMR3PutU32(pSSM, (uint32_t)name.length() + 1 /* term. 0 */);
        AssertRC(vrc);
        vrc = SSMR3PutStrZ(pSSM, name.c_str());
        AssertRC(vrc);

        Utf8Str hostPath = pSF->getHostPath();
        vrc = SSMR3PutU32(pSSM, (uint32_t)hostPath.length() + 1 /* term. 0 */);
        AssertRC(vrc);
        vrc = SSMR3PutStrZ(pSSM, hostPath.c_str());
        AssertRC(vrc);

        vrc = SSMR3PutBool(pSSM, !!pSF->isWritable());
        AssertRC(vrc);

        vrc = SSMR3PutBool(pSSM, !!pSF->isAutoMounted());
        AssertRC(vrc);
    }

    return;
}

/**
 * Callback handler to load various console data from the state file.
 * Called when the VM is being restored from the saved state.
 *
 * @param pvUser       pointer to Console
 * @param uVersion     Console unit version.
 *                     Should match sSSMConsoleVer.
 * @param uPass        The data pass.
 *
 * @note Should locks the Console object for writing, if necessary.
 */
//static
DECLCALLBACK(int)
Console::loadStateFileExec(PSSMHANDLE pSSM, void *pvUser, uint32_t uVersion, uint32_t uPass)
{
    LogFlowFunc(("\n"));

    if (SSM_VERSION_MAJOR_CHANGED(uVersion, sSSMConsoleVer))
        return VERR_VERSION_MISMATCH;
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    Console *that = static_cast<Console *>(pvUser);
    AssertReturn(that, VERR_INVALID_PARAMETER);

    /* Currently, nothing to do when we've been called from VMR3Load*. */
    return SSMR3SkipToEndOfUnit(pSSM);
}

/**
 * Method to load various console data from the state file.
 * Called from #loadDataFromSavedState.
 *
 * @param pvUser       pointer to Console
 * @param u32Version   Console unit version.
 *                     Should match sSSMConsoleVer.
 *
 * @note Locks the Console object for writing.
 */
int
Console::loadStateFileExecInternal(PSSMHANDLE pSSM, uint32_t u32Version)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), VERR_ACCESS_DENIED);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(m_mapSharedFolders.size() == 0, VERR_INTERNAL_ERROR);

    uint32_t size = 0;
    int vrc = SSMR3GetU32(pSSM, &size);
    AssertRCReturn(vrc, vrc);

    for (uint32_t i = 0; i < size; ++i)
    {
        Utf8Str strName;
        Utf8Str strHostPath;
        bool writable = true;
        bool autoMount = false;

        uint32_t szBuf = 0;
        char *buf = NULL;

        vrc = SSMR3GetU32(pSSM, &szBuf);
        AssertRCReturn(vrc, vrc);
        buf = new char[szBuf];
        vrc = SSMR3GetStrZ(pSSM, buf, szBuf);
        AssertRC(vrc);
        strName = buf;
        delete[] buf;

        vrc = SSMR3GetU32(pSSM, &szBuf);
        AssertRCReturn(vrc, vrc);
        buf = new char[szBuf];
        vrc = SSMR3GetStrZ(pSSM, buf, szBuf);
        AssertRC(vrc);
        strHostPath = buf;
        delete[] buf;

        if (u32Version > 0x00010000)
            SSMR3GetBool(pSSM, &writable);

        if (u32Version > 0x00010000) // ???
            SSMR3GetBool(pSSM, &autoMount);

        ComObjPtr<SharedFolder> pSharedFolder;
        pSharedFolder.createObject();
        HRESULT rc = pSharedFolder->init(this,
                                         strName,
                                         strHostPath,
                                         writable,
                                         autoMount,
                                         false /* fFailOnError */);
        AssertComRCReturn(rc, VERR_INTERNAL_ERROR);

        m_mapSharedFolders.insert(std::make_pair(strName, pSharedFolder));
    }

    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_GUEST_PROPS

// static
DECLCALLBACK(int) Console::doGuestPropNotification(void *pvExtension,
                                                   uint32_t u32Function,
                                                   void *pvParms,
                                                   uint32_t cbParms)
{
    using namespace guestProp;

    Assert(u32Function == 0); NOREF(u32Function);

    /*
     * No locking, as this is purely a notification which does not make any
     * changes to the object state.
     */
    PHOSTCALLBACKDATA   pCBData = reinterpret_cast<PHOSTCALLBACKDATA>(pvParms);
    AssertReturn(sizeof(HOSTCALLBACKDATA) == cbParms, VERR_INVALID_PARAMETER);
    AssertReturn(HOSTCALLBACKMAGIC == pCBData->u32Magic, VERR_INVALID_PARAMETER);
    Log5(("Console::doGuestPropNotification: pCBData={.pcszName=%s, .pcszValue=%s, .pcszFlags=%s}\n",
          pCBData->pcszName, pCBData->pcszValue, pCBData->pcszFlags));

    int  rc;
    Bstr name(pCBData->pcszName);
    Bstr value(pCBData->pcszValue);
    Bstr flags(pCBData->pcszFlags);
    ComObjPtr<Console> pConsole = reinterpret_cast<Console *>(pvExtension);
    HRESULT hrc = pConsole->mControl->PushGuestProperty(name.raw(),
                                                        value.raw(),
                                                        pCBData->u64Timestamp,
                                                        flags.raw());
    if (SUCCEEDED(hrc))
        rc = VINF_SUCCESS;
    else
    {
        LogFunc(("Console::doGuestPropNotification: hrc=%Rhrc pCBData={.pcszName=%s, .pcszValue=%s, .pcszFlags=%s}\n",
                 hrc, pCBData->pcszName, pCBData->pcszValue, pCBData->pcszFlags));
        rc = Global::vboxStatusCodeFromCOM(hrc);
    }
    return rc;
}

HRESULT Console::doEnumerateGuestProperties(CBSTR aPatterns,
                                            ComSafeArrayOut(BSTR, aNames),
                                            ComSafeArrayOut(BSTR, aValues),
                                            ComSafeArrayOut(LONG64, aTimestamps),
                                            ComSafeArrayOut(BSTR, aFlags))
{
    AssertReturn(m_pVMMDev, E_FAIL);

    using namespace guestProp;

    VBOXHGCMSVCPARM parm[3];

    Utf8Str utf8Patterns(aPatterns);
    parm[0].type = VBOX_HGCM_SVC_PARM_PTR;
    parm[0].u.pointer.addr = (void*)utf8Patterns.c_str();
    parm[0].u.pointer.size = (uint32_t)utf8Patterns.length() + 1;

    /*
     * Now things get slightly complicated. Due to a race with the guest adding
     * properties, there is no good way to know how much to enlarge a buffer for
     * the service to enumerate into. We choose a decent starting size and loop a
     * few times, each time retrying with the size suggested by the service plus
     * one Kb.
     */
    size_t cchBuf = 4096;
    Utf8Str Utf8Buf;
    int vrc = VERR_BUFFER_OVERFLOW;
    for (unsigned i = 0; i < 10 && (VERR_BUFFER_OVERFLOW == vrc); ++i)
    {
        try
        {
            Utf8Buf.reserve(cchBuf + 1024);
        }
        catch(...)
        {
            return E_OUTOFMEMORY;
        }
        parm[1].type = VBOX_HGCM_SVC_PARM_PTR;
        parm[1].u.pointer.addr = Utf8Buf.mutableRaw();
        parm[1].u.pointer.size = (uint32_t)cchBuf + 1024;
        vrc = m_pVMMDev->hgcmHostCall("VBoxGuestPropSvc", ENUM_PROPS_HOST, 3,
                                      &parm[0]);
        Utf8Buf.jolt();
        if (parm[2].type != VBOX_HGCM_SVC_PARM_32BIT)
            return setError(E_FAIL, tr("Internal application error"));
        cchBuf = parm[2].u.uint32;
    }
    if (VERR_BUFFER_OVERFLOW == vrc)
        return setError(E_UNEXPECTED,
                        tr("Temporary failure due to guest activity, please retry"));

    /*
     * Finally we have to unpack the data returned by the service into the safe
     * arrays supplied by the caller. We start by counting the number of entries.
     */
    const char *pszBuf
        = reinterpret_cast<const char *>(parm[1].u.pointer.addr);
    unsigned cEntries = 0;
    /* The list is terminated by a zero-length string at the end of a set
     * of four strings. */
    for (size_t i = 0; strlen(pszBuf + i) != 0; )
    {
       /* We are counting sets of four strings. */
       for (unsigned j = 0; j < 4; ++j)
           i += strlen(pszBuf + i) + 1;
       ++cEntries;
    }

    /*
     * And now we create the COM safe arrays and fill them in.
     */
    com::SafeArray<BSTR> names(cEntries);
    com::SafeArray<BSTR> values(cEntries);
    com::SafeArray<LONG64> timestamps(cEntries);
    com::SafeArray<BSTR> flags(cEntries);
    size_t iBuf = 0;
    /* Rely on the service to have formated the data correctly. */
    for (unsigned i = 0; i < cEntries; ++i)
    {
        size_t cchName = strlen(pszBuf + iBuf);
        Bstr(pszBuf + iBuf).detachTo(&names[i]);
        iBuf += cchName + 1;
        size_t cchValue = strlen(pszBuf + iBuf);
        Bstr(pszBuf + iBuf).detachTo(&values[i]);
        iBuf += cchValue + 1;
        size_t cchTimestamp = strlen(pszBuf + iBuf);
        timestamps[i] = RTStrToUInt64(pszBuf + iBuf);
        iBuf += cchTimestamp + 1;
        size_t cchFlags = strlen(pszBuf + iBuf);
        Bstr(pszBuf + iBuf).detachTo(&flags[i]);
        iBuf += cchFlags + 1;
    }
    names.detachTo(ComSafeArrayOutArg(aNames));
    values.detachTo(ComSafeArrayOutArg(aValues));
    timestamps.detachTo(ComSafeArrayOutArg(aTimestamps));
    flags.detachTo(ComSafeArrayOutArg(aFlags));
    return S_OK;
}

#endif /* VBOX_WITH_GUEST_PROPS */


// IConsole properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Console::COMGETTER(Machine)(IMachine **aMachine)
{
    CheckComArgOutPointerValid(aMachine);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mMachine is constant during life time, no need to lock */
    mMachine.queryInterfaceTo(aMachine);

    /* callers expect to get a valid reference, better fail than crash them */
    if (mMachine.isNull())
        return E_FAIL;

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(State)(MachineState_T *aMachineState)
{
    CheckComArgOutPointerValid(aMachineState);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* we return our local state (since it's always the same as on the server) */
    *aMachineState = mMachineState;

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(Guest)(IGuest **aGuest)
{
    CheckComArgOutPointerValid(aGuest);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mGuest is constant during life time, no need to lock */
    mGuest.queryInterfaceTo(aGuest);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(Keyboard)(IKeyboard **aKeyboard)
{
    CheckComArgOutPointerValid(aKeyboard);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mKeyboard is constant during life time, no need to lock */
    mKeyboard.queryInterfaceTo(aKeyboard);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(Mouse)(IMouse **aMouse)
{
    CheckComArgOutPointerValid(aMouse);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mMouse is constant during life time, no need to lock */
    mMouse.queryInterfaceTo(aMouse);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(Display)(IDisplay **aDisplay)
{
    CheckComArgOutPointerValid(aDisplay);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mDisplay is constant during life time, no need to lock */
    mDisplay.queryInterfaceTo(aDisplay);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(Debugger)(IMachineDebugger **aDebugger)
{
    CheckComArgOutPointerValid(aDebugger);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* we need a write lock because of the lazy mDebugger initialization*/
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* check if we have to create the debugger object */
    if (!mDebugger)
    {
        unconst(mDebugger).createObject();
        mDebugger->init(this);
    }

    mDebugger.queryInterfaceTo(aDebugger);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(USBDevices)(ComSafeArrayOut(IUSBDevice *, aUSBDevices))
{
    CheckComArgOutSafeArrayPointerValid(aUSBDevices);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    SafeIfaceArray<IUSBDevice> collection(mUSBDevices);
    collection.detachTo(ComSafeArrayOutArg(aUSBDevices));

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(RemoteUSBDevices)(ComSafeArrayOut(IHostUSBDevice *, aRemoteUSBDevices))
{
    CheckComArgOutSafeArrayPointerValid(aRemoteUSBDevices);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    SafeIfaceArray<IHostUSBDevice> collection(mRemoteUSBDevices);
    collection.detachTo(ComSafeArrayOutArg(aRemoteUSBDevices));

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(VRDEServerInfo)(IVRDEServerInfo **aVRDEServerInfo)
{
    CheckComArgOutPointerValid(aVRDEServerInfo);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mDisplay is constant during life time, no need to lock */
    mVRDEServerInfo.queryInterfaceTo(aVRDEServerInfo);

    return S_OK;
}

STDMETHODIMP
Console::COMGETTER(SharedFolders)(ComSafeArrayOut(ISharedFolder *, aSharedFolders))
{
    CheckComArgOutSafeArrayPointerValid(aSharedFolders);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* loadDataFromSavedState() needs a write lock */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Read console data stored in the saved state file (if not yet done) */
    HRESULT rc = loadDataFromSavedState();
    if (FAILED(rc)) return rc;

    SafeIfaceArray<ISharedFolder> sf(m_mapSharedFolders);
    sf.detachTo(ComSafeArrayOutArg(aSharedFolders));

    return S_OK;
}


STDMETHODIMP Console::COMGETTER(EventSource)(IEventSource ** aEventSource)
{
    CheckComArgOutPointerValid(aEventSource);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    // no need to lock - lifetime constant
    mEventSource.queryInterfaceTo(aEventSource);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(AttachedPciDevices)(ComSafeArrayOut(IPciDeviceAttachment *, aAttachments))
{
    CheckComArgOutSafeArrayPointerValid(aAttachments);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mBusMgr)
        mBusMgr->listAttachedPciDevices(ComSafeArrayOutArg(aAttachments));
    else
    {
        com::SafeIfaceArray<IPciDeviceAttachment> result((size_t)0);
        result.detachTo(ComSafeArrayOutArg(aAttachments));
    }

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(UseHostClipboard)(BOOL *aUseHostClipboard)
{
    CheckComArgOutPointerValid(aUseHostClipboard);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aUseHostClipboard = mfUseHostClipboard;

    return S_OK;
}

STDMETHODIMP Console::COMSETTER(UseHostClipboard)(BOOL aUseHostClipboard)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mfUseHostClipboard = aUseHostClipboard;

    return S_OK;
}

// IConsole methods
/////////////////////////////////////////////////////////////////////////////


STDMETHODIMP Console::PowerUp(IProgress **aProgress)
{
    return powerUp(aProgress, false /* aPaused */);
}

STDMETHODIMP Console::PowerUpPaused(IProgress **aProgress)
{
    return powerUp(aProgress, true /* aPaused */);
}

STDMETHODIMP Console::PowerDown(IProgress **aProgress)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("mMachineState=%d\n", mMachineState));

    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    switch (mMachineState)
    {
        case MachineState_Running:
        case MachineState_Paused:
        case MachineState_Stuck:
            break;

        /* Try cancel the teleportation. */
        case MachineState_Teleporting:
        case MachineState_TeleportingPausedVM:
            if (!mptrCancelableProgress.isNull())
            {
                HRESULT hrc = mptrCancelableProgress->Cancel();
                if (SUCCEEDED(hrc))
                    break;
            }
            return setError(VBOX_E_INVALID_VM_STATE, tr("Cannot power down at this point in a teleportation"));

        /* Try cancel the live snapshot. */
        case MachineState_LiveSnapshotting:
            if (!mptrCancelableProgress.isNull())
            {
                HRESULT hrc = mptrCancelableProgress->Cancel();
                if (SUCCEEDED(hrc))
                    break;
            }
            return setError(VBOX_E_INVALID_VM_STATE, tr("Cannot power down at this point in a live snapshot"));

        /* Try cancel the FT sync. */
        case MachineState_FaultTolerantSyncing:
            if (!mptrCancelableProgress.isNull())
            {
                HRESULT hrc = mptrCancelableProgress->Cancel();
                if (SUCCEEDED(hrc))
                    break;
            }
            return setError(VBOX_E_INVALID_VM_STATE, tr("Cannot power down at this point in a fault tolerant sync"));

        /* extra nice error message for a common case */
        case MachineState_Saved:
            return setError(VBOX_E_INVALID_VM_STATE, tr("Cannot power down a saved virtual machine"));
        case MachineState_Stopping:
            return setError(VBOX_E_INVALID_VM_STATE, tr("The virtual machine is being powered down"));
        default:
            return setError(VBOX_E_INVALID_VM_STATE,
                            tr("Invalid machine state: %s (must be Running, Paused or Stuck)"),
                            Global::stringifyMachineState(mMachineState));
    }

    LogFlowThisFunc(("Initiating SHUTDOWN request...\n"));

    /* memorize the current machine state */
    MachineState_T lastMachineState = mMachineState;

    HRESULT rc = S_OK;
    bool fBeganPowerDown = false;

    do
    {
        ComPtr<IProgress> pProgress;

        /*
         * request a progress object from the server
         * (this will set the machine state to Stopping on the server to block
         * others from accessing this machine)
         */
        rc = mControl->BeginPoweringDown(pProgress.asOutParam());
        if (FAILED(rc))
            break;

        fBeganPowerDown = true;

        /* sync the state with the server */
        setMachineStateLocally(MachineState_Stopping);

        /* setup task object and thread to carry out the operation asynchronously */
        std::auto_ptr<VMPowerDownTask> task(new VMPowerDownTask(this, pProgress));
        AssertBreakStmt(task->isOk(), rc = E_FAIL);

        int vrc = RTThreadCreate(NULL, Console::powerDownThread,
                                 (void *) task.get(), 0,
                                 RTTHREADTYPE_MAIN_WORKER, 0,
                                 "VMPowerDown");
        if (RT_FAILURE(vrc))
        {
            rc = setError(E_FAIL, "Could not create VMPowerDown thread (%Rrc)", vrc);
            break;
        }

        /* task is now owned by powerDownThread(), so release it */
        task.release();

        /* pass the progress to the caller */
        pProgress.queryInterfaceTo(aProgress);
    }
    while (0);

    if (FAILED(rc))
    {
        /* preserve existing error info */
        ErrorInfoKeeper eik;

        if (fBeganPowerDown)
        {
            /*
             * cancel the requested power down procedure.
             * This will reset the machine state to the state it had right
             * before calling mControl->BeginPoweringDown().
             */
            mControl->EndPoweringDown(eik.getResultCode(), eik.getText().raw());        }

        setMachineStateLocally(lastMachineState);
    }

    LogFlowThisFunc(("rc=%Rhrc\n", rc));
    LogFlowThisFuncLeave();

    return rc;
}

STDMETHODIMP Console::Reset()
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("mMachineState=%d\n", mMachineState));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (   mMachineState != MachineState_Running
        && mMachineState != MachineState_Teleporting
        && mMachineState != MachineState_LiveSnapshotting
        /** @todo r=bird: This should be allowed on paused VMs as well. Later.  */
       )
        return setInvalidMachineStateError();

    /* protect mpUVM */
    SafeVMPtr ptrVM(this);
    if (!ptrVM.isOk())
        return ptrVM.rc();

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

    int vrc = VMR3Reset(ptrVM);

    HRESULT rc = RT_SUCCESS(vrc) ? S_OK :
        setError(VBOX_E_VM_ERROR,
                 tr("Could not reset the machine (%Rrc)"),
                 vrc);

    LogFlowThisFunc(("mMachineState=%d, rc=%Rhrc\n", mMachineState, rc));
    LogFlowThisFuncLeave();
    return rc;
}

/*static*/ DECLCALLBACK(int) Console::unplugCpu(Console *pThis, PVM pVM, unsigned uCpu)
{
    LogFlowFunc(("pThis=%p pVM=%p uCpu=%u\n", pThis, pVM, uCpu));

    AssertReturn(pThis, VERR_INVALID_PARAMETER);

    int vrc = PDMR3DeviceDetach(pVM, "acpi", 0, uCpu, 0);
    Log(("UnplugCpu: rc=%Rrc\n", vrc));

    return vrc;
}

HRESULT Console::doCPURemove(ULONG aCpu, PVM pVM)
{
    HRESULT rc = S_OK;

    LogFlowThisFuncEnter();
    LogFlowThisFunc(("mMachineState=%d\n", mMachineState));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(m_pVMMDev, E_FAIL);
    PPDMIVMMDEVPORT pVmmDevPort = m_pVMMDev->getVMMDevPort();
    AssertReturn(pVmmDevPort, E_FAIL);

    if (   mMachineState != MachineState_Running
        && mMachineState != MachineState_Teleporting
        && mMachineState != MachineState_LiveSnapshotting
       )
        return setInvalidMachineStateError();

    /* Check if the CPU is present */
    BOOL fCpuAttached;
    rc = mMachine->GetCPUStatus(aCpu, &fCpuAttached);
    if (FAILED(rc))
        return rc;
    if (!fCpuAttached)
        return setError(E_FAIL, tr("CPU %d is not attached"), aCpu);

    /* Leave the lock before any EMT/VMMDev call. */
    alock.release();
    bool fLocked = true;

    /* Check if the CPU is unlocked */
    PPDMIBASE pBase;
    int vrc = PDMR3QueryDeviceLun(pVM, "acpi", 0, aCpu, &pBase);
    if (RT_SUCCESS(vrc))
    {
        Assert(pBase);
        PPDMIACPIPORT pApicPort = PDMIBASE_QUERY_INTERFACE(pBase, PDMIACPIPORT);

        /* Notify the guest if possible. */
        uint32_t idCpuCore, idCpuPackage;
        vrc = VMR3GetCpuCoreAndPackageIdFromCpuId(pVM, aCpu, &idCpuCore, &idCpuPackage); AssertRC(vrc);
        if (RT_SUCCESS(vrc))
            vrc = pVmmDevPort->pfnCpuHotUnplug(pVmmDevPort, idCpuCore, idCpuPackage);
        if (RT_SUCCESS(vrc))
        {
            unsigned cTries = 100;
            do
            {
                /* It will take some time until the event is processed in the guest. Wait...  */
                vrc = pApicPort ? pApicPort->pfnGetCpuStatus(pApicPort, aCpu, &fLocked) : VERR_INVALID_POINTER;
                if (RT_SUCCESS(vrc) && !fLocked)
                    break;

                /* Sleep a bit */
                RTThreadSleep(100);
            } while (cTries-- > 0);
        }
        else if (vrc == VERR_CPU_HOTPLUG_NOT_MONITORED_BY_GUEST)
        {
            /* Query one time. It is possible that the user ejected the CPU. */
            vrc = pApicPort ? pApicPort->pfnGetCpuStatus(pApicPort, aCpu, &fLocked) : VERR_INVALID_POINTER;
        }
    }

    /* If the CPU was unlocked we can detach it now. */
    if (RT_SUCCESS(vrc) && !fLocked)
    {
        /*
         * Call worker in EMT, that's faster and safer than doing everything
         * using VMR3ReqCall.
         */
        PVMREQ pReq;
        vrc = VMR3ReqCall(pVM, 0, &pReq, 0 /* no wait! */, VMREQFLAGS_VBOX_STATUS,
                          (PFNRT)Console::unplugCpu, 3,
                          this, pVM, aCpu);
        if (vrc == VERR_TIMEOUT || RT_SUCCESS(vrc))
        {
            vrc = VMR3ReqWait(pReq, RT_INDEFINITE_WAIT);
            AssertRC(vrc);
            if (RT_SUCCESS(vrc))
                vrc = pReq->iStatus;
        }
        VMR3ReqFree(pReq);

        if (RT_SUCCESS(vrc))
        {
            /* Detach it from the VM  */
            vrc = VMR3HotUnplugCpu(pVM, aCpu);
            AssertRC(vrc);
        }
        else
           rc = setError(VBOX_E_VM_ERROR,
                         tr("Hot-Remove failed (rc=%Rrc)"), vrc);
    }
    else
        rc = setError(VBOX_E_VM_ERROR,
                      tr("Hot-Remove was aborted because the CPU may still be used by the guest"), VERR_RESOURCE_BUSY);

    LogFlowThisFunc(("mMachineState=%d, rc=%Rhrc\n", mMachineState, rc));
    LogFlowThisFuncLeave();
    return rc;
}

/*static*/ DECLCALLBACK(int) Console::plugCpu(Console *pThis, PVM pVM, unsigned uCpu)
{
    LogFlowFunc(("pThis=%p uCpu=%u\n", pThis, uCpu));

    AssertReturn(pThis, VERR_INVALID_PARAMETER);

    int rc = VMR3HotPlugCpu(pVM, uCpu);
    AssertRC(rc);

    PCFGMNODE pInst = CFGMR3GetChild(CFGMR3GetRoot(pVM), "Devices/acpi/0/");
    AssertRelease(pInst);
    /* nuke anything which might have been left behind. */
    CFGMR3RemoveNode(CFGMR3GetChildF(pInst, "LUN#%d", uCpu));

#define RC_CHECK() do { if (RT_FAILURE(rc)) { AssertReleaseRC(rc); break; } } while (0)

    PCFGMNODE pLunL0;
    PCFGMNODE pCfg;
    rc = CFGMR3InsertNodeF(pInst, &pLunL0, "LUN#%d", uCpu);     RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",       "ACPICpu"); RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config",       &pCfg);     RC_CHECK();

    /*
     * Attach the driver.
     */
    PPDMIBASE pBase;
    rc = PDMR3DeviceAttach(pVM, "acpi", 0, uCpu, 0, &pBase); RC_CHECK();

    Log(("PlugCpu: rc=%Rrc\n", rc));

    CFGMR3Dump(pInst);

#undef RC_CHECK

    return VINF_SUCCESS;
}

HRESULT Console::doCPUAdd(ULONG aCpu, PVM pVM)
{
    HRESULT rc = S_OK;

    LogFlowThisFuncEnter();
    LogFlowThisFunc(("mMachineState=%d\n", mMachineState));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (   mMachineState != MachineState_Running
        && mMachineState != MachineState_Teleporting
        && mMachineState != MachineState_LiveSnapshotting
        /** @todo r=bird: This should be allowed on paused VMs as well. Later.  */
       )
        return setInvalidMachineStateError();

    AssertReturn(m_pVMMDev, E_FAIL);
    PPDMIVMMDEVPORT pDevPort = m_pVMMDev->getVMMDevPort();
    AssertReturn(pDevPort, E_FAIL);

    /* Check if the CPU is present */
    BOOL fCpuAttached;
    rc = mMachine->GetCPUStatus(aCpu, &fCpuAttached);
    if (FAILED(rc)) return rc;

    if (fCpuAttached)
        return setError(E_FAIL,
                        tr("CPU %d is already attached"), aCpu);

    /*
     * Call worker in EMT, that's faster and safer than doing everything
     * using VMR3ReqCall. Note that we separate VMR3ReqCall from VMR3ReqWait
     * here to make requests from under the lock in order to serialize them.
     */
    PVMREQ pReq;
    int vrc = VMR3ReqCall(pVM, 0, &pReq, 0 /* no wait! */, VMREQFLAGS_VBOX_STATUS,
                          (PFNRT)Console::plugCpu, 3,
                          this, pVM, aCpu);

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.release();

    if (vrc == VERR_TIMEOUT || RT_SUCCESS(vrc))
    {
        vrc = VMR3ReqWait(pReq, RT_INDEFINITE_WAIT);
        AssertRC(vrc);
        if (RT_SUCCESS(vrc))
            vrc = pReq->iStatus;
    }
    VMR3ReqFree(pReq);

    rc = RT_SUCCESS(vrc) ? S_OK :
        setError(VBOX_E_VM_ERROR,
                 tr("Could not add CPU to the machine (%Rrc)"),
                 vrc);

    if (RT_SUCCESS(vrc))
    {
        /* Notify the guest if possible. */
        uint32_t idCpuCore, idCpuPackage;
        vrc = VMR3GetCpuCoreAndPackageIdFromCpuId(pVM, aCpu, &idCpuCore, &idCpuPackage); AssertRC(vrc);
        if (RT_SUCCESS(vrc))
            vrc = pDevPort->pfnCpuHotPlug(pDevPort, idCpuCore, idCpuPackage);
        /** @todo warning if the guest doesn't support it */
    }

    LogFlowThisFunc(("mMachineState=%d, rc=%Rhrc\n", mMachineState, rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::Pause()
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    switch (mMachineState)
    {
        case MachineState_Running:
        case MachineState_Teleporting:
        case MachineState_LiveSnapshotting:
            break;

        case MachineState_Paused:
        case MachineState_TeleportingPausedVM:
        case MachineState_Saving:
            return setError(VBOX_E_INVALID_VM_STATE, tr("Already paused"));

        default:
            return setInvalidMachineStateError();
    }

    /* get the VM handle. */
    SafeVMPtr ptrVM(this);
    if (!ptrVM.isOk())
        return ptrVM.rc();

    LogFlowThisFunc(("Sending PAUSE request...\n"));

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

    int vrc = VMR3Suspend(ptrVM);

    HRESULT hrc = S_OK;
    if (RT_FAILURE(vrc))
        hrc = setError(VBOX_E_VM_ERROR, tr("Could not suspend the machine execution (%Rrc)"), vrc);

    LogFlowThisFunc(("hrc=%Rhrc\n", hrc));
    LogFlowThisFuncLeave();
    return hrc;
}

STDMETHODIMP Console::Resume()
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mMachineState != MachineState_Paused)
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("Cannot resume the machine as it is not paused (machine state: %s)"),
                        Global::stringifyMachineState(mMachineState));

    /* get the VM handle. */
    SafeVMPtr ptrVM(this);
    if (!ptrVM.isOk())
        return ptrVM.rc();

    LogFlowThisFunc(("Sending RESUME request...\n"));

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

#ifdef VBOX_WITH_EXTPACK
    int vrc = mptrExtPackManager->callAllVmPowerOnHooks(this, ptrVM); /** @todo called a few times too many... */
#else
    int vrc = VINF_SUCCESS;
#endif
    if (RT_SUCCESS(vrc))
    {
        if (VMR3GetState(ptrVM) == VMSTATE_CREATED)
            vrc = VMR3PowerOn(ptrVM); /* (PowerUpPaused) */
        else
            vrc = VMR3Resume(ptrVM);
    }

    HRESULT rc = RT_SUCCESS(vrc) ? S_OK :
        setError(VBOX_E_VM_ERROR,
                 tr("Could not resume the machine execution (%Rrc)"),
                 vrc);

    LogFlowThisFunc(("rc=%Rhrc\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::PowerButton()
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (   mMachineState != MachineState_Running
        && mMachineState != MachineState_Teleporting
        && mMachineState != MachineState_LiveSnapshotting
       )
        return setInvalidMachineStateError();

    /* get the VM handle. */
    SafeVMPtr ptrVM(this);
    if (!ptrVM.isOk())
        return ptrVM.rc();
/** @todo leave the console lock? */

    /* get the acpi device interface and press the button. */
    PPDMIBASE pBase;
    int vrc = PDMR3QueryDeviceLun(ptrVM, "acpi", 0, 0, &pBase);
    if (RT_SUCCESS(vrc))
    {
        Assert(pBase);
        PPDMIACPIPORT pPort = PDMIBASE_QUERY_INTERFACE(pBase, PDMIACPIPORT);
        if (pPort)
            vrc = pPort->pfnPowerButtonPress(pPort);
        else
            vrc = VERR_PDM_MISSING_INTERFACE;
    }

    HRESULT rc = RT_SUCCESS(vrc) ? S_OK :
        setError(VBOX_E_PDM_ERROR,
                 tr("Controlled power off failed (%Rrc)"),
                 vrc);

    LogFlowThisFunc(("rc=%Rhrc\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::GetPowerButtonHandled(BOOL *aHandled)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aHandled);

    *aHandled = FALSE;

    AutoCaller autoCaller(this);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (   mMachineState != MachineState_Running
        && mMachineState != MachineState_Teleporting
        && mMachineState != MachineState_LiveSnapshotting
       )
        return setInvalidMachineStateError();

    /* get the VM handle. */
    SafeVMPtr ptrVM(this);
    if (!ptrVM.isOk())
        return ptrVM.rc();
/** @todo leave the console lock? */

    /* get the acpi device interface and check if the button press was handled. */
    PPDMIBASE pBase;
    int vrc = PDMR3QueryDeviceLun(ptrVM, "acpi", 0, 0, &pBase);
    if (RT_SUCCESS(vrc))
    {
        Assert(pBase);
        PPDMIACPIPORT pPort = PDMIBASE_QUERY_INTERFACE(pBase, PDMIACPIPORT);
        if (pPort)
        {
            bool fHandled = false;
            vrc = pPort->pfnGetPowerButtonHandled(pPort, &fHandled);
            if (RT_SUCCESS(vrc))
                *aHandled = fHandled;
        }
        else
            vrc = VERR_PDM_MISSING_INTERFACE;
    }

    HRESULT rc = RT_SUCCESS(vrc) ? S_OK :
        setError(VBOX_E_PDM_ERROR,
            tr("Checking if the ACPI Power Button event was handled by the guest OS failed (%Rrc)"),
            vrc);

    LogFlowThisFunc(("rc=%Rhrc\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::GetGuestEnteredACPIMode(BOOL *aEntered)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aEntered);

    *aEntered = FALSE;

    AutoCaller autoCaller(this);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (   mMachineState != MachineState_Running
        && mMachineState != MachineState_Teleporting
        && mMachineState != MachineState_LiveSnapshotting
       )
        return setError(VBOX_E_INVALID_VM_STATE,
            tr("Invalid machine state %s when checking if the guest entered the ACPI mode)"),
            Global::stringifyMachineState(mMachineState));

    /* get the VM handle. */
    SafeVMPtr ptrVM(this);
    if (!ptrVM.isOk())
        return ptrVM.rc();

/** @todo leave the console lock? */

    /* get the acpi device interface and query the information. */
    PPDMIBASE pBase;
    int vrc = PDMR3QueryDeviceLun(ptrVM, "acpi", 0, 0, &pBase);
    if (RT_SUCCESS(vrc))
    {
        Assert(pBase);
        PPDMIACPIPORT pPort = PDMIBASE_QUERY_INTERFACE(pBase, PDMIACPIPORT);
        if (pPort)
        {
            bool fEntered = false;
            vrc = pPort->pfnGetGuestEnteredACPIMode(pPort, &fEntered);
            if (RT_SUCCESS(vrc))
                *aEntered = fEntered;
        }
        else
            vrc = VERR_PDM_MISSING_INTERFACE;
    }

    LogFlowThisFuncLeave();
    return S_OK;
}

STDMETHODIMP Console::SleepButton()
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mMachineState != MachineState_Running) /** @todo Live Migration: ??? */
        return setInvalidMachineStateError();

    /* get the VM handle. */
    SafeVMPtr ptrVM(this);
    if (!ptrVM.isOk())
        return ptrVM.rc();

/** @todo leave the console lock? */

    /* get the acpi device interface and press the sleep button. */
    PPDMIBASE pBase;
    int vrc = PDMR3QueryDeviceLun(ptrVM, "acpi", 0, 0, &pBase);
    if (RT_SUCCESS(vrc))
    {
        Assert(pBase);
        PPDMIACPIPORT pPort = PDMIBASE_QUERY_INTERFACE(pBase, PDMIACPIPORT);
        if (pPort)
            vrc = pPort->pfnSleepButtonPress(pPort);
        else
            vrc = VERR_PDM_MISSING_INTERFACE;
    }

    HRESULT rc = RT_SUCCESS(vrc) ? S_OK :
        setError(VBOX_E_PDM_ERROR,
            tr("Sending sleep button event failed (%Rrc)"),
            vrc);

    LogFlowThisFunc(("rc=%Rhrc\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::SaveState(IProgress **aProgress)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("mMachineState=%d\n", mMachineState));

    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (   mMachineState != MachineState_Running
        && mMachineState != MachineState_Paused)
    {
        return setError(VBOX_E_INVALID_VM_STATE,
            tr("Cannot save the execution state as the machine is not running or paused (machine state: %s)"),
            Global::stringifyMachineState(mMachineState));
    }

    /* memorize the current machine state */
    MachineState_T lastMachineState = mMachineState;

    if (mMachineState == MachineState_Running)
    {
        HRESULT rc = Pause();
        if (FAILED(rc))
            return rc;
    }

    HRESULT rc = S_OK;
    bool fBeganSavingState = false;
    bool fTaskCreationFailed = false;

    do
    {
        ComPtr<IProgress> pProgress;
        Bstr stateFilePath;

        /*
         * request a saved state file path from the server
         * (this will set the machine state to Saving on the server to block
         * others from accessing this machine)
         */
        rc = mControl->BeginSavingState(pProgress.asOutParam(),
                                        stateFilePath.asOutParam());
        if (FAILED(rc))
            break;

        fBeganSavingState = true;

        /* sync the state with the server */
        setMachineStateLocally(MachineState_Saving);

        /* ensure the directory for the saved state file exists */
        {
            Utf8Str dir = stateFilePath;
            dir.stripFilename();
            if (!RTDirExists(dir.c_str()))
            {
                int vrc = RTDirCreateFullPath(dir.c_str(), 0777);
                if (RT_FAILURE(vrc))
                {
                    rc = setError(VBOX_E_FILE_ERROR,
                        tr("Could not create a directory '%s' to save the state to (%Rrc)"),
                        dir.c_str(), vrc);
                    break;
                }
            }
        }

        /* create a task object early to ensure mpVM protection is successful */
        std::auto_ptr<VMSaveTask> task(new VMSaveTask(this, pProgress,
                                                      stateFilePath));
        rc = task->rc();
        /*
         * If we fail here it means a PowerDown() call happened on another
         * thread while we were doing Pause() (which leaves the Console lock).
         * We assign PowerDown() a higher precedence than SaveState(),
         * therefore just return the error to the caller.
         */
        if (FAILED(rc))
        {
            fTaskCreationFailed = true;
            break;
        }

        /* create a thread to wait until the VM state is saved */
        int vrc = RTThreadCreate(NULL, Console::saveStateThread, (void *)task.get(),
                                 0, RTTHREADTYPE_MAIN_WORKER, 0, "VMSave");
        if (RT_FAILURE(vrc))
        {
            rc = setError(E_FAIL, "Could not create VMSave thread (%Rrc)", vrc);
            break;
        }

        /* task is now owned by saveStateThread(), so release it */
        task.release();

        /* return the progress to the caller */
        pProgress.queryInterfaceTo(aProgress);
    } while (0);

    if (FAILED(rc) && !fTaskCreationFailed)
    {
        /* preserve existing error info */
        ErrorInfoKeeper eik;

        if (fBeganSavingState)
        {
            /*
             * cancel the requested save state procedure.
             * This will reset the machine state to the state it had right
             * before calling mControl->BeginSavingState().
             */
            mControl->EndSavingState(eik.getResultCode(), eik.getText().raw());
        }

        if (lastMachineState == MachineState_Running)
        {
            /* restore the paused state if appropriate */
            setMachineStateLocally(MachineState_Paused);
            /* restore the running state if appropriate */
            Resume();
        }
        else
            setMachineStateLocally(lastMachineState);
    }

    LogFlowThisFunc(("rc=%Rhrc\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::AdoptSavedState(IN_BSTR aSavedStateFile)
{
    CheckComArgStrNotEmptyOrNull(aSavedStateFile);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (   mMachineState != MachineState_PoweredOff
        && mMachineState != MachineState_Teleported
        && mMachineState != MachineState_Aborted
       )
        return setError(VBOX_E_INVALID_VM_STATE,
            tr("Cannot adopt the saved machine state as the machine is not in Powered Off, Teleported or Aborted state (machine state: %s)"),
            Global::stringifyMachineState(mMachineState));

    return mControl->AdoptSavedState(aSavedStateFile);
}

STDMETHODIMP Console::DiscardSavedState(BOOL aRemoveFile)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mMachineState != MachineState_Saved)
        return setError(VBOX_E_INVALID_VM_STATE,
            tr("Cannot delete the machine state as the machine is not in the saved state (machine state: %s)"),
            Global::stringifyMachineState(mMachineState));

    HRESULT rc = mControl->SetRemoveSavedStateFile(aRemoveFile);
    if (FAILED(rc)) return rc;

    /*
     * Saved -> PoweredOff transition will be detected in the SessionMachine
     * and properly handled.
     */
    rc = setMachineState(MachineState_PoweredOff);

    return rc;
}

/** read the value of a LEd. */
inline uint32_t readAndClearLed(PPDMLED pLed)
{
    if (!pLed)
        return 0;
    uint32_t u32 = pLed->Actual.u32 | pLed->Asserted.u32;
    pLed->Asserted.u32 = 0;
    return u32;
}

STDMETHODIMP Console::GetDeviceActivity(DeviceType_T aDeviceType,
                                        DeviceActivity_T *aDeviceActivity)
{
    CheckComArgNotNull(aDeviceActivity);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /*
     * Note: we don't lock the console object here because
     * readAndClearLed() should be thread safe.
     */

    /* Get LED array to read */
    PDMLEDCORE SumLed = {0};
    switch (aDeviceType)
    {
        case DeviceType_Floppy:
        case DeviceType_DVD:
        case DeviceType_HardDisk:
        {
            for (unsigned i = 0; i < RT_ELEMENTS(mapStorageLeds); ++i)
                if (maStorageDevType[i] == aDeviceType)
                    SumLed.u32 |= readAndClearLed(mapStorageLeds[i]);
            break;
        }

        case DeviceType_Network:
        {
            for (unsigned i = 0; i < RT_ELEMENTS(mapNetworkLeds); ++i)
                SumLed.u32 |= readAndClearLed(mapNetworkLeds[i]);
            break;
        }

        case DeviceType_USB:
        {
            for (unsigned i = 0; i < RT_ELEMENTS(mapUSBLed); ++i)
                SumLed.u32 |= readAndClearLed(mapUSBLed[i]);
            break;
        }

        case DeviceType_SharedFolder:
        {
            SumLed.u32 |= readAndClearLed(mapSharedFolderLed);
            break;
        }

        default:
            return setError(E_INVALIDARG,
                tr("Invalid device type: %d"),
                aDeviceType);
    }

    /* Compose the result */
    switch (SumLed.u32 & (PDMLED_READING | PDMLED_WRITING))
    {
        case 0:
            *aDeviceActivity = DeviceActivity_Idle;
            break;
        case PDMLED_READING:
            *aDeviceActivity = DeviceActivity_Reading;
            break;
        case PDMLED_WRITING:
        case PDMLED_READING | PDMLED_WRITING:
            *aDeviceActivity = DeviceActivity_Writing;
            break;
    }

    return S_OK;
}

STDMETHODIMP Console::AttachUSBDevice(IN_BSTR aId)
{
#ifdef VBOX_WITH_USB
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (   mMachineState != MachineState_Running
        && mMachineState != MachineState_Paused)
        return setError(VBOX_E_INVALID_VM_STATE,
            tr("Cannot attach a USB device to the machine which is not running or paused (machine state: %s)"),
            Global::stringifyMachineState(mMachineState));

    /* Get the VM handle. */
    SafeVMPtr ptrVM(this);
    if (!ptrVM.isOk())
        return ptrVM.rc();

    /* Don't proceed unless we've found the usb controller. */
    PPDMIBASE pBase = NULL;
    int vrc = PDMR3QueryLun(ptrVM, "usb-ohci", 0, 0, &pBase);
    if (RT_FAILURE(vrc))
        return setError(VBOX_E_PDM_ERROR,
            tr("The virtual machine does not have a USB controller"));

    /* leave the lock because the USB Proxy service may call us back
     * (via onUSBDeviceAttach()) */
    alock.leave();

    /* Request the device capture */
    return mControl->CaptureUSBDevice(aId);

#else   /* !VBOX_WITH_USB */
    return setError(VBOX_E_PDM_ERROR,
        tr("The virtual machine does not have a USB controller"));
#endif  /* !VBOX_WITH_USB */
}

STDMETHODIMP Console::DetachUSBDevice(IN_BSTR aId, IUSBDevice **aDevice)
{
#ifdef VBOX_WITH_USB
    CheckComArgOutPointerValid(aDevice);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Find it. */
    ComObjPtr<OUSBDevice> pUSBDevice;
    USBDeviceList::iterator it = mUSBDevices.begin();
    Guid uuid(aId);
    while (it != mUSBDevices.end())
    {
        if ((*it)->id() == uuid)
        {
            pUSBDevice = *it;
            break;
        }
        ++it;
    }

    if (!pUSBDevice)
        return setError(E_INVALIDARG,
            tr("USB device with UUID {%RTuuid} is not attached to this machine"),
            Guid(aId).raw());

    /*
     * Inform the USB device and USB proxy about what's cooking.
     */
    alock.leave();
    HRESULT rc2 = mControl->DetachUSBDevice(aId, false /* aDone */);
    if (FAILED(rc2))
        return rc2;
    alock.enter();

    /* Request the PDM to detach the USB device. */
    HRESULT rc = detachUSBDevice(it);

    if (SUCCEEDED(rc))
    {
        /* leave the lock since we don't need it any more (note though that
         * the USB Proxy service must not call us back here) */
        alock.leave();

        /* Request the device release. Even if it fails, the device will
         * remain as held by proxy, which is OK for us (the VM process). */
        rc = mControl->DetachUSBDevice(aId, true /* aDone */);
    }

    return rc;


#else   /* !VBOX_WITH_USB */
    return setError(VBOX_E_PDM_ERROR,
        tr("The virtual machine does not have a USB controller"));
#endif  /* !VBOX_WITH_USB */
}

STDMETHODIMP Console::FindUSBDeviceByAddress(IN_BSTR aAddress, IUSBDevice **aDevice)
{
#ifdef VBOX_WITH_USB
    CheckComArgStrNotEmptyOrNull(aAddress);
    CheckComArgOutPointerValid(aDevice);

    *aDevice = NULL;

    SafeIfaceArray<IUSBDevice> devsvec;
    HRESULT rc = COMGETTER(USBDevices)(ComSafeArrayAsOutParam(devsvec));
    if (FAILED(rc)) return rc;

    for (size_t i = 0; i < devsvec.size(); ++i)
    {
        Bstr address;
        rc = devsvec[i]->COMGETTER(Address)(address.asOutParam());
        if (FAILED(rc)) return rc;
        if (address == aAddress)
        {
            ComObjPtr<OUSBDevice> pUSBDevice;
            pUSBDevice.createObject();
            pUSBDevice->init(devsvec[i]);
            return pUSBDevice.queryInterfaceTo(aDevice);
        }
    }

    return setErrorNoLog(VBOX_E_OBJECT_NOT_FOUND,
        tr("Could not find a USB device with address '%ls'"),
        aAddress);

#else   /* !VBOX_WITH_USB */
    return E_NOTIMPL;
#endif  /* !VBOX_WITH_USB */
}

STDMETHODIMP Console::FindUSBDeviceById(IN_BSTR aId, IUSBDevice **aDevice)
{
#ifdef VBOX_WITH_USB
    CheckComArgExpr(aId, Guid(aId).isEmpty() == false);
    CheckComArgOutPointerValid(aDevice);

    *aDevice = NULL;

    SafeIfaceArray<IUSBDevice> devsvec;
    HRESULT rc = COMGETTER(USBDevices)(ComSafeArrayAsOutParam(devsvec));
    if (FAILED(rc)) return rc;

    for (size_t i = 0; i < devsvec.size(); ++i)
    {
        Bstr id;
        rc = devsvec[i]->COMGETTER(Id)(id.asOutParam());
        if (FAILED(rc)) return rc;
        if (id == aId)
        {
            ComObjPtr<OUSBDevice> pUSBDevice;
            pUSBDevice.createObject();
            pUSBDevice->init(devsvec[i]);
            return pUSBDevice.queryInterfaceTo(aDevice);
        }
    }

    return setErrorNoLog(VBOX_E_OBJECT_NOT_FOUND,
        tr("Could not find a USB device with uuid {%RTuuid}"),
        Guid(aId).raw());

#else   /* !VBOX_WITH_USB */
    return E_NOTIMPL;
#endif  /* !VBOX_WITH_USB */
}

STDMETHODIMP
Console::CreateSharedFolder(IN_BSTR aName, IN_BSTR aHostPath, BOOL aWritable, BOOL aAutoMount)
{
    CheckComArgStrNotEmptyOrNull(aName);
    CheckComArgStrNotEmptyOrNull(aHostPath);

    LogFlowThisFunc(("Entering for '%ls' -> '%ls'\n", aName, aHostPath));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    Utf8Str strName(aName);
    Utf8Str strHostPath(aHostPath);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /// @todo see @todo in AttachUSBDevice() about the Paused state
    if (mMachineState == MachineState_Saved)
        return setError(VBOX_E_INVALID_VM_STATE,
            tr("Cannot create a transient shared folder on the machine in the saved state"));
    if (   mMachineState != MachineState_PoweredOff
        && mMachineState != MachineState_Teleported
        && mMachineState != MachineState_Aborted
        && mMachineState != MachineState_Running
        && mMachineState != MachineState_Paused
       )
        return setError(VBOX_E_INVALID_VM_STATE,
            tr("Cannot create a transient shared folder on the machine while it is changing the state (machine state: %s)"),
            Global::stringifyMachineState(mMachineState));

    ComObjPtr<SharedFolder> pSharedFolder;
    HRESULT rc = findSharedFolder(strName, pSharedFolder, false /* aSetError */);
    if (SUCCEEDED(rc))
        return setError(VBOX_E_FILE_ERROR,
                        tr("Shared folder named '%s' already exists"),
                        strName.c_str());

    pSharedFolder.createObject();
    rc = pSharedFolder->init(this,
                             strName,
                             strHostPath,
                             aWritable,
                             aAutoMount,
                             true /* fFailOnError */);
    if (FAILED(rc)) return rc;

    /* If the VM is online and supports shared folders, share this folder
     * under the specified name. (Ignore any failure to obtain the VM handle.) */
    SafeVMPtrQuiet ptrVM(this);
    if (    ptrVM.isOk()
         && m_pVMMDev
         && m_pVMMDev->isShFlActive()
       )
    {
        /* first, remove the machine or the global folder if there is any */
        SharedFolderDataMap::const_iterator it;
        if (findOtherSharedFolder(aName, it))
        {
            rc = removeSharedFolder(aName);
            if (FAILED(rc))
                return rc;
        }

        /* second, create the given folder */
        rc = createSharedFolder(aName, SharedFolderData(aHostPath, aWritable, aAutoMount));
        if (FAILED(rc))
            return rc;
    }

    m_mapSharedFolders.insert(std::make_pair(aName, pSharedFolder));

    /* notify console callbacks after the folder is added to the list */
    fireSharedFolderChangedEvent(mEventSource, Scope_Session);

    LogFlowThisFunc(("Leaving for '%ls' -> '%ls'\n", aName, aHostPath));

    return rc;
}

STDMETHODIMP Console::RemoveSharedFolder(IN_BSTR aName)
{
    CheckComArgStrNotEmptyOrNull(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    LogFlowThisFunc(("Entering for '%ls'\n", aName));

    Utf8Str strName(aName);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /// @todo see @todo in AttachUSBDevice() about the Paused state
    if (mMachineState == MachineState_Saved)
        return setError(VBOX_E_INVALID_VM_STATE,
            tr("Cannot remove a transient shared folder from the machine in the saved state"));
    if (   mMachineState != MachineState_PoweredOff
        && mMachineState != MachineState_Teleported
        && mMachineState != MachineState_Aborted
        && mMachineState != MachineState_Running
        && mMachineState != MachineState_Paused
       )
        return setError(VBOX_E_INVALID_VM_STATE,
            tr("Cannot remove a transient shared folder from the machine while it is changing the state (machine state: %s)"),
            Global::stringifyMachineState(mMachineState));

    ComObjPtr<SharedFolder> pSharedFolder;
    HRESULT rc = findSharedFolder(aName, pSharedFolder, true /* aSetError */);
    if (FAILED(rc)) return rc;

    /* protect the VM handle (if not NULL) */
    SafeVMPtrQuiet ptrVM(this);
    if (    ptrVM.isOk()
         && m_pVMMDev
         && m_pVMMDev->isShFlActive()
       )
    {
        /* if the VM is online and supports shared folders, UNshare this
         * folder. */

        /* first, remove the given folder */
        rc = removeSharedFolder(strName);
        if (FAILED(rc)) return rc;

        /* first, remove the machine or the global folder if there is any */
        SharedFolderDataMap::const_iterator it;
        if (findOtherSharedFolder(strName, it))
        {
            rc = createSharedFolder(strName, it->second);
            /* don't check rc here because we need to remove the console
             * folder from the collection even on failure */
        }
    }

    m_mapSharedFolders.erase(strName);

    /* notify console callbacks after the folder is removed to the list */
    fireSharedFolderChangedEvent(mEventSource, Scope_Session);

    LogFlowThisFunc(("Leaving for '%ls'\n", aName));

    return rc;
}

STDMETHODIMP Console::TakeSnapshot(IN_BSTR aName,
                                   IN_BSTR aDescription,
                                   IProgress **aProgress)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aName='%ls' mMachineState=%d\n", aName, mMachineState));

    CheckComArgStrNotEmptyOrNull(aName);
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (Global::IsTransient(mMachineState))
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("Cannot take a snapshot of the machine while it is changing the state (machine state: %s)"),
                        Global::stringifyMachineState(mMachineState));

    HRESULT rc = S_OK;

    /* prepare the progress object:
       a) count the no. of hard disk attachments to get a matching no. of progress sub-operations */
    ULONG cOperations = 2;              // always at least setting up + finishing up
    ULONG ulTotalOperationsWeight = 2;  // one each for setting up + finishing up
    SafeIfaceArray<IMediumAttachment> aMediumAttachments;
    rc = mMachine->COMGETTER(MediumAttachments)(ComSafeArrayAsOutParam(aMediumAttachments));
    if (FAILED(rc))
        return setError(rc, tr("Cannot get medium attachments of the machine"));

    ULONG ulMemSize;
    rc = mMachine->COMGETTER(MemorySize)(&ulMemSize);
    if (FAILED(rc))
        return rc;

    for (size_t i = 0;
         i < aMediumAttachments.size();
         ++i)
    {
        DeviceType_T type;
        rc = aMediumAttachments[i]->COMGETTER(Type)(&type);
        if (FAILED(rc))
            return rc;

        if (type == DeviceType_HardDisk)
        {
            ++cOperations;

            // assume that creating a diff image takes as long as saving a 1MB state
            // (note, the same value must be used in SessionMachine::BeginTakingSnapshot() on the server!)
            ulTotalOperationsWeight += 1;
        }
    }

    // b) one extra sub-operations for online snapshots OR offline snapshots that have a saved state (needs to be copied)
    bool const fTakingSnapshotOnline = Global::IsOnline(mMachineState);

    LogFlowFunc(("fTakingSnapshotOnline = %d, mMachineState = %d\n", fTakingSnapshotOnline, mMachineState));

    if (fTakingSnapshotOnline)
    {
        ++cOperations;
        ulTotalOperationsWeight += ulMemSize;
    }

    // finally, create the progress object
    ComObjPtr<Progress> pProgress;
    pProgress.createObject();
    rc = pProgress->init(static_cast<IConsole*>(this),
                         Bstr(tr("Taking a snapshot of the virtual machine")).raw(),
                         mMachineState == MachineState_Running /* aCancelable */,
                         cOperations,
                         ulTotalOperationsWeight,
                         Bstr(tr("Setting up snapshot operation")).raw(),      // first sub-op description
                         1);        // ulFirstOperationWeight

    if (FAILED(rc))
        return rc;

    VMTakeSnapshotTask *pTask;
    if (!(pTask = new VMTakeSnapshotTask(this, pProgress, aName, aDescription)))
        return E_OUTOFMEMORY;

    Assert(pTask->mProgress);

    try
    {
        mptrCancelableProgress = pProgress;

        /*
         * If we fail here it means a PowerDown() call happened on another
         * thread while we were doing Pause() (which leaves the Console lock).
         * We assign PowerDown() a higher precedence than TakeSnapshot(),
         * therefore just return the error to the caller.
         */
        rc = pTask->rc();
        if (FAILED(rc)) throw rc;

        pTask->ulMemSize = ulMemSize;

        /* memorize the current machine state */
        pTask->lastMachineState = mMachineState;
        pTask->fTakingSnapshotOnline = fTakingSnapshotOnline;

        int vrc = RTThreadCreate(NULL,
                                 Console::fntTakeSnapshotWorker,
                                 (void *)pTask,
                                 0,
                                 RTTHREADTYPE_MAIN_WORKER,
                                 0,
                                 "ConsoleTakeSnap");
        if (FAILED(vrc))
            throw setError(E_FAIL,
                           tr("Could not create VMTakeSnap thread (%Rrc)"),
                           vrc);

        pTask->mProgress.queryInterfaceTo(aProgress);
    }
    catch (HRESULT erc)
    {
        delete pTask;
        rc = erc;
        mptrCancelableProgress.setNull();
    }

    LogFlowThisFunc(("rc=%Rhrc\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::DeleteSnapshot(IN_BSTR aId, IProgress **aProgress)
{
    CheckComArgExpr(aId, Guid(aId).isEmpty() == false);
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (Global::IsTransient(mMachineState))
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("Cannot delete a snapshot of the machine while it is changing the state (machine state: %s)"),
                        Global::stringifyMachineState(mMachineState));

    MachineState_T machineState = MachineState_Null;
    HRESULT rc = mControl->DeleteSnapshot(this, aId, aId, FALSE /* fDeleteAllChildren */, &machineState, aProgress);
    if (FAILED(rc)) return rc;

    setMachineStateLocally(machineState);
    return S_OK;
}

STDMETHODIMP Console::DeleteSnapshotAndAllChildren(IN_BSTR aId, IProgress **aProgress)
{
    CheckComArgExpr(aId, Guid(aId).isEmpty() == false);
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (Global::IsTransient(mMachineState))
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("Cannot delete a snapshot of the machine while it is changing the state (machine state: %s)"),
                        Global::stringifyMachineState(mMachineState));

    MachineState_T machineState = MachineState_Null;
    HRESULT rc = mControl->DeleteSnapshot(this, aId, aId, TRUE /* fDeleteAllChildren */, &machineState, aProgress);
    if (FAILED(rc)) return rc;

    setMachineStateLocally(machineState);
    return S_OK;
}

STDMETHODIMP Console::DeleteSnapshotRange(IN_BSTR aStartId, IN_BSTR aEndId, IProgress **aProgress)
{
    CheckComArgExpr(aStartId, Guid(aStartId).isEmpty() == false);
    CheckComArgExpr(aEndId, Guid(aEndId).isEmpty() == false);
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (Global::IsTransient(mMachineState))
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("Cannot delete a snapshot of the machine while it is changing the state (machine state: %s)"),
                        Global::stringifyMachineState(mMachineState));

    MachineState_T machineState = MachineState_Null;
    HRESULT rc = mControl->DeleteSnapshot(this, aStartId, aEndId, FALSE /* fDeleteAllChildren */, &machineState, aProgress);
    if (FAILED(rc)) return rc;

    setMachineStateLocally(machineState);
    return S_OK;
}

STDMETHODIMP Console::RestoreSnapshot(ISnapshot *aSnapshot, IProgress **aProgress)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (Global::IsOnlineOrTransient(mMachineState))
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("Cannot delete the current state of the running machine (machine state: %s)"),
                        Global::stringifyMachineState(mMachineState));

    MachineState_T machineState = MachineState_Null;
    HRESULT rc = mControl->RestoreSnapshot(this, aSnapshot, &machineState, aProgress);
    if (FAILED(rc)) return rc;

    setMachineStateLocally(machineState);
    return S_OK;
}

// Non-interface public methods
/////////////////////////////////////////////////////////////////////////////

/*static*/
HRESULT Console::setErrorStatic(HRESULT aResultCode, const char *pcsz, ...)
{
    va_list args;
    va_start(args, pcsz);
    HRESULT rc = setErrorInternal(aResultCode,
                                  getStaticClassIID(),
                                  getStaticComponentName(),
                                  Utf8Str(pcsz, args),
                                  false /* aWarning */,
                                  true /* aLogIt */);
    va_end(args);
    return rc;
}

HRESULT Console::setInvalidMachineStateError()
{
    return setError(VBOX_E_INVALID_VM_STATE,
                    tr("Invalid machine state: %s"),
                    Global::stringifyMachineState(mMachineState));
}


/**
 * @copydoc VirtualBox::handleUnexpectedExceptions
 */
/* static */
HRESULT Console::handleUnexpectedExceptions(RT_SRC_POS_DECL)
{
    try
    {
        /* re-throw the current exception */
        throw;
    }
    catch (const std::exception &err)
    {
        return setErrorStatic(E_FAIL,
                              tr("Unexpected exception: %s [%s]\n%s[%d] (%s)"),
                              err.what(), typeid(err).name(),
                              pszFile, iLine, pszFunction);
    }
    catch (...)
    {
        return setErrorStatic(E_FAIL,
                              tr("Unknown exception\n%s[%d] (%s)"),
                              pszFile, iLine, pszFunction);
    }

    /* should not get here */
    AssertFailed();
    return E_FAIL;
}

/* static */
const char *Console::convertControllerTypeToDev(StorageControllerType_T enmCtrlType)
{
    switch (enmCtrlType)
    {
        case StorageControllerType_LsiLogic:
            return "lsilogicscsi";
        case StorageControllerType_BusLogic:
            return "buslogic";
        case StorageControllerType_LsiLogicSas:
            return "lsilogicsas";
        case StorageControllerType_IntelAhci:
            return "ahci";
        case StorageControllerType_PIIX3:
        case StorageControllerType_PIIX4:
        case StorageControllerType_ICH6:
            return "piix3ide";
        case StorageControllerType_I82078:
            return "i82078";
        default:
            return NULL;
    }
}

HRESULT Console::convertBusPortDeviceToLun(StorageBus_T enmBus, LONG port, LONG device, unsigned &uLun)
{
    switch (enmBus)
    {
        case StorageBus_IDE:
        case StorageBus_Floppy:
        {
            AssertMsgReturn(port < 2 && port >= 0, ("%d\n", port), E_INVALIDARG);
            AssertMsgReturn(device < 2 && device >= 0, ("%d\n", device), E_INVALIDARG);
            uLun = 2 * port + device;
            return S_OK;
        }
        case StorageBus_SATA:
        case StorageBus_SCSI:
        case StorageBus_SAS:
        {
            uLun = port;
            return S_OK;
        }
        default:
            uLun = 0;
            AssertMsgFailedReturn(("%d\n", enmBus), E_INVALIDARG);
    }
}

// private methods
/////////////////////////////////////////////////////////////////////////////

/**
 * Process a medium change.
 *
 * @param aMediumAttachment The medium attachment with the new medium state.
 * @param fForce            Force medium chance, if it is locked or not.
 * @param pVM               Safe VM handle.
 *
 * @note Locks this object for writing.
 */
HRESULT Console::doMediumChange(IMediumAttachment *aMediumAttachment, bool fForce, PVM pVM)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    /* We will need to release the write lock before calling EMT */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;
    const char *pszDevice = NULL;

    SafeIfaceArray<IStorageController> ctrls;
    rc = mMachine->COMGETTER(StorageControllers)(ComSafeArrayAsOutParam(ctrls));
    AssertComRC(rc);
    IMedium *pMedium;
    rc = aMediumAttachment->COMGETTER(Medium)(&pMedium);
    AssertComRC(rc);
    Bstr mediumLocation;
    if (pMedium)
    {
        rc = pMedium->COMGETTER(Location)(mediumLocation.asOutParam());
        AssertComRC(rc);
    }

    Bstr attCtrlName;
    rc = aMediumAttachment->COMGETTER(Controller)(attCtrlName.asOutParam());
    AssertComRC(rc);
    ComPtr<IStorageController> pStorageController;
    for (size_t i = 0; i < ctrls.size(); ++i)
    {
        Bstr ctrlName;
        rc = ctrls[i]->COMGETTER(Name)(ctrlName.asOutParam());
        AssertComRC(rc);
        if (attCtrlName == ctrlName)
        {
            pStorageController = ctrls[i];
            break;
        }
    }
    if (pStorageController.isNull())
        return setError(E_FAIL,
                        tr("Could not find storage controller '%ls'"), attCtrlName.raw());

    StorageControllerType_T enmCtrlType;
    rc = pStorageController->COMGETTER(ControllerType)(&enmCtrlType);
    AssertComRC(rc);
    pszDevice = convertControllerTypeToDev(enmCtrlType);

    StorageBus_T enmBus;
    rc = pStorageController->COMGETTER(Bus)(&enmBus);
    AssertComRC(rc);
    ULONG uInstance;
    rc = pStorageController->COMGETTER(Instance)(&uInstance);
    AssertComRC(rc);
    BOOL fUseHostIOCache;
    rc = pStorageController->COMGETTER(UseHostIOCache)(&fUseHostIOCache);
    AssertComRC(rc);

    /*
     * Call worker in EMT, that's faster and safer than doing everything
     * using VMR3ReqCall. Note that we separate VMR3ReqCall from VMR3ReqWait
     * here to make requests from under the lock in order to serialize them.
     */
    PVMREQ pReq;
    int vrc = VMR3ReqCall(pVM,
                          VMCPUID_ANY,
                          &pReq,
                          0 /* no wait! */,
                          VMREQFLAGS_VBOX_STATUS,
                          (PFNRT)Console::changeRemovableMedium,
                          8,
                          this,
                          pVM,
                          pszDevice,
                          uInstance,
                          enmBus,
                          fUseHostIOCache,
                          aMediumAttachment,
                          fForce);

    /* leave the lock before waiting for a result (EMT will call us back!) */
    alock.leave();

    if (vrc == VERR_TIMEOUT || RT_SUCCESS(vrc))
    {
        vrc = VMR3ReqWait(pReq, RT_INDEFINITE_WAIT);
        AssertRC(vrc);
        if (RT_SUCCESS(vrc))
            vrc = pReq->iStatus;
    }
    VMR3ReqFree(pReq);

    if (RT_SUCCESS(vrc))
    {
        LogFlowThisFunc(("Returns S_OK\n"));
        return S_OK;
    }

    if (pMedium)
        return setError(E_FAIL,
                        tr("Could not mount the media/drive '%ls' (%Rrc)"),
                        mediumLocation.raw(), vrc);

    return setError(E_FAIL,
                    tr("Could not unmount the currently mounted media/drive (%Rrc)"),
                    vrc);
}

/**
 * Performs the medium change in EMT.
 *
 * @returns VBox status code.
 *
 * @param   pThis           Pointer to the Console object.
 * @param   pVM             The VM handle.
 * @param   pcszDevice      The PDM device name.
 * @param   uInstance       The PDM device instance.
 * @param   uLun            The PDM LUN number of the drive.
 * @param   fHostDrive      True if this is a host drive attachment.
 * @param   pszPath         The path to the media / drive which is now being mounted / captured.
 *                          If NULL no media or drive is attached and the LUN will be configured with
 *                          the default block driver with no media. This will also be the state if
 *                          mounting / capturing the specified media / drive fails.
 * @param   pszFormat       Medium format string, usually "RAW".
 * @param   fPassthrough    Enables using passthrough mode of the host DVD drive if applicable.
 *
 * @thread  EMT
 */
DECLCALLBACK(int) Console::changeRemovableMedium(Console *pConsole,
                                                 PVM pVM,
                                                 const char *pcszDevice,
                                                 unsigned uInstance,
                                                 StorageBus_T enmBus,
                                                 bool fUseHostIOCache,
                                                 IMediumAttachment *aMediumAtt,
                                                 bool fForce)
{
    LogFlowFunc(("pConsole=%p uInstance=%u pszDevice=%p:{%s} enmBus=%u, aMediumAtt=%p, fForce=%d\n",
                 pConsole, uInstance, pcszDevice, pcszDevice, enmBus, aMediumAtt, fForce));

    AssertReturn(pConsole, VERR_INVALID_PARAMETER);

    AutoCaller autoCaller(pConsole);
    AssertComRCReturn(autoCaller.rc(), VERR_ACCESS_DENIED);

    /*
     * Suspend the VM first.
     *
     * The VM must not be running since it might have pending I/O to
     * the drive which is being changed.
     */
    bool fResume;
    VMSTATE enmVMState = VMR3GetState(pVM);
    switch (enmVMState)
    {
        case VMSTATE_RESETTING:
        case VMSTATE_RUNNING:
        {
            LogFlowFunc(("Suspending the VM...\n"));
            /* disable the callback to prevent Console-level state change */
            pConsole->mVMStateChangeCallbackDisabled = true;
            int rc = VMR3Suspend(pVM);
            pConsole->mVMStateChangeCallbackDisabled = false;
            AssertRCReturn(rc, rc);
            fResume = true;
            break;
        }

        case VMSTATE_SUSPENDED:
        case VMSTATE_CREATED:
        case VMSTATE_OFF:
            fResume = false;
            break;

        case VMSTATE_RUNNING_LS:
        case VMSTATE_RUNNING_FT:
            return setErrorInternal(VBOX_E_INVALID_VM_STATE,
                                    COM_IIDOF(IConsole),
                                    getStaticComponentName(),
                                    (enmVMState == VMSTATE_RUNNING_LS) ? Utf8Str(tr("Cannot change drive during live migration")) : Utf8Str(tr("Cannot change drive during fault tolerant syncing")),
                                    false /*aWarning*/,
                                    true /*aLogIt*/);

        default:
            AssertMsgFailedReturn(("enmVMState=%d\n", enmVMState), VERR_ACCESS_DENIED);
    }

    /* Determine the base path for the device instance. */
    PCFGMNODE pCtlInst;
    pCtlInst = CFGMR3GetChildF(CFGMR3GetRoot(pVM), "Devices/%s/%u/", pcszDevice, uInstance);
    AssertReturn(pCtlInst, VERR_INTERNAL_ERROR);

    int rc = VINF_SUCCESS;
    int rcRet = VINF_SUCCESS;

    rcRet = pConsole->configMediumAttachment(pCtlInst,
                                             pcszDevice,
                                             uInstance,
                                             enmBus,
                                             fUseHostIOCache,
                                             false /* fSetupMerge */,
                                             false /* fBuiltinIoCache */,
                                             0 /* uMergeSource */,
                                             0 /* uMergeTarget */,
                                             aMediumAtt,
                                             pConsole->mMachineState,
                                             NULL /* phrc */,
                                             true /* fAttachDetach */,
                                             fForce /* fForceUnmount */,
                                             false  /* fHotplug */,
                                             pVM,
                                             NULL /* paLedDevType */);
    /** @todo this dumps everything attached to this device instance, which
     * is more than necessary. Dumping the changed LUN would be enough. */
    CFGMR3Dump(pCtlInst);

    /*
     * Resume the VM if necessary.
     */
    if (fResume)
    {
        LogFlowFunc(("Resuming the VM...\n"));
        /* disable the callback to prevent Console-level state change */
        pConsole->mVMStateChangeCallbackDisabled = true;
        rc = VMR3Resume(pVM);
        pConsole->mVMStateChangeCallbackDisabled = false;
        AssertRC(rc);
        if (RT_FAILURE(rc))
        {
            /* too bad, we failed. try to sync the console state with the VMM state */
            vmstateChangeCallback(pVM, VMSTATE_SUSPENDED, enmVMState, pConsole);
        }
        /// @todo (r=dmik) if we failed with drive mount, then the VMR3Resume
        // error (if any) will be hidden from the caller. For proper reporting
        // of such multiple errors to the caller we need to enhance the
        // IVirtualBoxError interface. For now, give the first error the higher
        // priority.
        if (RT_SUCCESS(rcRet))
            rcRet = rc;
    }

    LogFlowFunc(("Returning %Rrc\n", rcRet));
    return rcRet;
}


/**
 * Attach a new storage device to the VM.
 *
 * @param aMediumAttachment The medium attachment which is added.
 * @param pVM               Safe VM handle.
 *
 * @note Locks this object for writing.
 */
HRESULT Console::doStorageDeviceAttach(IMediumAttachment *aMediumAttachment, PVM pVM)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    /* We will need to release the write lock before calling EMT */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;
    const char *pszDevice = NULL;

    SafeIfaceArray<IStorageController> ctrls;
    rc = mMachine->COMGETTER(StorageControllers)(ComSafeArrayAsOutParam(ctrls));
    AssertComRC(rc);
    IMedium *pMedium;
    rc = aMediumAttachment->COMGETTER(Medium)(&pMedium);
    AssertComRC(rc);
    Bstr mediumLocation;
    if (pMedium)
    {
        rc = pMedium->COMGETTER(Location)(mediumLocation.asOutParam());
        AssertComRC(rc);
    }

    Bstr attCtrlName;
    rc = aMediumAttachment->COMGETTER(Controller)(attCtrlName.asOutParam());
    AssertComRC(rc);
    ComPtr<IStorageController> pStorageController;
    for (size_t i = 0; i < ctrls.size(); ++i)
    {
        Bstr ctrlName;
        rc = ctrls[i]->COMGETTER(Name)(ctrlName.asOutParam());
        AssertComRC(rc);
        if (attCtrlName == ctrlName)
        {
            pStorageController = ctrls[i];
            break;
        }
    }
    if (pStorageController.isNull())
        return setError(E_FAIL,
                        tr("Could not find storage controller '%ls'"), attCtrlName.raw());

    StorageControllerType_T enmCtrlType;
    rc = pStorageController->COMGETTER(ControllerType)(&enmCtrlType);
    AssertComRC(rc);
    pszDevice = convertControllerTypeToDev(enmCtrlType);

    StorageBus_T enmBus;
    rc = pStorageController->COMGETTER(Bus)(&enmBus);
    AssertComRC(rc);
    ULONG uInstance;
    rc = pStorageController->COMGETTER(Instance)(&uInstance);
    AssertComRC(rc);
    BOOL fUseHostIOCache;
    rc = pStorageController->COMGETTER(UseHostIOCache)(&fUseHostIOCache);
    AssertComRC(rc);

    /*
     * Call worker in EMT, that's faster and safer than doing everything
     * using VMR3ReqCall. Note that we separate VMR3ReqCall from VMR3ReqWait
     * here to make requests from under the lock in order to serialize them.
     */
    PVMREQ pReq;
    int vrc = VMR3ReqCall(pVM,
                          VMCPUID_ANY,
                          &pReq,
                          0 /* no wait! */,
                          VMREQFLAGS_VBOX_STATUS,
                          (PFNRT)Console::attachStorageDevice,
                          7,
                          this,
                          pVM,
                          pszDevice,
                          uInstance,
                          enmBus,
                          fUseHostIOCache,
                          aMediumAttachment);

    /* leave the lock before waiting for a result (EMT will call us back!) */
    alock.leave();

    if (vrc == VERR_TIMEOUT || RT_SUCCESS(vrc))
    {
        vrc = VMR3ReqWait(pReq, RT_INDEFINITE_WAIT);
        AssertRC(vrc);
        if (RT_SUCCESS(vrc))
            vrc = pReq->iStatus;
    }
    VMR3ReqFree(pReq);

    if (RT_SUCCESS(vrc))
    {
        LogFlowThisFunc(("Returns S_OK\n"));
        return S_OK;
    }

    if (!pMedium)
        return setError(E_FAIL,
                        tr("Could not mount the media/drive '%ls' (%Rrc)"),
                        mediumLocation.raw(), vrc);

    return setError(E_FAIL,
                    tr("Could not unmount the currently mounted media/drive (%Rrc)"),
                    vrc);
}


/**
 * Performs the storage attach operation in EMT.
 *
 * @returns VBox status code.
 *
 * @param   pThis           Pointer to the Console object.
 * @param   pVM             The VM handle.
 * @param   pcszDevice      The PDM device name.
 * @param   uInstance       The PDM device instance.
 *
 * @thread  EMT
 */
DECLCALLBACK(int) Console::attachStorageDevice(Console *pConsole,
                                               PVM pVM,
                                               const char *pcszDevice,
                                               unsigned uInstance,
                                               StorageBus_T enmBus,
                                               bool fUseHostIOCache,
                                               IMediumAttachment *aMediumAtt)
{
    LogFlowFunc(("pConsole=%p uInstance=%u pszDevice=%p:{%s} enmBus=%u, aMediumAtt=%p\n",
                 pConsole, uInstance, pcszDevice, pcszDevice, enmBus, aMediumAtt));

    AssertReturn(pConsole, VERR_INVALID_PARAMETER);

    AutoCaller autoCaller(pConsole);
    AssertComRCReturn(autoCaller.rc(), VERR_ACCESS_DENIED);

    /*
     * Suspend the VM first.
     *
     * The VM must not be running since it might have pending I/O to
     * the drive which is being changed.
     */
    bool fResume;
    VMSTATE enmVMState = VMR3GetState(pVM);
    switch (enmVMState)
    {
        case VMSTATE_RESETTING:
        case VMSTATE_RUNNING:
        {
            LogFlowFunc(("Suspending the VM...\n"));
            /* disable the callback to prevent Console-level state change */
            pConsole->mVMStateChangeCallbackDisabled = true;
            int rc = VMR3Suspend(pVM);
            pConsole->mVMStateChangeCallbackDisabled = false;
            AssertRCReturn(rc, rc);
            fResume = true;
            break;
        }

        case VMSTATE_SUSPENDED:
        case VMSTATE_CREATED:
        case VMSTATE_OFF:
            fResume = false;
            break;

        case VMSTATE_RUNNING_LS:
        case VMSTATE_RUNNING_FT:
            return setErrorInternal(VBOX_E_INVALID_VM_STATE,
                                    COM_IIDOF(IConsole),
                                    getStaticComponentName(),
                                    (enmVMState == VMSTATE_RUNNING_LS) ? Utf8Str(tr("Cannot change drive during live migration")) : Utf8Str(tr("Cannot change drive during fault tolerant syncing")),
                                    false /*aWarning*/,
                                    true /*aLogIt*/);

        default:
            AssertMsgFailedReturn(("enmVMState=%d\n", enmVMState), VERR_ACCESS_DENIED);
    }

    /* Determine the base path for the device instance. */
    PCFGMNODE pCtlInst;
    pCtlInst = CFGMR3GetChildF(CFGMR3GetRoot(pVM), "Devices/%s/%u/", pcszDevice, uInstance);
    AssertReturn(pCtlInst, VERR_INTERNAL_ERROR);

    int rc = VINF_SUCCESS;
    int rcRet = VINF_SUCCESS;

    rcRet = pConsole->configMediumAttachment(pCtlInst,
                                             pcszDevice,
                                             uInstance,
                                             enmBus,
                                             fUseHostIOCache,
                                             false /* fSetupMerge */,
                                             false /* fBuiltinIoCache */,
                                             0 /* uMergeSource */,
                                             0 /* uMergeTarget */,
                                             aMediumAtt,
                                             pConsole->mMachineState,
                                             NULL /* phrc */,
                                             true /* fAttachDetach */,
                                             false /* fForceUnmount */,
                                             true   /* fHotplug */,
                                             pVM,
                                             NULL /* paLedDevType */);
    /** @todo this dumps everything attached to this device instance, which
     * is more than necessary. Dumping the changed LUN would be enough. */
    CFGMR3Dump(pCtlInst);

    /*
     * Resume the VM if necessary.
     */
    if (fResume)
    {
        LogFlowFunc(("Resuming the VM...\n"));
        /* disable the callback to prevent Console-level state change */
        pConsole->mVMStateChangeCallbackDisabled = true;
        rc = VMR3Resume(pVM);
        pConsole->mVMStateChangeCallbackDisabled = false;
        AssertRC(rc);
        if (RT_FAILURE(rc))
        {
            /* too bad, we failed. try to sync the console state with the VMM state */
            vmstateChangeCallback(pVM, VMSTATE_SUSPENDED, enmVMState, pConsole);
        }
        /** @todo: if we failed with drive mount, then the VMR3Resume
         * error (if any) will be hidden from the caller. For proper reporting
         * of such multiple errors to the caller we need to enhance the
         * IVirtualBoxError interface. For now, give the first error the higher
         * priority.
         */
        if (RT_SUCCESS(rcRet))
            rcRet = rc;
    }

    LogFlowFunc(("Returning %Rrc\n", rcRet));
    return rcRet;
}

/**
 * Attach a new storage device to the VM.
 *
 * @param aMediumAttachment The medium attachment which is added.
 * @param pVM               Safe VM handle.
 *
 * @note Locks this object for writing.
 */
HRESULT Console::doStorageDeviceDetach(IMediumAttachment *aMediumAttachment, PVM pVM)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    /* We will need to release the write lock before calling EMT */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;
    const char *pszDevice = NULL;

    SafeIfaceArray<IStorageController> ctrls;
    rc = mMachine->COMGETTER(StorageControllers)(ComSafeArrayAsOutParam(ctrls));
    AssertComRC(rc);
    IMedium *pMedium;
    rc = aMediumAttachment->COMGETTER(Medium)(&pMedium);
    AssertComRC(rc);
    Bstr mediumLocation;
    if (pMedium)
    {
        rc = pMedium->COMGETTER(Location)(mediumLocation.asOutParam());
        AssertComRC(rc);
    }

    Bstr attCtrlName;
    rc = aMediumAttachment->COMGETTER(Controller)(attCtrlName.asOutParam());
    AssertComRC(rc);
    ComPtr<IStorageController> pStorageController;
    for (size_t i = 0; i < ctrls.size(); ++i)
    {
        Bstr ctrlName;
        rc = ctrls[i]->COMGETTER(Name)(ctrlName.asOutParam());
        AssertComRC(rc);
        if (attCtrlName == ctrlName)
        {
            pStorageController = ctrls[i];
            break;
        }
    }
    if (pStorageController.isNull())
        return setError(E_FAIL,
                        tr("Could not find storage controller '%ls'"), attCtrlName.raw());

    StorageControllerType_T enmCtrlType;
    rc = pStorageController->COMGETTER(ControllerType)(&enmCtrlType);
    AssertComRC(rc);
    pszDevice = convertControllerTypeToDev(enmCtrlType);

    StorageBus_T enmBus;
    rc = pStorageController->COMGETTER(Bus)(&enmBus);
    AssertComRC(rc);
    ULONG uInstance;
    rc = pStorageController->COMGETTER(Instance)(&uInstance);
    AssertComRC(rc);

    /*
     * Call worker in EMT, that's faster and safer than doing everything
     * using VMR3ReqCall. Note that we separate VMR3ReqCall from VMR3ReqWait
     * here to make requests from under the lock in order to serialize them.
     */
    PVMREQ pReq;
    int vrc = VMR3ReqCall(pVM,
                          VMCPUID_ANY,
                          &pReq,
                          0 /* no wait! */,
                          VMREQFLAGS_VBOX_STATUS,
                          (PFNRT)Console::detachStorageDevice,
                          6,
                          this,
                          pVM,
                          pszDevice,
                          uInstance,
                          enmBus,
                          aMediumAttachment);

    /* leave the lock before waiting for a result (EMT will call us back!) */
    alock.leave();

    if (vrc == VERR_TIMEOUT || RT_SUCCESS(vrc))
    {
        vrc = VMR3ReqWait(pReq, RT_INDEFINITE_WAIT);
        AssertRC(vrc);
        if (RT_SUCCESS(vrc))
            vrc = pReq->iStatus;
    }
    VMR3ReqFree(pReq);

    if (RT_SUCCESS(vrc))
    {
        LogFlowThisFunc(("Returns S_OK\n"));
        return S_OK;
    }

    if (!pMedium)
        return setError(E_FAIL,
                        tr("Could not mount the media/drive '%ls' (%Rrc)"),
                        mediumLocation.raw(), vrc);

    return setError(E_FAIL,
                    tr("Could not unmount the currently mounted media/drive (%Rrc)"),
                    vrc);
}

/**
 * Performs the storage detach operation in EMT.
 *
 * @returns VBox status code.
 *
 * @param   pThis           Pointer to the Console object.
 * @param   pVM             The VM handle.
 * @param   pcszDevice      The PDM device name.
 * @param   uInstance       The PDM device instance.
 *
 * @thread  EMT
 */
DECLCALLBACK(int) Console::detachStorageDevice(Console *pConsole,
                                               PVM pVM,
                                               const char *pcszDevice,
                                               unsigned uInstance,
                                               StorageBus_T enmBus,
                                               IMediumAttachment *pMediumAtt)
{
    LogFlowFunc(("pConsole=%p uInstance=%u pszDevice=%p:{%s} enmBus=%u, pMediumAtt=%p\n",
                 pConsole, uInstance, pcszDevice, pcszDevice, enmBus, pMediumAtt));

    AssertReturn(pConsole, VERR_INVALID_PARAMETER);

    AutoCaller autoCaller(pConsole);
    AssertComRCReturn(autoCaller.rc(), VERR_ACCESS_DENIED);

    /*
     * Suspend the VM first.
     *
     * The VM must not be running since it might have pending I/O to
     * the drive which is being changed.
     */
    bool fResume;
    VMSTATE enmVMState = VMR3GetState(pVM);
    switch (enmVMState)
    {
        case VMSTATE_RESETTING:
        case VMSTATE_RUNNING:
        {
            LogFlowFunc(("Suspending the VM...\n"));
            /* disable the callback to prevent Console-level state change */
            pConsole->mVMStateChangeCallbackDisabled = true;
            int rc = VMR3Suspend(pVM);
            pConsole->mVMStateChangeCallbackDisabled = false;
            AssertRCReturn(rc, rc);
            fResume = true;
            break;
        }

        case VMSTATE_SUSPENDED:
        case VMSTATE_CREATED:
        case VMSTATE_OFF:
            fResume = false;
            break;

        case VMSTATE_RUNNING_LS:
        case VMSTATE_RUNNING_FT:
            return setErrorInternal(VBOX_E_INVALID_VM_STATE,
                                    COM_IIDOF(IConsole),
                                    getStaticComponentName(),
                                    (enmVMState == VMSTATE_RUNNING_LS) ? Utf8Str(tr("Cannot change drive during live migration")) : Utf8Str(tr("Cannot change drive during fault tolerant syncing")),
                                    false /*aWarning*/,
                                    true /*aLogIt*/);

        default:
            AssertMsgFailedReturn(("enmVMState=%d\n", enmVMState), VERR_ACCESS_DENIED);
    }

    /* Determine the base path for the device instance. */
    PCFGMNODE pCtlInst;
    pCtlInst = CFGMR3GetChildF(CFGMR3GetRoot(pVM), "Devices/%s/%u/", pcszDevice, uInstance);
    AssertReturn(pCtlInst, VERR_INTERNAL_ERROR);

#define H()         AssertMsgReturn(!FAILED(hrc), ("hrc=%Rhrc\n", hrc), VERR_GENERAL_FAILURE)

    HRESULT hrc;
    int rc = VINF_SUCCESS;
    int rcRet = VINF_SUCCESS;
    unsigned uLUN;
    LONG lDev;
    LONG lPort;
    DeviceType_T lType;
    PCFGMNODE pLunL0 = NULL;
    PCFGMNODE pCfg = NULL;

    hrc = pMediumAtt->COMGETTER(Device)(&lDev);                             H();
    hrc = pMediumAtt->COMGETTER(Port)(&lPort);                              H();
    hrc = pMediumAtt->COMGETTER(Type)(&lType);                              H();
    hrc = Console::convertBusPortDeviceToLun(enmBus, lPort, lDev, uLUN);    H();

#undef H

    /* First check if the LUN really exists. */
    pLunL0 = CFGMR3GetChildF(pCtlInst, "LUN#%u", uLUN);
    if (pLunL0)
    {
        rc = PDMR3DeviceDetach(pVM, pcszDevice, uInstance, uLUN, 0);
        if (rc == VERR_PDM_NO_DRIVER_ATTACHED_TO_LUN)
            rc = VINF_SUCCESS;
        AssertRCReturn(rc, rc);
        CFGMR3RemoveNode(pLunL0);

        Utf8Str devicePath = Utf8StrFmt("%s/%u/LUN#%u", pcszDevice, uInstance, uLUN);
        pConsole->mapMediumAttachments.erase(devicePath);

    }
    else
        AssertFailedReturn(VERR_INTERNAL_ERROR);

    CFGMR3Dump(pCtlInst);

    /*
     * Resume the VM if necessary.
     */
    if (fResume)
    {
        LogFlowFunc(("Resuming the VM...\n"));
        /* disable the callback to prevent Console-level state change */
        pConsole->mVMStateChangeCallbackDisabled = true;
        rc = VMR3Resume(pVM);
        pConsole->mVMStateChangeCallbackDisabled = false;
        AssertRC(rc);
        if (RT_FAILURE(rc))
        {
            /* too bad, we failed. try to sync the console state with the VMM state */
            vmstateChangeCallback(pVM, VMSTATE_SUSPENDED, enmVMState, pConsole);
        }
        /** @todo: if we failed with drive mount, then the VMR3Resume
         * error (if any) will be hidden from the caller. For proper reporting
         * of such multiple errors to the caller we need to enhance the
         * IVirtualBoxError interface. For now, give the first error the higher
         * priority.
         */
        if (RT_SUCCESS(rcRet))
            rcRet = rc;
    }

    LogFlowFunc(("Returning %Rrc\n", rcRet));
    return rcRet;
}

/**
 * Called by IInternalSessionControl::OnNetworkAdapterChange().
 *
 * @note Locks this object for writing.
 */
HRESULT Console::onNetworkAdapterChange(INetworkAdapter *aNetworkAdapter, BOOL changeAdapter)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    /* don't trigger network change if the VM isn't running */
    SafeVMPtrQuiet ptrVM(this);
    if (ptrVM.isOk())
    {
        /* Get the properties we need from the adapter */
        BOOL fCableConnected, fTraceEnabled;
        rc = aNetworkAdapter->COMGETTER(CableConnected)(&fCableConnected);
        AssertComRC(rc);
        if (SUCCEEDED(rc))
        {
            rc = aNetworkAdapter->COMGETTER(TraceEnabled)(&fTraceEnabled);
            AssertComRC(rc);
        }
        if (SUCCEEDED(rc))
        {
            ULONG ulInstance;
            rc = aNetworkAdapter->COMGETTER(Slot)(&ulInstance);
            AssertComRC(rc);
            if (SUCCEEDED(rc))
            {
                /*
                 * Find the adapter instance, get the config interface and update
                 * the link state.
                 */
                NetworkAdapterType_T adapterType;
                rc = aNetworkAdapter->COMGETTER(AdapterType)(&adapterType);
                AssertComRC(rc);
                const char *pszAdapterName = networkAdapterTypeToName(adapterType);
                PPDMIBASE pBase;
                int vrc = PDMR3QueryDeviceLun(ptrVM, pszAdapterName, ulInstance, 0, &pBase);
                if (RT_SUCCESS(vrc))
                {
                    Assert(pBase);
                    PPDMINETWORKCONFIG pINetCfg;
                    pINetCfg = PDMIBASE_QUERY_INTERFACE(pBase, PDMINETWORKCONFIG);
                    if (pINetCfg)
                    {
                        Log(("Console::onNetworkAdapterChange: setting link state to %d\n",
                              fCableConnected));
                        vrc = pINetCfg->pfnSetLinkState(pINetCfg,
                                                        fCableConnected ? PDMNETWORKLINKSTATE_UP
                                                                        : PDMNETWORKLINKSTATE_DOWN);
                        ComAssertRC(vrc);
                    }
                    if (RT_SUCCESS(vrc) && changeAdapter)
                    {
                        VMSTATE enmVMState = VMR3GetState(ptrVM);
                        if (    enmVMState == VMSTATE_RUNNING    /** @todo LiveMigration: Forbid or deal correctly with the _LS variants */
                            ||  enmVMState == VMSTATE_SUSPENDED)
                        {
                            if (fTraceEnabled && fCableConnected && pINetCfg)
                            {
                                vrc = pINetCfg->pfnSetLinkState(pINetCfg, PDMNETWORKLINKSTATE_DOWN);
                                ComAssertRC(vrc);
                            }

                            rc = doNetworkAdapterChange(ptrVM, pszAdapterName, ulInstance, 0, aNetworkAdapter);

                            if (fTraceEnabled && fCableConnected && pINetCfg)
                            {
                                vrc = pINetCfg->pfnSetLinkState(pINetCfg, PDMNETWORKLINKSTATE_UP);
                                ComAssertRC(vrc);
                            }
                        }
                    }
                }
                else if (vrc == VERR_PDM_DEVICE_INSTANCE_NOT_FOUND)
                    return setError(E_FAIL,
                            tr("The network adapter #%u is not enabled"), ulInstance);
                else
                    ComAssertRC(vrc);

                if (RT_FAILURE(vrc))
                    rc = E_FAIL;
            }
        }
        ptrVM.release();
    }

    /* notify console callbacks on success */
    if (SUCCEEDED(rc))
        fireNetworkAdapterChangedEvent(mEventSource, aNetworkAdapter);

    LogFlowThisFunc(("Leaving rc=%#x\n", rc));
    return rc;
}

/**
 * Called by IInternalSessionControl::OnNATEngineChange().
 *
 * @note Locks this object for writing.
 */
HRESULT Console::onNATRedirectRuleChange(ULONG ulInstance, BOOL aNatRuleRemove,
                                         NATProtocol_T aProto, IN_BSTR aHostIp, LONG aHostPort, IN_BSTR aGuestIp, LONG aGuestPort)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    /* don't trigger nat engine change if the VM isn't running */
    SafeVMPtrQuiet ptrVM(this);
    if (ptrVM.isOk())
    {
        do
        {
            ComPtr<INetworkAdapter> pNetworkAdapter;
            rc = machine()->GetNetworkAdapter(ulInstance, pNetworkAdapter.asOutParam());
            if (   FAILED(rc)
                || pNetworkAdapter.isNull())
                break;

            /*
             * Find the adapter instance, get the config interface and update
             * the link state.
             */
            NetworkAdapterType_T adapterType;
            rc = pNetworkAdapter->COMGETTER(AdapterType)(&adapterType);
            if (FAILED(rc))
            {
                AssertComRC(rc);
                rc = E_FAIL;
                break;
            }

            const char *pszAdapterName = networkAdapterTypeToName(adapterType);
            PPDMIBASE pBase;
            int vrc = PDMR3QueryLun(ptrVM, pszAdapterName, ulInstance, 0, &pBase);
            if (RT_FAILURE(vrc))
            {
                ComAssertRC(vrc);
                rc = E_FAIL;
                break;
            }

            NetworkAttachmentType_T attachmentType;
            rc = pNetworkAdapter->COMGETTER(AttachmentType)(&attachmentType);
            if (   FAILED(rc)
                || attachmentType != NetworkAttachmentType_NAT)
            {
                rc = E_FAIL;
                break;
            }

            /* look down for PDMINETWORKNATCONFIG interface */
            PPDMINETWORKNATCONFIG pNetNatCfg = NULL;
            while (pBase)
            {
                pNetNatCfg = (PPDMINETWORKNATCONFIG)pBase->pfnQueryInterface(pBase, PDMINETWORKNATCONFIG_IID);
                if (pNetNatCfg)
                    break;
                /** @todo r=bird: This stinks! */
                PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pBase);
                pBase = pDrvIns->pDownBase;
            }
            if (!pNetNatCfg)
                break;

            bool fUdp = aProto == NATProtocol_UDP;
            vrc = pNetNatCfg->pfnRedirectRuleCommand(pNetNatCfg, aNatRuleRemove, fUdp,
                                                     Utf8Str(aHostIp).c_str(), aHostPort, Utf8Str(aGuestIp).c_str(),
                                                     aGuestPort);
            if (RT_FAILURE(vrc))
                rc = E_FAIL;
        } while (0); /* break loop */
        ptrVM.release();
    }

    LogFlowThisFunc(("Leaving rc=%#x\n", rc));
    return rc;
}


/**
 * Process a network adaptor change.
 *
 * @returns COM status code.
 *
 * @parma   pVM                 The VM handle (caller hold this safely).
 * @param   pszDevice           The PDM device name.
 * @param   uInstance           The PDM device instance.
 * @param   uLun                The PDM LUN number of the drive.
 * @param   aNetworkAdapter     The network adapter whose attachment needs to be changed
 *
 * @note Locks this object for writing.
 */
HRESULT Console::doNetworkAdapterChange(PVM pVM,
                                        const char *pszDevice,
                                        unsigned uInstance,
                                        unsigned uLun,
                                        INetworkAdapter *aNetworkAdapter)
{
    LogFlowThisFunc(("pszDevice=%p:{%s} uInstance=%u uLun=%u aNetworkAdapter=%p\n",
                      pszDevice, pszDevice, uInstance, uLun, aNetworkAdapter));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    /* We will need to release the write lock before calling EMT */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Get the VM handle. */
    SafeVMPtr ptrVM(this);
    if (!ptrVM.isOk())
        return ptrVM.rc();

    /*
     * Call worker in EMT, that's faster and safer than doing everything
     * using VM3ReqCall. Note that we separate VMR3ReqCall from VMR3ReqWait
     * here to make requests from under the lock in order to serialize them.
     */
    PVMREQ pReq;
    int vrc = VMR3ReqCall(pVM, 0 /*idDstCpu*/, &pReq, 0 /* no wait! */, VMREQFLAGS_VBOX_STATUS,
                          (PFNRT) Console::changeNetworkAttachment, 6,
                          this, ptrVM.raw(), pszDevice, uInstance, uLun, aNetworkAdapter);

    /* leave the lock before waiting for a result (EMT will call us back!) */
    alock.leave();

    if (vrc == VERR_TIMEOUT || RT_SUCCESS(vrc))
    {
        vrc = VMR3ReqWait(pReq, RT_INDEFINITE_WAIT);
        AssertRC(vrc);
        if (RT_SUCCESS(vrc))
            vrc = pReq->iStatus;
    }
    VMR3ReqFree(pReq);

    if (RT_SUCCESS(vrc))
    {
        LogFlowThisFunc(("Returns S_OK\n"));
        return S_OK;
    }

    return setError(E_FAIL,
                    tr("Could not change the network adaptor attachement type (%Rrc)"),
                    vrc);
}


/**
 * Performs the Network Adaptor change in EMT.
 *
 * @returns VBox status code.
 *
 * @param   pThis               Pointer to the Console object.
 * @param   pVM                 The VM handle.
 * @param   pszDevice           The PDM device name.
 * @param   uInstance           The PDM device instance.
 * @param   uLun                The PDM LUN number of the drive.
 * @param   aNetworkAdapter     The network adapter whose attachment needs to be changed
 *
 * @thread  EMT
 * @note Locks the Console object for writing.
 */
DECLCALLBACK(int) Console::changeNetworkAttachment(Console *pThis,
                                                   PVM pVM,
                                                   const char *pszDevice,
                                                   unsigned uInstance,
                                                   unsigned uLun,
                                                   INetworkAdapter *aNetworkAdapter)
{
    LogFlowFunc(("pThis=%p pszDevice=%p:{%s} uInstance=%u uLun=%u aNetworkAdapter=%p\n",
                 pThis, pszDevice, pszDevice, uInstance, uLun, aNetworkAdapter));

    AssertReturn(pThis, VERR_INVALID_PARAMETER);

    AutoCaller autoCaller(pThis);
    AssertComRCReturn(autoCaller.rc(), VERR_ACCESS_DENIED);

    ComPtr<IVirtualBox> pVirtualBox;
    pThis->mMachine->COMGETTER(Parent)(pVirtualBox.asOutParam());
    ComPtr<ISystemProperties> pSystemProperties;
    if (pVirtualBox)
        pVirtualBox->COMGETTER(SystemProperties)(pSystemProperties.asOutParam());
    ChipsetType_T chipsetType = ChipsetType_PIIX3;
    pThis->mMachine->COMGETTER(ChipsetType)(&chipsetType);
    ULONG maxNetworkAdapters = 0;
    if (pSystemProperties)
        pSystemProperties->GetMaxNetworkAdapters(chipsetType, &maxNetworkAdapters);
    AssertMsg(   (   !strcmp(pszDevice, "pcnet")
                  || !strcmp(pszDevice, "e1000")
                  || !strcmp(pszDevice, "virtio-net"))
              && uLun == 0
              && uInstance < maxNetworkAdapters,
              ("pszDevice=%s uLun=%d uInstance=%d\n", pszDevice, uLun, uInstance));
    Log(("pszDevice=%s uLun=%d uInstance=%d\n", pszDevice, uLun, uInstance));

    /*
     * Suspend the VM first.
     *
     * The VM must not be running since it might have pending I/O to
     * the drive which is being changed.
     */
    bool fResume;
    VMSTATE enmVMState = VMR3GetState(pVM);
    switch (enmVMState)
    {
        case VMSTATE_RESETTING:
        case VMSTATE_RUNNING:
        {
            LogFlowFunc(("Suspending the VM...\n"));
            /* disable the callback to prevent Console-level state change */
            pThis->mVMStateChangeCallbackDisabled = true;
            int rc = VMR3Suspend(pVM);
            pThis->mVMStateChangeCallbackDisabled = false;
            AssertRCReturn(rc, rc);
            fResume = true;
            break;
        }

        case VMSTATE_SUSPENDED:
        case VMSTATE_CREATED:
        case VMSTATE_OFF:
            fResume = false;
            break;

        default:
            AssertLogRelMsgFailedReturn(("enmVMState=%d\n", enmVMState), VERR_ACCESS_DENIED);
    }

    int rc = VINF_SUCCESS;
    int rcRet = VINF_SUCCESS;

    PCFGMNODE pCfg = NULL;          /* /Devices/Dev/.../Config/ */
    PCFGMNODE pLunL0 = NULL;        /* /Devices/Dev/0/LUN#0/ */
    PCFGMNODE pInst = CFGMR3GetChildF(CFGMR3GetRoot(pVM), "Devices/%s/%d/", pszDevice, uInstance);
    AssertRelease(pInst);

    rcRet = pThis->configNetwork(pszDevice, uInstance, uLun, aNetworkAdapter, pCfg, pLunL0, pInst,
                                 true /*fAttachDetach*/, false /*fIgnoreConnectFailure*/);

    /*
     * Resume the VM if necessary.
     */
    if (fResume)
    {
        LogFlowFunc(("Resuming the VM...\n"));
        /* disable the callback to prevent Console-level state change */
        pThis->mVMStateChangeCallbackDisabled = true;
        rc = VMR3Resume(pVM);
        pThis->mVMStateChangeCallbackDisabled = false;
        AssertRC(rc);
        if (RT_FAILURE(rc))
        {
            /* too bad, we failed. try to sync the console state with the VMM state */
            vmstateChangeCallback(pVM, VMSTATE_SUSPENDED, enmVMState, pThis);
        }
        /// @todo (r=dmik) if we failed with drive mount, then the VMR3Resume
        // error (if any) will be hidden from the caller. For proper reporting
        // of such multiple errors to the caller we need to enhance the
        // IVirtualBoxError interface. For now, give the first error the higher
        // priority.
        if (RT_SUCCESS(rcRet))
            rcRet = rc;
    }

    LogFlowFunc(("Returning %Rrc\n", rcRet));
    return rcRet;
}


/**
 * Called by IInternalSessionControl::OnSerialPortChange().
 *
 * @note Locks this object for writing.
 */
HRESULT Console::onSerialPortChange(ISerialPort *aSerialPort)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    /* don't trigger serial port change if the VM isn't running */
    SafeVMPtrQuiet ptrVM(this);
    if (ptrVM.isOk())
    {
        /* nothing to do so far */
        ptrVM.release();
    }

    /* notify console callbacks on success */
    if (SUCCEEDED(rc))
        fireSerialPortChangedEvent(mEventSource, aSerialPort);

    LogFlowThisFunc(("Leaving rc=%#x\n", rc));
    return rc;
}

/**
 * Called by IInternalSessionControl::OnParallelPortChange().
 *
 * @note Locks this object for writing.
 */
HRESULT Console::onParallelPortChange(IParallelPort *aParallelPort)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    /* don't trigger parallel port change if the VM isn't running */
    SafeVMPtrQuiet ptrVM(this);
    if (ptrVM.isOk())
    {
        /* nothing to do so far */
        ptrVM.release();
    }

    /* notify console callbacks on success */
    if (SUCCEEDED(rc))
        fireParallelPortChangedEvent(mEventSource, aParallelPort);

    LogFlowThisFunc(("Leaving rc=%#x\n", rc));
    return rc;
}

/**
 * Called by IInternalSessionControl::OnStorageControllerChange().
 *
 * @note Locks this object for writing.
 */
HRESULT Console::onStorageControllerChange()
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    /* don't trigger storage controller change if the VM isn't running */
    SafeVMPtrQuiet ptrVM(this);
    if (ptrVM.isOk())
    {
        /* nothing to do so far */
        ptrVM.release();
    }

    /* notify console callbacks on success */
    if (SUCCEEDED(rc))
        fireStorageControllerChangedEvent(mEventSource);

    LogFlowThisFunc(("Leaving rc=%#x\n", rc));
    return rc;
}

/**
 * Called by IInternalSessionControl::OnMediumChange().
 *
 * @note Locks this object for writing.
 */
HRESULT Console::onMediumChange(IMediumAttachment *aMediumAttachment, BOOL aForce)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    /* don't trigger medium change if the VM isn't running */
    SafeVMPtrQuiet ptrVM(this);
    if (ptrVM.isOk())
    {
        rc = doMediumChange(aMediumAttachment, !!aForce, ptrVM);
        ptrVM.release();
    }

    /* notify console callbacks on success */
    if (SUCCEEDED(rc))
        fireMediumChangedEvent(mEventSource, aMediumAttachment);

    LogFlowThisFunc(("Leaving rc=%#x\n", rc));
    return rc;
}

/**
 * Called by IInternalSessionControl::OnCPUChange().
 *
 * @note Locks this object for writing.
 */
HRESULT Console::onCPUChange(ULONG aCPU, BOOL aRemove)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    HRESULT rc = S_OK;

    /* don't trigger CPU change if the VM isn't running */
    SafeVMPtrQuiet ptrVM(this);
    if (ptrVM.isOk())
    {
        if (aRemove)
            rc = doCPURemove(aCPU, ptrVM);
        else
            rc = doCPUAdd(aCPU, ptrVM);
        ptrVM.release();
    }

    /* notify console callbacks on success */
    if (SUCCEEDED(rc))
        fireCPUChangedEvent(mEventSource, aCPU, aRemove);

    LogFlowThisFunc(("Leaving rc=%#x\n", rc));
    return rc;
}

/**
 * Called by IInternalSessionControl::OnCpuExecutionCapChange().
 *
 * @note Locks this object for writing.
 */
HRESULT Console::onCPUExecutionCapChange(ULONG aExecutionCap)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    /* don't trigger the CPU priority change if the VM isn't running */
    SafeVMPtrQuiet ptrVM(this);
    if (ptrVM.isOk())
    {
        if (   mMachineState == MachineState_Running
            || mMachineState == MachineState_Teleporting
            || mMachineState == MachineState_LiveSnapshotting
            )
        {
            /* No need to call in the EMT thread. */
            rc = VMR3SetCpuExecutionCap(ptrVM, aExecutionCap);
        }
        else
            rc = setInvalidMachineStateError();
        ptrVM.release();
    }

    /* notify console callbacks on success */
    if (SUCCEEDED(rc))
        fireCPUExecutionCapChangedEvent(mEventSource, aExecutionCap);

    LogFlowThisFunc(("Leaving rc=%#x\n", rc));
    return rc;
}

/**
 * Called by IInternalSessionControl::OnVRDEServerChange().
 *
 * @note Locks this object for writing.
 */
HRESULT Console::onVRDEServerChange(BOOL aRestart)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    if (    mVRDEServer
        &&  (   mMachineState == MachineState_Running
             || mMachineState == MachineState_Teleporting
             || mMachineState == MachineState_LiveSnapshotting
            )
       )
    {
        BOOL vrdpEnabled = FALSE;

        rc = mVRDEServer->COMGETTER(Enabled)(&vrdpEnabled);
        ComAssertComRCRetRC(rc);

        if (aRestart)
        {
            /* VRDP server may call this Console object back from other threads (VRDP INPUT or OUTPUT). */
            alock.leave();

            if (vrdpEnabled)
            {
                // If there was no VRDP server started the 'stop' will do nothing.
                // However if a server was started and this notification was called,
                // we have to restart the server.
                mConsoleVRDPServer->Stop();

                if (RT_FAILURE(mConsoleVRDPServer->Launch()))
                    rc = E_FAIL;
                else
                    mConsoleVRDPServer->EnableConnections();
            }
            else
            {
                mConsoleVRDPServer->Stop();
            }

            alock.enter();
        }
    }

    /* notify console callbacks on success */
    if (SUCCEEDED(rc))
        fireVRDEServerChangedEvent(mEventSource);

    return rc;
}

/**
 * @note Locks this object for reading.
 */
void Console::onVRDEServerInfoChange()
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    fireVRDEServerInfoChangedEvent(mEventSource);
}


/**
 * Called by IInternalSessionControl::OnUSBControllerChange().
 *
 * @note Locks this object for writing.
 */
HRESULT Console::onUSBControllerChange()
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    /* don't trigger USB controller change if the VM isn't running */
    SafeVMPtrQuiet ptrVM(this);
    if (ptrVM.isOk())
    {
        /// @todo implement one day.
        // Anyway, if we want to query the machine's USB Controller we need
        // to cache it to mUSBController in #init() (similar to mDVDDrive).
        //
        // bird: While the VM supports hot-plugging, I doubt any guest can
        // handle it at this time... :-)

        /* nothing to do so far */
        ptrVM.release();
    }

    /* notify console callbacks on success */
    if (SUCCEEDED(rc))
        fireUSBControllerChangedEvent(mEventSource);

    return rc;
}

/**
 * Called by IInternalSessionControl::OnSharedFolderChange().
 *
 * @note Locks this object for writing.
 */
HRESULT Console::onSharedFolderChange(BOOL aGlobal)
{
    LogFlowThisFunc(("aGlobal=%RTbool\n", aGlobal));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = fetchSharedFolders(aGlobal);

    /* notify console callbacks on success */
    if (SUCCEEDED(rc))
    {
        fireSharedFolderChangedEvent(mEventSource, aGlobal ? (Scope_T)Scope_Global : (Scope_T)Scope_Machine);
    }

    return rc;
}

/**
 * Called by IInternalSessionControl::OnUSBDeviceAttach() or locally by
 * processRemoteUSBDevices() after IInternalMachineControl::RunUSBDeviceFilters()
 * returns TRUE for a given remote USB device.
 *
 * @return S_OK if the device was attached to the VM.
 * @return failure if not attached.
 *
 * @param aDevice
 *     The device in question.
 * @param aMaskedIfs
 *     The interfaces to hide from the guest.
 *
 * @note Locks this object for writing.
 */
HRESULT Console::onUSBDeviceAttach(IUSBDevice *aDevice, IVirtualBoxErrorInfo *aError, ULONG aMaskedIfs)
{
#ifdef VBOX_WITH_USB
    LogFlowThisFunc(("aDevice=%p aError=%p\n", aDevice, aError));

    AutoCaller autoCaller(this);
    ComAssertComRCRetRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Get the VM pointer (we don't need error info, since it's a callback). */
    SafeVMPtrQuiet ptrVM(this);
    if (!ptrVM.isOk())
    {
        /* The VM may be no more operational when this message arrives
         * (e.g. it may be Saving or Stopping or just PoweredOff) --
         * autoVMCaller.rc() will return a failure in this case. */
        LogFlowThisFunc(("Attach request ignored (mMachineState=%d).\n",
                          mMachineState));
        return ptrVM.rc();
    }

    if (aError != NULL)
    {
        /* notify callbacks about the error */
        onUSBDeviceStateChange(aDevice, true /* aAttached */, aError);
        return S_OK;
    }

    /* Don't proceed unless there's at least one USB hub. */
    if (!PDMR3USBHasHub(ptrVM))
    {
        LogFlowThisFunc(("Attach request ignored (no USB controller).\n"));
        return E_FAIL;
    }

    HRESULT rc = attachUSBDevice(aDevice, aMaskedIfs);
    if (FAILED(rc))
    {
        /* take the current error info */
        com::ErrorInfoKeeper eik;
        /* the error must be a VirtualBoxErrorInfo instance */
        ComPtr<IVirtualBoxErrorInfo> pError = eik.takeError();
        Assert(!pError.isNull());
        if (!pError.isNull())
        {
            /* notify callbacks about the error */
            onUSBDeviceStateChange(aDevice, true /* aAttached */, pError);
        }
    }

    return rc;

#else   /* !VBOX_WITH_USB */
    return E_FAIL;
#endif  /* !VBOX_WITH_USB */
}

/**
 * Called by IInternalSessionControl::OnUSBDeviceDetach() and locally by
 * processRemoteUSBDevices().
 *
 * @note Locks this object for writing.
 */
HRESULT Console::onUSBDeviceDetach(IN_BSTR aId,
                                   IVirtualBoxErrorInfo *aError)
{
#ifdef VBOX_WITH_USB
    Guid Uuid(aId);
    LogFlowThisFunc(("aId={%RTuuid} aError=%p\n", Uuid.raw(), aError));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Find the device. */
    ComObjPtr<OUSBDevice> pUSBDevice;
    USBDeviceList::iterator it = mUSBDevices.begin();
    while (it != mUSBDevices.end())
    {
        LogFlowThisFunc(("it={%RTuuid}\n", (*it)->id().raw()));
        if ((*it)->id() == Uuid)
        {
            pUSBDevice = *it;
            break;
        }
        ++it;
    }


    if (pUSBDevice.isNull())
    {
        LogFlowThisFunc(("USB device not found.\n"));

        /* The VM may be no more operational when this message arrives
         * (e.g. it may be Saving or Stopping or just PoweredOff). Use
         * AutoVMCaller to detect it -- AutoVMCaller::rc() will return a
         * failure in this case. */

        AutoVMCallerQuiet autoVMCaller(this);
        if (FAILED(autoVMCaller.rc()))
        {
            LogFlowThisFunc(("Detach request ignored (mMachineState=%d).\n",
                              mMachineState));
            return autoVMCaller.rc();
        }

        /* the device must be in the list otherwise */
        AssertFailedReturn(E_FAIL);
    }

    if (aError != NULL)
    {
        /* notify callback about an error */
        onUSBDeviceStateChange(pUSBDevice, false /* aAttached */, aError);
        return S_OK;
    }

    HRESULT rc = detachUSBDevice(it);

    if (FAILED(rc))
    {
        /* take the current error info */
        com::ErrorInfoKeeper eik;
        /* the error must be a VirtualBoxErrorInfo instance */
        ComPtr<IVirtualBoxErrorInfo> pError = eik.takeError();
        Assert(!pError.isNull());
        if (!pError.isNull())
        {
            /* notify callbacks about the error */
            onUSBDeviceStateChange(pUSBDevice, false /* aAttached */, pError);
        }
    }

    return rc;

#else   /* !VBOX_WITH_USB */
    return E_FAIL;
#endif  /* !VBOX_WITH_USB */
}

/**
 * Called by IInternalSessionControl::OnBandwidthGroupChange().
 *
 * @note Locks this object for writing.
 */
HRESULT Console::onBandwidthGroupChange(IBandwidthGroup *aBandwidthGroup)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    /* don't trigger the CPU priority change if the VM isn't running */
    SafeVMPtrQuiet ptrVM(this);
    if (ptrVM.isOk())
    {
        if (   mMachineState == MachineState_Running
            || mMachineState == MachineState_Teleporting
            || mMachineState == MachineState_LiveSnapshotting
            )
        {
            /* No need to call in the EMT thread. */
            ULONG cMax;
            Bstr strName;
            rc = aBandwidthGroup->COMGETTER(Name)(strName.asOutParam());
            if (SUCCEEDED(rc))
                rc = aBandwidthGroup->COMGETTER(MaxMbPerSec)(&cMax);

            if (SUCCEEDED(rc))
            {
                int vrc;
                vrc = PDMR3AsyncCompletionBwMgrSetMaxForFile(ptrVM, Utf8Str(strName).c_str(),
                                                             cMax * _1M);
                AssertRC(vrc);
            }
        }
        else
            rc = setInvalidMachineStateError();
        ptrVM.release();
    }

    /* notify console callbacks on success */
    if (SUCCEEDED(rc))
        fireBandwidthGroupChangedEvent(mEventSource, aBandwidthGroup);

    LogFlowThisFunc(("Leaving rc=%#x\n", rc));
    return rc;
}

/**
 * Called by IInternalSessionControl::OnStorageDeviceChange().
 *
 * @note Locks this object for writing.
 */
HRESULT Console::onStorageDeviceChange(IMediumAttachment *aMediumAttachment, BOOL aRemove)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    /* don't trigger medium change if the VM isn't running */
    SafeVMPtrQuiet ptrVM(this);
    if (ptrVM.isOk())
    {
        if (aRemove)
            rc = doStorageDeviceDetach(aMediumAttachment, ptrVM);
        else
            rc = doStorageDeviceAttach(aMediumAttachment, ptrVM);
        ptrVM.release();
    }

    /* notify console callbacks on success */
    if (SUCCEEDED(rc))
        fireStorageDeviceChangedEvent(mEventSource, aMediumAttachment, aRemove);

    LogFlowThisFunc(("Leaving rc=%#x\n", rc));
    return rc;
}

/**
 * @note Temporarily locks this object for writing.
 */
HRESULT Console::getGuestProperty(IN_BSTR aName, BSTR *aValue,
                                  LONG64 *aTimestamp, BSTR *aFlags)
{
#ifndef VBOX_WITH_GUEST_PROPS
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_PROPS */
    if (!VALID_PTR(aName))
        return E_INVALIDARG;
    if (!VALID_PTR(aValue))
        return E_POINTER;
    if ((aTimestamp != NULL) && !VALID_PTR(aTimestamp))
        return E_POINTER;
    if ((aFlags != NULL) && !VALID_PTR(aFlags))
        return E_POINTER;

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    /* protect mpVM (if not NULL) */
    AutoVMCallerWeak autoVMCaller(this);
    if (FAILED(autoVMCaller.rc())) return autoVMCaller.rc();

    /* Note: validity of mVMMDev which is bound to uninit() is guaranteed by
     * autoVMCaller, so there is no need to hold a lock of this */

    HRESULT rc = E_UNEXPECTED;
    using namespace guestProp;

    try
    {
        VBOXHGCMSVCPARM parm[4];
        Utf8Str Utf8Name = aName;
        char szBuffer[MAX_VALUE_LEN + MAX_FLAGS_LEN];

        parm[0].type = VBOX_HGCM_SVC_PARM_PTR;
        parm[0].u.pointer.addr = (void*)Utf8Name.c_str();
        /* The + 1 is the null terminator */
        parm[0].u.pointer.size = (uint32_t)Utf8Name.length() + 1;
        parm[1].type = VBOX_HGCM_SVC_PARM_PTR;
        parm[1].u.pointer.addr = szBuffer;
        parm[1].u.pointer.size = sizeof(szBuffer);
        int vrc = m_pVMMDev->hgcmHostCall("VBoxGuestPropSvc", GET_PROP_HOST,
                                          4, &parm[0]);
        /* The returned string should never be able to be greater than our buffer */
        AssertLogRel(vrc != VERR_BUFFER_OVERFLOW);
        AssertLogRel(RT_FAILURE(vrc) || VBOX_HGCM_SVC_PARM_64BIT == parm[2].type);
        if (RT_SUCCESS(vrc) || (VERR_NOT_FOUND == vrc))
        {
            rc = S_OK;
            if (vrc != VERR_NOT_FOUND)
            {
                Utf8Str strBuffer(szBuffer);
                strBuffer.cloneTo(aValue);

                if (aTimestamp)
                    *aTimestamp = parm[2].u.uint64;

                if (aFlags)
                {
                    size_t iFlags = strBuffer.length() + 1;
                    Utf8Str(szBuffer + iFlags).cloneTo(aFlags);
                }
            }
            else
                aValue = NULL;
        }
        else
            rc = setError(E_UNEXPECTED,
                tr("The service call failed with the error %Rrc"),
                vrc);
    }
    catch(std::bad_alloc & /*e*/)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
#endif /* VBOX_WITH_GUEST_PROPS */
}

/**
 * @note Temporarily locks this object for writing.
 */
HRESULT Console::setGuestProperty(IN_BSTR aName, IN_BSTR aValue, IN_BSTR aFlags)
{
#ifndef VBOX_WITH_GUEST_PROPS
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_PROPS */
    if (!VALID_PTR(aName))
        return E_INVALIDARG;
    if ((aValue != NULL) && !VALID_PTR(aValue))
        return E_INVALIDARG;
    if ((aFlags != NULL) && !VALID_PTR(aFlags))
        return E_INVALIDARG;

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    /* protect mpVM (if not NULL) */
    AutoVMCallerWeak autoVMCaller(this);
    if (FAILED(autoVMCaller.rc())) return autoVMCaller.rc();

    /* Note: validity of mVMMDev which is bound to uninit() is guaranteed by
     * autoVMCaller, so there is no need to hold a lock of this */

    HRESULT rc = E_UNEXPECTED;
    using namespace guestProp;

    VBOXHGCMSVCPARM parm[3];
    Utf8Str Utf8Name = aName;
    int vrc = VINF_SUCCESS;

    parm[0].type = VBOX_HGCM_SVC_PARM_PTR;
    parm[0].u.pointer.addr = (void*)Utf8Name.c_str();
    /* The + 1 is the null terminator */
    parm[0].u.pointer.size = (uint32_t)Utf8Name.length() + 1;
    Utf8Str Utf8Value = aValue;
    if (aValue != NULL)
    {
        parm[1].type = VBOX_HGCM_SVC_PARM_PTR;
        parm[1].u.pointer.addr = (void*)Utf8Value.c_str();
        /* The + 1 is the null terminator */
        parm[1].u.pointer.size = (uint32_t)Utf8Value.length() + 1;
    }
    Utf8Str Utf8Flags = aFlags;
    if (aFlags != NULL)
    {
        parm[2].type = VBOX_HGCM_SVC_PARM_PTR;
        parm[2].u.pointer.addr = (void*)Utf8Flags.c_str();
        /* The + 1 is the null terminator */
        parm[2].u.pointer.size = (uint32_t)Utf8Flags.length() + 1;
    }
    if ((aValue != NULL) && (aFlags != NULL))
        vrc = m_pVMMDev->hgcmHostCall("VBoxGuestPropSvc", SET_PROP_HOST,
                                      3, &parm[0]);
    else if (aValue != NULL)
        vrc = m_pVMMDev->hgcmHostCall("VBoxGuestPropSvc", SET_PROP_VALUE_HOST,
                                    2, &parm[0]);
    else
        vrc = m_pVMMDev->hgcmHostCall("VBoxGuestPropSvc", DEL_PROP_HOST,
                                    1, &parm[0]);
    if (RT_SUCCESS(vrc))
        rc = S_OK;
    else
        rc = setError(E_UNEXPECTED,
            tr("The service call failed with the error %Rrc"),
            vrc);
    return rc;
#endif /* VBOX_WITH_GUEST_PROPS */
}


/**
 * @note Temporarily locks this object for writing.
 */
HRESULT Console::enumerateGuestProperties(IN_BSTR aPatterns,
                                          ComSafeArrayOut(BSTR, aNames),
                                          ComSafeArrayOut(BSTR, aValues),
                                          ComSafeArrayOut(LONG64, aTimestamps),
                                          ComSafeArrayOut(BSTR, aFlags))
{
#ifndef VBOX_WITH_GUEST_PROPS
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_PROPS */
    if (!VALID_PTR(aPatterns) && (aPatterns != NULL))
        return E_POINTER;
    if (ComSafeArrayOutIsNull(aNames))
        return E_POINTER;
    if (ComSafeArrayOutIsNull(aValues))
        return E_POINTER;
    if (ComSafeArrayOutIsNull(aTimestamps))
        return E_POINTER;
    if (ComSafeArrayOutIsNull(aFlags))
        return E_POINTER;

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    /* protect mpVM (if not NULL) */
    AutoVMCallerWeak autoVMCaller(this);
    if (FAILED(autoVMCaller.rc())) return autoVMCaller.rc();

    /* Note: validity of mVMMDev which is bound to uninit() is guaranteed by
     * autoVMCaller, so there is no need to hold a lock of this */

    return doEnumerateGuestProperties(aPatterns, ComSafeArrayOutArg(aNames),
                                      ComSafeArrayOutArg(aValues),
                                      ComSafeArrayOutArg(aTimestamps),
                                      ComSafeArrayOutArg(aFlags));
#endif /* VBOX_WITH_GUEST_PROPS */
}


/*
 * Internal: helper function for connecting progress reporting
 */
static int onlineMergeMediumProgress(void *pvUser, unsigned uPercentage)
{
    HRESULT rc = S_OK;
    IProgress *pProgress = static_cast<IProgress *>(pvUser);
    if (pProgress)
        rc = pProgress->SetCurrentOperationProgress(uPercentage);
    return SUCCEEDED(rc) ? VINF_SUCCESS : VERR_GENERAL_FAILURE;
}

/**
 * @note Temporarily locks this object for writing. bird: And/or reading?
 */
HRESULT Console::onlineMergeMedium(IMediumAttachment *aMediumAttachment,
                                   ULONG aSourceIdx, ULONG aTargetIdx,
                                   IMedium *aSource, IMedium *aTarget,
                                   BOOL aMergeForward,
                                   IMedium *aParentForTarget,
                                   ComSafeArrayIn(IMedium *, aChildrenToReparent),
                                   IProgress *aProgress)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    HRESULT rc = S_OK;
    int vrc = VINF_SUCCESS;

    /* Get the VM - must be done before the read-locking. */
    SafeVMPtr ptrVM(this);
    if (!ptrVM.isOk())
        return ptrVM.rc();

    /* We will need to release the lock before doing the actual merge */
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* paranoia - we don't want merges to happen while teleporting etc. */
    switch (mMachineState)
    {
        case MachineState_DeletingSnapshotOnline:
        case MachineState_DeletingSnapshotPaused:
            break;

        default:
            return setInvalidMachineStateError();
    }

    /** @todo AssertComRC -> AssertComRCReturn! Could potentially end up
     *        using uninitialized variables here. */
    BOOL fBuiltinIoCache;
    rc = mMachine->COMGETTER(IoCacheEnabled)(&fBuiltinIoCache);
    AssertComRC(rc);
    SafeIfaceArray<IStorageController> ctrls;
    rc = mMachine->COMGETTER(StorageControllers)(ComSafeArrayAsOutParam(ctrls));
    AssertComRC(rc);
    LONG lDev;
    rc = aMediumAttachment->COMGETTER(Device)(&lDev);
    AssertComRC(rc);
    LONG lPort;
    rc = aMediumAttachment->COMGETTER(Port)(&lPort);
    AssertComRC(rc);
    IMedium *pMedium;
    rc = aMediumAttachment->COMGETTER(Medium)(&pMedium);
    AssertComRC(rc);
    Bstr mediumLocation;
    if (pMedium)
    {
        rc = pMedium->COMGETTER(Location)(mediumLocation.asOutParam());
        AssertComRC(rc);
    }

    Bstr attCtrlName;
    rc = aMediumAttachment->COMGETTER(Controller)(attCtrlName.asOutParam());
    AssertComRC(rc);
    ComPtr<IStorageController> pStorageController;
    for (size_t i = 0; i < ctrls.size(); ++i)
    {
        Bstr ctrlName;
        rc = ctrls[i]->COMGETTER(Name)(ctrlName.asOutParam());
        AssertComRC(rc);
        if (attCtrlName == ctrlName)
        {
            pStorageController = ctrls[i];
            break;
        }
    }
    if (pStorageController.isNull())
        return setError(E_FAIL,
                        tr("Could not find storage controller '%ls'"),
                        attCtrlName.raw());

    StorageControllerType_T enmCtrlType;
    rc = pStorageController->COMGETTER(ControllerType)(&enmCtrlType);
    AssertComRC(rc);
    const char *pcszDevice = convertControllerTypeToDev(enmCtrlType);

    StorageBus_T enmBus;
    rc = pStorageController->COMGETTER(Bus)(&enmBus);
    AssertComRC(rc);
    ULONG uInstance;
    rc = pStorageController->COMGETTER(Instance)(&uInstance);
    AssertComRC(rc);
    BOOL fUseHostIOCache;
    rc = pStorageController->COMGETTER(UseHostIOCache)(&fUseHostIOCache);
    AssertComRC(rc);

    unsigned uLUN;
    rc = Console::convertBusPortDeviceToLun(enmBus, lPort, lDev, uLUN);
    AssertComRCReturnRC(rc);

    alock.release();

    /* Pause the VM, as it might have pending IO on this drive */
    VMSTATE enmVMState = VMR3GetState(ptrVM);
    if (mMachineState == MachineState_DeletingSnapshotOnline)
    {
        LogFlowFunc(("Suspending the VM...\n"));
        /* disable the callback to prevent Console-level state change */
        mVMStateChangeCallbackDisabled = true;
        int vrc2 = VMR3Suspend(ptrVM);
        mVMStateChangeCallbackDisabled = false;
        AssertRCReturn(vrc2, E_FAIL);
    }

    vrc = VMR3ReqCallWait(ptrVM,
                          VMCPUID_ANY,
                          (PFNRT)reconfigureMediumAttachment,
                          13,
                          this,
                          ptrVM.raw(),
                          pcszDevice,
                          uInstance,
                          enmBus,
                          fUseHostIOCache,
                          fBuiltinIoCache,
                          true /* fSetupMerge */,
                          aSourceIdx,
                          aTargetIdx,
                          aMediumAttachment,
                          mMachineState,
                          &rc);
    /* error handling is after resuming the VM */

    if (mMachineState == MachineState_DeletingSnapshotOnline)
    {
        LogFlowFunc(("Resuming the VM...\n"));
        /* disable the callback to prevent Console-level state change */
        mVMStateChangeCallbackDisabled = true;
        int vrc2 = VMR3Resume(ptrVM);
        mVMStateChangeCallbackDisabled = false;
        if (RT_FAILURE(vrc2))
        {
            /* too bad, we failed. try to sync the console state with the VMM state */
            AssertLogRelRC(vrc2);
            vmstateChangeCallback(ptrVM, VMSTATE_SUSPENDED, enmVMState, this);
        }
    }

    if (RT_FAILURE(vrc))
        return setError(E_FAIL, tr("%Rrc"), vrc);
    if (FAILED(rc))
        return rc;

    PPDMIBASE pIBase = NULL;
    PPDMIMEDIA pIMedium = NULL;
    vrc = PDMR3QueryDriverOnLun(ptrVM, pcszDevice, uInstance, uLUN, "VD", &pIBase);
    if (RT_SUCCESS(vrc))
    {
        if (pIBase)
        {
            pIMedium = (PPDMIMEDIA)pIBase->pfnQueryInterface(pIBase, PDMIMEDIA_IID);
            if (!pIMedium)
                return setError(E_FAIL, tr("could not query medium interface of controller"));
        }
        else
            return setError(E_FAIL, tr("could not query base interface of controller"));
    }

    /* Finally trigger the merge. */
    vrc = pIMedium->pfnMerge(pIMedium, onlineMergeMediumProgress, aProgress);
    if (RT_FAILURE(vrc))
        return setError(E_FAIL, tr("Failed to perform an online medium merge (%Rrc)"), vrc);

    /* Pause the VM, as it might have pending IO on this drive */
    enmVMState = VMR3GetState(ptrVM);
    if (mMachineState == MachineState_DeletingSnapshotOnline)
    {
        LogFlowFunc(("Suspending the VM...\n"));
        /* disable the callback to prevent Console-level state change */
        mVMStateChangeCallbackDisabled = true;
        int vrc2 = VMR3Suspend(ptrVM);
        mVMStateChangeCallbackDisabled = false;
        AssertRCReturn(vrc2, E_FAIL);
    }

    /* Update medium chain and state now, so that the VM can continue. */
    rc = mControl->FinishOnlineMergeMedium(aMediumAttachment, aSource, aTarget,
                                           aMergeForward, aParentForTarget,
                                           ComSafeArrayInArg(aChildrenToReparent));

    vrc = VMR3ReqCallWait(ptrVM,
                          VMCPUID_ANY,
                          (PFNRT)reconfigureMediumAttachment,
                          13,
                          this,
                          ptrVM.raw(),
                          pcszDevice,
                          uInstance,
                          enmBus,
                          fUseHostIOCache,
                          fBuiltinIoCache,
                          false /* fSetupMerge */,
                          0 /* uMergeSource */,
                          0 /* uMergeTarget */,
                          aMediumAttachment,
                          mMachineState,
                          &rc);
    /* error handling is after resuming the VM */

    if (mMachineState == MachineState_DeletingSnapshotOnline)
    {
        LogFlowFunc(("Resuming the VM...\n"));
        /* disable the callback to prevent Console-level state change */
        mVMStateChangeCallbackDisabled = true;
        int vrc2 = VMR3Resume(ptrVM);
        mVMStateChangeCallbackDisabled = false;
        AssertRC(vrc2);
        if (RT_FAILURE(vrc2))
        {
            /* too bad, we failed. try to sync the console state with the VMM state */
            vmstateChangeCallback(ptrVM, VMSTATE_SUSPENDED, enmVMState, this);
        }
    }

    if (RT_FAILURE(vrc))
        return setError(E_FAIL, tr("%Rrc"), vrc);
    if (FAILED(rc))
        return rc;

    return rc;
}


/**
 * Gets called by Session::UpdateMachineState()
 * (IInternalSessionControl::updateMachineState()).
 *
 * Must be called only in certain cases (see the implementation).
 *
 * @note Locks this object for writing.
 */
HRESULT Console::updateMachineState(MachineState_T aMachineState)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(   mMachineState == MachineState_Saving
                 || mMachineState == MachineState_LiveSnapshotting
                 || mMachineState == MachineState_RestoringSnapshot
                 || mMachineState == MachineState_DeletingSnapshot
                 || mMachineState == MachineState_DeletingSnapshotOnline
                 || mMachineState == MachineState_DeletingSnapshotPaused
                 , E_FAIL);

    return setMachineStateLocally(aMachineState);
}

/**
 * @note Locks this object for writing.
 */
void Console::onMousePointerShapeChange(bool fVisible, bool fAlpha,
                                        uint32_t xHot, uint32_t yHot,
                                        uint32_t width, uint32_t height,
                                        ComSafeArrayIn(BYTE,pShape))
{
#if 0
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("fVisible=%d, fAlpha=%d, xHot = %d, yHot = %d, width=%d, height=%d, shape=%p\n",
                      fVisible, fAlpha, xHot, yHot, width, height, pShape));
#endif

    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* We need a write lock because we alter the cached callback data */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Save the callback arguments */
    mCallbackData.mpsc.visible = fVisible;
    mCallbackData.mpsc.alpha = fAlpha;
    mCallbackData.mpsc.xHot = xHot;
    mCallbackData.mpsc.yHot = yHot;
    mCallbackData.mpsc.width = width;
    mCallbackData.mpsc.height = height;

    /* start with not valid */
    bool wasValid = mCallbackData.mpsc.valid;
    mCallbackData.mpsc.valid = false;

    com::SafeArray<BYTE> aShape(ComSafeArrayInArg(pShape));
    if (aShape.size() != 0)
        mCallbackData.mpsc.shape.initFrom(aShape);
    else
        mCallbackData.mpsc.shape.resize(0);
    mCallbackData.mpsc.valid = true;

    fireMousePointerShapeChangedEvent(mEventSource, fVisible, fAlpha, xHot, yHot, width, height, ComSafeArrayInArg(pShape));

#if 0
    LogFlowThisFuncLeave();
#endif
}

/**
 * @note Locks this object for writing.
 */
void Console::onMouseCapabilityChange(BOOL supportsAbsolute, BOOL supportsRelative, BOOL needsHostCursor)
{
    LogFlowThisFunc(("supportsAbsolute=%d supportsRelative=%d needsHostCursor=%d\n",
                      supportsAbsolute, supportsRelative, needsHostCursor));

    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* We need a write lock because we alter the cached callback data */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* save the callback arguments */
    mCallbackData.mcc.supportsAbsolute = supportsAbsolute;
    mCallbackData.mcc.supportsRelative = supportsRelative;
    mCallbackData.mcc.needsHostCursor = needsHostCursor;
    mCallbackData.mcc.valid = true;

    fireMouseCapabilityChangedEvent(mEventSource, supportsAbsolute, supportsRelative, needsHostCursor);
}

/**
 * @note Locks this object for reading.
 */
void Console::onStateChange(MachineState_T machineState)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    fireStateChangedEvent(mEventSource, machineState);
}

/**
 * @note Locks this object for reading.
 */
void Console::onAdditionsStateChange()
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    fireAdditionsStateChangedEvent(mEventSource);
}

/**
 * @note Locks this object for reading.
 *       This notification only is for reporting an incompatible
 *       Guest Additions interface, *not* the Guest Additions version!
 *
 *       The user will be notified inside the guest if new Guest
 *       Additions are available (via VBoxTray/VBoxClient).
 */
void Console::onAdditionsOutdated()
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
}

/**
 * @note Locks this object for writing.
 */
void Console::onKeyboardLedsChange(bool fNumLock, bool fCapsLock, bool fScrollLock)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* We need a write lock because we alter the cached callback data */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* save the callback arguments */
    mCallbackData.klc.numLock = fNumLock;
    mCallbackData.klc.capsLock = fCapsLock;
    mCallbackData.klc.scrollLock = fScrollLock;
    mCallbackData.klc.valid = true;

    fireKeyboardLedsChangedEvent(mEventSource, fNumLock, fCapsLock, fScrollLock);
}

/**
 * @note Locks this object for reading.
 */
void Console::onUSBDeviceStateChange(IUSBDevice *aDevice, bool aAttached,
                                     IVirtualBoxErrorInfo *aError)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    fireUSBDeviceStateChangedEvent(mEventSource, aDevice, aAttached, aError);
}

/**
 * @note Locks this object for reading.
 */
void Console::onRuntimeError(BOOL aFatal, IN_BSTR aErrorID, IN_BSTR aMessage)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    fireRuntimeErrorEvent(mEventSource, aFatal, aErrorID, aMessage);
}

/**
 * @note Locks this object for reading.
 */
HRESULT Console::onShowWindow(BOOL aCheck, BOOL *aCanShow, LONG64 *aWinId)
{
    AssertReturn(aCanShow, E_POINTER);
    AssertReturn(aWinId, E_POINTER);

    *aCanShow = FALSE;
    *aWinId = 0;

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    VBoxEventDesc evDesc;

    if (aCheck)
    {
        evDesc.init(mEventSource, VBoxEventType_OnCanShowWindow);
        BOOL fDelivered = evDesc.fire(5000); /* Wait up to 5 secs for delivery */
        //Assert(fDelivered);
        if (fDelivered)
        {
            ComPtr<IEvent> pEvent;
            evDesc.getEvent(pEvent.asOutParam());
            // bit clumsy
            ComPtr<ICanShowWindowEvent> pCanShowEvent = pEvent;
            if (pCanShowEvent)
            {
                BOOL fVetoed = FALSE;
                pCanShowEvent->IsVetoed(&fVetoed);
                *aCanShow = !fVetoed;
            }
            else
            {
                Assert(FALSE);
                *aCanShow = TRUE;
            }
        }
        else
            *aCanShow = TRUE;
    }
    else
    {
        evDesc.init(mEventSource, VBoxEventType_OnShowWindow, INT64_C(0));
        BOOL fDelivered = evDesc.fire(5000); /* Wait up to 5 secs for delivery */
        //Assert(fDelivered);
        if (fDelivered)
        {
            ComPtr<IEvent> pEvent;
            evDesc.getEvent(pEvent.asOutParam());
            ComPtr<IShowWindowEvent> pShowEvent = pEvent;
            LONG64 aEvWinId = 0;
            if (pShowEvent)
            {
                pShowEvent->COMGETTER(WinId)(&aEvWinId);
                if ((aEvWinId != 0) && (*aWinId == 0))
                    *aWinId = aEvWinId;
            }
            else
                Assert(FALSE);
        }
    }

    return S_OK;
}

// private methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Increases the usage counter of the mpVM pointer. Guarantees that
 * VMR3Destroy() will not be called on it at least until releaseVMCaller()
 * is called.
 *
 * If this method returns a failure, the caller is not allowed to use mpVM
 * and may return the failed result code to the upper level. This method sets
 * the extended error info on failure if \a aQuiet is false.
 *
 * Setting \a aQuiet to true is useful for methods that don't want to return
 * the failed result code to the caller when this method fails (e.g. need to
 * silently check for the mpVM availability).
 *
 * When mpVM is NULL but \a aAllowNullVM is true, a corresponding error will be
 * returned instead of asserting. Having it false is intended as a sanity check
 * for methods that have checked mMachineState and expect mpVM *NOT* to be NULL.
 *
 * @param aQuiet       true to suppress setting error info
 * @param aAllowNullVM true to accept mpVM being NULL and return a failure
 *                     (otherwise this method will assert if mpVM is NULL)
 *
 * @note Locks this object for writing.
 */
HRESULT Console::addVMCaller(bool aQuiet /* = false */,
                             bool aAllowNullVM /* = false */)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mVMDestroying)
    {
        /* powerDown() is waiting for all callers to finish */
        return aQuiet ? E_ACCESSDENIED : setError(E_ACCESSDENIED,
            tr("The virtual machine is being powered down"));
    }

    if (mpUVM == NULL)
    {
        Assert(aAllowNullVM == true);

        /* The machine is not powered up */
        return aQuiet ? E_ACCESSDENIED : setError(E_ACCESSDENIED,
            tr("The virtual machine is not powered up"));
    }

    ++mVMCallers;

    return S_OK;
}

/**
 * Decreases the usage counter of the mpVM pointer. Must always complete
 * the addVMCaller() call after the mpVM pointer is no more necessary.
 *
 * @note Locks this object for writing.
 */
void Console::releaseVMCaller()
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturnVoid(mpUVM != NULL);

    Assert(mVMCallers > 0);
    --mVMCallers;

    if (mVMCallers == 0 && mVMDestroying)
    {
        /* inform powerDown() there are no more callers */
        RTSemEventSignal(mVMZeroCallersSem);
    }
}


HRESULT Console::safeVMPtrRetainer(PVM *a_ppVM, PUVM *a_ppUVM, bool a_Quiet)
{
    *a_ppVM  = NULL;
    *a_ppUVM = NULL;

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /*
     * Repeat the checks done by addVMCaller.
     */
    if (mVMDestroying) /* powerDown() is waiting for all callers to finish */
        return a_Quiet
            ? E_ACCESSDENIED
            : setError(E_ACCESSDENIED, tr("The virtual machine is being powered down"));
    PUVM pUVM = mpUVM;
    if (!pUVM)
        return a_Quiet
            ? E_ACCESSDENIED
            : setError(E_ACCESSDENIED, tr("The virtual machine is was powered off"));

    /*
     * Retain a reference to the user mode VM handle and get the global handle.
     */
    uint32_t cRefs = VMR3RetainUVM(pUVM);
    if (cRefs == UINT32_MAX)
        return a_Quiet
            ? E_ACCESSDENIED
            : setError(E_ACCESSDENIED, tr("The virtual machine is was powered off"));

    PVM pVM = VMR3GetVM(pUVM);
    if (!pVM)
    {
        VMR3ReleaseUVM(pUVM);
        return a_Quiet
            ? E_ACCESSDENIED
            : setError(E_ACCESSDENIED, tr("The virtual machine is was powered off"));
    }

    /* done */
    *a_ppVM  = pVM;
    *a_ppUVM = pUVM;
    return S_OK;
}

void Console::safeVMPtrReleaser(PVM *a_ppVM, PUVM *a_ppUVM)
{
    if (*a_ppVM && *a_ppUVM)
        VMR3ReleaseUVM(*a_ppUVM);
    *a_ppVM  = NULL;
    *a_ppUVM = NULL;
}


/**
 * Initialize the release logging facility. In case something
 * goes wrong, there will be no release logging. Maybe in the future
 * we can add some logic to use different file names in this case.
 * Note that the logic must be in sync with Machine::DeleteSettings().
 */
HRESULT Console::consoleInitReleaseLog(const ComPtr<IMachine> aMachine)
{
    HRESULT hrc = S_OK;

    Bstr logFolder;
    hrc = aMachine->COMGETTER(LogFolder)(logFolder.asOutParam());
    if (FAILED(hrc)) return hrc;

    Utf8Str logDir = logFolder;

    /* make sure the Logs folder exists */
    Assert(logDir.length());
    if (!RTDirExists(logDir.c_str()))
        RTDirCreateFullPath(logDir.c_str(), 0777);

    Utf8Str logFile = Utf8StrFmt("%s%cVBox.log",
                                 logDir.c_str(), RTPATH_DELIMITER);
    Utf8Str pngFile = Utf8StrFmt("%s%cVBox.png",
                                 logDir.c_str(), RTPATH_DELIMITER);

    /*
     * Age the old log files
     * Rename .(n-1) to .(n), .(n-2) to .(n-1), ..., and the last log file to .1
     * Overwrite target files in case they exist.
     */
    ComPtr<IVirtualBox> pVirtualBox;
    aMachine->COMGETTER(Parent)(pVirtualBox.asOutParam());
    ComPtr<ISystemProperties> pSystemProperties;
    pVirtualBox->COMGETTER(SystemProperties)(pSystemProperties.asOutParam());
    ULONG cHistoryFiles = 3;
    pSystemProperties->COMGETTER(LogHistoryCount)(&cHistoryFiles);
    if (cHistoryFiles)
    {
        for (int i = cHistoryFiles-1; i >= 0; i--)
        {
            Utf8Str *files[] = { &logFile, &pngFile };
            Utf8Str oldName, newName;

            for (unsigned int j = 0; j < RT_ELEMENTS(files); ++j)
            {
                if (i > 0)
                    oldName = Utf8StrFmt("%s.%d", files[j]->c_str(), i);
                else
                    oldName = *files[j];
                newName = Utf8StrFmt("%s.%d", files[j]->c_str(), i + 1);
                /* If the old file doesn't exist, delete the new file (if it
                 * exists) to provide correct rotation even if the sequence is
                 * broken */
                if (   RTFileRename(oldName.c_str(), newName.c_str(), RTFILEMOVE_FLAGS_REPLACE)
                    == VERR_FILE_NOT_FOUND)
                    RTFileDelete(newName.c_str());
            }
        }
    }

    static const char * const   s_apszGroups[] = VBOX_LOGGROUP_NAMES;
    char                        szError[RTPATH_MAX + 128] = "";
    PRTLOGGER                   pReleaseLogger;
    uint32_t                    fFlags = RTLOGFLAGS_PREFIX_TIME_PROG | RTLOGFLAGS_RESTRICT_GROUPS;
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    fFlags |= RTLOGFLAGS_USECRLF;
#endif
    int vrc = RTLogCreateEx(&pReleaseLogger, fFlags, "all all.restrict default.unrestricted",
                            "VBOX_RELEASE_LOG", RT_ELEMENTS(s_apszGroups), s_apszGroups, RTLOGDEST_FILE,
                            NULL /* pfnBeginEnd */, 0 /* cHistory */, 0 /* cbHistoryFileMax */, 0 /* uHistoryTimeMax */,
                            szError, sizeof(szError), logFile.c_str());
    if (RT_SUCCESS(vrc))
    {
        RTLogSetGroupLimit(pReleaseLogger, 32768);

        /* some introductory information */
        RTTIMESPEC timeSpec;
        char szTmp[256];
        RTTimeSpecToString(RTTimeNow(&timeSpec), szTmp, sizeof(szTmp));
        RTLogRelLogger(pReleaseLogger, 0, ~0U,
                       "VirtualBox %s r%u %s (%s %s) release log\n"
#ifdef VBOX_BLEEDING_EDGE
                       "EXPERIMENTAL build " VBOX_BLEEDING_EDGE "\n"
#endif
                       "Log opened %s\n",
                       VBOX_VERSION_STRING, RTBldCfgRevision(), VBOX_BUILD_TARGET,
                       __DATE__, __TIME__, szTmp);

        vrc = RTSystemQueryOSInfo(RTSYSOSINFO_PRODUCT, szTmp, sizeof(szTmp));
        if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
            RTLogRelLogger(pReleaseLogger, 0, ~0U, "OS Product: %s\n", szTmp);
        vrc = RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szTmp, sizeof(szTmp));
        if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
            RTLogRelLogger(pReleaseLogger, 0, ~0U, "OS Release: %s\n", szTmp);
        vrc = RTSystemQueryOSInfo(RTSYSOSINFO_VERSION, szTmp, sizeof(szTmp));
        if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
            RTLogRelLogger(pReleaseLogger, 0, ~0U, "OS Version: %s\n", szTmp);
        vrc = RTSystemQueryOSInfo(RTSYSOSINFO_SERVICE_PACK, szTmp, sizeof(szTmp));
        if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
            RTLogRelLogger(pReleaseLogger, 0, ~0U, "OS Service Pack: %s\n", szTmp);
        vrc = RTSystemQueryDmiString(RTSYSDMISTR_PRODUCT_NAME, szTmp, sizeof(szTmp));
        if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
            RTLogRelLogger(pReleaseLogger, 0, ~0U, "DMI Product Name: %s\n", szTmp);
        vrc = RTSystemQueryDmiString(RTSYSDMISTR_PRODUCT_VERSION, szTmp, sizeof(szTmp));
        if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
            RTLogRelLogger(pReleaseLogger, 0, ~0U, "DMI Product Version: %s\n", szTmp);

        ComPtr<IHost> pHost;
        pVirtualBox->COMGETTER(Host)(pHost.asOutParam());
        ULONG cMbHostRam = 0;
        ULONG cMbHostRamAvail = 0;
        pHost->COMGETTER(MemorySize)(&cMbHostRam);
        pHost->COMGETTER(MemoryAvailable)(&cMbHostRamAvail);
        RTLogRelLogger(pReleaseLogger, 0, ~0U, "Host RAM: %uMB RAM, available: %uMB\n",
                       cMbHostRam, cMbHostRamAvail);

        /* the package type is interesting for Linux distributions */
        char szExecName[RTPATH_MAX];
        char *pszExecName = RTProcGetExecutablePath(szExecName, sizeof(szExecName));
        RTLogRelLogger(pReleaseLogger, 0, ~0U,
                       "Executable: %s\n"
                       "Process ID: %u\n"
                       "Package type: %s"
#ifdef VBOX_OSE
                       " (OSE)"
#endif
                       "\n",
                       pszExecName ? pszExecName : "unknown",
                       RTProcSelf(),
                       VBOX_PACKAGE_STRING);

        /* register this logger as the release logger */
        RTLogRelSetDefaultInstance(pReleaseLogger);
        hrc = S_OK;

        /* Explicitly flush the log in case of VBOX_RELEASE_LOG=buffered. */
        RTLogFlush(pReleaseLogger);
    }
    else
        hrc = setError(E_FAIL,
            tr("Failed to open release log (%s, %Rrc)"),
            szError, vrc);

    /* If we've made any directory changes, flush the directory to increase
       the likelihood that the log file will be usable after a system panic.

       Tip: Try 'export VBOX_RELEASE_LOG_FLAGS=flush' if the last bits of the log
            is missing. Just don't have too high hopes for this to help. */
    if (SUCCEEDED(hrc) || cHistoryFiles)
        RTDirFlush(logDir.c_str());

    return hrc;
}

/**
 * Common worker for PowerUp and PowerUpPaused.
 *
 * @returns COM status code.
 *
 * @param   aProgress       Where to return the progress object.
 * @param   aPaused         true if PowerUpPaused called.
 */
HRESULT Console::powerUp(IProgress **aProgress, bool aPaused)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("mMachineState=%d\n", mMachineState));

    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;
    ComObjPtr<Progress> pPowerupProgress;
    bool fBeganPoweringUp = false;

    try
    {
        if (Global::IsOnlineOrTransient(mMachineState))
            throw setError(VBOX_E_INVALID_VM_STATE,
                tr("The virtual machine is already running or busy (machine state: %s)"),
                Global::stringifyMachineState(mMachineState));

        /* test and clear the TeleporterEnabled property  */
        BOOL fTeleporterEnabled;
        rc = mMachine->COMGETTER(TeleporterEnabled)(&fTeleporterEnabled);
        if (FAILED(rc))
            throw rc;
#if 0 /** @todo we should save it afterwards, but that isn't necessarily a good idea. Find a better place for this (VBoxSVC).  */
        if (fTeleporterEnabled)
        {
            rc = mMachine->COMSETTER(TeleporterEnabled)(FALSE);
            if (FAILED(rc))
                throw rc;
        }
#endif

        /* test the FaultToleranceState property  */
        FaultToleranceState_T enmFaultToleranceState;
        rc = mMachine->COMGETTER(FaultToleranceState)(&enmFaultToleranceState);
        if (FAILED(rc))
            throw rc;
        BOOL fFaultToleranceSyncEnabled = (enmFaultToleranceState == FaultToleranceState_Standby);

        /* Create a progress object to track progress of this operation. Must
         * be done as early as possible (together with BeginPowerUp()) as this
         * is vital for communicating as much as possible early powerup
         * failure information to the API caller */
        pPowerupProgress.createObject();
        Bstr progressDesc;
        if (mMachineState == MachineState_Saved)
            progressDesc = tr("Restoring virtual machine");
        else if (fTeleporterEnabled)
            progressDesc = tr("Teleporting virtual machine");
        else if (fFaultToleranceSyncEnabled)
            progressDesc = tr("Fault Tolerance syncing of remote virtual machine");
        else
            progressDesc = tr("Starting virtual machine");
        if (    mMachineState == MachineState_Saved
            ||  (!fTeleporterEnabled && !fFaultToleranceSyncEnabled))
            rc = pPowerupProgress->init(static_cast<IConsole *>(this),
                                        progressDesc.raw(),
                                        FALSE /* aCancelable */);
        else
        if (fTeleporterEnabled)
            rc = pPowerupProgress->init(static_cast<IConsole *>(this),
                                        progressDesc.raw(),
                                        TRUE /* aCancelable */,
                                        3    /* cOperations */,
                                        10   /* ulTotalOperationsWeight */,
                                        Bstr(tr("Teleporting virtual machine")).raw(),
                                        1    /* ulFirstOperationWeight */,
                                        NULL);
        else
        if (fFaultToleranceSyncEnabled)
            rc = pPowerupProgress->init(static_cast<IConsole *>(this),
                                        progressDesc.raw(),
                                        TRUE /* aCancelable */,
                                        3    /* cOperations */,
                                        10   /* ulTotalOperationsWeight */,
                                        Bstr(tr("Fault Tolerance syncing of remote virtual machine")).raw(),
                                        1    /* ulFirstOperationWeight */,
                                        NULL);

        if (FAILED(rc))
            throw rc;

        /* Tell VBoxSVC and Machine about the progress object so they can
           combine/proxy it to any openRemoteSession caller. */
        LogFlowThisFunc(("Calling BeginPowerUp...\n"));
        rc = mControl->BeginPowerUp(pPowerupProgress);
        if (FAILED(rc))
        {
            LogFlowThisFunc(("BeginPowerUp failed\n"));
            throw rc;
        }
        fBeganPoweringUp = true;

        /** @todo this code prevents starting a VM with unavailable bridged
         * networking interface. The only benefit is a slightly better error
         * message, which should be moved to the driver code. This is the
         * only reason why I left the code in for now. The driver allows
         * unavailable bridged networking interfaces in certain circumstances,
         * and this is sabotaged by this check. The VM will initially have no
         * network connectivity, but the user can fix this at runtime. */
#if 0
        /* the network cards will undergo a quick consistency check */
        for (ULONG slot = 0;
             slot < maxNetworkAdapters;
             ++slot)
        {
            ComPtr<INetworkAdapter> pNetworkAdapter;
            mMachine->GetNetworkAdapter(slot, pNetworkAdapter.asOutParam());
            BOOL enabled = FALSE;
            pNetworkAdapter->COMGETTER(Enabled)(&enabled);
            if (!enabled)
                continue;

            NetworkAttachmentType_T netattach;
            pNetworkAdapter->COMGETTER(AttachmentType)(&netattach);
            switch (netattach)
            {
                case NetworkAttachmentType_Bridged:
                {
                    /* a valid host interface must have been set */
                    Bstr hostif;
                    pNetworkAdapter->COMGETTER(HostInterface)(hostif.asOutParam());
                    if (hostif.isEmpty())
                    {
                        throw setError(VBOX_E_HOST_ERROR,
                            tr("VM cannot start because host interface networking requires a host interface name to be set"));
                    }
                    ComPtr<IVirtualBox> pVirtualBox;
                    mMachine->COMGETTER(Parent)(pVirtualBox.asOutParam());
                    ComPtr<IHost> pHost;
                    pVirtualBox->COMGETTER(Host)(pHost.asOutParam());
                    ComPtr<IHostNetworkInterface> pHostInterface;
                    if (!SUCCEEDED(pHost->FindHostNetworkInterfaceByName(hostif.raw(),
                                                                         pHostInterface.asOutParam())))
                    {
                        throw setError(VBOX_E_HOST_ERROR,
                            tr("VM cannot start because the host interface '%ls' does not exist"),
                            hostif.raw());
                    }
                    break;
                }
                default:
                    break;
            }
        }
#endif // 0

        /* Read console data stored in the saved state file (if not yet done) */
        rc = loadDataFromSavedState();
        if (FAILED(rc))
            throw rc;

        /* Check all types of shared folders and compose a single list */
        SharedFolderDataMap sharedFolders;
        {
            /* first, insert global folders */
            for (SharedFolderDataMap::const_iterator it = m_mapGlobalSharedFolders.begin();
                 it != m_mapGlobalSharedFolders.end();
                 ++it)
            {
                const SharedFolderData &d = it->second;
                sharedFolders[it->first] = d;
            }

            /* second, insert machine folders */
            for (SharedFolderDataMap::const_iterator it = m_mapMachineSharedFolders.begin();
                 it != m_mapMachineSharedFolders.end();
                 ++it)
            {
                const SharedFolderData &d = it->second;
                sharedFolders[it->first] = d;
            }

            /* third, insert console folders */
            for (SharedFolderMap::const_iterator it = m_mapSharedFolders.begin();
                 it != m_mapSharedFolders.end();
                 ++it)
            {
                SharedFolder *pSF = it->second;
                AutoCaller sfCaller(pSF);
                AutoReadLock sfLock(pSF COMMA_LOCKVAL_SRC_POS);
                sharedFolders[it->first] = SharedFolderData(pSF->getHostPath(),
                                                            pSF->isWritable(),
                                                            pSF->isAutoMounted());
            }
        }

        Bstr savedStateFile;

        /*
         * Saved VMs will have to prove that their saved states seem kosher.
         */
        if (mMachineState == MachineState_Saved)
        {
            rc = mMachine->COMGETTER(StateFilePath)(savedStateFile.asOutParam());
            if (FAILED(rc))
                throw rc;
            ComAssertRet(!savedStateFile.isEmpty(), E_FAIL);
            int vrc = SSMR3ValidateFile(Utf8Str(savedStateFile).c_str(), false /* fChecksumIt */);
            if (RT_FAILURE(vrc))
                throw setError(VBOX_E_FILE_ERROR,
                                tr("VM cannot start because the saved state file '%ls' is invalid (%Rrc). Delete the saved state prior to starting the VM"),
                                savedStateFile.raw(), vrc);
        }

        LogFlowThisFunc(("Checking if canceled...\n"));
        BOOL fCanceled;
        rc = pPowerupProgress->COMGETTER(Canceled)(&fCanceled);
        if (FAILED(rc))
            throw rc;
        if (fCanceled)
        {
            LogFlowThisFunc(("Canceled in BeginPowerUp\n"));
            throw setError(E_FAIL, tr("Powerup was canceled"));
        }
        LogFlowThisFunc(("Not canceled yet.\n"));

        /* setup task object and thread to carry out the operation
         * asynchronously */

        std::auto_ptr<VMPowerUpTask> task(new VMPowerUpTask(this, pPowerupProgress));
        ComAssertComRCRetRC(task->rc());

        task->mConfigConstructor = configConstructor;
        task->mSharedFolders = sharedFolders;
        task->mStartPaused = aPaused;
        if (mMachineState == MachineState_Saved)
            task->mSavedStateFile = savedStateFile;
        task->mTeleporterEnabled = fTeleporterEnabled;
        task->mEnmFaultToleranceState = enmFaultToleranceState;

        /* Reset differencing hard disks for which autoReset is true,
         * but only if the machine has no snapshots OR the current snapshot
         * is an OFFLINE snapshot; otherwise we would reset the current
         * differencing image of an ONLINE snapshot which contains the disk
         * state of the machine while it was previously running, but without
         * the corresponding machine state, which is equivalent to powering
         * off a running machine and not good idea
         */
        ComPtr<ISnapshot> pCurrentSnapshot;
        rc = mMachine->COMGETTER(CurrentSnapshot)(pCurrentSnapshot.asOutParam());
        if (FAILED(rc))
            throw rc;

        BOOL fCurrentSnapshotIsOnline = false;
        if (pCurrentSnapshot)
        {
            rc = pCurrentSnapshot->COMGETTER(Online)(&fCurrentSnapshotIsOnline);
            if (FAILED(rc))
                throw rc;
        }

        if (!fCurrentSnapshotIsOnline)
        {
            LogFlowThisFunc(("Looking for immutable images to reset\n"));

            com::SafeIfaceArray<IMediumAttachment> atts;
            rc = mMachine->COMGETTER(MediumAttachments)(ComSafeArrayAsOutParam(atts));
            if (FAILED(rc))
                throw rc;

            for (size_t i = 0;
                 i < atts.size();
                 ++i)
            {
                DeviceType_T devType;
                rc = atts[i]->COMGETTER(Type)(&devType);
                /** @todo later applies to floppies as well */
                if (devType == DeviceType_HardDisk)
                {
                    ComPtr<IMedium> pMedium;
                    rc = atts[i]->COMGETTER(Medium)(pMedium.asOutParam());
                    if (FAILED(rc))
                        throw rc;

                    /* needs autoreset? */
                    BOOL autoReset = FALSE;
                    rc = pMedium->COMGETTER(AutoReset)(&autoReset);
                    if (FAILED(rc))
                        throw rc;

                    if (autoReset)
                    {
                        ComPtr<IProgress> pResetProgress;
                        rc = pMedium->Reset(pResetProgress.asOutParam());
                        if (FAILED(rc))
                            throw rc;

                        /* save for later use on the powerup thread */
                        task->hardDiskProgresses.push_back(pResetProgress);
                    }
                }
            }
        }
        else
            LogFlowThisFunc(("Machine has a current snapshot which is online, skipping immutable images reset\n"));

        rc = consoleInitReleaseLog(mMachine);
        if (FAILED(rc))
            throw rc;
#ifdef VBOX_WITH_EXTPACK
        mptrExtPackManager->dumpAllToReleaseLog();
#endif

#ifdef RT_OS_SOLARIS
        /* setup host core dumper for the VM */
        Bstr value;
        HRESULT hrc = mMachine->GetExtraData(Bstr("VBoxInternal2/CoreDumpEnabled").raw(), value.asOutParam());
        if (SUCCEEDED(hrc) && value == "1")
        {
            Bstr coreDumpDir, coreDumpReplaceSys, coreDumpLive;
            mMachine->GetExtraData(Bstr("VBoxInternal2/CoreDumpDir").raw(), coreDumpDir.asOutParam());
            mMachine->GetExtraData(Bstr("VBoxInternal2/CoreDumpReplaceSystemDump").raw(), coreDumpReplaceSys.asOutParam());
            mMachine->GetExtraData(Bstr("VBoxInternal2/CoreDumpLive").raw(), coreDumpLive.asOutParam());

            uint32_t fCoreFlags = 0;
            if (   coreDumpReplaceSys.isEmpty() == false
                && Utf8Str(coreDumpReplaceSys).toUInt32() == 1)
            {
                fCoreFlags |= RTCOREDUMPER_FLAGS_REPLACE_SYSTEM_DUMP;
            }

            if (   coreDumpLive.isEmpty() == false
                && Utf8Str(coreDumpLive).toUInt32() == 1)
            {
                fCoreFlags |= RTCOREDUMPER_FLAGS_LIVE_CORE;
            }

            Utf8Str strDumpDir(coreDumpDir);
            const char *pszDumpDir = strDumpDir.c_str();
            if (   pszDumpDir
                && *pszDumpDir == '\0')
                pszDumpDir = NULL;

            int vrc;
            if (   pszDumpDir
                && !RTDirExists(pszDumpDir))
            {
                /*
                 * Try create the directory.
                 */
                vrc = RTDirCreateFullPath(pszDumpDir, 0777);
                if (RT_FAILURE(vrc))
                    throw setError(E_FAIL, "Failed to setup CoreDumper. Couldn't create dump directory '%s' (%Rrc)\n", pszDumpDir, vrc);
            }

            vrc = RTCoreDumperSetup(pszDumpDir, fCoreFlags);
            if (RT_FAILURE(vrc))
                throw setError(E_FAIL, "Failed to setup CoreDumper (%Rrc)", vrc);
            else
                LogRel(("CoreDumper setup successful. pszDumpDir=%s fFlags=%#x\n", pszDumpDir ? pszDumpDir : ".", fCoreFlags));
        }
#endif

        /* pass the progress object to the caller if requested */
        if (aProgress)
        {
            if (task->hardDiskProgresses.size() == 0)
            {
                /* there are no other operations to track, return the powerup
                 * progress only */
                pPowerupProgress.queryInterfaceTo(aProgress);
            }
            else
            {
                /* create a combined progress object */
                ComObjPtr<CombinedProgress> pProgress;
                pProgress.createObject();
                VMPowerUpTask::ProgressList progresses(task->hardDiskProgresses);
                progresses.push_back(ComPtr<IProgress> (pPowerupProgress));
                rc = pProgress->init(static_cast<IConsole *>(this),
                                     progressDesc.raw(), progresses.begin(),
                                     progresses.end());
                AssertComRCReturnRC(rc);
                pProgress.queryInterfaceTo(aProgress);
            }
        }

        int vrc = RTThreadCreate(NULL, Console::powerUpThread,
                                 (void *)task.get(), 0,
                                 RTTHREADTYPE_MAIN_WORKER, 0, "VMPowerUp");
        if (RT_FAILURE(vrc))
            throw setError(E_FAIL, "Could not create VMPowerUp thread (%Rrc)", vrc);

        /* task is now owned by powerUpThread(), so release it */
        task.release();

        /* finally, set the state: no right to fail in this method afterwards
         * since we've already started the thread and it is now responsible for
         * any error reporting and appropriate state change! */
        if (mMachineState == MachineState_Saved)
            setMachineState(MachineState_Restoring);
        else if (fTeleporterEnabled)
            setMachineState(MachineState_TeleportingIn);
        else if (enmFaultToleranceState == FaultToleranceState_Standby)
            setMachineState(MachineState_FaultTolerantSyncing);
        else
            setMachineState(MachineState_Starting);
    }
    catch (HRESULT aRC) { rc = aRC; }

    if (FAILED(rc) && fBeganPoweringUp)
    {

        /* The progress object will fetch the current error info */
        if (!pPowerupProgress.isNull())
            pPowerupProgress->notifyComplete(rc);

        /* Save the error info across the IPC below. Can't be done before the
         * progress notification above, as saving the error info deletes it
         * from the current context, and thus the progress object wouldn't be
         * updated correctly. */
        ErrorInfoKeeper eik;

        /* signal end of operation */
        mControl->EndPowerUp(rc);
    }

    LogFlowThisFunc(("mMachineState=%d, rc=%Rhrc\n", mMachineState, rc));
    LogFlowThisFuncLeave();
    return rc;
}

/**
 * Internal power off worker routine.
 *
 * This method may be called only at certain places with the following meaning
 * as shown below:
 *
 * - if the machine state is either Running or Paused, a normal
 *   Console-initiated powerdown takes place (e.g. PowerDown());
 * - if the machine state is Saving, saveStateThread() has successfully done its
 *   job;
 * - if the machine state is Starting or Restoring, powerUpThread() has failed
 *   to start/load the VM;
 * - if the machine state is Stopping, the VM has powered itself off (i.e. not
 *   as a result of the powerDown() call).
 *
 * Calling it in situations other than the above will cause unexpected behavior.
 *
 * Note that this method should be the only one that destroys mpVM and sets it
 * to NULL.
 *
 * @param aProgress Progress object to run (may be NULL).
 *
 * @note Locks this object for writing.
 *
 * @note Never call this method from a thread that called addVMCaller() or
 *       instantiated an AutoVMCaller object; first call releaseVMCaller() or
 *       release(). Otherwise it will deadlock.
 */
HRESULT Console::powerDown(IProgress *aProgress /*= NULL*/)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Total # of steps for the progress object. Must correspond to the
     * number of "advance percent count" comments in this method! */
    enum { StepCount = 7 };
    /* current step */
    ULONG step = 0;

    HRESULT rc = S_OK;
    int vrc = VINF_SUCCESS;

    /* sanity */
    Assert(mVMDestroying == false);

    PUVM     pUVM  = mpUVM;                 Assert(pUVM != NULL);
    uint32_t cRefs = VMR3RetainUVM(pUVM);   Assert(cRefs != UINT32_MAX);

    AssertMsg(   mMachineState == MachineState_Running
              || mMachineState == MachineState_Paused
              || mMachineState == MachineState_Stuck
              || mMachineState == MachineState_Starting
              || mMachineState == MachineState_Stopping
              || mMachineState == MachineState_Saving
              || mMachineState == MachineState_Restoring
              || mMachineState == MachineState_TeleportingPausedVM
              || mMachineState == MachineState_FaultTolerantSyncing
              || mMachineState == MachineState_TeleportingIn
              , ("Invalid machine state: %s\n", Global::stringifyMachineState(mMachineState)));

    LogRel(("Console::powerDown(): A request to power off the VM has been issued (mMachineState=%s, InUninit=%d)\n",
            Global::stringifyMachineState(mMachineState), autoCaller.state() == InUninit));

    /* Check if we need to power off the VM. In case of mVMPoweredOff=true, the
     * VM has already powered itself off in vmstateChangeCallback() and is just
     * notifying Console about that. In case of Starting or Restoring,
     * powerUpThread() is calling us on failure, so the VM is already off at
     * that point. */
    if (   !mVMPoweredOff
        && (   mMachineState == MachineState_Starting
            || mMachineState == MachineState_Restoring
            || mMachineState == MachineState_FaultTolerantSyncing
            || mMachineState == MachineState_TeleportingIn)
       )
        mVMPoweredOff = true;

    /*
     * Go to Stopping state if not already there.
     *
     * Note that we don't go from Saving/Restoring to Stopping because
     * vmstateChangeCallback() needs it to set the state to Saved on
     * VMSTATE_TERMINATED. In terms of protecting from inappropriate operations
     * while leaving the lock below, Saving or Restoring should be fine too.
     * Ditto for TeleportingPausedVM -> Teleported.
     */
    if (   mMachineState != MachineState_Saving
        && mMachineState != MachineState_Restoring
        && mMachineState != MachineState_Stopping
        && mMachineState != MachineState_TeleportingIn
        && mMachineState != MachineState_TeleportingPausedVM
        && mMachineState != MachineState_FaultTolerantSyncing
       )
        setMachineState(MachineState_Stopping);

    /* ----------------------------------------------------------------------
     * DONE with necessary state changes, perform the power down actions (it's
     * safe to leave the object lock now if needed)
     * ---------------------------------------------------------------------- */

    /* Stop the VRDP server to prevent new clients connection while VM is being
     * powered off. */
    if (mConsoleVRDPServer)
    {
        LogFlowThisFunc(("Stopping VRDP server...\n"));

        /* Leave the lock since EMT will call us back as addVMCaller()
         * in updateDisplayData(). */
        alock.leave();

        mConsoleVRDPServer->Stop();

        alock.enter();
    }

    /* advance percent count */
    if (aProgress)
        aProgress->SetCurrentOperationProgress(99 * (++step) / StepCount );


    /* ----------------------------------------------------------------------
     * Now, wait for all mpVM callers to finish their work if there are still
     * some on other threads. NO methods that need mpVM (or initiate other calls
     * that need it) may be called after this point
     * ---------------------------------------------------------------------- */

    /* go to the destroying state to prevent from adding new callers */
    mVMDestroying = true;

    if (mVMCallers > 0)
    {
        /* lazy creation */
        if (mVMZeroCallersSem == NIL_RTSEMEVENT)
            RTSemEventCreate(&mVMZeroCallersSem);

        LogFlowThisFunc(("Waiting for mpVM callers (%d) to drop to zero...\n",
                          mVMCallers));

        alock.leave();

        RTSemEventWait(mVMZeroCallersSem, RT_INDEFINITE_WAIT);

        alock.enter();
    }

    /* advance percent count */
    if (aProgress)
        aProgress->SetCurrentOperationProgress(99 * (++step) / StepCount );

    vrc = VINF_SUCCESS;

    /*
     * Power off the VM if not already done that.
     * Leave the lock since EMT will call vmstateChangeCallback.
     *
     * Note that VMR3PowerOff() may fail here (invalid VMSTATE) if the
     * VM-(guest-)initiated power off happened in parallel a ms before this
     * call. So far, we let this error pop up on the user's side.
     */
    if (!mVMPoweredOff)
    {
        LogFlowThisFunc(("Powering off the VM...\n"));
        alock.leave();
        vrc = VMR3PowerOff(VMR3GetVM(pUVM));
#ifdef VBOX_WITH_EXTPACK
        mptrExtPackManager->callAllVmPowerOffHooks(this, VMR3GetVM(pUVM));
#endif
        alock.enter();
    }

    /* advance percent count */
    if (aProgress)
        aProgress->SetCurrentOperationProgress(99 * (++step) / StepCount );

#ifdef VBOX_WITH_HGCM
    /* Shutdown HGCM services before destroying the VM. */
    if (m_pVMMDev)
    {
        LogFlowThisFunc(("Shutdown HGCM...\n"));

        /* Leave the lock since EMT will call us back as addVMCaller() */
        alock.leave();

        m_pVMMDev->hgcmShutdown();

        alock.enter();
    }

    /* advance percent count */
    if (aProgress)
        aProgress->SetCurrentOperationProgress(99 * (++step) / StepCount);

#endif /* VBOX_WITH_HGCM */

    LogFlowThisFunc(("Ready for VM destruction.\n"));

    /* If we are called from Console::uninit(), then try to destroy the VM even
     * on failure (this will most likely fail too, but what to do?..) */
    if (RT_SUCCESS(vrc) || autoCaller.state() == InUninit)
    {
        /* If the machine has an USB controller, release all USB devices
         * (symmetric to the code in captureUSBDevices()) */
        bool fHasUSBController = false;
        {
            PPDMIBASE pBase;
            vrc = PDMR3QueryLun(VMR3GetVM(pUVM), "usb-ohci", 0, 0, &pBase);
            if (RT_SUCCESS(vrc))
            {
                fHasUSBController = true;
                detachAllUSBDevices(false /* aDone */);
            }
        }

        /* Now we've got to destroy the VM as well. (mpVM is not valid beyond
         * this point). We leave the lock before calling VMR3Destroy() because
         * it will result into calling destructors of drivers associated with
         * Console children which may in turn try to lock Console (e.g. by
         * instantiating SafeVMPtr to access mpVM). It's safe here because
         * mVMDestroying is set which should prevent any activity. */

        /* Set mpUVM to NULL early just in case if some old code is not using
         * addVMCaller()/releaseVMCaller(). (We have our own ref on pUVM.) */
        VMR3ReleaseUVM(mpUVM);
        mpUVM = NULL;

        LogFlowThisFunc(("Destroying the VM...\n"));

        alock.leave();

        vrc = VMR3Destroy(VMR3GetVM(pUVM));

        /* take the lock again */
        alock.enter();

        /* advance percent count */
        if (aProgress)
            aProgress->SetCurrentOperationProgress(99 * (++step) / StepCount);

        if (RT_SUCCESS(vrc))
        {
            LogFlowThisFunc(("Machine has been destroyed (mMachineState=%d)\n",
                              mMachineState));
            /* Note: the Console-level machine state change happens on the
             * VMSTATE_TERMINATE state change in vmstateChangeCallback(). If
             * powerDown() is called from EMT (i.e. from vmstateChangeCallback()
             * on receiving VM-initiated VMSTATE_OFF), VMSTATE_TERMINATE hasn't
             * occurred yet. This is okay, because mMachineState is already
             * Stopping in this case, so any other attempt to call PowerDown()
             * will be rejected. */
        }
        else
        {
            /* bad bad bad, but what to do? (Give Console our UVM ref.) */
            mpUVM = pUVM;
            pUVM = NULL;
            rc = setError(VBOX_E_VM_ERROR,
                tr("Could not destroy the machine. (Error: %Rrc)"),
                vrc);
        }

        /* Complete the detaching of the USB devices. */
        if (fHasUSBController)
            detachAllUSBDevices(true /* aDone */);

        /* advance percent count */
        if (aProgress)
            aProgress->SetCurrentOperationProgress(99 * (++step) / StepCount);
    }
    else
    {
        rc = setError(VBOX_E_VM_ERROR,
            tr("Could not power off the machine. (Error: %Rrc)"),
            vrc);
    }

    /*
     * Finished with the destruction.
     *
     * Note that if something impossible happened and we've failed to destroy
     * the VM, mVMDestroying will remain true and mMachineState will be
     * something like Stopping, so most Console methods will return an error
     * to the caller.
     */
    if (mpUVM != NULL)
        VMR3ReleaseUVM(pUVM);
    else
        mVMDestroying = false;

    if (SUCCEEDED(rc))
        mCallbackData.clear();

    LogFlowThisFuncLeave();
    return rc;
}

/**
 * @note Locks this object for writing.
 */
HRESULT Console::setMachineState(MachineState_T aMachineState,
                                 bool aUpdateServer /* = true */)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    if (mMachineState != aMachineState)
    {
        LogThisFunc(("machineState=%s -> %s aUpdateServer=%RTbool\n",
                     Global::stringifyMachineState(mMachineState), Global::stringifyMachineState(aMachineState), aUpdateServer));
        mMachineState = aMachineState;

        /// @todo (dmik)
        //      possibly, we need to redo onStateChange() using the dedicated
        //      Event thread, like it is done in VirtualBox. This will make it
        //      much safer (no deadlocks possible if someone tries to use the
        //      console from the callback), however, listeners will lose the
        //      ability to synchronously react to state changes (is it really
        //      necessary??)
        LogFlowThisFunc(("Doing onStateChange()...\n"));
        onStateChange(aMachineState);
        LogFlowThisFunc(("Done onStateChange()\n"));

        if (aUpdateServer)
        {
            /* Server notification MUST be done from under the lock; otherwise
             * the machine state here and on the server might go out of sync
             * which can lead to various unexpected results (like the machine
             * state being >= MachineState_Running on the server, while the
             * session state is already SessionState_Unlocked at the same time
             * there).
             *
             * Cross-lock conditions should be carefully watched out: calling
             * UpdateState we will require Machine and SessionMachine locks
             * (remember that here we're holding the Console lock here, and also
             * all locks that have been entered by the thread before calling
             * this method).
             */
            LogFlowThisFunc(("Doing mControl->UpdateState()...\n"));
            rc = mControl->UpdateState(aMachineState);
            LogFlowThisFunc(("mControl->UpdateState()=%Rhrc\n", rc));
        }
    }

    return rc;
}

/**
 * Searches for a shared folder with the given logical name
 * in the collection of shared folders.
 *
 * @param aName            logical name of the shared folder
 * @param aSharedFolder    where to return the found object
 * @param aSetError        whether to set the error info if the folder is
 *                         not found
 * @return
 *     S_OK when found or E_INVALIDARG when not found
 *
 * @note The caller must lock this object for writing.
 */
HRESULT Console::findSharedFolder(const Utf8Str &strName,
                                  ComObjPtr<SharedFolder> &aSharedFolder,
                                  bool aSetError /* = false */)
{
    /* sanity check */
    AssertReturn(isWriteLockOnCurrentThread(), E_FAIL);

    SharedFolderMap::const_iterator it = m_mapSharedFolders.find(strName);
    if (it != m_mapSharedFolders.end())
    {
        aSharedFolder = it->second;
        return S_OK;
    }

    if (aSetError)
        setError(VBOX_E_FILE_ERROR,
            tr("Could not find a shared folder named '%s'."),
            strName.c_str());

    return VBOX_E_FILE_ERROR;
}

/**
 * Fetches the list of global or machine shared folders from the server.
 *
 * @param aGlobal true to fetch global folders.
 *
 * @note The caller must lock this object for writing.
 */
HRESULT Console::fetchSharedFolders(BOOL aGlobal)
{
    /* sanity check */
    AssertReturn(AutoCaller(this).state() == InInit ||
                 isWriteLockOnCurrentThread(), E_FAIL);

    LogFlowThisFunc(("Entering\n"));

    /* Check if we're online and keep it that way. */
    SafeVMPtrQuiet ptrVM(this);
    AutoVMCallerQuietWeak autoVMCaller(this);
    bool const online = ptrVM.isOk()
                     && m_pVMMDev
                     && m_pVMMDev->isShFlActive();

    HRESULT rc = S_OK;

    try
    {
        if (aGlobal)
        {
            /// @todo grab & process global folders when they are done
        }
        else
        {
            SharedFolderDataMap oldFolders;
            if (online)
                oldFolders = m_mapMachineSharedFolders;

            m_mapMachineSharedFolders.clear();

            SafeIfaceArray<ISharedFolder> folders;
            rc = mMachine->COMGETTER(SharedFolders)(ComSafeArrayAsOutParam(folders));
            if (FAILED(rc)) throw rc;

            for (size_t i = 0; i < folders.size(); ++i)
            {
                ComPtr<ISharedFolder> pSharedFolder = folders[i];

                Bstr bstrName;
                Bstr bstrHostPath;
                BOOL writable;
                BOOL autoMount;

                rc = pSharedFolder->COMGETTER(Name)(bstrName.asOutParam());
                if (FAILED(rc)) throw rc;
                Utf8Str strName(bstrName);

                rc = pSharedFolder->COMGETTER(HostPath)(bstrHostPath.asOutParam());
                if (FAILED(rc)) throw rc;
                Utf8Str strHostPath(bstrHostPath);

                rc = pSharedFolder->COMGETTER(Writable)(&writable);
                if (FAILED(rc)) throw rc;

                rc = pSharedFolder->COMGETTER(AutoMount)(&autoMount);
                if (FAILED(rc)) throw rc;

                m_mapMachineSharedFolders.insert(std::make_pair(strName,
                                                                SharedFolderData(strHostPath, writable, autoMount)));

                /* send changes to HGCM if the VM is running */
                if (online)
                {
                    SharedFolderDataMap::iterator it = oldFolders.find(strName);
                    if (    it == oldFolders.end()
                         || it->second.m_strHostPath != strHostPath)
                    {
                        /* a new machine folder is added or
                         * the existing machine folder is changed */
                        if (m_mapSharedFolders.find(strName) != m_mapSharedFolders.end())
                            ; /* the console folder exists, nothing to do */
                        else
                        {
                            /* remove the old machine folder (when changed)
                             * or the global folder if any (when new) */
                            if (    it != oldFolders.end()
                                 || m_mapGlobalSharedFolders.find(strName) != m_mapGlobalSharedFolders.end()
                               )
                            {
                                rc = removeSharedFolder(strName);
                                if (FAILED(rc)) throw rc;
                            }

                            /* create the new machine folder */
                            rc = createSharedFolder(strName,
                                                    SharedFolderData(strHostPath,
                                                                     writable,
                                                                     autoMount));
                            if (FAILED(rc)) throw rc;
                        }
                    }
                    /* forget the processed (or identical) folder */
                    if (it != oldFolders.end())
                        oldFolders.erase(it);
                }
            }

            /* process outdated (removed) folders */
            if (online)
            {
                for (SharedFolderDataMap::const_iterator it = oldFolders.begin();
                     it != oldFolders.end(); ++it)
                {
                    if (m_mapSharedFolders.find(it->first) != m_mapSharedFolders.end())
                        ; /* the console folder exists, nothing to do */
                    else
                    {
                        /* remove the outdated machine folder */
                        rc = removeSharedFolder(it->first);
                        if (FAILED(rc)) throw rc;

                        /* create the global folder if there is any */
                        SharedFolderDataMap::const_iterator git =
                            m_mapGlobalSharedFolders.find(it->first);
                        if (git != m_mapGlobalSharedFolders.end())
                        {
                            rc = createSharedFolder(git->first, git->second);
                            if (FAILED(rc)) throw rc;
                        }
                    }
                }
            }
        }
    }
    catch (HRESULT rc2)
    {
        if (online)
            setVMRuntimeErrorCallbackF(ptrVM, this, 0, "BrokenSharedFolder",
                                       N_("Broken shared folder!"));
    }

    LogFlowThisFunc(("Leaving\n"));

    return rc;
}

/**
 * Searches for a shared folder with the given name in the list of machine
 * shared folders and then in the list of the global shared folders.
 *
 * @param aName    Name of the folder to search for.
 * @param aIt      Where to store the pointer to the found folder.
 * @return         @c true if the folder was found and @c false otherwise.
 *
 * @note The caller must lock this object for reading.
 */
bool Console::findOtherSharedFolder(const Utf8Str &strName,
                                    SharedFolderDataMap::const_iterator &aIt)
{
    /* sanity check */
    AssertReturn(isWriteLockOnCurrentThread(), false);

    /* first, search machine folders */
    aIt = m_mapMachineSharedFolders.find(strName);
    if (aIt != m_mapMachineSharedFolders.end())
        return true;

    /* second, search machine folders */
    aIt = m_mapGlobalSharedFolders.find(strName);
    if (aIt != m_mapGlobalSharedFolders.end())
        return true;

    return false;
}

/**
 * Calls the HGCM service to add a shared folder definition.
 *
 * @param aName        Shared folder name.
 * @param aHostPath    Shared folder path.
 *
 * @note Must be called from under AutoVMCaller and when mpVM != NULL!
 * @note Doesn't lock anything.
 */
HRESULT Console::createSharedFolder(const Utf8Str &strName, const SharedFolderData &aData)
{
    ComAssertRet(strName.isNotEmpty(), E_FAIL);
    ComAssertRet(aData.m_strHostPath.isNotEmpty(), E_FAIL);

    /* sanity checks */
    AssertReturn(mpUVM, E_FAIL);
    AssertReturn(m_pVMMDev && m_pVMMDev->isShFlActive(), E_FAIL);

    VBOXHGCMSVCPARM parms[SHFL_CPARMS_ADD_MAPPING2];
    SHFLSTRING *pFolderName, *pMapName;
    size_t cbString;

    Log(("Adding shared folder '%s' -> '%s'\n", strName.c_str(), aData.m_strHostPath.c_str()));

    // check whether the path is valid and exists
    char hostPathFull[RTPATH_MAX];
    int vrc = RTPathAbsEx(NULL,
                          aData.m_strHostPath.c_str(),
                          hostPathFull,
                          sizeof(hostPathFull));
    if (RT_FAILURE(vrc))
        return setError(E_INVALIDARG,
                        tr("Invalid shared folder path: '%s' (%Rrc)"),
                        aData.m_strHostPath.c_str(), vrc);
    if (!RTPathExists(hostPathFull))
        return setError(E_INVALIDARG,
                        tr("Shared folder path '%s' does not exist on the host"),
                        aData.m_strHostPath.c_str());
    /* Check whether the path is full (absolute) */
    if (RTPathCompare(aData.m_strHostPath.c_str(), hostPathFull) != 0)
        return setError(E_INVALIDARG,
                        tr("Shared folder path '%s' is not absolute"),
                        aData.m_strHostPath.c_str());

    // now that we know the path is good, give it to HGCM

    Bstr bstrName(strName);
    Bstr bstrHostPath(aData.m_strHostPath);

    cbString = (bstrHostPath.length() + 1) * sizeof(RTUTF16);
    if (cbString >= UINT16_MAX)
        return setError(E_INVALIDARG, tr("The name is too long"));
    pFolderName = (SHFLSTRING*)RTMemAllocZ(sizeof(SHFLSTRING) + cbString);
    Assert(pFolderName);
    memcpy(pFolderName->String.ucs2, bstrHostPath.raw(), cbString);

    pFolderName->u16Size   = (uint16_t)cbString;
    pFolderName->u16Length = (uint16_t)cbString - sizeof(RTUTF16);

    parms[0].type = VBOX_HGCM_SVC_PARM_PTR;
    parms[0].u.pointer.addr = pFolderName;
    parms[0].u.pointer.size = sizeof(SHFLSTRING) + (uint16_t)cbString;

    cbString = (bstrName.length() + 1) * sizeof(RTUTF16);
    if (cbString >= UINT16_MAX)
    {
        RTMemFree(pFolderName);
        return setError(E_INVALIDARG, tr("The host path is too long"));
    }
    pMapName = (SHFLSTRING*)RTMemAllocZ(sizeof(SHFLSTRING) + cbString);
    Assert(pMapName);
    memcpy(pMapName->String.ucs2, bstrName.raw(), cbString);

    pMapName->u16Size   = (uint16_t)cbString;
    pMapName->u16Length = (uint16_t)cbString - sizeof(RTUTF16);

    parms[1].type = VBOX_HGCM_SVC_PARM_PTR;
    parms[1].u.pointer.addr = pMapName;
    parms[1].u.pointer.size = sizeof(SHFLSTRING) + (uint16_t)cbString;

    parms[2].type = VBOX_HGCM_SVC_PARM_32BIT;
    parms[2].u.uint32 = aData.m_fWritable;

    /*
     * Auto-mount flag; is indicated by using the SHFL_CPARMS_ADD_MAPPING2
     * define below.  This shows the host service that we have supplied
     * an additional parameter (auto-mount) and keeps the actual command
     * backwards compatible.
     */
    parms[3].type = VBOX_HGCM_SVC_PARM_32BIT;
    parms[3].u.uint32 = aData.m_fAutoMount;

    vrc = m_pVMMDev->hgcmHostCall("VBoxSharedFolders",
                                  SHFL_FN_ADD_MAPPING,
                                  SHFL_CPARMS_ADD_MAPPING2, &parms[0]);
    RTMemFree(pFolderName);
    RTMemFree(pMapName);

    if (RT_FAILURE(vrc))
        return setError(E_FAIL,
            tr("Could not create a shared folder '%s' mapped to '%s' (%Rrc)"),
            strName.c_str(), aData.m_strHostPath.c_str(), vrc);

    return S_OK;
}

/**
 * Calls the HGCM service to remove the shared folder definition.
 *
 * @param aName        Shared folder name.
 *
 * @note Must be called from under AutoVMCaller and when mpVM != NULL!
 * @note Doesn't lock anything.
 */
HRESULT Console::removeSharedFolder(const Utf8Str &strName)
{
    ComAssertRet(strName.isNotEmpty(), E_FAIL);

    /* sanity checks */
    AssertReturn(mpUVM, E_FAIL);
    AssertReturn(m_pVMMDev && m_pVMMDev->isShFlActive(), E_FAIL);

    VBOXHGCMSVCPARM parms;
    SHFLSTRING *pMapName;
    size_t cbString;

    Log(("Removing shared folder '%s'\n", strName.c_str()));

    Bstr bstrName(strName);
    cbString = (bstrName.length() + 1) * sizeof(RTUTF16);
    if (cbString >= UINT16_MAX)
        return setError(E_INVALIDARG, tr("The name is too long"));
    pMapName = (SHFLSTRING *) RTMemAllocZ(sizeof(SHFLSTRING) + cbString);
    Assert(pMapName);
    memcpy(pMapName->String.ucs2, bstrName.raw(), cbString);

    pMapName->u16Size   = (uint16_t)cbString;
    pMapName->u16Length = (uint16_t)cbString - sizeof(RTUTF16);

    parms.type = VBOX_HGCM_SVC_PARM_PTR;
    parms.u.pointer.addr = pMapName;
    parms.u.pointer.size = sizeof(SHFLSTRING) + (uint16_t)cbString;

    int vrc = m_pVMMDev->hgcmHostCall("VBoxSharedFolders",
                                      SHFL_FN_REMOVE_MAPPING,
                                      1, &parms);
    RTMemFree(pMapName);
    if (RT_FAILURE(vrc))
        return setError(E_FAIL,
                        tr("Could not remove the shared folder '%s' (%Rrc)"),
                        strName.c_str(), vrc);

    return S_OK;
}

/**
 * VM state callback function. Called by the VMM
 * using its state machine states.
 *
 * Primarily used to handle VM initiated power off, suspend and state saving,
 * but also for doing termination completed work (VMSTATE_TERMINATE).
 *
 * In general this function is called in the context of the EMT.
 *
 * @param   aVM         The VM handle.
 * @param   aState      The new state.
 * @param   aOldState   The old state.
 * @param   aUser       The user argument (pointer to the Console object).
 *
 * @note Locks the Console object for writing.
 */
DECLCALLBACK(void) Console::vmstateChangeCallback(PVM aVM,
                                                  VMSTATE aState,
                                                  VMSTATE aOldState,
                                                  void *aUser)
{
    LogFlowFunc(("Changing state from %s to %s (aVM=%p)\n",
                 VMR3GetStateName(aOldState), VMR3GetStateName(aState), aVM));

    Console *that = static_cast<Console *>(aUser);
    AssertReturnVoid(that);

    AutoCaller autoCaller(that);

    /* Note that we must let this method proceed even if Console::uninit() has
     * been already called. In such case this VMSTATE change is a result of:
     * 1) powerDown() called from uninit() itself, or
     * 2) VM-(guest-)initiated power off. */
    AssertReturnVoid(   autoCaller.isOk()
                     || autoCaller.state() == InUninit);

    switch (aState)
    {
        /*
         * The VM has terminated
         */
        case VMSTATE_OFF:
        {
            AutoWriteLock alock(that COMMA_LOCKVAL_SRC_POS);

            if (that->mVMStateChangeCallbackDisabled)
                break;

            /* Do we still think that it is running? It may happen if this is a
             * VM-(guest-)initiated shutdown/poweroff.
             */
            if (   that->mMachineState != MachineState_Stopping
                && that->mMachineState != MachineState_Saving
                && that->mMachineState != MachineState_Restoring
                && that->mMachineState != MachineState_TeleportingIn
                && that->mMachineState != MachineState_FaultTolerantSyncing
                && that->mMachineState != MachineState_TeleportingPausedVM
                && !that->mVMIsAlreadyPoweringOff
               )
            {
                LogFlowFunc(("VM has powered itself off but Console still thinks it is running. Notifying.\n"));

                /* prevent powerDown() from calling VMR3PowerOff() again */
                Assert(that->mVMPoweredOff == false);
                that->mVMPoweredOff = true;

                /*
                 * request a progress object from the server
                 * (this will set the machine state to Stopping on the server
                 * to block others from accessing this machine)
                 */
                ComPtr<IProgress> pProgress;
                HRESULT rc = that->mControl->BeginPoweringDown(pProgress.asOutParam());
                AssertComRC(rc);

                /* sync the state with the server */
                that->setMachineStateLocally(MachineState_Stopping);

                /* Setup task object and thread to carry out the operation
                 * asynchronously (if we call powerDown() right here but there
                 * is one or more mpVM callers (added with addVMCaller()) we'll
                 * deadlock).
                 */
                std::auto_ptr<VMPowerDownTask> task(new VMPowerDownTask(that,
                                                                        pProgress));

                 /* If creating a task failed, this can currently mean one of
                  * two: either Console::uninit() has been called just a ms
                  * before (so a powerDown() call is already on the way), or
                  * powerDown() itself is being already executed. Just do
                  * nothing.
                  */
                if (!task->isOk())
                {
                    LogFlowFunc(("Console is already being uninitialized.\n"));
                    break;
                }

                int vrc = RTThreadCreate(NULL, Console::powerDownThread,
                                         (void *) task.get(), 0,
                                         RTTHREADTYPE_MAIN_WORKER, 0,
                                         "VMPowerDown");
                AssertMsgRCBreak(vrc, ("Could not create VMPowerDown thread (%Rrc)\n", vrc));

                /* task is now owned by powerDownThread(), so release it */
                task.release();
            }
            break;
        }

        /* The VM has been completely destroyed.
         *
         * Note: This state change can happen at two points:
         *       1) At the end of VMR3Destroy() if it was not called from EMT.
         *       2) At the end of vmR3EmulationThread if VMR3Destroy() was
         *          called by EMT.
         */
        case VMSTATE_TERMINATED:
        {
            AutoWriteLock alock(that COMMA_LOCKVAL_SRC_POS);

            if (that->mVMStateChangeCallbackDisabled)
                break;

            /* Terminate host interface networking. If aVM is NULL, we've been
             * manually called from powerUpThread() either before calling
             * VMR3Create() or after VMR3Create() failed, so no need to touch
             * networking.
             */
            if (aVM)
                that->powerDownHostInterfaces();

            /* From now on the machine is officially powered down or remains in
             * the Saved state.
             */
            switch (that->mMachineState)
            {
                default:
                    AssertFailed();
                    /* fall through */
                case MachineState_Stopping:
                    /* successfully powered down */
                    that->setMachineState(MachineState_PoweredOff);
                    break;
                case MachineState_Saving:
                    /* successfully saved */
                    that->setMachineState(MachineState_Saved);
                    break;
                case MachineState_Starting:
                    /* failed to start, but be patient: set back to PoweredOff
                     * (for similarity with the below) */
                    that->setMachineState(MachineState_PoweredOff);
                    break;
                case MachineState_Restoring:
                    /* failed to load the saved state file, but be patient: set
                     * back to Saved (to preserve the saved state file) */
                    that->setMachineState(MachineState_Saved);
                    break;
                case MachineState_TeleportingIn:
                    /* Teleportation failed or was canceled.  Back to powered off. */
                    that->setMachineState(MachineState_PoweredOff);
                    break;
                case MachineState_TeleportingPausedVM:
                    /* Successfully teleported the VM. */
                    that->setMachineState(MachineState_Teleported);
                    break;
                case MachineState_FaultTolerantSyncing:
                    /* Fault tolerant sync failed or was canceled.  Back to powered off. */
                    that->setMachineState(MachineState_PoweredOff);
                    break;
            }
            break;
        }

        case VMSTATE_RESETTING:
        {
#ifdef VBOX_WITH_GUEST_PROPS
            /* Do not take any read/write locks here! */
            that->guestPropertiesHandleVMReset();
#endif
            break;
        }

        case VMSTATE_SUSPENDED:
        {
            AutoWriteLock alock(that COMMA_LOCKVAL_SRC_POS);

            if (that->mVMStateChangeCallbackDisabled)
                break;

            switch (that->mMachineState)
            {
                case MachineState_Teleporting:
                    that->setMachineState(MachineState_TeleportingPausedVM);
                    break;

                case MachineState_LiveSnapshotting:
                    that->setMachineState(MachineState_Saving);
                    break;

                case MachineState_TeleportingPausedVM:
                case MachineState_Saving:
                case MachineState_Restoring:
                case MachineState_Stopping:
                case MachineState_TeleportingIn:
                case MachineState_FaultTolerantSyncing:
                    /* The worker thread handles the transition. */
                    break;

                default:
                    AssertMsgFailed(("%s\n", Global::stringifyMachineState(that->mMachineState)));
                case MachineState_Running:
                    that->setMachineState(MachineState_Paused);
                    break;

                case MachineState_Paused:
                    /* Nothing to do. */
                    break;
            }
            break;
        }

        case VMSTATE_SUSPENDED_LS:
        case VMSTATE_SUSPENDED_EXT_LS:
        {
            AutoWriteLock alock(that COMMA_LOCKVAL_SRC_POS);
            if (that->mVMStateChangeCallbackDisabled)
                break;
            switch (that->mMachineState)
            {
                case MachineState_Teleporting:
                    that->setMachineState(MachineState_TeleportingPausedVM);
                    break;

                case MachineState_LiveSnapshotting:
                    that->setMachineState(MachineState_Saving);
                    break;

                case MachineState_TeleportingPausedVM:
                case MachineState_Saving:
                    /* ignore */
                    break;

                default:
                    AssertMsgFailed(("%s/%s -> %s\n", Global::stringifyMachineState(that->mMachineState), VMR3GetStateName(aOldState),  VMR3GetStateName(aState) ));
                    that->setMachineState(MachineState_Paused);
                    break;
            }
            break;
        }

        case VMSTATE_RUNNING:
        {
            if (   aOldState == VMSTATE_POWERING_ON
                || aOldState == VMSTATE_RESUMING
                || aOldState == VMSTATE_RUNNING_FT)
            {
                AutoWriteLock alock(that COMMA_LOCKVAL_SRC_POS);

                if (that->mVMStateChangeCallbackDisabled)
                    break;

                Assert(   (   (   that->mMachineState == MachineState_Starting
                               || that->mMachineState == MachineState_Paused)
                           && aOldState == VMSTATE_POWERING_ON)
                       || (   (   that->mMachineState == MachineState_Restoring
                               || that->mMachineState == MachineState_TeleportingIn
                               || that->mMachineState == MachineState_Paused
                               || that->mMachineState == MachineState_Saving
                              )
                           && aOldState == VMSTATE_RESUMING)
                       || (   that->mMachineState == MachineState_FaultTolerantSyncing
                           && aOldState == VMSTATE_RUNNING_FT));

                that->setMachineState(MachineState_Running);
            }

            break;
        }

        case VMSTATE_RUNNING_LS:
            AssertMsg(   that->mMachineState == MachineState_LiveSnapshotting
                      || that->mMachineState == MachineState_Teleporting,
                      ("%s/%s -> %s\n", Global::stringifyMachineState(that->mMachineState), VMR3GetStateName(aOldState),  VMR3GetStateName(aState) ));
            break;

        case VMSTATE_RUNNING_FT:
            AssertMsg(that->mMachineState == MachineState_FaultTolerantSyncing,
                      ("%s/%s -> %s\n", Global::stringifyMachineState(that->mMachineState), VMR3GetStateName(aOldState),  VMR3GetStateName(aState) ));
            break;

        case VMSTATE_FATAL_ERROR:
        {
            AutoWriteLock alock(that COMMA_LOCKVAL_SRC_POS);

            if (that->mVMStateChangeCallbackDisabled)
                break;

            /* Fatal errors are only for running VMs. */
            Assert(Global::IsOnline(that->mMachineState));

            /* Note! 'Pause' is used here in want of something better.  There
             *       are currently only two places where fatal errors might be
             *       raised, so it is not worth adding a new externally
             *       visible state for this yet.  */
            that->setMachineState(MachineState_Paused);
            break;
        }

        case VMSTATE_GURU_MEDITATION:
        {
            AutoWriteLock alock(that COMMA_LOCKVAL_SRC_POS);

            if (that->mVMStateChangeCallbackDisabled)
                break;

            /* Guru are only for running VMs */
            Assert(Global::IsOnline(that->mMachineState));

            that->setMachineState(MachineState_Stuck);
            break;
        }

        default: /* shut up gcc */
            break;
    }
}

#ifdef VBOX_WITH_USB
/**
 * Sends a request to VMM to attach the given host device.
 * After this method succeeds, the attached device will appear in the
 * mUSBDevices collection.
 *
 * @param aHostDevice  device to attach
 *
 * @note Synchronously calls EMT.
 * @note Must be called from under this object's lock.
 */
HRESULT Console::attachUSBDevice(IUSBDevice *aHostDevice, ULONG aMaskedIfs)
{
    AssertReturn(aHostDevice, E_FAIL);
    AssertReturn(isWriteLockOnCurrentThread(), E_FAIL);

    /* still want a lock object because we need to leave it */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT hrc;

    /*
     * Get the address and the Uuid, and call the pfnCreateProxyDevice roothub
     * method in EMT (using usbAttachCallback()).
     */
    Bstr BstrAddress;
    hrc = aHostDevice->COMGETTER(Address)(BstrAddress.asOutParam());
    ComAssertComRCRetRC(hrc);

    Utf8Str Address(BstrAddress);

    Bstr id;
    hrc = aHostDevice->COMGETTER(Id)(id.asOutParam());
    ComAssertComRCRetRC(hrc);
    Guid uuid(id);

    BOOL fRemote = FALSE;
    hrc = aHostDevice->COMGETTER(Remote)(&fRemote);
    ComAssertComRCRetRC(hrc);

    /* Get the VM handle. */
    SafeVMPtr ptrVM(this);
    if (!ptrVM.isOk())
        return ptrVM.rc();

    LogFlowThisFunc(("Proxying USB device '%s' {%RTuuid}...\n",
                      Address.c_str(), uuid.raw()));

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

/** @todo just do everything here and only wrap the PDMR3Usb call. That'll offload some notification stuff from the EMT thread. */
    int vrc = VMR3ReqCallWait(ptrVM, VMCPUID_ANY,
                              (PFNRT)usbAttachCallback, 7,
                              this, ptrVM.raw(), aHostDevice, uuid.raw(), fRemote, Address.c_str(), aMaskedIfs);

    /* restore the lock */
    alock.enter();

    /* hrc is S_OK here */

    if (RT_FAILURE(vrc))
    {
        LogWarningThisFunc(("Failed to create proxy device for '%s' {%RTuuid} (%Rrc)\n",
                            Address.c_str(), uuid.raw(), vrc));

        switch (vrc)
        {
            case VERR_VUSB_NO_PORTS:
                hrc = setError(E_FAIL,
                    tr("Failed to attach the USB device. (No available ports on the USB controller)."));
                break;
            case VERR_VUSB_USBFS_PERMISSION:
                hrc = setError(E_FAIL,
                    tr("Not permitted to open the USB device, check usbfs options"));
                break;
            default:
                hrc = setError(E_FAIL,
                    tr("Failed to create a proxy device for the USB device. (Error: %Rrc)"),
                    vrc);
                break;
        }
    }

    return hrc;
}

/**
 * USB device attach callback used by AttachUSBDevice().
 * Note that AttachUSBDevice() doesn't return until this callback is executed,
 * so we don't use AutoCaller and don't care about reference counters of
 * interface pointers passed in.
 *
 * @thread EMT
 * @note Locks the console object for writing.
 */
//static
DECLCALLBACK(int)
Console::usbAttachCallback(Console *that, PVM pVM, IUSBDevice *aHostDevice, PCRTUUID aUuid, bool aRemote, const char *aAddress, ULONG aMaskedIfs)
{
    LogFlowFuncEnter();
    LogFlowFunc(("that={%p}\n", that));

    AssertReturn(that && aUuid, VERR_INVALID_PARAMETER);

    void *pvRemoteBackend = NULL;
    if (aRemote)
    {
        RemoteUSBDevice *pRemoteUSBDevice = static_cast<RemoteUSBDevice *>(aHostDevice);
        Guid guid(*aUuid);

        pvRemoteBackend = that->consoleVRDPServer()->USBBackendRequestPointer(pRemoteUSBDevice->clientId(), &guid);
        if (!pvRemoteBackend)
            return VERR_INVALID_PARAMETER; /* The clientId is invalid then. */
    }

    USHORT portVersion = 1;
    HRESULT hrc = aHostDevice->COMGETTER(PortVersion)(&portVersion);
    AssertComRCReturn(hrc, VERR_GENERAL_FAILURE);
    Assert(portVersion == 1 || portVersion == 2);

    int vrc = PDMR3USBCreateProxyDevice(pVM, aUuid, aRemote, aAddress, pvRemoteBackend,
                                        portVersion == 1 ? VUSB_STDVER_11 : VUSB_STDVER_20, aMaskedIfs);
    if (RT_SUCCESS(vrc))
    {
        /* Create a OUSBDevice and add it to the device list */
        ComObjPtr<OUSBDevice> pUSBDevice;
        pUSBDevice.createObject();
        hrc = pUSBDevice->init(aHostDevice);
        AssertComRC(hrc);

        AutoWriteLock alock(that COMMA_LOCKVAL_SRC_POS);
        that->mUSBDevices.push_back(pUSBDevice);
        LogFlowFunc(("Attached device {%RTuuid}\n", pUSBDevice->id().raw()));

        /* notify callbacks */
        that->onUSBDeviceStateChange(pUSBDevice, true /* aAttached */, NULL);
    }

    LogFlowFunc(("vrc=%Rrc\n", vrc));
    LogFlowFuncLeave();
    return vrc;
}

/**
 * Sends a request to VMM to detach the given host device.  After this method
 * succeeds, the detached device will disappear from the mUSBDevices
 * collection.
 *
 * @param aIt  Iterator pointing to the device to detach.
 *
 * @note Synchronously calls EMT.
 * @note Must be called from under this object's lock.
 */
HRESULT Console::detachUSBDevice(USBDeviceList::iterator &aIt)
{
    AssertReturn(isWriteLockOnCurrentThread(), E_FAIL);

    /* still want a lock object because we need to leave it */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Get the VM handle. */
    SafeVMPtr ptrVM(this);
    if (!ptrVM.isOk())
        return ptrVM.rc();

    /* if the device is attached, then there must at least one USB hub. */
    AssertReturn(PDMR3USBHasHub(ptrVM), E_FAIL);

    LogFlowThisFunc(("Detaching USB proxy device {%RTuuid}...\n",
                     (*aIt)->id().raw()));

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

/** @todo just do everything here and only wrap the PDMR3Usb call. That'll offload some notification stuff from the EMT thread. */
    int vrc = VMR3ReqCallWait(ptrVM, VMCPUID_ANY,
                              (PFNRT)usbDetachCallback, 5,
                              this, ptrVM.raw(), &aIt, (*aIt)->id().raw());
    ComAssertRCRet(vrc, E_FAIL);

    return S_OK;
}

/**
 * USB device detach callback used by DetachUSBDevice().
 * Note that DetachUSBDevice() doesn't return until this callback is executed,
 * so we don't use AutoCaller and don't care about reference counters of
 * interface pointers passed in.
 *
 * @thread EMT
 * @note Locks the console object for writing.
 */
//static
DECLCALLBACK(int)
Console::usbDetachCallback(Console *that, PVM pVM, USBDeviceList::iterator *aIt, PCRTUUID aUuid)
{
    LogFlowFuncEnter();
    LogFlowFunc(("that={%p}\n", that));

    AssertReturn(that && aUuid, VERR_INVALID_PARAMETER);
    ComObjPtr<OUSBDevice> pUSBDevice = **aIt;

    /*
     * If that was a remote device, release the backend pointer.
     * The pointer was requested in usbAttachCallback.
     */
    BOOL fRemote = FALSE;

    HRESULT hrc2 = (**aIt)->COMGETTER(Remote)(&fRemote);
    if (FAILED(hrc2))
        setErrorStatic(hrc2, "GetRemote() failed");

    if (fRemote)
    {
        Guid guid(*aUuid);
        that->consoleVRDPServer()->USBBackendReleasePointer(&guid);
    }

    int vrc = PDMR3USBDetachDevice(pVM, aUuid);

    if (RT_SUCCESS(vrc))
    {
        AutoWriteLock alock(that COMMA_LOCKVAL_SRC_POS);

        /* Remove the device from the collection */
        that->mUSBDevices.erase(*aIt);
        LogFlowFunc(("Detached device {%RTuuid}\n", pUSBDevice->id().raw()));

        /* notify callbacks */
        that->onUSBDeviceStateChange(pUSBDevice, false /* aAttached */, NULL);
    }

    LogFlowFunc(("vrc=%Rrc\n", vrc));
    LogFlowFuncLeave();
    return vrc;
}
#endif /* VBOX_WITH_USB */

/* Note: FreeBSD needs this whether netflt is used or not. */
#if ((defined(RT_OS_LINUX) && !defined(VBOX_WITH_NETFLT)) || defined(RT_OS_FREEBSD))
/**
 * Helper function to handle host interface device creation and attachment.
 *
 * @param   networkAdapter the network adapter which attachment should be reset
 * @return  COM status code
 *
 * @note The caller must lock this object for writing.
 *
 * @todo Move this back into the driver!
 */
HRESULT Console::attachToTapInterface(INetworkAdapter *networkAdapter)
{
    LogFlowThisFunc(("\n"));
    /* sanity check */
    AssertReturn(isWriteLockOnCurrentThread(), E_FAIL);

# ifdef VBOX_STRICT
    /* paranoia */
    NetworkAttachmentType_T attachment;
    networkAdapter->COMGETTER(AttachmentType)(&attachment);
    Assert(attachment == NetworkAttachmentType_Bridged);
# endif /* VBOX_STRICT */

    HRESULT rc = S_OK;

    ULONG slot = 0;
    rc = networkAdapter->COMGETTER(Slot)(&slot);
    AssertComRC(rc);

# ifdef RT_OS_LINUX
    /*
     * Allocate a host interface device
     */
    int rcVBox = RTFileOpen(&maTapFD[slot], "/dev/net/tun",
                            RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_INHERIT);
    if (RT_SUCCESS(rcVBox))
    {
        /*
         * Set/obtain the tap interface.
         */
        struct ifreq IfReq;
        memset(&IfReq, 0, sizeof(IfReq));
        /* The name of the TAP interface we are using */
        Bstr tapDeviceName;
        rc = networkAdapter->COMGETTER(BridgedInterface)(tapDeviceName.asOutParam());
        if (FAILED(rc))
            tapDeviceName.setNull(); /* Is this necessary? */
        if (tapDeviceName.isEmpty())
        {
            LogRel(("No TAP device name was supplied.\n"));
            rc = setError(E_FAIL, tr("No TAP device name was supplied for the host networking interface"));
        }

        if (SUCCEEDED(rc))
        {
            /* If we are using a static TAP device then try to open it. */
            Utf8Str str(tapDeviceName);
            if (str.length() <= sizeof(IfReq.ifr_name))
                strcpy(IfReq.ifr_name, str.c_str());
            else
                memcpy(IfReq.ifr_name, str.c_str(), sizeof(IfReq.ifr_name) - 1); /** @todo bitch about names which are too long... */
            IfReq.ifr_flags = IFF_TAP | IFF_NO_PI;
            rcVBox = ioctl(maTapFD[slot], TUNSETIFF, &IfReq);
            if (rcVBox != 0)
            {
                LogRel(("Failed to open the host network interface %ls\n", tapDeviceName.raw()));
                rc = setError(E_FAIL,
                    tr("Failed to open the host network interface %ls"),
                    tapDeviceName.raw());
            }
        }
        if (SUCCEEDED(rc))
        {
            /*
             * Make it pollable.
             */
            if (fcntl(maTapFD[slot], F_SETFL, O_NONBLOCK) != -1)
            {
                Log(("attachToTapInterface: %RTfile %ls\n", maTapFD[slot], tapDeviceName.raw()));
                /*
                 * Here is the right place to communicate the TAP file descriptor and
                 * the host interface name to the server if/when it becomes really
                 * necessary.
                 */
                maTAPDeviceName[slot] = tapDeviceName;
                rcVBox = VINF_SUCCESS;
            }
            else
            {
                int iErr = errno;

                LogRel(("Configuration error: Failed to configure /dev/net/tun non blocking. Error: %s\n", strerror(iErr)));
                rcVBox = VERR_HOSTIF_BLOCKING;
                rc = setError(E_FAIL,
                    tr("could not set up the host networking device for non blocking access: %s"),
                    strerror(errno));
            }
        }
    }
    else
    {
        LogRel(("Configuration error: Failed to open /dev/net/tun rc=%Rrc\n", rcVBox));
        switch (rcVBox)
        {
            case VERR_ACCESS_DENIED:
                /* will be handled by our caller */
                rc = rcVBox;
                break;
            default:
                rc = setError(E_FAIL,
                    tr("Could not set up the host networking device: %Rrc"),
                    rcVBox);
                break;
        }
    }

# elif defined(RT_OS_FREEBSD)
    /*
     * Set/obtain the tap interface.
     */
    /* The name of the TAP interface we are using */
    Bstr tapDeviceName;
    rc = networkAdapter->COMGETTER(BridgedInterface)(tapDeviceName.asOutParam());
    if (FAILED(rc))
        tapDeviceName.setNull(); /* Is this necessary? */
    if (tapDeviceName.isEmpty())
    {
        LogRel(("No TAP device name was supplied.\n"));
        rc = setError(E_FAIL, tr("No TAP device name was supplied for the host networking interface"));
    }
    char szTapdev[1024] = "/dev/";
    /* If we are using a static TAP device then try to open it. */
    Utf8Str str(tapDeviceName);
    if (str.length() + strlen(szTapdev) <= sizeof(szTapdev))
        strcat(szTapdev, str.c_str());
    else
        memcpy(szTapdev + strlen(szTapdev), str.c_str(),
               sizeof(szTapdev) - strlen(szTapdev) - 1); /** @todo bitch about names which are too long... */
    int rcVBox = RTFileOpen(&maTapFD[slot], szTapdev,
                            RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_INHERIT | RTFILE_O_NON_BLOCK);

    if (RT_SUCCESS(rcVBox))
        maTAPDeviceName[slot] = tapDeviceName;
    else
    {
        switch (rcVBox)
        {
            case VERR_ACCESS_DENIED:
                /* will be handled by our caller */
                rc = rcVBox;
                break;
            default:
                rc = setError(E_FAIL,
                    tr("Failed to open the host network interface %ls"),
                    tapDeviceName.raw());
                break;
        }
    }
# else
#  error "huh?"
# endif
    /* in case of failure, cleanup. */
    if (RT_FAILURE(rcVBox) && SUCCEEDED(rc))
    {
        LogRel(("General failure attaching to host interface\n"));
        rc = setError(E_FAIL,
            tr("General failure attaching to host interface"));
    }
    LogFlowThisFunc(("rc=%d\n", rc));
    return rc;
}


/**
 * Helper function to handle detachment from a host interface
 *
 * @param   networkAdapter the network adapter which attachment should be reset
 * @return  COM status code
 *
 * @note The caller must lock this object for writing.
 *
 * @todo Move this back into the driver!
 */
HRESULT Console::detachFromTapInterface(INetworkAdapter *networkAdapter)
{
    /* sanity check */
    LogFlowThisFunc(("\n"));
    AssertReturn(isWriteLockOnCurrentThread(), E_FAIL);

    HRESULT rc = S_OK;
# ifdef VBOX_STRICT
    /* paranoia */
    NetworkAttachmentType_T attachment;
    networkAdapter->COMGETTER(AttachmentType)(&attachment);
    Assert(attachment == NetworkAttachmentType_Bridged);
# endif /* VBOX_STRICT */

    ULONG slot = 0;
    rc = networkAdapter->COMGETTER(Slot)(&slot);
    AssertComRC(rc);

    /* is there an open TAP device? */
    if (maTapFD[slot] != NIL_RTFILE)
    {
        /*
         * Close the file handle.
         */
        Bstr tapDeviceName, tapTerminateApplication;
        bool isStatic = true;
        rc = networkAdapter->COMGETTER(BridgedInterface)(tapDeviceName.asOutParam());
        if (FAILED(rc) || tapDeviceName.isEmpty())
        {
            /* If the name is empty, this is a dynamic TAP device, so close it now,
               so that the termination script can remove the interface. Otherwise we still
               need the FD to pass to the termination script. */
            isStatic = false;
            int rcVBox = RTFileClose(maTapFD[slot]);
            AssertRC(rcVBox);
            maTapFD[slot] = NIL_RTFILE;
        }
        if (isStatic)
        {
            /* If we are using a static TAP device, we close it now, after having called the
               termination script. */
            int rcVBox = RTFileClose(maTapFD[slot]);
            AssertRC(rcVBox);
        }
        /* the TAP device name and handle are no longer valid */
        maTapFD[slot] = NIL_RTFILE;
        maTAPDeviceName[slot] = "";
    }
    LogFlowThisFunc(("returning %d\n", rc));
    return rc;
}
#endif /* (RT_OS_LINUX || RT_OS_FREEBSD) && !VBOX_WITH_NETFLT */

/**
 * Called at power down to terminate host interface networking.
 *
 * @note The caller must lock this object for writing.
 */
HRESULT Console::powerDownHostInterfaces()
{
    LogFlowThisFunc(("\n"));

    /* sanity check */
    AssertReturn(isWriteLockOnCurrentThread(), E_FAIL);

    /*
     * host interface termination handling
     */
    HRESULT rc = S_OK;
    ComPtr<IVirtualBox> pVirtualBox;
    mMachine->COMGETTER(Parent)(pVirtualBox.asOutParam());
    ComPtr<ISystemProperties> pSystemProperties;
    if (pVirtualBox)
        pVirtualBox->COMGETTER(SystemProperties)(pSystemProperties.asOutParam());
    ChipsetType_T chipsetType = ChipsetType_PIIX3;
    mMachine->COMGETTER(ChipsetType)(&chipsetType);
    ULONG maxNetworkAdapters = 0;
    if (pSystemProperties)
        pSystemProperties->GetMaxNetworkAdapters(chipsetType, &maxNetworkAdapters);

    for (ULONG slot = 0; slot < maxNetworkAdapters; slot++)
    {
        ComPtr<INetworkAdapter> pNetworkAdapter;
        rc = mMachine->GetNetworkAdapter(slot, pNetworkAdapter.asOutParam());
        if (FAILED(rc)) break;

        BOOL enabled = FALSE;
        pNetworkAdapter->COMGETTER(Enabled)(&enabled);
        if (!enabled)
            continue;

        NetworkAttachmentType_T attachment;
        pNetworkAdapter->COMGETTER(AttachmentType)(&attachment);
        if (attachment == NetworkAttachmentType_Bridged)
        {
#if ((defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD)) && !defined(VBOX_WITH_NETFLT))
            HRESULT rc2 = detachFromTapInterface(pNetworkAdapter);
            if (FAILED(rc2) && SUCCEEDED(rc))
                rc = rc2;
#endif /* (RT_OS_LINUX || RT_OS_FREEBSD) && !VBOX_WITH_NETFLT */
        }
    }

    return rc;
}


/**
 * Process callback handler for VMR3LoadFromFile, VMR3LoadFromStream, VMR3Save
 * and VMR3Teleport.
 *
 * @param   pVM         The VM handle.
 * @param   uPercent    Completion percentage (0-100).
 * @param   pvUser      Pointer to an IProgress instance.
 * @return  VINF_SUCCESS.
 */
/*static*/
DECLCALLBACK(int) Console::stateProgressCallback(PVM pVM, unsigned uPercent, void *pvUser)
{
    IProgress *pProgress = static_cast<IProgress *>(pvUser);

    /* update the progress object */
    if (pProgress)
        pProgress->SetCurrentOperationProgress(uPercent);

    return VINF_SUCCESS;
}

/**
 * @copydoc FNVMATERROR
 *
 * @remarks Might be some tiny serialization concerns with access to the string
 *          object here...
 */
/*static*/ DECLCALLBACK(void)
Console::genericVMSetErrorCallback(PVM pVM, void *pvUser, int rc, RT_SRC_POS_DECL,
                                   const char *pszErrorFmt, va_list va)
{
    Utf8Str *pErrorText = (Utf8Str *)pvUser;
    AssertPtr(pErrorText);

    /* We ignore RT_SRC_POS_DECL arguments to avoid confusion of end-users. */
    va_list va2;
    va_copy(va2, va);

    /* Append to any the existing error message. */
    if (pErrorText->length())
        *pErrorText = Utf8StrFmt("%s.\n%N (%Rrc)", pErrorText->c_str(),
                                 pszErrorFmt, &va2, rc, rc);
    else
        *pErrorText = Utf8StrFmt("%N (%Rrc)", pszErrorFmt, &va2, rc, rc);

    va_end(va2);
}

/**
 * VM runtime error callback function.
 * See VMSetRuntimeError for the detailed description of parameters.
 *
 * @param   pVM             The VM handle.
 * @param   pvUser          The user argument.
 * @param   fFlags          The action flags. See VMSETRTERR_FLAGS_*.
 * @param   pszErrorId      Error ID string.
 * @param   pszFormat       Error message format string.
 * @param   va              Error message arguments.
 * @thread EMT.
 */
/* static */ DECLCALLBACK(void)
Console::setVMRuntimeErrorCallback(PVM pVM, void *pvUser, uint32_t fFlags,
                                   const char *pszErrorId,
                                   const char *pszFormat, va_list va)
{
    bool const fFatal = !!(fFlags & VMSETRTERR_FLAGS_FATAL);
    LogFlowFuncEnter();

    Console *that = static_cast<Console *>(pvUser);
    AssertReturnVoid(that);

    Utf8Str message(pszFormat, va);

    LogRel(("Console: VM runtime error: fatal=%RTbool, errorID=%s message=\"%s\"\n",
             fFatal, pszErrorId, message.c_str()));

    that->onRuntimeError(BOOL(fFatal), Bstr(pszErrorId).raw(),
                         Bstr(message).raw());

    LogFlowFuncLeave();
}

/**
 * Captures USB devices that match filters of the VM.
 * Called at VM startup.
 *
 * @param   pVM     The VM handle.
 *
 * @note The caller must lock this object for writing.
 */
HRESULT Console::captureUSBDevices(PVM pVM)
{
    LogFlowThisFunc(("\n"));

    /* sanity check */
    ComAssertRet(isWriteLockOnCurrentThread(), E_FAIL);

    /* If the machine has an USB controller, ask the USB proxy service to
     * capture devices */
    PPDMIBASE pBase;
    int vrc = PDMR3QueryLun(pVM, "usb-ohci", 0, 0, &pBase);
    if (RT_SUCCESS(vrc))
    {
        /* leave the lock before calling Host in VBoxSVC since Host may call
         * us back from under its lock (e.g. onUSBDeviceAttach()) which would
         * produce an inter-process dead-lock otherwise. */
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        alock.leave();

        HRESULT hrc = mControl->AutoCaptureUSBDevices();
        ComAssertComRCRetRC(hrc);
    }
    else if (   vrc == VERR_PDM_DEVICE_NOT_FOUND
             || vrc == VERR_PDM_DEVICE_INSTANCE_NOT_FOUND)
        vrc = VINF_SUCCESS;
    else
        AssertRC(vrc);

    return RT_SUCCESS(vrc) ? S_OK : E_FAIL;
}


/**
 * Detach all USB device which are attached to the VM for the
 * purpose of clean up and such like.
 *
 * @note The caller must lock this object for writing.
 */
void Console::detachAllUSBDevices(bool aDone)
{
    LogFlowThisFunc(("aDone=%RTbool\n", aDone));

    /* sanity check */
    AssertReturnVoid(isWriteLockOnCurrentThread());

    mUSBDevices.clear();

    /* leave the lock before calling Host in VBoxSVC since Host may call
     * us back from under its lock (e.g. onUSBDeviceAttach()) which would
     * produce an inter-process dead-lock otherwise. */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    alock.leave();

    mControl->DetachAllUSBDevices(aDone);
}

/**
 * @note Locks this object for writing.
 */
void Console::processRemoteUSBDevices(uint32_t u32ClientId, VRDEUSBDEVICEDESC *pDevList, uint32_t cbDevList, bool fDescExt)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("u32ClientId = %d, pDevList=%p, cbDevList = %d, fDescExt = %d\n", u32ClientId, pDevList, cbDevList, fDescExt));

    AutoCaller autoCaller(this);
    if (!autoCaller.isOk())
    {
        /* Console has been already uninitialized, deny request */
        AssertMsgFailed(("Console is already uninitialized\n"));
        LogFlowThisFunc(("Console is already uninitialized\n"));
        LogFlowThisFuncLeave();
        return;
    }

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /*
     * Mark all existing remote USB devices as dirty.
     */
    for (RemoteUSBDeviceList::iterator it = mRemoteUSBDevices.begin();
         it != mRemoteUSBDevices.end();
         ++it)
    {
        (*it)->dirty(true);
    }

    /*
     * Process the pDevList and add devices those are not already in the mRemoteUSBDevices list.
     */
    /** @todo (sunlover) REMOTE_USB Strict validation of the pDevList. */
    VRDEUSBDEVICEDESC *e = pDevList;

    /* The cbDevList condition must be checked first, because the function can
     * receive pDevList = NULL and cbDevList = 0 on client disconnect.
     */
    while (cbDevList >= 2 && e->oNext)
    {
        /* Sanitize incoming strings in case they aren't valid UTF-8. */
        if (e->oManufacturer)
            RTStrPurgeEncoding((char *)e + e->oManufacturer);
        if (e->oProduct)
            RTStrPurgeEncoding((char *)e + e->oProduct);
        if (e->oSerialNumber)
            RTStrPurgeEncoding((char *)e + e->oSerialNumber);

        LogFlowThisFunc(("vendor %04X, product %04X, name = %s\n",
                          e->idVendor, e->idProduct,
                          e->oProduct? (char *)e + e->oProduct: ""));

        bool fNewDevice = true;

        for (RemoteUSBDeviceList::iterator it = mRemoteUSBDevices.begin();
             it != mRemoteUSBDevices.end();
             ++it)
        {
            if ((*it)->devId() == e->id
                && (*it)->clientId() == u32ClientId)
            {
               /* The device is already in the list. */
               (*it)->dirty(false);
               fNewDevice = false;
               break;
            }
        }

        if (fNewDevice)
        {
            LogRel(("Remote USB: ++++ Vendor %04X. Product %04X. Name = [%s].\n",
                    e->idVendor, e->idProduct, e->oProduct? (char *)e + e->oProduct: ""));

            /* Create the device object and add the new device to list. */
            ComObjPtr<RemoteUSBDevice> pUSBDevice;
            pUSBDevice.createObject();
            pUSBDevice->init(u32ClientId, e, fDescExt);

            mRemoteUSBDevices.push_back(pUSBDevice);

            /* Check if the device is ok for current USB filters. */
            BOOL fMatched = FALSE;
            ULONG fMaskedIfs = 0;

            HRESULT hrc = mControl->RunUSBDeviceFilters(pUSBDevice, &fMatched, &fMaskedIfs);

            AssertComRC(hrc);

            LogFlowThisFunc(("USB filters return %d %#x\n", fMatched, fMaskedIfs));

            if (fMatched)
            {
                hrc = onUSBDeviceAttach(pUSBDevice, NULL, fMaskedIfs);

                /// @todo (r=dmik) warning reporting subsystem

                if (hrc == S_OK)
                {
                    LogFlowThisFunc(("Device attached\n"));
                    pUSBDevice->captured(true);
                }
            }
        }

        if (cbDevList < e->oNext)
        {
            LogWarningThisFunc(("cbDevList %d > oNext %d\n",
                                 cbDevList, e->oNext));
            break;
        }

        cbDevList -= e->oNext;

        e = (VRDEUSBDEVICEDESC *)((uint8_t *)e + e->oNext);
    }

    /*
     * Remove dirty devices, that is those which are not reported by the server anymore.
     */
    for (;;)
    {
        ComObjPtr<RemoteUSBDevice> pUSBDevice;

        RemoteUSBDeviceList::iterator it = mRemoteUSBDevices.begin();
        while (it != mRemoteUSBDevices.end())
        {
            if ((*it)->dirty())
            {
                pUSBDevice = *it;
                break;
            }

            ++it;
        }

        if (!pUSBDevice)
        {
            break;
        }

        USHORT vendorId = 0;
        pUSBDevice->COMGETTER(VendorId)(&vendorId);

        USHORT productId = 0;
        pUSBDevice->COMGETTER(ProductId)(&productId);

        Bstr product;
        pUSBDevice->COMGETTER(Product)(product.asOutParam());

        LogRel(("Remote USB: ---- Vendor %04X. Product %04X. Name = [%ls].\n",
                vendorId, productId, product.raw()));

        /* Detach the device from VM. */
        if (pUSBDevice->captured())
        {
            Bstr uuid;
            pUSBDevice->COMGETTER(Id)(uuid.asOutParam());
            onUSBDeviceDetach(uuid.raw(), NULL);
        }

        /* And remove it from the list. */
        mRemoteUSBDevices.erase(it);
    }

    LogFlowThisFuncLeave();
}

/**
 * Progress cancelation callback for fault tolerance VM poweron
 */
static void faultToleranceProgressCancelCallback(void *pvUser)
{
    PVM pVM = (PVM)pvUser;

    if (pVM)
        FTMR3CancelStandby(pVM);
}

/**
 * Thread function which starts the VM (also from saved state) and
 * track progress.
 *
 * @param   Thread      The thread id.
 * @param   pvUser      Pointer to a VMPowerUpTask structure.
 * @return  VINF_SUCCESS (ignored).
 *
 * @note Locks the Console object for writing.
 */
/*static*/
DECLCALLBACK(int) Console::powerUpThread(RTTHREAD Thread, void *pvUser)
{
    LogFlowFuncEnter();

    std::auto_ptr<VMPowerUpTask> task(static_cast<VMPowerUpTask *>(pvUser));
    AssertReturn(task.get(), VERR_INVALID_PARAMETER);

    AssertReturn(!task->mConsole.isNull(), VERR_INVALID_PARAMETER);
    AssertReturn(!task->mProgress.isNull(), VERR_INVALID_PARAMETER);

    VirtualBoxBase::initializeComForThread();

    HRESULT rc = S_OK;
    int vrc = VINF_SUCCESS;

    /* Set up a build identifier so that it can be seen from core dumps what
     * exact build was used to produce the core. */
    static char saBuildID[40];
    RTStrPrintf(saBuildID, sizeof(saBuildID), "%s%s%s%s VirtualBox %s r%u %s%s%s%s",
                "BU", "IL", "DI", "D", VBOX_VERSION_STRING, RTBldCfgRevision(), "BU", "IL", "DI", "D");

    ComObjPtr<Console> pConsole = task->mConsole;

    /* Note: no need to use addCaller() because VMPowerUpTask does that */

    /* The lock is also used as a signal from the task initiator (which
     * releases it only after RTThreadCreate()) that we can start the job */
    AutoWriteLock alock(pConsole COMMA_LOCKVAL_SRC_POS);

    /* sanity */
    Assert(pConsole->mpUVM == NULL);

    try
    {
        // Create the VMM device object, which starts the HGCM thread; do this only
        // once for the console, for the pathological case that the same console
        // object is used to power up a VM twice. VirtualBox 4.0: we now do that
        // here instead of the Console constructor (see Console::init())
        if (!pConsole->m_pVMMDev)
        {
            pConsole->m_pVMMDev = new VMMDev(pConsole);
            AssertReturn(pConsole->m_pVMMDev, E_FAIL);
        }

        /* wait for auto reset ops to complete so that we can successfully lock
         * the attached hard disks by calling LockMedia() below */
        for (VMPowerUpTask::ProgressList::const_iterator
             it = task->hardDiskProgresses.begin();
             it != task->hardDiskProgresses.end(); ++it)
        {
            HRESULT rc2 = (*it)->WaitForCompletion(-1);
            AssertComRC(rc2);
        }

        /*
         * Lock attached media. This method will also check their accessibility.
         * If we're a teleporter, we'll have to postpone this action so we can
         * migrate between local processes.
         *
         * Note! The media will be unlocked automatically by
         *       SessionMachine::setMachineState() when the VM is powered down.
         */
        if (    !task->mTeleporterEnabled
            &&  task->mEnmFaultToleranceState != FaultToleranceState_Standby)
        {
            rc = pConsole->mControl->LockMedia();
            if (FAILED(rc)) throw rc;
        }

        /* Create the VRDP server. In case of headless operation, this will
         * also create the framebuffer, required at VM creation.
         */
        ConsoleVRDPServer *server = pConsole->consoleVRDPServer();
        Assert(server);

        /* Does VRDP server call Console from the other thread?
         * Not sure (and can change), so leave the lock just in case.
         */
        alock.leave();
        vrc = server->Launch();
        alock.enter();

        if (vrc == VERR_NET_ADDRESS_IN_USE)
        {
            Utf8Str errMsg;
            Bstr bstr;
            pConsole->mVRDEServer->GetVRDEProperty(Bstr("TCP/Ports").raw(), bstr.asOutParam());
            Utf8Str ports = bstr;
            errMsg = Utf8StrFmt(tr("VirtualBox Remote Desktop Extension server can't bind to the port: %s"),
                                ports.c_str());
            LogRel(("VRDE: Warning: failed to launch VRDE server (%Rrc): '%s'\n",
                    vrc, errMsg.c_str()));
        }
        else if (vrc == VINF_NOT_SUPPORTED)
        {
            /* This means that the VRDE is not installed. */
            LogRel(("VRDE: VirtualBox Remote Desktop Extension is not available.\n"));
        }
        else if (RT_FAILURE(vrc))
        {
            /* Fail, if the server is installed but can't start. */
            Utf8Str errMsg;
            switch (vrc)
            {
                case VERR_FILE_NOT_FOUND:
                {
                    /* VRDE library file is missing. */
                    errMsg = Utf8StrFmt(tr("Could not find the VirtualBox Remote Desktop Extension library."));
                    break;
                }
                default:
                    errMsg = Utf8StrFmt(tr("Failed to launch Remote Desktop Extension server (%Rrc)"),
                                        vrc);
            }
            LogRel(("VRDE: Failed: (%Rrc), error message: '%s'\n",
                     vrc, errMsg.c_str()));
            throw setErrorStatic(E_FAIL, errMsg.c_str());
        }

        ComPtr<IMachine> pMachine = pConsole->machine();
        ULONG cCpus = 1;
        pMachine->COMGETTER(CPUCount)(&cCpus);

        /*
         * Create the VM
         */
        PVM pVM;
        /*
         * leave the lock since EMT will call Console. It's safe because
         * mMachineState is either Starting or Restoring state here.
         */
        alock.leave();

        vrc = VMR3Create(cCpus,
                         pConsole->mpVmm2UserMethods,
                         Console::genericVMSetErrorCallback,
                         &task->mErrorMsg,
                         task->mConfigConstructor,
                         static_cast<Console *>(pConsole),
                         &pVM);

        alock.enter();

        /* Enable client connections to the server. */
        pConsole->consoleVRDPServer()->EnableConnections();

        if (RT_SUCCESS(vrc))
        {
            do
            {
                /*
                 * Register our load/save state file handlers
                 */
                vrc = SSMR3RegisterExternal(pVM, sSSMConsoleUnit, 0 /*iInstance*/, sSSMConsoleVer, 0 /* cbGuess */,
                                            NULL, NULL, NULL,
                                            NULL, saveStateFileExec, NULL,
                                            NULL, loadStateFileExec, NULL,
                                            static_cast<Console *>(pConsole));
                AssertRCBreak(vrc);

                vrc = static_cast<Console *>(pConsole)->getDisplay()->registerSSM(pVM);
                AssertRC(vrc);
                if (RT_FAILURE(vrc))
                    break;

                /*
                 * Synchronize debugger settings
                 */
                MachineDebugger *machineDebugger = pConsole->getMachineDebugger();
                if (machineDebugger)
                    machineDebugger->flushQueuedSettings();

                /*
                 * Shared Folders
                 */
                if (pConsole->m_pVMMDev->isShFlActive())
                {
                    /* Does the code below call Console from the other thread?
                     * Not sure, so leave the lock just in case. */
                    alock.leave();

                    for (SharedFolderDataMap::const_iterator it = task->mSharedFolders.begin();
                         it != task->mSharedFolders.end();
                         ++it)
                    {
                        const SharedFolderData &d = it->second;
                        rc = pConsole->createSharedFolder(it->first, d);
                        if (FAILED(rc))
                        {
                            ErrorInfoKeeper eik;
                            setVMRuntimeErrorCallbackF(pVM, pConsole, 0, "BrokenSharedFolder",
                                                       N_("The shared folder '%s' could not be set up: %ls.\n"
                                                          "The shared folder setup will not be complete. It is recommended to power down the virtual machine and fix the shared folder settings while the machine is not running"),
                                                       it->first.c_str(), eik.getText().raw());
                        }
                    }
                    if (FAILED(rc))
                        rc = S_OK;          // do not fail with broken shared folders

                    /* enter the lock again */
                    alock.enter();
                }

                /*
                 * Capture USB devices.
                 */
                rc = pConsole->captureUSBDevices(pVM);
                if (FAILED(rc)) break;

                /* leave the lock before a lengthy operation */
                alock.leave();

                /* Load saved state? */
                if (task->mSavedStateFile.length())
                {
                    LogFlowFunc(("Restoring saved state from '%s'...\n",
                                 task->mSavedStateFile.c_str()));

                    vrc = VMR3LoadFromFile(pVM,
                                           task->mSavedStateFile.c_str(),
                                           Console::stateProgressCallback,
                                           static_cast<IProgress *>(task->mProgress));

                    if (RT_SUCCESS(vrc))
                    {
                        if (task->mStartPaused)
                            /* done */
                            pConsole->setMachineState(MachineState_Paused);
                        else
                        {
                            /* Start/Resume the VM execution */
#ifdef VBOX_WITH_EXTPACK
                            vrc = pConsole->mptrExtPackManager->callAllVmPowerOnHooks(pConsole, pVM);
#endif
                            if (RT_SUCCESS(vrc))
                                vrc = VMR3Resume(pVM);
                            AssertLogRelRC(vrc);
                        }
                    }

                    /* Power off in case we failed loading or resuming the VM */
                    if (RT_FAILURE(vrc))
                    {
                        int vrc2 = VMR3PowerOff(pVM); AssertLogRelRC(vrc2);
#ifdef VBOX_WITH_EXTPACK
                        pConsole->mptrExtPackManager->callAllVmPowerOffHooks(pConsole, pVM);
#endif
                    }
                }
                else if (task->mTeleporterEnabled)
                {
                    /* -> ConsoleImplTeleporter.cpp */
                    bool fPowerOffOnFailure;
                    rc = pConsole->teleporterTrg(VMR3GetUVM(pVM), pMachine, &task->mErrorMsg, task->mStartPaused,
                                                 task->mProgress, &fPowerOffOnFailure);
                    if (FAILED(rc) && fPowerOffOnFailure)
                    {
                        ErrorInfoKeeper eik;
                        int vrc2 = VMR3PowerOff(pVM); AssertLogRelRC(vrc2);
#ifdef VBOX_WITH_EXTPACK
                        pConsole->mptrExtPackManager->callAllVmPowerOffHooks(pConsole, pVM);
#endif
                    }
                }
                else if (task->mEnmFaultToleranceState != FaultToleranceState_Inactive)
                {
                    /*
                     * Get the config.
                     */
                    ULONG uPort;
                    ULONG uInterval;
                    Bstr bstrAddress, bstrPassword;

                    rc = pMachine->COMGETTER(FaultTolerancePort)(&uPort);
                    if (SUCCEEDED(rc))
                    {
                        rc = pMachine->COMGETTER(FaultToleranceSyncInterval)(&uInterval);
                        if (SUCCEEDED(rc))
                            rc = pMachine->COMGETTER(FaultToleranceAddress)(bstrAddress.asOutParam());
                        if (SUCCEEDED(rc))
                            rc = pMachine->COMGETTER(FaultTolerancePassword)(bstrPassword.asOutParam());
                    }
                    if (task->mProgress->setCancelCallback(faultToleranceProgressCancelCallback, pVM))
                    {
                        if (SUCCEEDED(rc))
                        {
                            Utf8Str strAddress(bstrAddress);
                            const char *pszAddress = strAddress.isEmpty() ? NULL : strAddress.c_str();
                            Utf8Str strPassword(bstrPassword);
                            const char *pszPassword = strPassword.isEmpty() ? NULL : strPassword.c_str();

                            /* Power on the FT enabled VM. */
#ifdef VBOX_WITH_EXTPACK
                            vrc = pConsole->mptrExtPackManager->callAllVmPowerOnHooks(pConsole, pVM);
#endif
                            if (RT_SUCCESS(vrc))
                                vrc = FTMR3PowerOn(pVM,
                                                   task->mEnmFaultToleranceState == FaultToleranceState_Master /* fMaster */,
                                                   uInterval,
                                                   pszAddress,
                                                   uPort,
                                                   pszPassword);
                            AssertLogRelRC(vrc);
                        }
                        task->mProgress->setCancelCallback(NULL, NULL);
                    }
                    else
                        rc = E_FAIL;
                }
                else if (task->mStartPaused)
                    /* done */
                    pConsole->setMachineState(MachineState_Paused);
                else
                {
                    /* Power on the VM (i.e. start executing) */
#ifdef VBOX_WITH_EXTPACK
                    vrc = pConsole->mptrExtPackManager->callAllVmPowerOnHooks(pConsole, pVM);
#endif
                    if (RT_SUCCESS(vrc))
                        vrc = VMR3PowerOn(pVM);
                    AssertLogRelRC(vrc);
                }

                /* enter the lock again */
                alock.enter();
            }
            while (0);

            /* On failure, destroy the VM */
            if (FAILED(rc) || RT_FAILURE(vrc))
            {
                /* preserve existing error info */
                ErrorInfoKeeper eik;

                /* powerDown() will call VMR3Destroy() and do all necessary
                 * cleanup (VRDP, USB devices) */
                HRESULT rc2 = pConsole->powerDown();
                AssertComRC(rc2);
            }
            else
            {
                /*
                 * Deregister the VMSetError callback. This is necessary as the
                 * pfnVMAtError() function passed to VMR3Create() is supposed to
                 * be sticky but our error callback isn't.
                 */
                alock.leave();
                VMR3AtErrorDeregister(pVM, Console::genericVMSetErrorCallback, &task->mErrorMsg);
                /** @todo register another VMSetError callback? */
                alock.enter();
            }
        }
        else
        {
            /*
             * If VMR3Create() failed it has released the VM memory.
             */
            VMR3ReleaseUVM(pConsole->mpUVM);
            pConsole->mpUVM = NULL;
        }

        if (SUCCEEDED(rc) && RT_FAILURE(vrc))
        {
            /* If VMR3Create() or one of the other calls in this function fail,
             * an appropriate error message has been set in task->mErrorMsg.
             * However since that happens via a callback, the rc status code in
             * this function is not updated.
             */
            if (!task->mErrorMsg.length())
            {
                /* If the error message is not set but we've got a failure,
                 * convert the VBox status code into a meaningful error message.
                 * This becomes unused once all the sources of errors set the
                 * appropriate error message themselves.
                 */
                AssertMsgFailed(("Missing error message during powerup for status code %Rrc\n", vrc));
                task->mErrorMsg = Utf8StrFmt(tr("Failed to start VM execution (%Rrc)"),
                                             vrc);
            }

            /* Set the error message as the COM error.
             * Progress::notifyComplete() will pick it up later. */
            throw setErrorStatic(E_FAIL, task->mErrorMsg.c_str());
        }
    }
    catch (HRESULT aRC) { rc = aRC; }

    if (   pConsole->mMachineState == MachineState_Starting
        || pConsole->mMachineState == MachineState_Restoring
        || pConsole->mMachineState == MachineState_TeleportingIn
       )
    {
        /* We are still in the Starting/Restoring state. This means one of:
         *
         * 1) we failed before VMR3Create() was called;
         * 2) VMR3Create() failed.
         *
         * In both cases, there is no need to call powerDown(), but we still
         * need to go back to the PoweredOff/Saved state. Reuse
         * vmstateChangeCallback() for that purpose.
         */

        /* preserve existing error info */
        ErrorInfoKeeper eik;

        Assert(pConsole->mpUVM == NULL);
        vmstateChangeCallback(NULL, VMSTATE_TERMINATED, VMSTATE_CREATING,
                              pConsole);
    }

    /*
     * Evaluate the final result. Note that the appropriate mMachineState value
     * is already set by vmstateChangeCallback() in all cases.
     */

    /* leave the lock, don't need it any more */
    alock.leave();

    if (SUCCEEDED(rc))
    {
        /* Notify the progress object of the success */
        task->mProgress->notifyComplete(S_OK);
    }
    else
    {
        /* The progress object will fetch the current error info */
        task->mProgress->notifyComplete(rc);
        LogRel(("Power up failed (vrc=%Rrc, rc=%Rhrc (%#08X))\n", vrc, rc, rc));
    }

    /* Notify VBoxSVC and any waiting openRemoteSession progress object. */
    pConsole->mControl->EndPowerUp(rc);

#if defined(RT_OS_WINDOWS)
    /* uninitialize COM */
    CoUninitialize();
#endif

    LogFlowFuncLeave();

    return VINF_SUCCESS;
}


/**
 * Reconfigures a medium attachment (part of taking or deleting an online snapshot).
 *
 * @param   pConsole      Reference to the console object.
 * @param   pVM           The VM handle.
 * @param   lInstance     The instance of the controller.
 * @param   pcszDevice    The name of the controller type.
 * @param   enmBus        The storage bus type of the controller.
 * @param   fSetupMerge   Whether to set up a medium merge
 * @param   uMergeSource  Merge source image index
 * @param   uMergeTarget  Merge target image index
 * @param   aMediumAtt    The medium attachment.
 * @param   aMachineState The current machine state.
 * @param   phrc          Where to store com error - only valid if we return VERR_GENERAL_FAILURE.
 * @return  VBox status code.
 */
/* static */
DECLCALLBACK(int) Console::reconfigureMediumAttachment(Console *pConsole,
                                                       PVM pVM,
                                                       const char *pcszDevice,
                                                       unsigned uInstance,
                                                       StorageBus_T enmBus,
                                                       bool fUseHostIOCache,
                                                       bool fBuiltinIoCache,
                                                       bool fSetupMerge,
                                                       unsigned uMergeSource,
                                                       unsigned uMergeTarget,
                                                       IMediumAttachment *aMediumAtt,
                                                       MachineState_T aMachineState,
                                                       HRESULT *phrc)
{
    LogFlowFunc(("pVM=%p aMediumAtt=%p phrc=%p\n", pVM, aMediumAtt, phrc));

    int             rc;
    HRESULT         hrc;
    Bstr            bstr;
    *phrc = S_OK;
#define RC_CHECK() do { if (RT_FAILURE(rc)) { AssertMsgFailed(("rc=%Rrc\n", rc)); return rc; } } while (0)
#define H() do { if (FAILED(hrc)) { AssertMsgFailed(("hrc=%Rhrc (%#x)\n", hrc, hrc)); *phrc = hrc; return VERR_GENERAL_FAILURE; } } while (0)

    /* Ignore attachments other than hard disks, since at the moment they are
     * not subject to snapshotting in general. */
    DeviceType_T lType;
    hrc = aMediumAtt->COMGETTER(Type)(&lType);                                  H();
    if (lType != DeviceType_HardDisk)
        return VINF_SUCCESS;

    /* Determine the base path for the device instance. */
    PCFGMNODE pCtlInst;
    pCtlInst = CFGMR3GetChildF(CFGMR3GetRoot(pVM), "Devices/%s/%u/", pcszDevice, uInstance);
    AssertReturn(pCtlInst, VERR_INTERNAL_ERROR);

    /* Update the device instance configuration. */
    rc = pConsole->configMediumAttachment(pCtlInst,
                                          pcszDevice,
                                          uInstance,
                                          enmBus,
                                          fUseHostIOCache,
                                          fBuiltinIoCache,
                                          fSetupMerge,
                                          uMergeSource,
                                          uMergeTarget,
                                          aMediumAtt,
                                          aMachineState,
                                          phrc,
                                          true /* fAttachDetach */,
                                          false /* fForceUnmount */,
                                          false /* fHotplug */,
                                          pVM,
                                          NULL /* paLedDevType */);
    /** @todo this dumps everything attached to this device instance, which
     * is more than necessary. Dumping the changed LUN would be enough. */
    CFGMR3Dump(pCtlInst);
    RC_CHECK();

#undef RC_CHECK
#undef H

    LogFlowFunc(("Returns success\n"));
    return VINF_SUCCESS;
}

/**
 * Progress cancelation callback employed by Console::fntTakeSnapshotWorker.
 */
static void takesnapshotProgressCancelCallback(void *pvUser)
{
    PUVM pUVM = (PUVM)pvUser;
    SSMR3Cancel(VMR3GetVM(pUVM));
}

/**
 * Worker thread created by Console::TakeSnapshot.
 * @param Thread The current thread (ignored).
 * @param pvUser The task.
 * @return VINF_SUCCESS (ignored).
 */
/*static*/
DECLCALLBACK(int) Console::fntTakeSnapshotWorker(RTTHREAD Thread, void *pvUser)
{
    VMTakeSnapshotTask *pTask = (VMTakeSnapshotTask*)pvUser;

    // taking a snapshot consists of the following:

    // 1) creating a diff image for each virtual hard disk, into which write operations go after
    //    the snapshot has been created (done in VBoxSVC, in SessionMachine::BeginTakingSnapshot)
    // 2) creating a Snapshot object with the state of the machine (hardware + storage,
    //    done in VBoxSVC, also in SessionMachine::BeginTakingSnapshot)
    // 3) saving the state of the virtual machine (here, in the VM process, if the machine is online)

    Console    *that                 = pTask->mConsole;
    bool        fBeganTakingSnapshot = false;
    bool        fSuspenededBySave    = false;

    AutoCaller autoCaller(that);
    if (FAILED(autoCaller.rc()))
    {
        that->mptrCancelableProgress.setNull();
        return autoCaller.rc();
    }

    AutoWriteLock alock(that COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    try
    {
        /* STEP 1 + 2:
         * request creating the diff images on the server and create the snapshot object
         * (this will set the machine state to Saving on the server to block
         * others from accessing this machine)
         */
        rc = that->mControl->BeginTakingSnapshot(that,
                                                 pTask->bstrName.raw(),
                                                 pTask->bstrDescription.raw(),
                                                 pTask->mProgress,
                                                 pTask->fTakingSnapshotOnline,
                                                 pTask->bstrSavedStateFile.asOutParam());
        if (FAILED(rc))
            throw rc;

        fBeganTakingSnapshot = true;

        /*
         * state file is non-null only when the VM is paused
         * (i.e. creating a snapshot online)
         */
        bool f =     (!pTask->bstrSavedStateFile.isEmpty() &&  pTask->fTakingSnapshotOnline)
                  || ( pTask->bstrSavedStateFile.isEmpty() && !pTask->fTakingSnapshotOnline);
        if (!f)
            throw setErrorStatic(E_FAIL, "Invalid state of saved state file");

        /* sync the state with the server */
        if (pTask->lastMachineState == MachineState_Running)
            that->setMachineStateLocally(MachineState_LiveSnapshotting);
        else
            that->setMachineStateLocally(MachineState_Saving);

        // STEP 3: save the VM state (if online)
        if (pTask->fTakingSnapshotOnline)
        {
            Utf8Str strSavedStateFile(pTask->bstrSavedStateFile);

            SafeVMPtr ptrVM(that);
            if (!ptrVM.isOk())
                throw ptrVM.rc();

            pTask->mProgress->SetNextOperation(Bstr(tr("Saving the machine state")).raw(),
                                               pTask->ulMemSize);       // operation weight, same as computed when setting up progress object
            pTask->mProgress->setCancelCallback(takesnapshotProgressCancelCallback, ptrVM.rawUVM());

            alock.leave();
            LogFlowFunc(("VMR3Save...\n"));
            int vrc = VMR3Save(ptrVM,
                               strSavedStateFile.c_str(),
                               true /*fContinueAfterwards*/,
                               Console::stateProgressCallback,
                               static_cast<IProgress *>(pTask->mProgress),
                               &fSuspenededBySave);
            alock.enter();
            if (RT_FAILURE(vrc))
                throw setErrorStatic(E_FAIL,
                                     tr("Failed to save the machine state to '%s' (%Rrc)"),
                                     strSavedStateFile.c_str(), vrc);

            pTask->mProgress->setCancelCallback(NULL, NULL);
            if (!pTask->mProgress->notifyPointOfNoReturn())
                throw setErrorStatic(E_FAIL, tr("Canceled"));
            that->mptrCancelableProgress.setNull();

            // STEP 4: reattach hard disks
            LogFlowFunc(("Reattaching new differencing hard disks...\n"));

            pTask->mProgress->SetNextOperation(Bstr(tr("Reconfiguring medium attachments")).raw(),
                                               1);       // operation weight, same as computed when setting up progress object

            com::SafeIfaceArray<IMediumAttachment> atts;
            rc = that->mMachine->COMGETTER(MediumAttachments)(ComSafeArrayAsOutParam(atts));
            if (FAILED(rc))
                throw rc;

            for (size_t i = 0;
                i < atts.size();
                ++i)
            {
                ComPtr<IStorageController> pStorageController;
                Bstr controllerName;
                ULONG lInstance;
                StorageControllerType_T enmController;
                StorageBus_T enmBus;
                BOOL fUseHostIOCache;

                /*
                * We can't pass a storage controller object directly
                * (g++ complains about not being able to pass non POD types through '...')
                * so we have to query needed values here and pass them.
                */
                rc = atts[i]->COMGETTER(Controller)(controllerName.asOutParam());
                if (FAILED(rc))
                    throw rc;

                rc = that->mMachine->GetStorageControllerByName(controllerName.raw(),
                                                                pStorageController.asOutParam());
                if (FAILED(rc))
                    throw rc;

                rc = pStorageController->COMGETTER(ControllerType)(&enmController);
                if (FAILED(rc))
                    throw rc;
                rc = pStorageController->COMGETTER(Instance)(&lInstance);
                if (FAILED(rc))
                    throw rc;
                rc = pStorageController->COMGETTER(Bus)(&enmBus);
                if (FAILED(rc))
                    throw rc;
                rc = pStorageController->COMGETTER(UseHostIOCache)(&fUseHostIOCache);
                if (FAILED(rc))
                    throw rc;

                const char *pcszDevice = Console::convertControllerTypeToDev(enmController);

                BOOL fBuiltinIoCache;
                rc = that->mMachine->COMGETTER(IoCacheEnabled)(&fBuiltinIoCache);
                if (FAILED(rc))
                    throw rc;

                /*
                 * don't leave the lock since reconfigureMediumAttachment
                 * isn't going to need the Console lock.
                 */
                vrc = VMR3ReqCallWait(ptrVM,
                                      VMCPUID_ANY,
                                      (PFNRT)reconfigureMediumAttachment,
                                      13,
                                      that,
                                      ptrVM.raw(),
                                      pcszDevice,
                                      lInstance,
                                      enmBus,
                                      fUseHostIOCache,
                                      fBuiltinIoCache,
                                      false /* fSetupMerge */,
                                      0 /* uMergeSource */,
                                      0 /* uMergeTarget */,
                                      atts[i],
                                      that->mMachineState,
                                      &rc);
                if (RT_FAILURE(vrc))
                    throw setErrorStatic(E_FAIL, Console::tr("%Rrc"), vrc);
                if (FAILED(rc))
                    throw rc;
            }
        }

        /*
         * finalize the requested snapshot object.
         * This will reset the machine state to the state it had right
         * before calling mControl->BeginTakingSnapshot().
         */
        rc = that->mControl->EndTakingSnapshot(TRUE /*aSuccess*/);
        // do not throw rc here because we can't call EndTakingSnapshot() twice
        LogFlowFunc(("EndTakingSnapshot -> %Rhrc [mMachineState=%s]\n", rc, Global::stringifyMachineState(that->mMachineState)));
    }
    catch (HRESULT rcThrown)
    {
        /* preserve existing error info */
        ErrorInfoKeeper eik;

        if (fBeganTakingSnapshot)
            that->mControl->EndTakingSnapshot(FALSE /*aSuccess*/);

        rc = rcThrown;
        LogFunc(("Caught %Rhrc [mMachineState=%s]\n", rc, Global::stringifyMachineState(that->mMachineState)));
    }
    Assert(alock.isWriteLockOnCurrentThread());

    if (FAILED(rc)) /* Must come before calling setMachineState. */
        pTask->mProgress->notifyComplete(rc);

    /*
     * Fix up the machine state.
     *
     * For live snapshots we do all the work, for the two other variations we
     * just update the local copy.
     */
    MachineState_T enmMachineState;
    that->mMachine->COMGETTER(State)(&enmMachineState);
    if (   that->mMachineState == MachineState_LiveSnapshotting
        || that->mMachineState == MachineState_Saving)
    {

        if (!pTask->fTakingSnapshotOnline)
            that->setMachineStateLocally(pTask->lastMachineState);
        else if (SUCCEEDED(rc))
        {
            Assert(   pTask->lastMachineState == MachineState_Running
                   || pTask->lastMachineState == MachineState_Paused);
            Assert(that->mMachineState == MachineState_Saving);
            if (pTask->lastMachineState == MachineState_Running)
            {
                LogFlowFunc(("VMR3Resume...\n"));
                SafeVMPtr ptrVM(that);
                alock.leave();
                int vrc = VMR3Resume(ptrVM);
                alock.enter();
                if (RT_FAILURE(vrc))
                {
                    rc = setErrorStatic(VBOX_E_VM_ERROR, tr("Could not resume the machine execution (%Rrc)"), vrc);
                    pTask->mProgress->notifyComplete(rc);
                    if (that->mMachineState == MachineState_Saving)
                        that->setMachineStateLocally(MachineState_Paused);
                }
            }
            else
                that->setMachineStateLocally(MachineState_Paused);
        }
        else
        {
            /** @todo this could probably be made more generic and reused elsewhere. */
            /* paranoid cleanup on for a failed online snapshot. */
            VMSTATE enmVMState = VMR3GetStateU(that->mpUVM);
            switch (enmVMState)
            {
                case VMSTATE_RUNNING:
                case VMSTATE_RUNNING_LS:
                case VMSTATE_DEBUGGING:
                case VMSTATE_DEBUGGING_LS:
                case VMSTATE_POWERING_OFF:
                case VMSTATE_POWERING_OFF_LS:
                case VMSTATE_RESETTING:
                case VMSTATE_RESETTING_LS:
                    Assert(!fSuspenededBySave);
                    that->setMachineState(MachineState_Running);
                    break;

                case VMSTATE_GURU_MEDITATION:
                case VMSTATE_GURU_MEDITATION_LS:
                    that->setMachineState(MachineState_Stuck);
                    break;

                case VMSTATE_FATAL_ERROR:
                case VMSTATE_FATAL_ERROR_LS:
                    if (pTask->lastMachineState == MachineState_Paused)
                        that->setMachineStateLocally(pTask->lastMachineState);
                    else
                        that->setMachineState(MachineState_Paused);
                    break;

                default:
                    AssertMsgFailed(("%s\n", VMR3GetStateName(enmVMState)));
                case VMSTATE_SUSPENDED:
                case VMSTATE_SUSPENDED_LS:
                case VMSTATE_SUSPENDING:
                case VMSTATE_SUSPENDING_LS:
                case VMSTATE_SUSPENDING_EXT_LS:
                    if (fSuspenededBySave)
                    {
                        Assert(pTask->lastMachineState == MachineState_Running);
                        LogFlowFunc(("VMR3Resume (on failure)...\n"));
                        SafeVMPtr ptrVM(that);
                        alock.leave();
                        int vrc = VMR3Resume(ptrVM); AssertLogRelRC(vrc);
                        alock.enter();
                        if (RT_FAILURE(vrc))
                            that->setMachineState(MachineState_Paused);
                    }
                    else if (pTask->lastMachineState == MachineState_Paused)
                        that->setMachineStateLocally(pTask->lastMachineState);
                    else
                        that->setMachineState(MachineState_Paused);
                    break;
            }

        }
    }
    /*else: somebody else has change the state... Leave it. */

    /* check the remote state to see that we got it right. */
    that->mMachine->COMGETTER(State)(&enmMachineState);
    AssertLogRelMsg(that->mMachineState == enmMachineState,
                    ("mMachineState=%s enmMachineState=%s\n", Global::stringifyMachineState(that->mMachineState),
                     Global::stringifyMachineState(enmMachineState) ));


    if (SUCCEEDED(rc)) /* The failure cases are handled above. */
        pTask->mProgress->notifyComplete(rc);

    delete pTask;

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

/**
 * Thread for executing the saved state operation.
 *
 * @param   Thread      The thread handle.
 * @param   pvUser      Pointer to a VMSaveTask structure.
 * @return  VINF_SUCCESS (ignored).
 *
 * @note Locks the Console object for writing.
 */
/*static*/
DECLCALLBACK(int) Console::saveStateThread(RTTHREAD Thread, void *pvUser)
{
    LogFlowFuncEnter();

    std::auto_ptr<VMSaveTask> task(static_cast<VMSaveTask*>(pvUser));
    AssertReturn(task.get(), VERR_INVALID_PARAMETER);

    Assert(task->mSavedStateFile.length());
    Assert(task->mProgress.isNull());
    Assert(!task->mServerProgress.isNull());

    const ComObjPtr<Console> &that = task->mConsole;
    Utf8Str errMsg;
    HRESULT rc = S_OK;

    LogFlowFunc(("Saving the state to '%s'...\n", task->mSavedStateFile.c_str()));

    bool fSuspenededBySave;
    int vrc = VMR3Save(task->mpVM,
                       task->mSavedStateFile.c_str(),
                       false, /*fContinueAfterwards*/
                       Console::stateProgressCallback,
                       static_cast<IProgress *>(task->mServerProgress),
                       &fSuspenededBySave);
    if (RT_FAILURE(vrc))
    {
        errMsg = Utf8StrFmt(Console::tr("Failed to save the machine state to '%s' (%Rrc)"),
                            task->mSavedStateFile.c_str(), vrc);
        rc = E_FAIL;
    }
    Assert(!fSuspenededBySave);

    /* lock the console once we're going to access it */
    AutoWriteLock thatLock(that COMMA_LOCKVAL_SRC_POS);

    /* synchronize the state with the server */
    if (SUCCEEDED(rc))
    {
        /*
         * The machine has been successfully saved, so power it down
         * (vmstateChangeCallback() will set state to Saved on success).
         * Note: we release the task's VM caller, otherwise it will
         * deadlock.
         */
        task->releaseVMCaller();
        rc = that->powerDown();
    }

    /*
     * Finalize the requested save state procedure. In case of failure it will
     * reset the machine state to the state it had right before calling
     * mControl->BeginSavingState(). This must be the last thing because it
     * will set the progress to completed, and that means that the frontend
     * can immediately uninit the associated console object.
     */
    that->mControl->EndSavingState(rc, Bstr(errMsg).raw());

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

/**
 * Thread for powering down the Console.
 *
 * @param   Thread      The thread handle.
 * @param   pvUser      Pointer to the VMTask structure.
 * @return  VINF_SUCCESS (ignored).
 *
 * @note Locks the Console object for writing.
 */
/*static*/
DECLCALLBACK(int) Console::powerDownThread(RTTHREAD Thread, void *pvUser)
{
    LogFlowFuncEnter();

    std::auto_ptr<VMPowerDownTask> task(static_cast<VMPowerDownTask *>(pvUser));
    AssertReturn(task.get(), VERR_INVALID_PARAMETER);

    AssertReturn(task->isOk(), VERR_GENERAL_FAILURE);

    Assert(task->mProgress.isNull());

    const ComObjPtr<Console> &that = task->mConsole;

    /* Note: no need to use addCaller() to protect Console because VMTask does
     * that */

    /* wait until the method tat started us returns */
    AutoWriteLock thatLock(that COMMA_LOCKVAL_SRC_POS);

    /* release VM caller to avoid the powerDown() deadlock */
    task->releaseVMCaller();

    that->powerDown(task->mServerProgress);

    /* complete the operation */
    that->mControl->EndPoweringDown(S_OK, Bstr().raw());

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{VMM2USERMETHODS,pfnSaveState}
 */
/*static*/ DECLCALLBACK(int)
Console::vmm2User_SaveState(PCVMM2USERMETHODS pThis, PUVM pUVM)
{
    Console *pConsole = ((MYVMM2USERMETHODS *)pThis)->pConsole;
    NOREF(pUVM);

    /*
     * For now, just call SaveState.  We should probably try notify the GUI so
     * it can pop up a progress object and stuff.
     */
    HRESULT hrc = pConsole->SaveState(NULL);
    return SUCCEEDED(hrc) ? VINF_SUCCESS : Global::vboxStatusCodeFromCOM(hrc);
}

/**
 * @interface_method_impl{VMM2USERMETHODS,pfnNotifyEmtInit}
 */
/*static*/ DECLCALLBACK(void)
Console::vmm2User_NotifyEmtInit(PCVMM2USERMETHODS pThis, PUVM pUVM, PUVMCPU pUVCpu)
{
    NOREF(pThis); NOREF(pUVM); NOREF(pUVCpu);
    VirtualBoxBase::initializeComForThread();
}

/**
 * @interface_method_impl{VMM2USERMETHODS,pfnNotifyEmtTerm}
 */
/*static*/ DECLCALLBACK(void)
Console::vmm2User_NotifyEmtTerm(PCVMM2USERMETHODS pThis, PUVM pUVM, PUVMCPU pUVCpu)
{
    NOREF(pThis); NOREF(pUVM); NOREF(pUVCpu);
    VirtualBoxBase::uninitializeComForThread();
}

/**
 * @interface_method_impl{VMM2USERMETHODS,pfnNotifyPdmtInit}
 */
/*static*/ DECLCALLBACK(void)
Console::vmm2User_NotifyPdmtInit(PCVMM2USERMETHODS pThis, PUVM pUVM)
{
    NOREF(pThis); NOREF(pUVM);
    VirtualBoxBase::initializeComForThread();
}

/**
 * @interface_method_impl{VMM2USERMETHODS,pfnNotifyPdmtTerm}
 */
/*static*/ DECLCALLBACK(void)
Console::vmm2User_NotifyPdmtTerm(PCVMM2USERMETHODS pThis, PUVM pUVM)
{
    NOREF(pThis); NOREF(pUVM);
    VirtualBoxBase::uninitializeComForThread();
}




/**
 * The Main status driver instance data.
 */
typedef struct DRVMAINSTATUS
{
    /** The LED connectors. */
    PDMILEDCONNECTORS   ILedConnectors;
    /** Pointer to the LED ports interface above us. */
    PPDMILEDPORTS       pLedPorts;
    /** Pointer to the array of LED pointers. */
    PPDMLED            *papLeds;
    /** The unit number corresponding to the first entry in the LED array. */
    RTUINT              iFirstLUN;
    /** The unit number corresponding to the last entry in the LED array.
     * (The size of the LED array is iLastLUN - iFirstLUN + 1.) */
    RTUINT              iLastLUN;
    /** Pointer to the driver instance. */
    PPDMDRVINS          pDrvIns;
    /** The Media Notify interface. */
    PDMIMEDIANOTIFY     IMediaNotify;
    /** Map for translating PDM storage controller/LUN information to
     * IMediumAttachment references. */
    Console::MediumAttachmentMap *pmapMediumAttachments;
    /** Device name+instance for mapping */
    char                *pszDeviceInstance;
    /** Pointer to the Console object, for driver triggered activities. */
    Console             *pConsole;
} DRVMAINSTATUS, *PDRVMAINSTATUS;


/**
 * Notification about a unit which have been changed.
 *
 * The driver must discard any pointers to data owned by
 * the unit and requery it.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   iLUN            The unit number.
 */
DECLCALLBACK(void) Console::drvStatus_UnitChanged(PPDMILEDCONNECTORS pInterface, unsigned iLUN)
{
    PDRVMAINSTATUS pData = (PDRVMAINSTATUS)((uintptr_t)pInterface - RT_OFFSETOF(DRVMAINSTATUS, ILedConnectors));
    if (iLUN >= pData->iFirstLUN && iLUN <= pData->iLastLUN)
    {
        PPDMLED pLed;
        int rc = pData->pLedPorts->pfnQueryStatusLed(pData->pLedPorts, iLUN, &pLed);
        if (RT_FAILURE(rc))
            pLed = NULL;
        ASMAtomicWritePtr(&pData->papLeds[iLUN - pData->iFirstLUN], pLed);
        Log(("drvStatus_UnitChanged: iLUN=%d pLed=%p\n", iLUN, pLed));
    }
}


/**
 * Notification about a medium eject.
 *
 * @returns VBox status.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   uLUN            The unit number.
 */
DECLCALLBACK(int) Console::drvStatus_MediumEjected(PPDMIMEDIANOTIFY pInterface, unsigned uLUN)
{
    PDRVMAINSTATUS pData = (PDRVMAINSTATUS)((uintptr_t)pInterface - RT_OFFSETOF(DRVMAINSTATUS, IMediaNotify));
    PPDMDRVINS pDrvIns = pData->pDrvIns;
    LogFunc(("uLUN=%d\n", uLUN));
    if (pData->pmapMediumAttachments)
    {
        AutoWriteLock alock(pData->pConsole COMMA_LOCKVAL_SRC_POS);

        ComPtr<IMediumAttachment> pMediumAtt;
        Utf8Str devicePath = Utf8StrFmt("%s/LUN#%u", pData->pszDeviceInstance, uLUN);
        Console::MediumAttachmentMap::const_iterator end = pData->pmapMediumAttachments->end();
        Console::MediumAttachmentMap::const_iterator it = pData->pmapMediumAttachments->find(devicePath);
        if (it != end)
            pMediumAtt = it->second;
        Assert(!pMediumAtt.isNull());
        if (!pMediumAtt.isNull())
        {
            IMedium *pMedium;
            HRESULT rc = pMediumAtt->COMGETTER(Medium)(&pMedium);
            AssertComRC(rc);
            BOOL fHostDrive = FALSE;
            rc = pMedium->COMGETTER(HostDrive)(&fHostDrive);
            AssertComRC(rc);
            if (!fHostDrive)
            {
                alock.release();

                ComPtr<IMediumAttachment> pNewMediumAtt;
                rc = pData->pConsole->mControl->EjectMedium(pMediumAtt, pNewMediumAtt.asOutParam());
                if (SUCCEEDED(rc))
                    fireMediumChangedEvent(pData->pConsole->mEventSource, pNewMediumAtt);

                alock.acquire();
                if (pNewMediumAtt != pMediumAtt)
                {
                    pData->pmapMediumAttachments->erase(devicePath);
                    pData->pmapMediumAttachments->insert(std::make_pair(devicePath, pNewMediumAtt));
                }
            }
        }
    }
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
DECLCALLBACK(void *)  Console::drvStatus_QueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVMAINSTATUS pThis = PDMINS_2_DATA(pDrvIns, PDRVMAINSTATUS);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMILEDCONNECTORS, &pThis->ILedConnectors);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIANOTIFY, &pThis->IMediaNotify);
    return NULL;
}


/**
 * Destruct a status driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 */
DECLCALLBACK(void) Console::drvStatus_Destruct(PPDMDRVINS pDrvIns)
{
    PDRVMAINSTATUS pData = PDMINS_2_DATA(pDrvIns, PDRVMAINSTATUS);
    LogFlowFunc(("iInstance=%d\n", pDrvIns->iInstance));
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    if (pData->papLeds)
    {
        unsigned iLed = pData->iLastLUN - pData->iFirstLUN + 1;
        while (iLed-- > 0)
            ASMAtomicWriteNullPtr(&pData->papLeds[iLed]);
    }
}


/**
 * Construct a status driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
DECLCALLBACK(int) Console::drvStatus_Construct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVMAINSTATUS pData = PDMINS_2_DATA(pDrvIns, PDRVMAINSTATUS);
    LogFlowFunc(("iInstance=%d\n", pDrvIns->iInstance));
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "papLeds\0pmapMediumAttachments\0DeviceInstance\0pConsole\0First\0Last\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;
    AssertMsgReturn(PDMDrvHlpNoAttach(pDrvIns) == VERR_PDM_NO_ATTACHED_DRIVER,
                    ("Configuration error: Not possible to attach anything to this driver!\n"),
                    VERR_PDM_DRVINS_NO_ATTACH);

    /*
     * Data.
     */
    pDrvIns->IBase.pfnQueryInterface        = Console::drvStatus_QueryInterface;
    pData->ILedConnectors.pfnUnitChanged    = Console::drvStatus_UnitChanged;
    pData->IMediaNotify.pfnEjected          = Console::drvStatus_MediumEjected;
    pData->pDrvIns                          = pDrvIns;
    pData->pszDeviceInstance                = NULL;

    /*
     * Read config.
     */
    int rc = CFGMR3QueryPtr(pCfg, "papLeds", (void **)&pData->papLeds);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Failed to query the \"papLeds\" value! rc=%Rrc\n", rc));
        return rc;
    }

    rc = CFGMR3QueryPtrDef(pCfg, "pmapMediumAttachments", (void **)&pData->pmapMediumAttachments, NULL);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Failed to query the \"pmapMediumAttachments\" value! rc=%Rrc\n", rc));
        return rc;
    }
    if (pData->pmapMediumAttachments)
    {
        rc = CFGMR3QueryStringAlloc(pCfg, "DeviceInstance", &pData->pszDeviceInstance);
        if (RT_FAILURE(rc))
        {
            AssertMsgFailed(("Configuration error: Failed to query the \"DeviceInstance\" value! rc=%Rrc\n", rc));
            return rc;
        }
        rc = CFGMR3QueryPtr(pCfg, "pConsole", (void **)&pData->pConsole);
        if (RT_FAILURE(rc))
        {
            AssertMsgFailed(("Configuration error: Failed to query the \"pConsole\" value! rc=%Rrc\n", rc));
            return rc;
        }
    }

    rc = CFGMR3QueryU32(pCfg, "First", &pData->iFirstLUN);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->iFirstLUN = 0;
    else if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Failed to query the \"First\" value! rc=%Rrc\n", rc));
        return rc;
    }

    rc = CFGMR3QueryU32(pCfg, "Last", &pData->iLastLUN);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->iLastLUN = 0;
    else if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Failed to query the \"Last\" value! rc=%Rrc\n", rc));
        return rc;
    }
    if (pData->iFirstLUN > pData->iLastLUN)
    {
        AssertMsgFailed(("Configuration error: Invalid unit range %u-%u\n", pData->iFirstLUN, pData->iLastLUN));
        return VERR_GENERAL_FAILURE;
    }

    /*
     * Get the ILedPorts interface of the above driver/device and
     * query the LEDs we want.
     */
    pData->pLedPorts = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMILEDPORTS);
    AssertMsgReturn(pData->pLedPorts, ("Configuration error: No led ports interface above!\n"),
                    VERR_PDM_MISSING_INTERFACE_ABOVE);

    for (unsigned i = pData->iFirstLUN; i <= pData->iLastLUN; ++i)
        Console::drvStatus_UnitChanged(&pData->ILedConnectors, i);

    return VINF_SUCCESS;
}


/**
 * Console status driver (LED) registration record.
 */
const PDMDRVREG Console::DrvStatusReg =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "MainStatus",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Main status driver (Main as in the API).",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_STATUS,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVMAINSTATUS),
    /* pfnConstruct */
    Console::drvStatus_Construct,
    /* pfnDestruct */
    Console::drvStatus_Destruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
