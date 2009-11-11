/* $Id$ */

/** @file
 *
 * VBox Console COM Class implementation
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
#endif

#include "ConsoleImpl.h"

#include "Global.h"
#include "GuestImpl.h"
#include "KeyboardImpl.h"
#include "MouseImpl.h"
#include "DisplayImpl.h"
#include "MachineDebuggerImpl.h"
#include "USBDeviceImpl.h"
#include "RemoteUSBDeviceImpl.h"
#include "SharedFolderImpl.h"
#include "AudioSnifferInterface.h"
#include "ConsoleVRDPServer.h"
#include "VMMDev.h"
#include "package-generated.h"

// generated header
#include "SchemaDefs.h"

#include "Logging.h"

#include <VBox/com/array.h>

#include <iprt/asm.h>
#include <iprt/buildconfig.h>
#include <iprt/cpputils.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/ldr.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/system.h>

#include <VBox/vmapi.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/vusb.h>
#include <VBox/mm.h>
#include <VBox/ssm.h>
#include <VBox/version.h>
#ifdef VBOX_WITH_USB
#   include <VBox/pdmusb.h>
#endif

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
 * as a Console::mpVM caller with the same meaning as above. See
 * Console::addVMCaller() for more info.
 */
struct VMTask
{
    VMTask(Console *aConsole, bool aUsesVMPtr)
        : mConsole(aConsole),
          mCallerAdded(false),
          mVMCallerAdded(false)
    {
        AssertReturnVoid(aConsole);
        mRC = aConsole->addCaller();
        if (SUCCEEDED(mRC))
        {
            mCallerAdded = true;
            if (aUsesVMPtr)
            {
                mRC = aConsole->addVMCaller();
                if (SUCCEEDED(mRC))
                    mVMCallerAdded = true;
            }
        }
    }

    ~VMTask()
    {
        if (mVMCallerAdded)
            mConsole->releaseVMCaller();
        if (mCallerAdded)
            mConsole->releaseCaller();
    }

    HRESULT rc() const { return mRC; }
    bool isOk() const { return SUCCEEDED(rc()); }

    /** Releases the Console caller before destruction. Not normally necessary. */
    void releaseCaller()
    {
        AssertReturnVoid(mCallerAdded);
        mConsole->releaseCaller();
        mCallerAdded = false;
    }

    /** Releases the VM caller before destruction. Not normally necessary. */
    void releaseVMCaller()
    {
        AssertReturnVoid(mVMCallerAdded);
        mConsole->releaseVMCaller();
        mVMCallerAdded = false;
    }

    const ComObjPtr<Console> mConsole;

private:

    HRESULT mRC;
    bool mCallerAdded : 1;
    bool mVMCallerAdded : 1;
};

struct VMProgressTask : public VMTask
{
    VMProgressTask(Console *aConsole,
                   Progress *aProgress,
                   bool aUsesVMPtr)
        : VMTask(aConsole, aUsesVMPtr),
          mProgress(aProgress)
    {}

    const ComObjPtr<Progress> mProgress;

    Utf8Str mErrorMsg;
};

struct VMTakeSnapshotTask : public VMProgressTask
{
    VMTakeSnapshotTask(Console *aConsole,
                       Progress *aProgress,
                       IN_BSTR aName,
                       IN_BSTR aDescription)
        : VMProgressTask(aConsole, aProgress, false /* aUsesVMPtr */),
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

struct VMPowerUpTask : public VMProgressTask
{
    VMPowerUpTask(Console *aConsole,
                  Progress *aProgress)
        : VMProgressTask(aConsole, aProgress, false /* aUsesVMPtr */),
          mSetVMErrorCallback(NULL),
          mConfigConstructor(NULL),
          mStartPaused(false),
          mTeleporterEnabled(FALSE)
    {}

    PFNVMATERROR mSetVMErrorCallback;
    PFNCFGMCONSTRUCTOR mConfigConstructor;
    Utf8Str mSavedStateFile;
    Console::SharedFolderDataMap mSharedFolders;
    bool mStartPaused;
    BOOL mTeleporterEnabled;

    typedef std::list< ComPtr<IMedium> > HardDiskList;
    HardDiskList hardDisks;

    /* array of progress objects for hard disk reset operations */
    typedef std::list< ComPtr<IProgress> > ProgressList;
    ProgressList hardDiskProgresses;
};

struct VMSaveTask : public VMProgressTask
{
    VMSaveTask(Console *aConsole, Progress *aProgress)
        : VMProgressTask(aConsole, aProgress, true /* aUsesVMPtr */),
          mLastMachineState(MachineState_Null)
    {}

    Utf8Str mSavedStateFile;
    MachineState_T mLastMachineState;
    ComPtr<IProgress> mServerProgress;
};

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

Console::Console()
    : mSavedStateDataLoaded(false)
    , mConsoleVRDPServer(NULL)
    , mpVM(NULL)
    , mVMCallers(0)
    , mVMZeroCallersSem(NIL_RTSEMEVENT)
    , mVMDestroying(false)
    , mVMPoweredOff(false)
    , mVMMDev(NULL)
    , mAudioSniffer(NULL)
    , mVMStateChangeCallbackDisabled(false)
    , mMachineState(MachineState_PoweredOff)
{
    for (ULONG slot = 0; slot < SchemaDefs::NetworkAdapterCount; ++slot)
        meAttachmentType[slot] = NetworkAttachmentType_Null;
}

Console::~Console()
{}

HRESULT Console::FinalConstruct()
{
    LogFlowThisFunc(("\n"));

    memset(mapFDLeds, 0, sizeof(mapFDLeds));
    memset(mapIDELeds, 0, sizeof(mapIDELeds));
    memset(mapSATALeds, 0, sizeof(mapSATALeds));
    memset(mapSCSILeds, 0, sizeof(mapSCSILeds));
    memset(mapNetworkLeds, 0, sizeof(mapNetworkLeds));
    memset(&mapUSBLed, 0, sizeof(mapUSBLed));
    memset(&mapSharedFolderLed, 0, sizeof(mapSharedFolderLed));

    return S_OK;
}

void Console::FinalRelease()
{
    LogFlowThisFunc(("\n"));

    uninit();
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

    memset(&mCallbackData, 0, sizeof(mCallbackData));

    /* Cache essential properties and objects */

    rc = mMachine->COMGETTER(State)(&mMachineState);
    AssertComRCReturnRC(rc);

#ifdef VBOX_WITH_VRDP
    rc = mMachine->COMGETTER(VRDPServer)(unconst(mVRDPServer).asOutParam());
    AssertComRCReturnRC(rc);
#endif

    /* Create associated child COM objects */

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

    unconst(mRemoteDisplayInfo).createObject();
    rc = mRemoteDisplayInfo->init(this);
    AssertComRCReturnRC(rc);

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

    unconst(mVMMDev) = new VMMDev(this);
    AssertReturn(mVMMDev, E_FAIL);

    unconst(mAudioSniffer) = new AudioSniffer(this);
    AssertReturn(mAudioSniffer, E_FAIL);

    /* Confirm a successful initialization when it's the case */
    autoInitSpan.setSucceeded();

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

    /*
     * Uninit all children that use addDependentChild()/removeDependentChild()
     * in their init()/uninit() methods.
     */
    uninitDependentChildren();

    /* power down the VM if necessary */
    if (mpVM)
    {
        powerDown();
        Assert(mpVM == NULL);
    }

    if (mVMZeroCallersSem != NIL_RTSEMEVENT)
    {
        RTSemEventDestroy(mVMZeroCallersSem);
        mVMZeroCallersSem = NIL_RTSEMEVENT;
    }

    if (mAudioSniffer)
    {
        delete mAudioSniffer;
        unconst(mAudioSniffer) = NULL;
    }

    if (mVMMDev)
    {
        delete mVMMDev;
        unconst(mVMMDev) = NULL;
    }

    mGlobalSharedFolders.clear();
    mMachineSharedFolders.clear();

    mSharedFolders.clear();
    mRemoteUSBDevices.clear();
    mUSBDevices.clear();

    if (mRemoteDisplayInfo)
    {
        mRemoteDisplayInfo->uninit();
        unconst(mRemoteDisplayInfo).setNull();;
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

#ifdef VBOX_WITH_VRDP
    unconst(mVRDPServer).setNull();
#endif

    unconst(mControl).setNull();
    unconst(mMachine).setNull();

    /* Release all callbacks. Do this after uninitializing the components,
     * as some of them are well-behaved and unregister their callbacks.
     * These would trigger error messages complaining about trying to
     * unregister a non-registered callback. */
    mCallbacks.clear();

    /* dynamically allocated members of mCallbackData are uninitialized
     * at the end of powerDown() */
    Assert(!mCallbackData.mpsc.valid && mCallbackData.mpsc.shape == NULL);
    Assert(!mCallbackData.mcc.valid);
    Assert(!mCallbackData.klc.valid);

    LogFlowThisFuncLeave();
}

#ifdef VBOX_WITH_GUEST_PROPS
bool Console::enabledGuestPropertiesVRDP(void)
{
    Bstr value;
    HRESULT hrc = mMachine->GetExtraData(Bstr("VBoxInternal2/EnableGuestPropertiesVRDP"), value.asOutParam());
    if (hrc == S_OK)
    {
        if (value == "1")
        {
            return true;
        }
    }
    return false;
}

void Console::updateGuestPropertiesVRDPLogon(uint32_t u32ClientId, const char *pszUser, const char *pszDomain)
{
    if (!enabledGuestPropertiesVRDP())
    {
        return;
    }

    int rc;
    char *pszPropertyName;

    rc = RTStrAPrintf(&pszPropertyName, "/VirtualBox/HostInfo/VRDP/Client/%u/Name", u32ClientId);
    if (RT_SUCCESS(rc))
    {
        Bstr clientName;
        mRemoteDisplayInfo->COMGETTER(ClientName)(clientName.asOutParam());

        mMachine->SetGuestProperty(Bstr(pszPropertyName), clientName, Bstr("RDONLYGUEST"));
        RTStrFree(pszPropertyName);
    }

    rc = RTStrAPrintf(&pszPropertyName, "/VirtualBox/HostInfo/VRDP/Client/%u/User", u32ClientId);
    if (RT_SUCCESS(rc))
    {
        mMachine->SetGuestProperty(Bstr(pszPropertyName), Bstr(pszUser), Bstr("RDONLYGUEST"));
        RTStrFree(pszPropertyName);
    }

    rc = RTStrAPrintf(&pszPropertyName, "/VirtualBox/HostInfo/VRDP/Client/%u/Domain", u32ClientId);
    if (RT_SUCCESS(rc))
    {
        mMachine->SetGuestProperty(Bstr(pszPropertyName), Bstr(pszDomain), Bstr("RDONLYGUEST"));
        RTStrFree(pszPropertyName);
    }

    char *pszClientId;
    rc = RTStrAPrintf(&pszClientId, "%d", u32ClientId);
    if (RT_SUCCESS(rc))
    {
        mMachine->SetGuestProperty(Bstr("/VirtualBox/HostInfo/VRDP/LastConnectedClient"), Bstr(pszClientId), Bstr("RDONLYGUEST"));
        RTStrFree(pszClientId);
    }

    return;
}

void Console::updateGuestPropertiesVRDPDisconnect(uint32_t u32ClientId)
{
    if (!enabledGuestPropertiesVRDP())
    {
        return;
    }

    int rc;
    char *pszPropertyName;

    rc = RTStrAPrintf(&pszPropertyName, "/VirtualBox/HostInfo/VRDP/Client/%u/Name", u32ClientId);
    if (RT_SUCCESS(rc))
    {
        mMachine->SetGuestProperty(Bstr(pszPropertyName), Bstr(""), Bstr("RDONLYGUEST"));
        RTStrFree(pszPropertyName);
    }

    rc = RTStrAPrintf(&pszPropertyName, "/VirtualBox/HostInfo/VRDP/Client/%u/User", u32ClientId);
    if (RT_SUCCESS(rc))
    {
        mMachine->SetGuestProperty(Bstr(pszPropertyName), Bstr(""), Bstr("RDONLYGUEST"));
        RTStrFree(pszPropertyName);
    }

    rc = RTStrAPrintf(&pszPropertyName, "/VirtualBox/HostInfo/VRDP/Client/%u/Domain", u32ClientId);
    if (RT_SUCCESS(rc))
    {
        mMachine->SetGuestProperty(Bstr(pszPropertyName), Bstr(""), Bstr("RDONLYGUEST"));
        RTStrFree(pszPropertyName);
    }

    char *pszClientId;
    rc = RTStrAPrintf(&pszClientId, "%d", u32ClientId);
    if (RT_SUCCESS(rc))
    {
        mMachine->SetGuestProperty(Bstr("/VirtualBox/HostInfo/VRDP/LastDisconnectedClient"), Bstr(pszClientId), Bstr("RDONLYGUEST"));
        RTStrFree(pszClientId);
    }

    return;
}
#endif /* VBOX_WITH_GUEST_PROPS */


int Console::VRDPClientLogon(uint32_t u32ClientId, const char *pszUser, const char *pszPassword, const char *pszDomain)
{
    LogFlowFuncEnter();
    LogFlowFunc(("%d, %s, %s, %s\n", u32ClientId, pszUser, pszPassword, pszDomain));

    AutoCaller autoCaller(this);
    if (!autoCaller.isOk())
    {
        /* Console has been already uninitialized, deny request */
        LogRel(("VRDPAUTH: Access denied (Console uninitialized).\n"));
        LogFlowFuncLeave();
        return VERR_ACCESS_DENIED;
    }

    Bstr id;
    HRESULT hrc = mMachine->COMGETTER(Id)(id.asOutParam());
    Guid uuid = Guid(id);

    AssertComRCReturn(hrc, VERR_ACCESS_DENIED);

    VRDPAuthType_T authType = VRDPAuthType_Null;
    hrc = mVRDPServer->COMGETTER(AuthType)(&authType);
    AssertComRCReturn(hrc, VERR_ACCESS_DENIED);

    ULONG authTimeout = 0;
    hrc = mVRDPServer->COMGETTER(AuthTimeout)(&authTimeout);
    AssertComRCReturn(hrc, VERR_ACCESS_DENIED);

    VRDPAuthResult result = VRDPAuthAccessDenied;
    VRDPAuthGuestJudgement guestJudgement = VRDPAuthGuestNotAsked;

    LogFlowFunc(("Auth type %d\n", authType));

    LogRel(("VRDPAUTH: User: [%s]. Domain: [%s]. Authentication type: [%s]\n",
                pszUser, pszDomain,
                authType == VRDPAuthType_Null?
                    "Null":
                    (authType == VRDPAuthType_External?
                        "External":
                        (authType == VRDPAuthType_Guest?
                            "Guest":
                            "INVALID"
                        )
                    )
            ));

    switch (authType)
    {
        case VRDPAuthType_Null:
        {
            result = VRDPAuthAccessGranted;
            break;
        }

        case VRDPAuthType_External:
        {
            /* Call the external library. */
            result = mConsoleVRDPServer->Authenticate(uuid, guestJudgement, pszUser, pszPassword, pszDomain, u32ClientId);

            if (result != VRDPAuthDelegateToGuest)
            {
                break;
            }

            LogRel(("VRDPAUTH: Delegated to guest.\n"));

            LogFlowFunc(("External auth asked for guest judgement\n"));
        } /* pass through */

        case VRDPAuthType_Guest:
        {
            guestJudgement = VRDPAuthGuestNotReacted;

            if (mVMMDev)
            {
                /* Issue the request to guest. Assume that the call does not require EMT. It should not. */

                /* Ask the guest to judge these credentials. */
                uint32_t u32GuestFlags = VMMDEV_SETCREDENTIALS_JUDGE;

                int rc = mVMMDev->getVMMDevPort()->pfnSetCredentials(mVMMDev->getVMMDevPort(),
                             pszUser, pszPassword, pszDomain, u32GuestFlags);

                if (RT_SUCCESS(rc))
                {
                    /* Wait for guest. */
                    rc = mVMMDev->WaitCredentialsJudgement(authTimeout, &u32GuestFlags);

                    if (RT_SUCCESS(rc))
                    {
                        switch (u32GuestFlags & (VMMDEV_CREDENTIALS_JUDGE_OK | VMMDEV_CREDENTIALS_JUDGE_DENY | VMMDEV_CREDENTIALS_JUDGE_NOJUDGEMENT))
                        {
                            case VMMDEV_CREDENTIALS_JUDGE_DENY:        guestJudgement = VRDPAuthGuestAccessDenied;  break;
                            case VMMDEV_CREDENTIALS_JUDGE_NOJUDGEMENT: guestJudgement = VRDPAuthGuestNoJudgement;   break;
                            case VMMDEV_CREDENTIALS_JUDGE_OK:          guestJudgement = VRDPAuthGuestAccessGranted; break;
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

            if (authType == VRDPAuthType_External)
            {
                LogRel(("VRDPAUTH: Guest judgement %d.\n", guestJudgement));
                LogFlowFunc(("External auth called again with guest judgement = %d\n", guestJudgement));
                result = mConsoleVRDPServer->Authenticate(uuid, guestJudgement, pszUser, pszPassword, pszDomain, u32ClientId);
            }
            else
            {
                switch (guestJudgement)
                {
                    case VRDPAuthGuestAccessGranted:
                        result = VRDPAuthAccessGranted;
                        break;
                    default:
                        result = VRDPAuthAccessDenied;
                        break;
                }
            }
        } break;

        default:
            AssertFailed();
    }

    LogFlowFunc(("Result = %d\n", result));
    LogFlowFuncLeave();

    if (result != VRDPAuthAccessGranted)
    {
        /* Reject. */
        LogRel(("VRDPAUTH: Access denied.\n"));
        return VERR_ACCESS_DENIED;
    }

    LogRel(("VRDPAUTH: Access granted.\n"));

    /* Multiconnection check must be made after authentication, so bad clients would not interfere with a good one. */
    BOOL allowMultiConnection = FALSE;
    hrc = mVRDPServer->COMGETTER(AllowMultiConnection)(&allowMultiConnection);
    AssertComRCReturn(hrc, VERR_ACCESS_DENIED);

    BOOL reuseSingleConnection = FALSE;
    hrc = mVRDPServer->COMGETTER(ReuseSingleConnection)(&reuseSingleConnection);
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
                LogRel(("VRDPAUTH: Multiple connections are not enabled. Disconnecting existing client.\n"));
                mConsoleVRDPServer->DisconnectClient(mu32SingleRDPClientId, false);
            }
            else
            {
                /* Reject. */
                LogRel(("VRDPAUTH: Multiple connections are not enabled. Access denied.\n"));
                return VERR_ACCESS_DENIED;
            }
        }

        /* Save the connected client id. From now on it will be necessary to disconnect this one. */
        mu32SingleRDPClientId = u32ClientId;
    }

#ifdef VBOX_WITH_GUEST_PROPS
    updateGuestPropertiesVRDPLogon(u32ClientId, pszUser, pszDomain);
#endif /* VBOX_WITH_GUEST_PROPS */

    return VINF_SUCCESS;
}

void Console::VRDPClientConnect(uint32_t u32ClientId)
{
    LogFlowFuncEnter();

    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

#ifdef VBOX_WITH_VRDP
    uint32_t u32Clients = ASMAtomicIncU32(&mcVRDPClients);

    if (u32Clients == 1)
    {
        getVMMDev()->getVMMDevPort()->
            pfnVRDPChange(getVMMDev()->getVMMDevPort(),
                          true, VRDP_EXPERIENCE_LEVEL_FULL); // @todo configurable
    }

    NOREF(u32ClientId);
    mDisplay->VideoAccelVRDP(true);
#endif /* VBOX_WITH_VRDP */

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

#ifdef VBOX_WITH_VRDP
    uint32_t u32Clients = ASMAtomicDecU32(&mcVRDPClients);

    if (u32Clients == 0)
    {
        getVMMDev()->getVMMDevPort()->
            pfnVRDPChange(getVMMDev()->getVMMDevPort(),
                          false, 0);
    }

    mDisplay->VideoAccelVRDP(false);
#endif /* VBOX_WITH_VRDP */

    if (fu32Intercepted & VRDP_CLIENT_INTERCEPT_USB)
    {
        mConsoleVRDPServer->USBBackendDelete(u32ClientId);
    }

#ifdef VBOX_WITH_VRDP
    if (fu32Intercepted & VRDP_CLIENT_INTERCEPT_CLIPBOARD)
    {
        mConsoleVRDPServer->ClipboardDelete(u32ClientId);
    }

    if (fu32Intercepted & VRDP_CLIENT_INTERCEPT_AUDIO)
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
#endif /* VBOX_WITH_VRDP */

    Bstr uuid;
    HRESULT hrc = mMachine->COMGETTER(Id)(uuid.asOutParam());
    AssertComRC(hrc);

    VRDPAuthType_T authType = VRDPAuthType_Null;
    hrc = mVRDPServer->COMGETTER(AuthType)(&authType);
    AssertComRC(hrc);

    if (authType == VRDPAuthType_External)
        mConsoleVRDPServer->AuthDisconnect(uuid, u32ClientId);

#ifdef VBOX_WITH_GUEST_PROPS
    updateGuestPropertiesVRDPDisconnect(u32ClientId);
#endif /* VBOX_WITH_GUEST_PROPS */

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

#ifdef VBOX_WITH_VRDP
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
#endif

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

#ifdef VBOX_WITH_VRDP
    mConsoleVRDPServer->ClipboardCreate(u32ClientId);
#endif /* VBOX_WITH_VRDP */

    LogFlowFuncLeave();
    return;
}


//static
const char *Console::sSSMConsoleUnit = "ConsoleData";
//static
uint32_t Console::sSSMConsoleVer = 0x00010001;

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
            tr("The saved state file '%ls' is invalid (%Rrc). Discard the saved state and try again"),
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

    AutoReadLock alock(that);

    int vrc = SSMR3PutU32(pSSM, (uint32_t)that->mSharedFolders.size());
    AssertRC(vrc);

    for (SharedFolderMap::const_iterator it = that->mSharedFolders.begin();
         it != that->mSharedFolders.end();
         ++ it)
    {
        ComObjPtr<SharedFolder> folder = (*it).second;
        // don't lock the folder because methods we access are const

        Utf8Str name = folder->name();
        vrc = SSMR3PutU32(pSSM, (uint32_t)name.length() + 1 /* term. 0 */);
        AssertRC(vrc);
        vrc = SSMR3PutStrZ(pSSM, name.c_str());
        AssertRC(vrc);

        Utf8Str hostPath = folder->hostPath();
        vrc = SSMR3PutU32(pSSM, (uint32_t)hostPath.length() + 1 /* term. 0 */);
        AssertRC(vrc);
        vrc = SSMR3PutStrZ(pSSM, hostPath.c_str());
        AssertRC(vrc);

        vrc = SSMR3PutBool(pSSM, !!folder->writable());
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

    AutoWriteLock alock(this);

    AssertReturn(mSharedFolders.size() == 0, VERR_INTERNAL_ERROR);

    uint32_t size = 0;
    int vrc = SSMR3GetU32(pSSM, &size);
    AssertRCReturn(vrc, vrc);

    for (uint32_t i = 0; i < size; ++ i)
    {
        Bstr name;
        Bstr hostPath;
        bool writable = true;

        uint32_t szBuf = 0;
        char *buf = NULL;

        vrc = SSMR3GetU32(pSSM, &szBuf);
        AssertRCReturn(vrc, vrc);
        buf = new char[szBuf];
        vrc = SSMR3GetStrZ(pSSM, buf, szBuf);
        AssertRC(vrc);
        name = buf;
        delete[] buf;

        vrc = SSMR3GetU32(pSSM, &szBuf);
        AssertRCReturn(vrc, vrc);
        buf = new char[szBuf];
        vrc = SSMR3GetStrZ(pSSM, buf, szBuf);
        AssertRC(vrc);
        hostPath = buf;
        delete[] buf;

        if (u32Version > 0x00010000)
            SSMR3GetBool(pSSM, &writable);

        ComObjPtr<SharedFolder> sharedFolder;
        sharedFolder.createObject();
        HRESULT rc = sharedFolder->init(this, name, hostPath, writable);
        AssertComRCReturn(rc, VERR_INTERNAL_ERROR);

        mSharedFolders.insert(std::make_pair(name, sharedFolder));
    }

    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_GUEST_PROPS
// static
DECLCALLBACK(int)
Console::doGuestPropNotification(void *pvExtension, uint32_t,
                                 void *pvParms, uint32_t cbParms)
{
    using namespace guestProp;

//     LogFlowFunc(("pvExtension=%p, pvParms=%p, cbParms=%u\n", pvExtension, pvParms, cbParms));
    int rc = VINF_SUCCESS;
    /* No locking, as this is purely a notification which does not make any
     * changes to the object state. */
    PHOSTCALLBACKDATA pCBData = reinterpret_cast<PHOSTCALLBACKDATA>(pvParms);
    AssertReturn(sizeof(HOSTCALLBACKDATA) == cbParms, VERR_INVALID_PARAMETER);
    AssertReturn(HOSTCALLBACKMAGIC == pCBData->u32Magic, VERR_INVALID_PARAMETER);
    ComObjPtr<Console> pConsole = reinterpret_cast<Console *>(pvExtension);
//     LogFlowFunc(("pCBData->pcszName=%s, pCBData->pcszValue=%s, pCBData->pcszFlags=%s\n", pCBData->pcszName, pCBData->pcszValue, pCBData->pcszFlags));
    Bstr name(pCBData->pcszName);
    Bstr value(pCBData->pcszValue);
    Bstr flags(pCBData->pcszFlags);
    if (   name.isNull()
        || (value.isNull() && (pCBData->pcszValue != NULL))
        || (flags.isNull() && (pCBData->pcszFlags != NULL))
       )
        rc = VERR_NO_MEMORY;
    else
    {
        HRESULT hrc = pConsole->mControl->PushGuestProperty(name, value,
                                                            pCBData->u64Timestamp,
                                                            flags);
        if (FAILED(hrc))
        {
//             LogFunc(("pConsole->mControl->PushGuestProperty failed, hrc=0x%x\n", hrc));
//             LogFunc(("pCBData->pcszName=%s\n", pCBData->pcszName));
//             LogFunc(("pCBData->pcszValue=%s\n", pCBData->pcszValue));
//             LogFunc(("pCBData->pcszFlags=%s\n", pCBData->pcszFlags));
            rc = VERR_UNRESOLVED_ERROR;  /** @todo translate error code */
        }
    }
//     LogFlowFunc(("rc=%Rrc\n", rc));
    return rc;
}

HRESULT Console::doEnumerateGuestProperties(CBSTR aPatterns,
                                            ComSafeArrayOut(BSTR, aNames),
                                            ComSafeArrayOut(BSTR, aValues),
                                            ComSafeArrayOut(ULONG64, aTimestamps),
                                            ComSafeArrayOut(BSTR, aFlags))
{
    using namespace guestProp;

    VBOXHGCMSVCPARM parm[3];

    Utf8Str utf8Patterns(aPatterns);
    parm[0].type = VBOX_HGCM_SVC_PARM_PTR;
    parm[0].u.pointer.addr = utf8Patterns.mutableRaw();
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
        vrc = mVMMDev->hgcmHostCall("VBoxGuestPropSvc", ENUM_PROPS_HOST, 3,
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
    com::SafeArray<ULONG64> timestamps(cEntries);
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
#endif


// IConsole properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Console::COMGETTER(Machine)(IMachine **aMachine)
{
    CheckComArgOutPointerValid(aMachine);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

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
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    /* we return our local state (since it's always the same as on the server) */
    *aMachineState = mMachineState;

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(Guest)(IGuest **aGuest)
{
    CheckComArgOutPointerValid(aGuest);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* mGuest is constant during life time, no need to lock */
    mGuest.queryInterfaceTo(aGuest);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(Keyboard)(IKeyboard **aKeyboard)
{
    CheckComArgOutPointerValid(aKeyboard);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* mKeyboard is constant during life time, no need to lock */
    mKeyboard.queryInterfaceTo(aKeyboard);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(Mouse)(IMouse **aMouse)
{
    CheckComArgOutPointerValid(aMouse);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* mMouse is constant during life time, no need to lock */
    mMouse.queryInterfaceTo(aMouse);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(Display)(IDisplay **aDisplay)
{
    CheckComArgOutPointerValid(aDisplay);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* mDisplay is constant during life time, no need to lock */
    mDisplay.queryInterfaceTo(aDisplay);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(Debugger)(IMachineDebugger **aDebugger)
{
    CheckComArgOutPointerValid(aDebugger);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* we need a write lock because of the lazy mDebugger initialization*/
    AutoWriteLock alock(this);

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
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    SafeIfaceArray<IUSBDevice> collection(mUSBDevices);
    collection.detachTo(ComSafeArrayOutArg(aUSBDevices));

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(RemoteUSBDevices)(ComSafeArrayOut(IHostUSBDevice *, aRemoteUSBDevices))
{
    CheckComArgOutSafeArrayPointerValid(aRemoteUSBDevices);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    SafeIfaceArray<IHostUSBDevice> collection(mRemoteUSBDevices);
    collection.detachTo(ComSafeArrayOutArg(aRemoteUSBDevices));

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(RemoteDisplayInfo)(IRemoteDisplayInfo **aRemoteDisplayInfo)
{
    CheckComArgOutPointerValid(aRemoteDisplayInfo);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* mDisplay is constant during life time, no need to lock */
    mRemoteDisplayInfo.queryInterfaceTo(aRemoteDisplayInfo);

    return S_OK;
}

STDMETHODIMP
Console::COMGETTER(SharedFolders)(ComSafeArrayOut(ISharedFolder *, aSharedFolders))
{
    CheckComArgOutSafeArrayPointerValid(aSharedFolders);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* loadDataFromSavedState() needs a write lock */
    AutoWriteLock alock(this);

    /* Read console data stored in the saved state file (if not yet done) */
    HRESULT rc = loadDataFromSavedState();
    CheckComRCReturnRC(rc);

    SafeIfaceArray<ISharedFolder> sf(mSharedFolders);
    sf.detachTo(ComSafeArrayOutArg(aSharedFolders));

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
    if (aProgress == NULL)
        return E_POINTER;

    LogFlowThisFuncEnter();
    LogFlowThisFunc(("mMachineState=%d\n", mMachineState));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

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

        /* extra nice error message for a common case */
        case MachineState_Saved:
            return setError(VBOX_E_INVALID_VM_STATE, tr("Cannot power down a saved virtual machine"));
        case MachineState_Stopping:
            return setError(VBOX_E_INVALID_VM_STATE, tr("Virtual machine is being powered down"));
        default:
            return setError(VBOX_E_INVALID_VM_STATE,
                            tr("Invalid machine state: %s (must be Running, Paused or Stuck)"),
                            Global::stringifyMachineState(mMachineState));
    }

    LogFlowThisFunc(("Initiating SHUTDOWN request...\n"));

    /* create an IProgress object to track progress of this operation */
    ComObjPtr<Progress> progress;
    progress.createObject();
    progress->init(static_cast<IConsole *>(this),
                   Bstr(tr("Stopping virtual machine")),
                   FALSE /* aCancelable */);

    /* setup task object and thread to carry out the operation asynchronously */
    std::auto_ptr<VMProgressTask> task(new VMProgressTask(this, progress, true /* aUsesVMPtr */));
    AssertReturn(task->isOk(), E_FAIL);

    int vrc = RTThreadCreate(NULL, Console::powerDownThread,
                             (void *) task.get(), 0,
                             RTTHREADTYPE_MAIN_WORKER, 0,
                             "VMPowerDown");
    ComAssertMsgRCRet(vrc, ("Could not create VMPowerDown thread (%Rrc)", vrc), E_FAIL);

    /* task is now owned by powerDownThread(), so release it */
    task.release();

    /* go to Stopping state to forbid state-dependant operations */
    setMachineState(MachineState_Stopping);

    /* pass the progress to the caller */
    progress.queryInterfaceTo(aProgress);

    LogFlowThisFuncLeave();

    return S_OK;
}

STDMETHODIMP Console::Reset()
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("mMachineState=%d\n", mMachineState));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    if (   mMachineState != MachineState_Running
        && mMachineState != MachineState_Teleporting
        && mMachineState != MachineState_LiveSnapshotting
        /** @todo r=bird: This should be allowed on paused VMs as well. Later.  */
       )
        return setError(VBOX_E_INVALID_VM_STATE,
            tr("Invalid machine state: %s"),
            Global::stringifyMachineState(mMachineState));

    /* protect mpVM */
    AutoVMCaller autoVMCaller(this);
    CheckComRCReturnRC(autoVMCaller.rc());

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

    int vrc = VMR3Reset(mpVM);

    HRESULT rc = RT_SUCCESS(vrc) ? S_OK :
        setError(VBOX_E_VM_ERROR,
            tr("Could not reset the machine (%Rrc)"),
            vrc);

    LogFlowThisFunc(("mMachineState=%d, rc=%08X\n", mMachineState, rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::Pause()
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    switch (mMachineState)
    {
        case MachineState_Running:
        case MachineState_Teleporting:
        case MachineState_LiveSnapshotting:
            break;

        case MachineState_Paused:
        case MachineState_TeleportingPausedVM:
            return setError(VBOX_E_INVALID_VM_STATE, tr("Already paused"));

        default:
            return setError(VBOX_E_INVALID_VM_STATE,
                            tr("Invalid machine state: %s"),
                            Global::stringifyMachineState(mMachineState));
    }

    /* protect mpVM */
    AutoVMCaller autoVMCaller(this);
    CheckComRCReturnRC(autoVMCaller.rc());

    LogFlowThisFunc(("Sending PAUSE request...\n"));

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

    int vrc = VMR3Suspend(mpVM);

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
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    if (mMachineState != MachineState_Paused)
        return setError(VBOX_E_INVALID_VM_STATE,
            tr("Cannot resume the machine as it is not paused (machine state: %s)"),
            Global::stringifyMachineState(mMachineState));

    /* protect mpVM */
    AutoVMCaller autoVMCaller(this);
    CheckComRCReturnRC(autoVMCaller.rc());

    LogFlowThisFunc(("Sending RESUME request...\n"));

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

    int vrc;
    if (VMR3GetState(mpVM) == VMSTATE_CREATED)
        vrc = VMR3PowerOn(mpVM); /* (PowerUpPaused) */
    else
        vrc = VMR3Resume(mpVM);

    HRESULT rc = RT_SUCCESS(vrc) ? S_OK :
        setError(VBOX_E_VM_ERROR,
            tr("Could not resume the machine execution (%Rrc)"),
            vrc);

    LogFlowThisFunc(("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::PowerButton()
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    if (   mMachineState != MachineState_Running
        && mMachineState != MachineState_Teleporting
        && mMachineState != MachineState_LiveSnapshotting
       )
        return setError(VBOX_E_INVALID_VM_STATE,
            tr("Invalid machine state: %s"),
            Global::stringifyMachineState(mMachineState));

    /* protect mpVM */
    AutoVMCaller autoVMCaller(this);
    CheckComRCReturnRC(autoVMCaller.rc());

    PPDMIBASE pBase;
    int vrc = PDMR3QueryDeviceLun(mpVM, "acpi", 0, 0, &pBase);
    if (RT_SUCCESS(vrc))
    {
        Assert(pBase);
        PPDMIACPIPORT pPort =
            (PPDMIACPIPORT) pBase->pfnQueryInterface(pBase, PDMINTERFACE_ACPI_PORT);
        vrc = pPort ? pPort->pfnPowerButtonPress(pPort) : VERR_INVALID_POINTER;
    }

    HRESULT rc = RT_SUCCESS(vrc) ? S_OK :
        setError(VBOX_E_PDM_ERROR,
            tr("Controlled power off failed (%Rrc)"),
            vrc);

    LogFlowThisFunc(("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::GetPowerButtonHandled(BOOL *aHandled)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aHandled);

    *aHandled = FALSE;

    AutoCaller autoCaller(this);

    AutoWriteLock alock(this);

    if (   mMachineState != MachineState_Running
        && mMachineState != MachineState_Teleporting
        && mMachineState != MachineState_LiveSnapshotting
       )
        return setError(VBOX_E_INVALID_VM_STATE,
            tr("Invalid machine state: %s"),
            Global::stringifyMachineState(mMachineState));

    /* protect mpVM */
    AutoVMCaller autoVMCaller(this);
    CheckComRCReturnRC(autoVMCaller.rc());

    PPDMIBASE pBase;
    int vrc = PDMR3QueryDeviceLun(mpVM, "acpi", 0, 0, &pBase);
    bool handled = false;
    if (RT_SUCCESS(vrc))
    {
        Assert(pBase);
        PPDMIACPIPORT pPort =
            (PPDMIACPIPORT) pBase->pfnQueryInterface(pBase, PDMINTERFACE_ACPI_PORT);
        vrc = pPort ? pPort->pfnGetPowerButtonHandled(pPort, &handled) : VERR_INVALID_POINTER;
    }

    HRESULT rc = RT_SUCCESS(vrc) ? S_OK :
        setError(VBOX_E_PDM_ERROR,
            tr("Checking if the ACPI Power Button event was handled by the guest OS failed (%Rrc)"),
            vrc);

    *aHandled = handled;

    LogFlowThisFunc(("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::GetGuestEnteredACPIMode(BOOL *aEntered)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aEntered);

    *aEntered = FALSE;

    AutoCaller autoCaller(this);

    AutoWriteLock alock(this);

    if (   mMachineState != MachineState_Running
        && mMachineState != MachineState_Teleporting
        && mMachineState != MachineState_LiveSnapshotting
       )
        return setError(VBOX_E_INVALID_VM_STATE,
            tr("Invalid machine state %s when checking if the guest entered the ACPI mode)"),
            Global::stringifyMachineState(mMachineState));

    /* protect mpVM */
    AutoVMCaller autoVMCaller(this);
    CheckComRCReturnRC(autoVMCaller.rc());

    PPDMIBASE pBase;
    int vrc = PDMR3QueryDeviceLun(mpVM, "acpi", 0, 0, &pBase);
    bool entered = false;
    if (RT_SUCCESS(vrc))
    {
        Assert(pBase);
        PPDMIACPIPORT pPort =
            (PPDMIACPIPORT) pBase->pfnQueryInterface(pBase, PDMINTERFACE_ACPI_PORT);
        vrc = pPort ? pPort->pfnGetGuestEnteredACPIMode(pPort, &entered) : VERR_INVALID_POINTER;
    }

    *aEntered = RT_SUCCESS(vrc) ? entered : false;

    LogFlowThisFuncLeave();
    return S_OK;
}

STDMETHODIMP Console::SleepButton()
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    if (mMachineState != MachineState_Running) /** @todo Live Migration: ??? */
        return setError(VBOX_E_INVALID_VM_STATE,
            tr("Invalid machine state: %s)"),
            Global::stringifyMachineState(mMachineState));

    /* protect mpVM */
    AutoVMCaller autoVMCaller(this);
    CheckComRCReturnRC(autoVMCaller.rc());

    PPDMIBASE pBase;
    int vrc = PDMR3QueryDeviceLun(mpVM, "acpi", 0, 0, &pBase);
    if (RT_SUCCESS(vrc))
    {
        Assert(pBase);
        PPDMIACPIPORT pPort =
            (PPDMIACPIPORT) pBase->pfnQueryInterface(pBase, PDMINTERFACE_ACPI_PORT);
        vrc = pPort ? pPort->pfnSleepButtonPress(pPort) : VERR_INVALID_POINTER;
    }

    HRESULT rc = RT_SUCCESS(vrc) ? S_OK :
        setError(VBOX_E_PDM_ERROR,
            tr("Sending sleep button event failed (%Rrc)"),
            vrc);

    LogFlowThisFunc(("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::SaveState(IProgress **aProgress)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("mMachineState=%d\n", mMachineState));

    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

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
        CheckComRCReturnRC(rc);
    }

    HRESULT rc = S_OK;

    /* create a progress object to track operation completion */
    ComObjPtr<Progress> progress;
    progress.createObject();
    progress->init(static_cast<IConsole *>(this),
                   Bstr(tr("Saving the execution state of the virtual machine")),
                   FALSE /* aCancelable */);

    bool fBeganSavingState = false;
    bool fTaskCreationFailed = false;

    do
    {
        /* create a task object early to ensure mpVM protection is successful */
        std::auto_ptr <VMSaveTask> task(new VMSaveTask(this, progress));
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

        Bstr stateFilePath;

        /*
         * request a saved state file path from the server
         * (this will set the machine state to Saving on the server to block
         * others from accessing this machine)
         */
        rc = mControl->BeginSavingState(progress, stateFilePath.asOutParam());
        CheckComRCBreakRC(rc);

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
                        dir.raw(), vrc);
                    break;
                }
            }
        }

        /* setup task object and thread to carry out the operation asynchronously */
        task->mSavedStateFile = stateFilePath;
        /* set the state the operation thread will restore when it is finished */
        task->mLastMachineState = lastMachineState;

        /* create a thread to wait until the VM state is saved */
        int vrc = RTThreadCreate(NULL, Console::saveStateThread, (void *) task.get(),
                                 0, RTTHREADTYPE_MAIN_WORKER, 0, "VMSave");

        ComAssertMsgRCBreak(vrc, ("Could not create VMSave thread (%Rrc)", vrc),
                            rc = E_FAIL);

        /* task is now owned by saveStateThread(), so release it */
        task.release();

        /* return the progress to the caller */
        progress.queryInterfaceTo(aProgress);
    }
    while (0);

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
            mControl->EndSavingState(FALSE);
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

    LogFlowThisFunc(("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::AdoptSavedState(IN_BSTR aSavedStateFile)
{
    CheckComArgNotNull(aSavedStateFile);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    if (   mMachineState != MachineState_PoweredOff
        && mMachineState != MachineState_Teleported
        && mMachineState != MachineState_Aborted
       )
        return setError(VBOX_E_INVALID_VM_STATE,
            tr("Cannot adopt the saved machine state as the machine is not in Powered Off, Teleported or Aborted state (machine state: %s)"),
            Global::stringifyMachineState(mMachineState));

    return mControl->AdoptSavedState(aSavedStateFile);
}

STDMETHODIMP Console::ForgetSavedState(BOOL aRemove)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    if (mMachineState != MachineState_Saved)
        return setError(VBOX_E_INVALID_VM_STATE,
            tr("Cannot discard the machine state as the machine is not in the saved state (machine state: %s)"),
            Global::stringifyMachineState(mMachineState));

    HRESULT rc = S_OK;

    rc = mControl->SetRemoveSavedState(aRemove);
    CheckComRCReturnRC(rc);

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
    CheckComRCReturnRC(autoCaller.rc());

    /*
     * Note: we don't lock the console object here because
     * readAndClearLed() should be thread safe.
     */

    /* Get LED array to read */
    PDMLEDCORE SumLed = {0};
    switch (aDeviceType)
    {
        case DeviceType_Floppy:
        {
            for (unsigned i = 0; i < RT_ELEMENTS(mapFDLeds); ++i)
                SumLed.u32 |= readAndClearLed(mapFDLeds[i]);
            break;
        }

        case DeviceType_DVD:
        {
            SumLed.u32 |= readAndClearLed(mapIDELeds[2]);
            break;
        }

        case DeviceType_HardDisk:
        {
            SumLed.u32 |= readAndClearLed(mapIDELeds[0]);
            SumLed.u32 |= readAndClearLed(mapIDELeds[1]);
            SumLed.u32 |= readAndClearLed(mapIDELeds[3]);
            for (unsigned i = 0; i < RT_ELEMENTS(mapSATALeds); ++i)
                SumLed.u32 |= readAndClearLed(mapSATALeds[i]);
            for (unsigned i = 0; i < RT_ELEMENTS(mapSCSILeds); ++i)
                SumLed.u32 |= readAndClearLed(mapSCSILeds[i]);
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
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    if (   mMachineState != MachineState_Running
        && mMachineState != MachineState_Paused)
        return setError(VBOX_E_INVALID_VM_STATE,
            tr("Cannot attach a USB device to the machine which is not running or paused (machine state: %s)"),
            Global::stringifyMachineState(mMachineState));

    /* protect mpVM */
    AutoVMCaller autoVMCaller(this);
    CheckComRCReturnRC(autoVMCaller.rc());

    /* Don't proceed unless we've found the usb controller. */
    PPDMIBASE pBase = NULL;
    int vrc = PDMR3QueryLun(mpVM, "usb-ohci", 0, 0, &pBase);
    if (RT_FAILURE(vrc))
        return setError(VBOX_E_PDM_ERROR,
            tr("The virtual machine does not have a USB controller"));

    /* leave the lock because the USB Proxy service may call us back
     * (via onUSBDeviceAttach()) */
    alock.leave();

    /* Request the device capture */
    HRESULT rc = mControl->CaptureUSBDevice(aId);
    CheckComRCReturnRC(rc);

    return rc;

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
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    /* Find it. */
    ComObjPtr<OUSBDevice> device;
    USBDeviceList::iterator it = mUSBDevices.begin();
    Guid uuid(aId);
    while (it != mUSBDevices.end())
    {
        if ((*it)->id() == uuid)
        {
            device = *it;
            break;
        }
        ++ it;
    }

    if (!device)
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
    CheckComArgNotNull(aAddress);
    CheckComArgOutPointerValid(aDevice);

    *aDevice = NULL;

    SafeIfaceArray<IUSBDevice> devsvec;
    HRESULT rc = COMGETTER(USBDevices)(ComSafeArrayAsOutParam(devsvec));
    CheckComRCReturnRC(rc);

    for (size_t i = 0; i < devsvec.size(); ++i)
    {
        Bstr address;
        rc = devsvec[i]->COMGETTER(Address)(address.asOutParam());
        CheckComRCReturnRC(rc);
        if (address == aAddress)
        {
            ComObjPtr<OUSBDevice> found;
            found.createObject();
            found->init(devsvec[i]);
            return found.queryInterfaceTo(aDevice);
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
    CheckComRCReturnRC(rc);

    for (size_t i = 0; i < devsvec.size(); ++i)
    {
        Bstr id;
        rc = devsvec[i]->COMGETTER(Id)(id.asOutParam());
        CheckComRCReturnRC(rc);
        if (id == aId)
        {
            ComObjPtr<OUSBDevice> found;
            found.createObject();
            found->init(devsvec[i]);
            return found.queryInterfaceTo(aDevice);
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
Console::CreateSharedFolder(IN_BSTR aName, IN_BSTR aHostPath, BOOL aWritable)
{
    CheckComArgNotNull(aName);
    CheckComArgNotNull(aHostPath);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

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

    ComObjPtr<SharedFolder> sharedFolder;
    HRESULT rc = findSharedFolder(aName, sharedFolder, false /* aSetError */);
    if (SUCCEEDED(rc))
        return setError(VBOX_E_FILE_ERROR,
            tr("Shared folder named '%ls' already exists"),
            aName);

    sharedFolder.createObject();
    rc = sharedFolder->init(this, aName, aHostPath, aWritable);
    CheckComRCReturnRC(rc);

    /* protect mpVM (if not NULL) */
    AutoVMCallerQuietWeak autoVMCaller(this);

    if (mpVM && autoVMCaller.isOk() && mVMMDev->isShFlActive())
    {
        /* If the VM is online and supports shared folders, share this folder
         * under the specified name. */

        /* first, remove the machine or the global folder if there is any */
        SharedFolderDataMap::const_iterator it;
        if (findOtherSharedFolder(aName, it))
        {
            rc = removeSharedFolder(aName);
            CheckComRCReturnRC(rc);
        }

        /* second, create the given folder */
        rc = createSharedFolder(aName, SharedFolderData(aHostPath, aWritable));
        CheckComRCReturnRC(rc);
    }

    mSharedFolders.insert(std::make_pair(aName, sharedFolder));

    /* notify console callbacks after the folder is added to the list */
    {
        CallbackList::iterator it = mCallbacks.begin();
        while (it != mCallbacks.end())
            (*it++)->OnSharedFolderChange(Scope_Session);
    }

    return rc;
}

STDMETHODIMP Console::RemoveSharedFolder(IN_BSTR aName)
{
    CheckComArgNotNull(aName);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

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

    ComObjPtr<SharedFolder> sharedFolder;
    HRESULT rc = findSharedFolder(aName, sharedFolder, true /* aSetError */);
    CheckComRCReturnRC(rc);

    /* protect mpVM (if not NULL) */
    AutoVMCallerQuietWeak autoVMCaller(this);

    if (mpVM && autoVMCaller.isOk() && mVMMDev->isShFlActive())
    {
        /* if the VM is online and supports shared folders, UNshare this
         * folder. */

        /* first, remove the given folder */
        rc = removeSharedFolder(aName);
        CheckComRCReturnRC(rc);

        /* first, remove the machine or the global folder if there is any */
        SharedFolderDataMap::const_iterator it;
        if (findOtherSharedFolder(aName, it))
        {
            rc = createSharedFolder(aName, it->second);
            /* don't check rc here because we need to remove the console
             * folder from the collection even on failure */
        }
    }

    mSharedFolders.erase(aName);

    /* notify console callbacks after the folder is removed to the list */
    {
        CallbackList::iterator it = mCallbacks.begin();
        while (it != mCallbacks.end())
            (*it++)->OnSharedFolderChange(Scope_Session);
    }

    return rc;
}

STDMETHODIMP Console::TakeSnapshot(IN_BSTR aName,
                                   IN_BSTR aDescription,
                                   IProgress **aProgress)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aName='%ls' mMachineState=%08X\n", aName, mMachineState));

    CheckComArgNotNull(aName);
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

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
    if (FAILED(rc)) return rc;

    for (size_t i = 0;
         i < aMediumAttachments.size();
         ++i)
    {
        DeviceType_T type;
        rc = aMediumAttachments[i]->COMGETTER(Type)(&type);
        if (FAILED(rc)) return rc;

        if (type == DeviceType_HardDisk)
        {
            ++cOperations;

            // assume that creating a diff image takes as long as saving a 1 MB state
            // (note, the same value must be used in SessionMachine::BeginTakingSnapshot() on the server!)
            ulTotalOperationsWeight += 1;
        }
    }

    // b) one extra sub-operations for online snapshots OR offline snapshots that have a saved state (needs to be copied)
    bool fTakingSnapshotOnline = ((mMachineState == MachineState_Running) || (mMachineState == MachineState_Paused));

    LogFlowFunc(("fTakingSnapshotOnline = %d, mMachineState = %d\n", fTakingSnapshotOnline, mMachineState));

    if (    fTakingSnapshotOnline
         || (mMachineState == MachineState_Saved)
       )
    {
        ++cOperations;

        ulTotalOperationsWeight += ulMemSize;
    }

    // finally, create the progress object
    ComObjPtr<Progress> pProgress;
    pProgress.createObject();
    rc = pProgress->init(static_cast<IConsole*>(this),
                         Bstr(tr("Taking a snapshot of the virtual machine")),
                         FALSE /* aCancelable */,
                         cOperations,
                         ulTotalOperationsWeight,
                         Bstr(tr("Setting up snapshot operation")),      // first sub-op description
                         1);        // ulFirstOperationWeight

    if (FAILED(rc)) return rc;

    VMTakeSnapshotTask *pTask;
    if (!(pTask = new VMTakeSnapshotTask(this, pProgress, aName, aDescription)))
        return VERR_NO_MEMORY;

    Assert(pTask->mProgress);

    try
    {
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
                                 (void*)pTask,
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
    catch (HRESULT rc)
    {
        delete pTask;
        NOREF(rc);
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
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    if (Global::IsOnlineOrTransient(mMachineState))
        return setError(VBOX_E_INVALID_VM_STATE,
            tr("Cannot discard a snapshot of the running machine (machine state: %s)"),
            Global::stringifyMachineState(mMachineState));

    MachineState_T machineState = MachineState_Null;
    HRESULT rc = mControl->DeleteSnapshot(this, aId, &machineState, aProgress);
    CheckComRCReturnRC(rc);

    setMachineStateLocally(machineState);
    return S_OK;
}

STDMETHODIMP Console::RestoreSnapshot(ISnapshot *aSnapshot, IProgress **aProgress)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    if (Global::IsOnlineOrTransient(mMachineState))
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("Cannot discard the current state of the running machine (machine state: %s)"),
                        Global::stringifyMachineState(mMachineState));

    MachineState_T machineState = MachineState_Null;
    HRESULT rc = mControl->RestoreSnapshot(this, aSnapshot, &machineState, aProgress);
    CheckComRCReturnRC(rc);

    setMachineStateLocally(machineState);
    return S_OK;
}

STDMETHODIMP Console::RegisterCallback(IConsoleCallback *aCallback)
{
    CheckComArgNotNull(aCallback);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

#if 0 /** @todo r=bird,r=pritesh: must check that the interface id match correct or we might screw up with old code! */
    void *dummy;
    HRESULT hrc = aCallback->QueryInterface(NS_GET_IID(IConsoleCallback), &dummy);
    if (FAILED(hrc))
        return hrc;
    aCallback->Release();
#endif

    AutoWriteLock alock(this);

    mCallbacks.push_back(CallbackList::value_type(aCallback));

    /* Inform the callback about the current status (for example, the new
     * callback must know the current mouse capabilities and the pointer
     * shape in order to properly integrate the mouse pointer). */

    if (mCallbackData.mpsc.valid)
        aCallback->OnMousePointerShapeChange(mCallbackData.mpsc.visible,
                                             mCallbackData.mpsc.alpha,
                                             mCallbackData.mpsc.xHot,
                                             mCallbackData.mpsc.yHot,
                                             mCallbackData.mpsc.width,
                                             mCallbackData.mpsc.height,
                                             mCallbackData.mpsc.shape);
    if (mCallbackData.mcc.valid)
        aCallback->OnMouseCapabilityChange(mCallbackData.mcc.supportsAbsolute,
                                           mCallbackData.mcc.needsHostCursor);

    aCallback->OnAdditionsStateChange();

    if (mCallbackData.klc.valid)
        aCallback->OnKeyboardLedsChange(mCallbackData.klc.numLock,
                                        mCallbackData.klc.capsLock,
                                        mCallbackData.klc.scrollLock);

    /* Note: we don't call OnStateChange for new callbacks because the
     * machine state is a) not actually changed on callback registration
     * and b) can be always queried from Console. */

    return S_OK;
}

STDMETHODIMP Console::UnregisterCallback(IConsoleCallback *aCallback)
{
    CheckComArgNotNull(aCallback);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    CallbackList::iterator it;
    it = std::find(mCallbacks.begin(),
                   mCallbacks.end(),
                   CallbackList::value_type(aCallback));
    if (it == mCallbacks.end())
        return setError(E_INVALIDARG,
            tr("The given callback handler is not registered"));

    mCallbacks.erase(it);
    return S_OK;
}

// Non-interface public methods
/////////////////////////////////////////////////////////////////////////////


/* static */
const char *Console::convertControllerTypeToDev(StorageControllerType_T enmCtrlType)
{
    switch (enmCtrlType)
    {
        case StorageControllerType_LsiLogic:
            return "lsilogicscsi";
        case StorageControllerType_BusLogic:
            return "buslogic";
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
        {
            uLun = port;
            return S_OK;
        }
        default:
            uLun = 0;
            AssertMsgFailedReturn(("%d\n", enmBus), E_INVALIDARG);
    }
}

/**
 * Process a medium change.
 *
 * @param aMediumAttachment The medium attachment with the new medium state.
 * @param fForce            Force medium chance, if it is locked or not.
 *
 * @note Locks this object for writing.
 */
HRESULT Console::doMediumChange(IMediumAttachment *aMediumAttachment, bool fForce)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    /* We will need to release the write lock before calling EMT */
    AutoWriteLock alock(this);

    HRESULT rc = S_OK;
    const char *pszDevice = NULL;
    unsigned uInstance = 0;
    unsigned uLun = 0;
    BOOL fHostDrive = FALSE;
    Utf8Str location;
    Utf8Str format;
    BOOL fPassthrough = FALSE;

    SafeIfaceArray<IStorageController> ctrls;
    rc = mMachine->COMGETTER(StorageControllers)(ComSafeArrayAsOutParam(ctrls));
    AssertComRC(rc);
    Bstr attCtrlName;
    rc = aMediumAttachment->COMGETTER(Controller)(attCtrlName.asOutParam());
    AssertComRC(rc);
    ComPtr<IStorageController> ctrl;
    for (size_t i = 0; i < ctrls.size(); ++i)
    {
        Bstr ctrlName;
        rc = ctrls[i]->COMGETTER(Name)(ctrlName.asOutParam());
        AssertComRC(rc);
        if (attCtrlName == ctrlName)
        {
            ctrl = ctrls[i];
            break;
        }
    }
    if (ctrl.isNull())
    {
        return setError(E_FAIL,
                        tr("Could not find storage controller '%ls'"), attCtrlName.raw());
    }
    StorageControllerType_T enmCtrlType;
    rc = ctrl->COMGETTER(ControllerType)(&enmCtrlType);
    AssertComRC(rc);
    pszDevice = convertControllerTypeToDev(enmCtrlType);

    /** @todo support multiple instances of a controller */
    uInstance = 0;

    LONG device;
    rc = aMediumAttachment->COMGETTER(Device)(&device);
    AssertComRC(rc);
    LONG port;
    rc = aMediumAttachment->COMGETTER(Port)(&port);
    AssertComRC(rc);
    StorageBus_T enmBus;
    rc = ctrl->COMGETTER(Bus)(&enmBus);
    AssertComRC(rc);
    rc = convertBusPortDeviceToLun(enmBus, port, device, uLun);
    AssertComRCReturnRC(rc);

    ComPtr<IMedium> medium;
    rc = aMediumAttachment->COMGETTER(Medium)(medium.asOutParam());
    if (SUCCEEDED(rc) && !medium.isNull())
    {
        Bstr loc;
        rc = medium->COMGETTER(Location)(loc.asOutParam());
        AssertComRC(rc);
        location = loc;
        Bstr fmt;
        rc = medium->COMGETTER(Format)(fmt.asOutParam());
        AssertComRC(rc);
        format = fmt;
        rc = medium->COMGETTER(HostDrive)(&fHostDrive);
        AssertComRC(rc);
    }
    rc = aMediumAttachment->COMGETTER(Passthrough)(&fPassthrough);
    AssertComRC(rc);

    /* protect mpVM */
    AutoVMCaller autoVMCaller(this);
    AssertComRCReturnRC(autoVMCaller.rc());

    /*
     * Call worker in EMT, that's faster and safer than doing everything
     * using VMR3ReqCall. Note that we separate VMR3ReqCall from VMR3ReqWait
     * here to make requests from under the lock in order to serialize them.
     */
    PVMREQ pReq;
    int vrc = VMR3ReqCall(mpVM, VMCPUID_ANY, &pReq, 0 /* no wait! */, VMREQFLAGS_VBOX_STATUS,
                          (PFNRT)Console::changeDrive, 9,
                          this, pszDevice, uInstance, uLun, !!fHostDrive, location.raw(), format.raw(), !!fPassthrough, fForce);

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

    if (!location.isEmpty())
        return setError(E_FAIL,
            tr("Could not mount the media/drive '%s' (%Rrc)"),
            location.raw(), vrc);

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
 * @param   pszDevice       The PDM device name.
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
 * @note Locks the Console object for writing.
 * @todo the error handling in this method needs to be improved seriously - what if mounting fails...
 */
DECLCALLBACK(int) Console::changeDrive(Console *pThis, const char *pszDevice, unsigned uInstance, unsigned uLun,
                                       bool fHostDrive, const char *pszPath, const char *pszFormat, bool fPassthrough, bool fForce)
{
/// @todo change this to use the same code as in ConsoleImpl2.cpp
    LogFlowFunc(("pThis=%p pszDevice=%p:{%s} uInstance=%u uLun=%u fHostDrive=%d pszPath=%p:{%s} pszFormat=%p:{%s} fPassthrough=%d fForce=%d\n",
                 pThis, pszDevice, pszDevice, uInstance, uLun, fHostDrive, pszPath, pszPath, pszFormat, pszFormat, fPassthrough, fForce));

    AssertReturn(pThis, VERR_INVALID_PARAMETER);

    AutoCaller autoCaller(pThis);
    AssertComRCReturn(autoCaller.rc(), VERR_ACCESS_DENIED);

    /* protect mpVM */
    AutoVMCaller autoVMCaller(pThis);
    CheckComRCReturnRC(autoVMCaller.rc());

    PVM pVM = pThis->mpVM;

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

        case VMSTATE_RUNNING_LS:
            return setError(VBOX_E_INVALID_VM_STATE, tr("Cannot change drive during live migration"));

        default:
            AssertMsgFailedReturn(("enmVMState=%d\n", enmVMState), VERR_ACCESS_DENIED);
    }

    int rc = VINF_SUCCESS;
    int rcRet = VINF_SUCCESS;

    /*
       In general locking the object before doing VMR3* calls is quite safe
       here, since we're on EMT. Anyway we lock for write after eventually
       suspending the vm. The reason is that in the vmstateChangeCallback the
       var mVMStateChangeCallbackDisabled is checked under a lock also, which
       can lead to an dead lock. The write lock is necessary because we
       indirectly modify the meDVDState/meFloppyState members (pointed to by
       peState).
     */
    AutoWriteLock alock(pThis);

    do
    {
        /*
         * Unmount existing media / detach host drive.
         */
        PPDMIBASE pBase;
        rc = PDMR3QueryLun(pVM, pszDevice, uInstance, uLun, &pBase);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_PDM_LUN_NOT_FOUND || rc == VERR_PDM_NO_DRIVER_ATTACHED_TO_LUN)
                rc = VINF_SUCCESS;
            AssertRC(rc);
        }
        else
        {
            PPDMIMOUNT pIMount = NULL;
            pIMount = (PPDMIMOUNT) pBase->pfnQueryInterface(pBase, PDMINTERFACE_MOUNT);
            AssertBreakStmt(pIMount, rc = VERR_INVALID_POINTER);

            /*
             * Unmount the media.
             */
            rc = pIMount->pfnUnmount(pIMount, fForce);
            if (rc == VERR_PDM_MEDIA_NOT_MOUNTED)
                rc = VINF_SUCCESS;

            if (RT_SUCCESS(rc))
            {
                rc = PDMR3DeviceDetach(pVM, pszDevice, uInstance, uLun, PDM_TACH_FLAGS_NOT_HOT_PLUG);
                if (rc == VERR_PDM_NO_DRIVER_ATTACHED_TO_LUN)
                    rc = VINF_SUCCESS;
            }
        }

        if (RT_FAILURE(rc))
        {
            rcRet = rc;
            break;
        }

        /** @todo this does a very thorough job. usually it's too much,
         * as a simple medium change (without changing between host attachment
         * and image) could be done with a lot less effort, by just using the
         * pfnUnmount and pfnMount interfaces. Later. */

        /*
         * Construct a new driver configuration.
         */
        PCFGMNODE pInst = CFGMR3GetChildF(CFGMR3GetRoot(pVM), "Devices/%s/%d/", pszDevice, uInstance);
        AssertRelease(pInst);
        /* nuke anything which might have been left behind. */
        CFGMR3RemoveNode(CFGMR3GetChildF(pInst, "LUN#%d", uLun));

#define RC_CHECK() do { if (RT_FAILURE(rc)) { AssertReleaseRC(rc); break; } } while (0)

        PCFGMNODE pLunL0;
        PCFGMNODE pCfg;
        rc = CFGMR3InsertNodeF(pInst, &pLunL0, "LUN#%d", uLun);     RC_CHECK();

        if (fHostDrive)
        {
            rc = CFGMR3InsertString(pLunL0, "Driver",       !strcmp(pszDevice, "i82078") ? "HostFloppy" : "HostDVD");   RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0,   "Config",       &pCfg);     RC_CHECK();
            Assert(pszPath && *pszPath);
            rc = CFGMR3InsertString(pCfg,   "Path",         pszPath);   RC_CHECK();
            if (strcmp(pszDevice, "i82078"))
            {
                rc = CFGMR3InsertInteger(pCfg, "Passthrough", fPassthrough); RC_CHECK();
            }
        }
        else
        {
            /* create a new block driver config */
            rc = CFGMR3InsertString(pLunL0, "Driver",       "Block");   RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0,   "Config",       &pCfg);     RC_CHECK();
            rc = CFGMR3InsertString(pCfg,   "Type",         !strcmp(pszDevice, "i82078") ? "Floppy 1.44" : "DVD"); RC_CHECK();
            rc = CFGMR3InsertInteger(pCfg,  "Mountable",    1);         RC_CHECK();
        }

        /*
         * Attach the driver.
         */
        rc = PDMR3DeviceAttach(pVM, pszDevice, uInstance, uLun, PDM_TACH_FLAGS_NOT_HOT_PLUG, &pBase); RC_CHECK();

        if (!fHostDrive && pszPath && *pszPath)
        {
            PCFGMNODE pLunL1;
            rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1); RC_CHECK();
            rc = CFGMR3InsertString(pLunL1, "Driver",       "VD");      RC_CHECK();
            rc = CFGMR3InsertNode(pLunL1,   "Config",       &pCfg);     RC_CHECK();
            rc = CFGMR3InsertString(pCfg,   "Path",         pszPath);   RC_CHECK();
            rc = CFGMR3InsertString(pCfg,   "Format",       pszFormat); RC_CHECK();
            if (strcmp(pszDevice, "i82078"))
            {
                rc = CFGMR3InsertInteger(pCfg, "ReadOnly",  1);         RC_CHECK();
            }
            /** @todo later pass full VDConfig information and parent images */
        }

        /* Dump the new controller configuration. */
        CFGMR3Dump(pInst);

        if (!fHostDrive && pszPath && *pszPath)
        {
            PPDMIMOUNT pIMount = NULL;
            pIMount = (PPDMIMOUNT) pBase->pfnQueryInterface(pBase, PDMINTERFACE_MOUNT);
            if (!pIMount)
            {
                AssertFailed();
                return rc;
            }

            rc = pIMount->pfnMount(pIMount, NULL , NULL);
        }

#undef RC_CHECK

        if (RT_FAILURE(rc) && RT_SUCCESS(rcRet))
            rcRet = rc;

    }
    while (0);

    /*
       Unlock before resuming because the vmstateChangeCallback problem
       described above.
     */
    alock.unlock();

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
 * Called by IInternalSessionControl::OnNetworkAdapterChange().
 *
 * @note Locks this object for writing.
 */
HRESULT Console::onNetworkAdapterChange(INetworkAdapter *aNetworkAdapter, BOOL changeAdapter)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    /* Don't do anything if the VM isn't running */
    if (!mpVM)
        return S_OK;

    /* protect mpVM */
    AutoVMCaller autoVMCaller(this);
    CheckComRCReturnRC(autoVMCaller.rc());

    /* Get the properties we need from the adapter */
    BOOL fCableConnected, fTraceEnabled;
    HRESULT rc = aNetworkAdapter->COMGETTER(CableConnected)(&fCableConnected);
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
             * Find the pcnet instance, get the config interface and update
             * the link state.
             */
            NetworkAdapterType_T adapterType;
            rc = aNetworkAdapter->COMGETTER(AdapterType)(&adapterType);
            AssertComRC(rc);
            const char *pszAdapterName = NULL;
            switch (adapterType)
            {
                case NetworkAdapterType_Am79C970A:
                case NetworkAdapterType_Am79C973:
                    pszAdapterName = "pcnet";
                    break;
#ifdef VBOX_WITH_E1000
                case NetworkAdapterType_I82540EM:
                case NetworkAdapterType_I82543GC:
                case NetworkAdapterType_I82545EM:
                    pszAdapterName = "e1000";
                    break;
#endif
#ifdef VBOX_WITH_VIRTIO
                case NetworkAdapterType_Virtio:
                    pszAdapterName = "virtio-net";
                    break;
#endif
                default:
                    AssertFailed();
                    pszAdapterName = "unknown";
                    break;
            }

            PPDMIBASE pBase;
            int vrc = PDMR3QueryDeviceLun(mpVM, pszAdapterName, ulInstance, 0, &pBase);
            ComAssertRC(vrc);
            if (RT_SUCCESS(vrc))
            {
                Assert(pBase);
                PPDMINETWORKCONFIG pINetCfg;
                pINetCfg = (PPDMINETWORKCONFIG)pBase->pfnQueryInterface(pBase, PDMINTERFACE_NETWORK_CONFIG);
                if (pINetCfg)
                {
                    Log(("Console::onNetworkAdapterChange: setting link state to %d\n",
                          fCableConnected));
                    vrc = pINetCfg->pfnSetLinkState(pINetCfg,
                                                    fCableConnected ? PDMNETWORKLINKSTATE_UP
                                                                    : PDMNETWORKLINKSTATE_DOWN);
                    ComAssertRC(vrc);
                }
#ifdef VBOX_DYNAMIC_NET_ATTACH
                if (RT_SUCCESS(vrc) && changeAdapter)
                {
                    VMSTATE enmVMState = VMR3GetState(mpVM);
                    if (    enmVMState == VMSTATE_RUNNING    /** @todo LiveMigration: Forbit or deal correctly with the _LS variants */
                        ||  enmVMState == VMSTATE_SUSPENDED)
                    {
                        if (fTraceEnabled && fCableConnected && pINetCfg)
                        {
                            vrc = pINetCfg->pfnSetLinkState(pINetCfg, PDMNETWORKLINKSTATE_DOWN);
                            ComAssertRC(vrc);
                        }

                        rc = doNetworkAdapterChange(pszAdapterName, ulInstance, 0, aNetworkAdapter);

                        if (fTraceEnabled && fCableConnected && pINetCfg)
                        {
                            vrc = pINetCfg->pfnSetLinkState(pINetCfg, PDMNETWORKLINKSTATE_UP);
                            ComAssertRC(vrc);
                        }
                    }
                }
#endif /* VBOX_DYNAMIC_NET_ATTACH */
            }

            if (RT_FAILURE(vrc))
                rc = E_FAIL;
        }
    }

    /* notify console callbacks on success */
    if (SUCCEEDED(rc))
    {
        CallbackList::iterator it = mCallbacks.begin();
        while (it != mCallbacks.end())
            (*it++)->OnNetworkAdapterChange(aNetworkAdapter);
    }

    LogFlowThisFunc(("Leaving rc=%#x\n", rc));
    return rc;
}


#ifdef VBOX_DYNAMIC_NET_ATTACH
/**
 * Process a network adaptor change.
 *
 * @returns COM status code.
 *
 * @param   pszDevice           The PDM device name.
 * @param   uInstance           The PDM device instance.
 * @param   uLun                The PDM LUN number of the drive.
 * @param   aNetworkAdapter     The network adapter whose attachment needs to be changed
 *
 * @note Locks this object for writing.
 */
HRESULT Console::doNetworkAdapterChange(const char *pszDevice,
                                        unsigned uInstance,
                                        unsigned uLun,
                                        INetworkAdapter *aNetworkAdapter)
{
    LogFlowThisFunc(("pszDevice=%p:{%s} uInstance=%u uLun=%u aNetworkAdapter=%p\n",
                      pszDevice, pszDevice, uInstance, uLun, aNetworkAdapter));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    /* We will need to release the write lock before calling EMT */
    AutoWriteLock alock(this);

    /* protect mpVM */
    AutoVMCaller autoVMCaller(this);
    CheckComRCReturnRC(autoVMCaller.rc());

    /*
     * Call worker in EMT, that's faster and safer than doing everything
     * using VM3ReqCall. Note that we separate VMR3ReqCall from VMR3ReqWait
     * here to make requests from under the lock in order to serialize them.
     */
    PVMREQ pReq;
    int vrc = VMR3ReqCall(mpVM, 0 /*idDstCpu*/, &pReq, 0 /* no wait! */, VMREQFLAGS_VBOX_STATUS,
                          (PFNRT) Console::changeNetworkAttachment, 5,
                          this, pszDevice, uInstance, uLun, aNetworkAdapter);

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
 * @param   pszDevice           The PDM device name.
 * @param   uInstance           The PDM device instance.
 * @param   uLun                The PDM LUN number of the drive.
 * @param   aNetworkAdapter     The network adapter whose attachment needs to be changed
 *
 * @thread  EMT
 * @note Locks the Console object for writing.
 */
DECLCALLBACK(int) Console::changeNetworkAttachment(Console *pThis,
                                                   const char *pszDevice,
                                                   unsigned uInstance,
                                                   unsigned uLun,
                                                   INetworkAdapter *aNetworkAdapter)
{
    LogFlowFunc(("pThis=%p pszDevice=%p:{%s} uInstance=%u uLun=%u aNetworkAdapter=%p\n",
                 pThis, pszDevice, pszDevice, uInstance, uLun, aNetworkAdapter));

    AssertReturn(pThis, VERR_INVALID_PARAMETER);

    AssertMsg(   (   !strcmp(pszDevice, "pcnet")
                  || !strcmp(pszDevice, "e1000")
                  || !strcmp(pszDevice, "virtio-net"))
              && (uLun == 0)
              && (uInstance < SchemaDefs::NetworkAdapterCount),
              ("pszDevice=%s uLun=%d uInstance=%d\n", pszDevice, uLun, uInstance));
    Log(("pszDevice=%s uLun=%d uInstance=%d\n", pszDevice, uLun, uInstance));

    AutoCaller autoCaller(pThis);
    AssertComRCReturn(autoCaller.rc(), VERR_ACCESS_DENIED);

    /* protect mpVM */
    AutoVMCaller autoVMCaller(pThis);
    CheckComRCReturnRC(autoVMCaller.rc());

    PVM pVM = pThis->mpVM;

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

    rcRet = configNetwork(pThis, pszDevice, uInstance, uLun, aNetworkAdapter, pCfg, pLunL0, pInst, true);

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
#endif /* VBOX_DYNAMIC_NET_ATTACH */


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

    AutoWriteLock alock(this);

    /* Don't do anything if the VM isn't running */
    if (!mpVM)
        return S_OK;

    HRESULT rc = S_OK;

    /* protect mpVM */
    AutoVMCaller autoVMCaller(this);
    CheckComRCReturnRC(autoVMCaller.rc());

    /* nothing to do so far */

    /* notify console callbacks on success */
    if (SUCCEEDED(rc))
    {
        CallbackList::iterator it = mCallbacks.begin();
        while (it != mCallbacks.end())
            (*it++)->OnSerialPortChange(aSerialPort);
    }

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

    AutoWriteLock alock(this);

    /* Don't do anything if the VM isn't running */
    if (!mpVM)
        return S_OK;

    HRESULT rc = S_OK;

    /* protect mpVM */
    AutoVMCaller autoVMCaller(this);
    CheckComRCReturnRC(autoVMCaller.rc());

    /* nothing to do so far */

    /* notify console callbacks on success */
    if (SUCCEEDED(rc))
    {
        CallbackList::iterator it = mCallbacks.begin();
        while (it != mCallbacks.end())
            (*it++)->OnParallelPortChange(aParallelPort);
    }

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

    AutoWriteLock alock(this);

    /* Don't do anything if the VM isn't running */
    if (!mpVM)
        return S_OK;

    HRESULT rc = S_OK;

    /* protect mpVM */
    AutoVMCaller autoVMCaller(this);
    CheckComRCReturnRC(autoVMCaller.rc());

    /* nothing to do so far */

    /* notify console callbacks on success */
    if (SUCCEEDED(rc))
    {
        CallbackList::iterator it = mCallbacks.begin();
        while (it != mCallbacks.end())
            (*it++)->OnStorageControllerChange();
    }

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

    AutoWriteLock alock(this);

    /* Don't do anything if the VM isn't running */
    if (!mpVM)
        return S_OK;

    HRESULT rc = S_OK;

    /* protect mpVM */
    AutoVMCaller autoVMCaller(this);
    CheckComRCReturnRC(autoVMCaller.rc());

    rc = doMediumChange(aMediumAttachment, !!aForce);

    /* notify console callbacks on success */
    if (SUCCEEDED(rc))
    {
        CallbackList::iterator it = mCallbacks.begin();
        while (it != mCallbacks.end())
            (*it++)->OnMediumChange(aMediumAttachment);
    }

    LogFlowThisFunc(("Leaving rc=%#x\n", rc));
    return rc;
}

/**
 * Called by IInternalSessionControl::OnVRDPServerChange().
 *
 * @note Locks this object for writing.
 */
HRESULT Console::onVRDPServerChange()
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = S_OK;

    if (    mVRDPServer
        &&  (   mMachineState == MachineState_Running
             || mMachineState == MachineState_Teleporting
             || mMachineState == MachineState_LiveSnapshotting
            )
       )
    {
        BOOL vrdpEnabled = FALSE;

        rc = mVRDPServer->COMGETTER(Enabled)(&vrdpEnabled);
        ComAssertComRCRetRC(rc);

        /* VRDP server may call this Console object back from other threads (VRDP INPUT or OUTPUT). */
        alock.leave();

        if (vrdpEnabled)
        {
            // If there was no VRDP server started the 'stop' will do nothing.
            // However if a server was started and this notification was called,
            // we have to restart the server.
            mConsoleVRDPServer->Stop();

            if (RT_FAILURE(mConsoleVRDPServer->Launch()))
            {
                rc = E_FAIL;
            }
            else
            {
                mConsoleVRDPServer->EnableConnections();
            }
        }
        else
        {
            mConsoleVRDPServer->Stop();
        }

        alock.enter();
    }

    /* notify console callbacks on success */
    if (SUCCEEDED(rc))
    {
        CallbackList::iterator it = mCallbacks.begin();
        while (it != mCallbacks.end())
            (*it++)->OnVRDPServerChange();
    }

    return rc;
}

/**
 * @note Locks this object for reading.
 */
void Console::onRemoteDisplayInfoChange()
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoReadLock alock(this);

    CallbackList::iterator it = mCallbacks.begin();
    while (it != mCallbacks.end())
        (*it++)->OnRemoteDisplayInfoChange();
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

    AutoWriteLock alock(this);

    /* Ignore if no VM is running yet. */
    if (!mpVM)
        return S_OK;

    HRESULT rc = S_OK;

/// @todo (dmik)
// check for the Enabled state and disable virtual USB controller??
// Anyway, if we want to query the machine's USB Controller we need to cache
// it to mUSBController in #init() (as it is done with mDVDDrive).
//
// bird: While the VM supports hot-plugging, I doubt any guest can handle it at this time... :-)
//
//    /* protect mpVM */
//    AutoVMCaller autoVMCaller(this);
//    CheckComRCReturnRC(autoVMCaller.rc());

    /* notify console callbacks on success */
    if (SUCCEEDED(rc))
    {
        CallbackList::iterator it = mCallbacks.begin();
        while (it != mCallbacks.end())
            (*it++)->OnUSBControllerChange();
    }

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

    AutoWriteLock alock(this);

    HRESULT rc = fetchSharedFolders(aGlobal);

    /* notify console callbacks on success */
    if (SUCCEEDED(rc))
    {
        CallbackList::iterator it = mCallbacks.begin();
        while (it != mCallbacks.end())
            (*it++)->OnSharedFolderChange(aGlobal ? (Scope_T)Scope_Global
                                                  : (Scope_T)Scope_Machine);
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

    AutoWriteLock alock(this);

    /* protect mpVM (we don't need error info, since it's a callback) */
    AutoVMCallerQuiet autoVMCaller(this);
    if (FAILED(autoVMCaller.rc()))
    {
        /* The VM may be no more operational when this message arrives
         * (e.g. it may be Saving or Stopping or just PoweredOff) --
         * autoVMCaller.rc() will return a failure in this case. */
        LogFlowThisFunc(("Attach request ignored (mMachineState=%d).\n",
                          mMachineState));
        return autoVMCaller.rc();
    }

    if (aError != NULL)
    {
        /* notify callbacks about the error */
        onUSBDeviceStateChange(aDevice, true /* aAttached */, aError);
        return S_OK;
    }

    /* Don't proceed unless there's at least one USB hub. */
    if (!PDMR3USBHasHub(mpVM))
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
        ComPtr<IVirtualBoxErrorInfo> error = eik.takeError();
        Assert(!error.isNull());
        if (!error.isNull())
        {
            /* notify callbacks about the error */
            onUSBDeviceStateChange(aDevice, true /* aAttached */, error);
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

    AutoWriteLock alock(this);

    /* Find the device. */
    ComObjPtr<OUSBDevice> device;
    USBDeviceList::iterator it = mUSBDevices.begin();
    while (it != mUSBDevices.end())
    {
        LogFlowThisFunc(("it={%RTuuid}\n", (*it)->id().raw()));
        if ((*it)->id() == Uuid)
        {
            device = *it;
            break;
        }
        ++ it;
    }


    if (device.isNull())
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
        onUSBDeviceStateChange(device, false /* aAttached */, aError);
        return S_OK;
    }

    HRESULT rc = detachUSBDevice(it);

    if (FAILED(rc))
    {
        /* take the current error info */
        com::ErrorInfoKeeper eik;
        /* the error must be a VirtualBoxErrorInfo instance */
        ComPtr<IVirtualBoxErrorInfo> error = eik.takeError();
        Assert(!error.isNull());
        if (!error.isNull())
        {
            /* notify callbacks about the error */
            onUSBDeviceStateChange(device, false /* aAttached */, error);
        }
    }

    return rc;

#else   /* !VBOX_WITH_USB */
    return E_FAIL;
#endif  /* !VBOX_WITH_USB */
}

/**
 * @note Temporarily locks this object for writing.
 */
HRESULT Console::getGuestProperty(IN_BSTR aName, BSTR *aValue,
                                  ULONG64 *aTimestamp, BSTR *aFlags)
{
#ifndef VBOX_WITH_GUEST_PROPS
    ReturnComNotImplemented();
#else
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
    CheckComRCReturnRC(autoVMCaller.rc());

    /* Note: validity of mVMMDev which is bound to uninit() is guaranteed by
     * autoVMCaller, so there is no need to hold a lock of this */

    HRESULT rc = E_UNEXPECTED;
    using namespace guestProp;

    try
    {
        VBOXHGCMSVCPARM parm[4];
        Utf8Str Utf8Name = aName;
        char pszBuffer[MAX_VALUE_LEN + MAX_FLAGS_LEN];

        parm[0].type = VBOX_HGCM_SVC_PARM_PTR;
        parm[0].u.pointer.addr = (void*)Utf8Name.c_str();
        /* The + 1 is the null terminator */
        parm[0].u.pointer.size = (uint32_t)Utf8Name.length() + 1;
        parm[1].type = VBOX_HGCM_SVC_PARM_PTR;
        parm[1].u.pointer.addr = pszBuffer;
        parm[1].u.pointer.size = sizeof(pszBuffer);
        int vrc = mVMMDev->hgcmHostCall("VBoxGuestPropSvc", GET_PROP_HOST,
                                        4, &parm[0]);
        /* The returned string should never be able to be greater than our buffer */
        AssertLogRel(vrc != VERR_BUFFER_OVERFLOW);
        AssertLogRel(RT_FAILURE(vrc) || VBOX_HGCM_SVC_PARM_64BIT == parm[2].type);
        if (RT_SUCCESS(vrc) || (VERR_NOT_FOUND == vrc))
        {
            rc = S_OK;
            if (vrc != VERR_NOT_FOUND)
            {
                Utf8Str strBuffer(pszBuffer);
                strBuffer.cloneTo(aValue);

                *aTimestamp = parm[2].u.uint64;

                size_t iFlags = strBuffer.length() + 1;
                Utf8Str(pszBuffer + iFlags).cloneTo(aFlags);
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
#else
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
    CheckComRCReturnRC(autoVMCaller.rc());

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
        vrc = mVMMDev->hgcmHostCall("VBoxGuestPropSvc", SET_PROP_HOST,
                                    3, &parm[0]);
    else if (aValue != NULL)
        vrc = mVMMDev->hgcmHostCall("VBoxGuestPropSvc", SET_PROP_VALUE_HOST,
                                    2, &parm[0]);
    else
        vrc = mVMMDev->hgcmHostCall("VBoxGuestPropSvc", DEL_PROP_HOST,
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
                                          ComSafeArrayOut(ULONG64, aTimestamps),
                                          ComSafeArrayOut(BSTR, aFlags))
{
#ifndef VBOX_WITH_GUEST_PROPS
    ReturnComNotImplemented();
#else
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
    CheckComRCReturnRC(autoVMCaller.rc());

    /* Note: validity of mVMMDev which is bound to uninit() is guaranteed by
     * autoVMCaller, so there is no need to hold a lock of this */

    return doEnumerateGuestProperties(aPatterns, ComSafeArrayOutArg(aNames),
                                      ComSafeArrayOutArg(aValues),
                                      ComSafeArrayOutArg(aTimestamps),
                                      ComSafeArrayOutArg(aFlags));
#endif /* VBOX_WITH_GUEST_PROPS */
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

    AutoWriteLock alock(this);

    AssertReturn(   mMachineState == MachineState_Saving
                 || mMachineState == MachineState_LiveSnapshotting
                 || mMachineState == MachineState_RestoringSnapshot
                 || mMachineState == MachineState_DeletingSnapshot
                 , E_FAIL);

    return setMachineStateLocally(aMachineState);
}

/**
 * @note Locks this object for writing.
 */
void Console::onMousePointerShapeChange(bool fVisible, bool fAlpha,
                                        uint32_t xHot, uint32_t yHot,
                                        uint32_t width, uint32_t height,
                                        void *pShape)
{
#if 0
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("fVisible=%d, fAlpha=%d, xHot = %d, yHot = %d, width=%d, height=%d, shape=%p\n",
                      fVisible, fAlpha, xHot, yHot, width, height, pShape));
#endif

    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* We need a write lock because we alter the cached callback data */
    AutoWriteLock alock(this);

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

    if (pShape != NULL)
    {
        size_t cb = (width + 7) / 8 * height; /* size of the AND mask */
        cb = ((cb + 3) & ~3) + width * 4 * height; /* + gap + size of the XOR mask */
        /* try to reuse the old shape buffer if the size is the same */
        if (!wasValid)
            mCallbackData.mpsc.shape = NULL;
        else
        if (mCallbackData.mpsc.shape != NULL && mCallbackData.mpsc.shapeSize != cb)
        {
            RTMemFree(mCallbackData.mpsc.shape);
            mCallbackData.mpsc.shape = NULL;
        }
        if (mCallbackData.mpsc.shape == NULL)
        {
            mCallbackData.mpsc.shape = (BYTE *) RTMemAllocZ(cb);
            AssertReturnVoid(mCallbackData.mpsc.shape);
        }
        mCallbackData.mpsc.shapeSize = cb;
        memcpy(mCallbackData.mpsc.shape, pShape, cb);
    }
    else
    {
        if (wasValid && mCallbackData.mpsc.shape != NULL)
            RTMemFree(mCallbackData.mpsc.shape);
        mCallbackData.mpsc.shape = NULL;
        mCallbackData.mpsc.shapeSize = 0;
    }

    mCallbackData.mpsc.valid = true;

    CallbackList::iterator it = mCallbacks.begin();
    while (it != mCallbacks.end())
        (*it++)->OnMousePointerShapeChange(fVisible, fAlpha, xHot, yHot,
                                           width, height, (BYTE *) pShape);

#if 0
    LogFlowThisFuncLeave();
#endif
}

/**
 * @note Locks this object for writing.
 */
void Console::onMouseCapabilityChange(BOOL supportsAbsolute, BOOL needsHostCursor)
{
    LogFlowThisFunc(("supportsAbsolute=%d needsHostCursor=%d\n",
                      supportsAbsolute, needsHostCursor));

    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* We need a write lock because we alter the cached callback data */
    AutoWriteLock alock(this);

    /* save the callback arguments */
    mCallbackData.mcc.supportsAbsolute = supportsAbsolute;
    mCallbackData.mcc.needsHostCursor = needsHostCursor;
    mCallbackData.mcc.valid = true;

    CallbackList::iterator it = mCallbacks.begin();
    while (it != mCallbacks.end())
    {
        Log2(("Console::onMouseCapabilityChange: calling %p\n", (void*)*it));
        (*it++)->OnMouseCapabilityChange(supportsAbsolute, needsHostCursor);
    }
}

/**
 * @note Locks this object for reading.
 */
void Console::onStateChange(MachineState_T machineState)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoReadLock alock(this);

    CallbackList::iterator it = mCallbacks.begin();
    while (it != mCallbacks.end())
        (*it++)->OnStateChange(machineState);
}

/**
 * @note Locks this object for reading.
 */
void Console::onAdditionsStateChange()
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoReadLock alock(this);

    CallbackList::iterator it = mCallbacks.begin();
    while (it != mCallbacks.end())
        (*it++)->OnAdditionsStateChange();
}

/**
 * @note Locks this object for reading.
 */
void Console::onAdditionsOutdated()
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoReadLock alock(this);

    /** @todo Use the On-Screen Display feature to report the fact.
     * The user should be told to install additions that are
     * provided with the current VBox build:
     * VBOX_VERSION_MAJOR.VBOX_VERSION_MINOR.VBOX_VERSION_BUILD
     */
}

/**
 * @note Locks this object for writing.
 */
void Console::onKeyboardLedsChange(bool fNumLock, bool fCapsLock, bool fScrollLock)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* We need a write lock because we alter the cached callback data */
    AutoWriteLock alock(this);

    /* save the callback arguments */
    mCallbackData.klc.numLock = fNumLock;
    mCallbackData.klc.capsLock = fCapsLock;
    mCallbackData.klc.scrollLock = fScrollLock;
    mCallbackData.klc.valid = true;

    CallbackList::iterator it = mCallbacks.begin();
    while (it != mCallbacks.end())
        (*it++)->OnKeyboardLedsChange(fNumLock, fCapsLock, fScrollLock);
}

/**
 * @note Locks this object for reading.
 */
void Console::onUSBDeviceStateChange(IUSBDevice *aDevice, bool aAttached,
                                     IVirtualBoxErrorInfo *aError)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoReadLock alock(this);

    CallbackList::iterator it = mCallbacks.begin();
    while (it != mCallbacks.end())
        (*it++)->OnUSBDeviceStateChange(aDevice, aAttached, aError);
}

/**
 * @note Locks this object for reading.
 */
void Console::onRuntimeError(BOOL aFatal, IN_BSTR aErrorID, IN_BSTR aMessage)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoReadLock alock(this);

    CallbackList::iterator it = mCallbacks.begin();
    while (it != mCallbacks.end())
        (*it++)->OnRuntimeError(aFatal, aErrorID, aMessage);
}

/**
 * @note Locks this object for reading.
 */
HRESULT Console::onShowWindow(BOOL aCheck, BOOL *aCanShow, ULONG64 *aWinId)
{
    AssertReturn(aCanShow, E_POINTER);
    AssertReturn(aWinId, E_POINTER);

    *aCanShow = FALSE;
    *aWinId = 0;

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    HRESULT rc = S_OK;
    CallbackList::iterator it = mCallbacks.begin();

    if (aCheck)
    {
        while (it != mCallbacks.end())
        {
            BOOL canShow = FALSE;
            rc = (*it++)->OnCanShowWindow(&canShow);
            AssertComRC(rc);
            if (FAILED(rc) || !canShow)
                return rc;
        }
        *aCanShow = TRUE;
    }
    else
    {
        while (it != mCallbacks.end())
        {
            ULONG64 winId = 0;
            rc = (*it++)->OnShowWindow(&winId);
            AssertComRC(rc);
            if (FAILED(rc))
                return rc;
            /* only one callback may return non-null winId */
            Assert(*aWinId == 0 || winId == 0);
            if (*aWinId == 0)
                *aWinId = winId;
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

    AutoWriteLock alock(this);

    if (mVMDestroying)
    {
        /* powerDown() is waiting for all callers to finish */
        return aQuiet ? E_ACCESSDENIED : setError(E_ACCESSDENIED,
            tr("Virtual machine is being powered down"));
    }

    if (mpVM == NULL)
    {
        Assert(aAllowNullVM == true);

        /* The machine is not powered up */
        return aQuiet ? E_ACCESSDENIED : setError(E_ACCESSDENIED,
            tr("Virtual machine is not powered up"));
    }

    ++ mVMCallers;

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

    AutoWriteLock alock(this);

    AssertReturnVoid(mpVM != NULL);

    Assert(mVMCallers > 0);
    --mVMCallers;

    if (mVMCallers == 0 && mVMDestroying)
    {
        /* inform powerDown() there are no more callers */
        RTSemEventSignal(mVMZeroCallersSem);
    }
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
    CheckComRCReturnRC(hrc);

    Utf8Str logDir = logFolder;

    /* make sure the Logs folder exists */
    Assert(logDir.length());
    if (!RTDirExists(logDir.c_str()))
        RTDirCreateFullPath(logDir.c_str(), 0777);

    Utf8Str logFile = Utf8StrFmt("%s%cVBox.log",
                                 logDir.raw(), RTPATH_DELIMITER);
    Utf8Str pngFile = Utf8StrFmt("%s%cVBox.png",
                                 logDir.raw(), RTPATH_DELIMITER);

    /*
     * Age the old log files
     * Rename .(n-1) to .(n), .(n-2) to .(n-1), ..., and the last log file to .1
     * Overwrite target files in case they exist.
     */
    ComPtr<IVirtualBox> virtualBox;
    aMachine->COMGETTER(Parent)(virtualBox.asOutParam());
    ComPtr<ISystemProperties> systemProperties;
    virtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());
    ULONG uLogHistoryCount = 3;
    systemProperties->COMGETTER(LogHistoryCount)(&uLogHistoryCount);
    ComPtr<IHost> host;
    virtualBox->COMGETTER(Host)(host.asOutParam());
    ULONG uHostRamMb = 0, uHostRamAvailMb = 0;
    host->COMGETTER(MemorySize)(&uHostRamMb);
    host->COMGETTER(MemoryAvailable)(&uHostRamAvailMb);
    if (uLogHistoryCount)
    {
        for (int i = uLogHistoryCount-1; i >= 0; i--)
        {
            Utf8Str *files[] = { &logFile, &pngFile };
            Utf8Str oldName, newName;

            for (unsigned int j = 0; j < RT_ELEMENTS(files); ++ j)
            {
                if (i > 0)
                    oldName = Utf8StrFmt("%s.%d", files[j]->raw(), i);
                else
                    oldName = *files[j];
                newName = Utf8StrFmt("%s.%d", files[j]->raw(), i + 1);
                /* If the old file doesn't exist, delete the new file (if it
                 * exists) to provide correct rotation even if the sequence is
                 * broken */
                if (   RTFileRename(oldName.c_str(), newName.c_str(), RTFILEMOVE_FLAGS_REPLACE)
                    == VERR_FILE_NOT_FOUND)
                    RTFileDelete(newName.c_str());
            }
        }
    }

    PRTLOGGER loggerRelease;
    static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
    RTUINT fFlags = RTLOGFLAGS_PREFIX_TIME_PROG;
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    fFlags |= RTLOGFLAGS_USECRLF;
#endif
    char szError[RTPATH_MAX + 128] = "";
    int vrc = RTLogCreateEx(&loggerRelease, fFlags, "all",
                            "VBOX_RELEASE_LOG", RT_ELEMENTS(s_apszGroups), s_apszGroups,
                            RTLOGDEST_FILE, szError, sizeof(szError), logFile.raw());
    if (RT_SUCCESS(vrc))
    {
        /* some introductory information */
        RTTIMESPEC timeSpec;
        char szTmp[256];
        RTTimeSpecToString(RTTimeNow(&timeSpec), szTmp, sizeof(szTmp));
        RTLogRelLogger(loggerRelease, 0, ~0U,
                       "VirtualBox %s r%u %s (%s %s) release log\n"
#ifdef VBOX_BLEEDING_EDGE
                       "EXPERIMENTAL build " VBOX_BLEEDING_EDGE "\n"
#endif
                       "Log opened %s\n",
                       VBOX_VERSION_STRING, RTBldCfgRevision(), VBOX_BUILD_TARGET,
                       __DATE__, __TIME__, szTmp);

        vrc = RTSystemQueryOSInfo(RTSYSOSINFO_PRODUCT, szTmp, sizeof(szTmp));
        if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
            RTLogRelLogger(loggerRelease, 0, ~0U, "OS Product: %s\n", szTmp);
        vrc = RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szTmp, sizeof(szTmp));
        if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
            RTLogRelLogger(loggerRelease, 0, ~0U, "OS Release: %s\n", szTmp);
        vrc = RTSystemQueryOSInfo(RTSYSOSINFO_VERSION, szTmp, sizeof(szTmp));
        if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
            RTLogRelLogger(loggerRelease, 0, ~0U, "OS Version: %s\n", szTmp);
        vrc = RTSystemQueryOSInfo(RTSYSOSINFO_SERVICE_PACK, szTmp, sizeof(szTmp));
        if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
            RTLogRelLogger(loggerRelease, 0, ~0U, "OS Service Pack: %s\n", szTmp);
        RTLogRelLogger(loggerRelease, 0, ~0U, "Host RAM: %uMB RAM, available: %uMB\n",
                       uHostRamMb, uHostRamAvailMb);
        /* the package type is interesting for Linux distributions */
        char szExecName[RTPATH_MAX];
        char *pszExecName = RTProcGetExecutableName(szExecName, sizeof(szExecName));
        RTLogRelLogger(loggerRelease, 0, ~0U,
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
        RTLogRelSetDefaultInstance(loggerRelease);
        hrc = S_OK;
    }
    else
        hrc = setError(E_FAIL,
            tr("Failed to open release log (%s, %Rrc)"),
            szError, vrc);

    return hrc;
}

/**
 * Common worker for PowerUp and PowerUpPaused.
 *
 * @returns COM status code.
 *
 * @param   aProgress       Where to return the progress object.
 * @param   aPaused         true if PowerUpPaused called.
 *
 * @todo move down to powerDown();
 */
HRESULT Console::powerUp(IProgress **aProgress, bool aPaused)
{
    if (aProgress == NULL)
        return E_POINTER;

    LogFlowThisFuncEnter();
    LogFlowThisFunc(("mMachineState=%d\n", mMachineState));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    if (Global::IsOnlineOrTransient(mMachineState))
        return setError(VBOX_E_INVALID_VM_STATE,
            tr("Virtual machine is already running or busy (machine state: %s)"),
            Global::stringifyMachineState(mMachineState));

    HRESULT rc = S_OK;

    /* the network cards will undergo a quick consistency check */
    for (ULONG slot = 0; slot < SchemaDefs::NetworkAdapterCount; slot ++)
    {
        ComPtr<INetworkAdapter> adapter;
        mMachine->GetNetworkAdapter(slot, adapter.asOutParam());
        BOOL enabled = FALSE;
        adapter->COMGETTER(Enabled)(&enabled);
        if (!enabled)
            continue;

        NetworkAttachmentType_T netattach;
        adapter->COMGETTER(AttachmentType)(&netattach);
        switch (netattach)
        {
            case NetworkAttachmentType_Bridged:
            {
#ifdef RT_OS_WINDOWS
                /* a valid host interface must have been set */
                Bstr hostif;
                adapter->COMGETTER(HostInterface)(hostif.asOutParam());
                if (!hostif)
                {
                    return setError(VBOX_E_HOST_ERROR,
                        tr("VM cannot start because host interface networking requires a host interface name to be set"));
                }
                ComPtr<IVirtualBox> virtualBox;
                mMachine->COMGETTER(Parent)(virtualBox.asOutParam());
                ComPtr<IHost> host;
                virtualBox->COMGETTER(Host)(host.asOutParam());
                ComPtr<IHostNetworkInterface> hostInterface;
                if (!SUCCEEDED(host->FindHostNetworkInterfaceByName(hostif, hostInterface.asOutParam())))
                {
                    return setError(VBOX_E_HOST_ERROR,
                        tr("VM cannot start because the host interface '%ls' does not exist"),
                        hostif.raw());
                }
#endif /* RT_OS_WINDOWS */
                break;
            }
            default:
                break;
        }
    }

    /* Read console data stored in the saved state file (if not yet done) */
    rc = loadDataFromSavedState();
    CheckComRCReturnRC(rc);

    /* Check all types of shared folders and compose a single list */
    SharedFolderDataMap sharedFolders;
    {
        /* first, insert global folders */
        for (SharedFolderDataMap::const_iterator it = mGlobalSharedFolders.begin();
             it != mGlobalSharedFolders.end(); ++ it)
            sharedFolders[it->first] = it->second;

        /* second, insert machine folders */
        for (SharedFolderDataMap::const_iterator it = mMachineSharedFolders.begin();
             it != mMachineSharedFolders.end(); ++ it)
            sharedFolders[it->first] = it->second;

        /* third, insert console folders */
        for (SharedFolderMap::const_iterator it = mSharedFolders.begin();
             it != mSharedFolders.end(); ++ it)
            sharedFolders[it->first] = SharedFolderData(it->second->hostPath(), it->second->writable());
    }

    Bstr savedStateFile;

    /*
     * Saved VMs will have to prove that their saved states seem kosher.
     */
    if (mMachineState == MachineState_Saved)
    {
        rc = mMachine->COMGETTER(StateFilePath)(savedStateFile.asOutParam());
        CheckComRCReturnRC(rc);
        ComAssertRet(!!savedStateFile, E_FAIL);
        int vrc = SSMR3ValidateFile(Utf8Str(savedStateFile).c_str(), false /* fChecksumIt */);
        if (RT_FAILURE(vrc))
            return setError(VBOX_E_FILE_ERROR,
                            tr("VM cannot start because the saved state file '%ls' is invalid (%Rrc). Discard the saved state prior to starting the VM"),
                            savedStateFile.raw(), vrc);
    }

    /* test and clear the TeleporterEnabled property  */
    BOOL fTeleporterEnabled;
    rc = mMachine->COMGETTER(TeleporterEnabled)(&fTeleporterEnabled);
    CheckComRCReturnRC(rc);
    if (fTeleporterEnabled)
    {
        rc = mMachine->COMSETTER(TeleporterEnabled)(FALSE);
        CheckComRCReturnRC(rc);
    }

    /* create a progress object to track progress of this operation */
    ComObjPtr<Progress> powerupProgress;
    powerupProgress.createObject();
    Bstr progressDesc;
    if (mMachineState == MachineState_Saved)
        progressDesc = tr("Restoring virtual machine");
    else if (fTeleporterEnabled)
        progressDesc = tr("Teleporting virtual machine");
    else
        progressDesc = tr("Starting virtual machine");
    rc = powerupProgress->init(static_cast<IConsole *>(this),
                               progressDesc,
                               fTeleporterEnabled /* aCancelable */);
    CheckComRCReturnRC(rc);

    /* setup task object and thread to carry out the operation
     * asynchronously */

    std::auto_ptr<VMPowerUpTask> task(new VMPowerUpTask(this, powerupProgress));
    ComAssertComRCRetRC(task->rc());

    task->mSetVMErrorCallback = setVMErrorCallback;
    task->mConfigConstructor = configConstructor;
    task->mSharedFolders = sharedFolders;
    task->mStartPaused = aPaused;
    if (mMachineState == MachineState_Saved)
        task->mSavedStateFile = savedStateFile;
    task->mTeleporterEnabled = fTeleporterEnabled;

    /* Reset differencing hard disks for which autoReset is true */
    {
        com::SafeIfaceArray<IMediumAttachment> atts;
        rc = mMachine->COMGETTER(MediumAttachments)(ComSafeArrayAsOutParam(atts));
        CheckComRCReturnRC(rc);

        for (size_t i = 0; i < atts.size(); ++ i)
        {
            DeviceType_T devType;
            rc = atts[i]->COMGETTER(Type)(&devType);
            /** @todo later applies to floppies as well */
            if (devType == DeviceType_HardDisk)
            {
                ComPtr<IMedium> medium;
                rc = atts[i]->COMGETTER(Medium)(medium.asOutParam());
                CheckComRCReturnRC(rc);

                /* save for later use on the powerup thread */
                task->hardDisks.push_back(medium);

                /* needs autoreset? */
                BOOL autoReset = FALSE;
                rc = medium->COMGETTER(AutoReset)(&autoReset);
                CheckComRCReturnRC(rc);

                if (autoReset)
                {
                    ComPtr<IProgress> resetProgress;
                    rc = medium->Reset(resetProgress.asOutParam());
                    CheckComRCReturnRC(rc);

                    /* save for later use on the powerup thread */
                    task->hardDiskProgresses.push_back(resetProgress);
                }
            }
        }
    }

    rc = consoleInitReleaseLog(mMachine);
    CheckComRCReturnRC(rc);

    /* pass the progress object to the caller if requested */
    if (aProgress)
    {
        if (task->hardDiskProgresses.size() == 0)
        {
            /* there are no other operations to track, return the powerup
             * progress only */
            powerupProgress.queryInterfaceTo(aProgress);
        }
        else
        {
            /* create a combined progress object */
            ComObjPtr<CombinedProgress> progress;
            progress.createObject();
            VMPowerUpTask::ProgressList progresses(task->hardDiskProgresses);
            progresses.push_back(ComPtr<IProgress> (powerupProgress));
            rc = progress->init(static_cast<IConsole *>(this),
                                progressDesc, progresses.begin(),
                                progresses.end());
            AssertComRCReturnRC(rc);
            progress.queryInterfaceTo(aProgress);
        }
    }

    int vrc = RTThreadCreate(NULL, Console::powerUpThread, (void *) task.get(),
                             0, RTTHREADTYPE_MAIN_WORKER, 0, "VMPowerUp");

    ComAssertMsgRCRet(vrc, ("Could not create VMPowerUp thread (%Rrc)", vrc),
                      E_FAIL);

    /* task is now owned by powerUpThread(), so release it */
    task.release();

    /* finally, set the state: no right to fail in this method afterwards
     * since we've already started the thread and it is now responsible for
     * any error reporting and appropriate state change! */

    if (mMachineState == MachineState_Saved)
        setMachineState(MachineState_Restoring);
    else if (fTeleporterEnabled)
        setMachineState(MachineState_TeleportingIn);
    else
        setMachineState(MachineState_Starting);

    LogFlowThisFunc(("mMachineState=%d\n", mMachineState));
    LogFlowThisFuncLeave();
    return S_OK;
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
HRESULT Console::powerDown(Progress *aProgress /*= NULL*/)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    /* Total # of steps for the progress object. Must correspond to the
     * number of "advance percent count" comments in this method! */
    enum { StepCount = 7 };
    /* current step */
    ULONG step = 0;

    HRESULT rc = S_OK;
    int vrc = VINF_SUCCESS;

    /* sanity */
    Assert(mVMDestroying == false);

    Assert(mpVM != NULL);

    AssertMsg(   mMachineState == MachineState_Running
              || mMachineState == MachineState_Paused
              || mMachineState == MachineState_Stuck
              || mMachineState == MachineState_Starting
              || mMachineState == MachineState_Stopping
              || mMachineState == MachineState_Saving
              || mMachineState == MachineState_Restoring
              || mMachineState == MachineState_TeleportingPausedVM
              || mMachineState == MachineState_TeleportingIn         /** @todo Teleportation ???*/
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
            || mMachineState == MachineState_TeleportingIn)
       )
        mVMPoweredOff = true;

    /* go to Stopping state if not already there. Note that we don't go from
     * Saving/Restoring to Stopping because vmstateChangeCallback() needs it to
     * set the state to Saved on VMSTATE_TERMINATED. In terms of protecting from
     * inappropriate operations while leaving the lock below, Saving or
     * Restoring should be fine too.  Ditto for Teleporting* -> Teleported. */
    if (   mMachineState != MachineState_Saving
        && mMachineState != MachineState_Restoring
        && mMachineState != MachineState_Stopping
        && mMachineState != MachineState_TeleportingIn
        && mMachineState != MachineState_TeleportingPausedVM
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
        aProgress->SetCurrentOperationProgress(99 * (++ step) / StepCount );

#ifdef VBOX_WITH_HGCM

# ifdef VBOX_WITH_GUEST_PROPS  /** @todo r=bird: This may be premature, the VM may still be running at this point! */

    /* Save all guest property store entries to the machine XML file */
    com::SafeArray<BSTR> namesOut;
    com::SafeArray<BSTR> valuesOut;
    com::SafeArray<ULONG64> timestampsOut;
    com::SafeArray<BSTR> flagsOut;
    Bstr pattern("");
    if (pattern.isNull()) /** @todo r=bird: What is pattern actually used for?  And, again, what's is the out-of-memory policy in main? */
        rc = E_OUTOFMEMORY;
    else
        rc = doEnumerateGuestProperties(Bstr(""), ComSafeArrayAsOutParam(namesOut),
                                        ComSafeArrayAsOutParam(valuesOut),
                                        ComSafeArrayAsOutParam(timestampsOut),
                                        ComSafeArrayAsOutParam(flagsOut));
    if (SUCCEEDED(rc))
    {
        try
        {
            std::vector <BSTR> names;
            std::vector <BSTR> values;
            std::vector <ULONG64> timestamps;
            std::vector <BSTR> flags;
            for (unsigned i = 0; i < namesOut.size(); ++i)
            {
                uint32_t fFlags;
                guestProp::validateFlags(Utf8Str(flagsOut[i]).raw(), &fFlags);
                if (   !(fFlags & guestProp::TRANSIENT)
                    || mMachineState == MachineState_Saving
                    || mMachineState == MachineState_LiveSnapshotting
                  )
                {
                    names.push_back(namesOut[i]);
                    values.push_back(valuesOut[i]);
                    timestamps.push_back(timestampsOut[i]);
                    flags.push_back(flagsOut[i]);
                }
            }
            com::SafeArray<BSTR> namesIn(names);
            com::SafeArray<BSTR> valuesIn(values);
            com::SafeArray<ULONG64> timestampsIn(timestamps);
            com::SafeArray<BSTR> flagsIn(flags);
            if (   namesIn.isNull()
                || valuesIn.isNull()
                || timestampsIn.isNull()
                || flagsIn.isNull()
                )
                throw std::bad_alloc();
            /* PushGuestProperties() calls DiscardSettings(), which calls us back */
            alock.leave();
            mControl->PushGuestProperties(ComSafeArrayAsInParam(namesIn),
                                          ComSafeArrayAsInParam(valuesIn),
                                          ComSafeArrayAsInParam(timestampsIn),
                                          ComSafeArrayAsInParam(flagsIn));
            alock.enter();
        }
        catch (std::bad_alloc)
        {
            rc = E_OUTOFMEMORY;
        }
    }

    /* advance percent count */
    if (aProgress)
        aProgress->SetCurrentOperationProgress(99 * (++ step) / StepCount );

# endif /* VBOX_WITH_GUEST_PROPS defined */

    /* Shutdown HGCM services before stopping the guest, because they might
     * need a cleanup. */
    if (mVMMDev)
    {
        LogFlowThisFunc(("Shutdown HGCM...\n"));

        /* Leave the lock since EMT will call us back as addVMCaller() */
        alock.leave();

        mVMMDev->hgcmShutdown();

        alock.enter();
    }

    /* advance percent count */
    if (aProgress)
        aProgress->SetCurrentOperationProgress(99 * (++ step) / StepCount );

#endif /* VBOX_WITH_HGCM */

    /* ----------------------------------------------------------------------
     * Now, wait for all mpVM callers to finish their work if there are still
     * some on other threads. NO methods that need mpVM (or initiate other calls
     * that need it) may be called after this point
     * ---------------------------------------------------------------------- */

    if (mVMCallers > 0)
    {
        /* go to the destroying state to prevent from adding new callers */
        mVMDestroying = true;

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
        aProgress->SetCurrentOperationProgress(99 * (++ step) / StepCount );

    vrc = VINF_SUCCESS;

    /* Power off the VM if not already done that */
    if (!mVMPoweredOff)
    {
        LogFlowThisFunc(("Powering off the VM...\n"));

        /* Leave the lock since EMT will call us back on VMR3PowerOff() */
        alock.leave();

        vrc = VMR3PowerOff(mpVM);

        /* Note that VMR3PowerOff() may fail here (invalid VMSTATE) if the
         * VM-(guest-)initiated power off happened in parallel a ms before this
         * call. So far, we let this error pop up on the user's side. */

        alock.enter();

    }
    else
    {
        /* reset the flag for further re-use */
        mVMPoweredOff = false;
    }

    /* advance percent count */
    if (aProgress)
        aProgress->SetCurrentOperationProgress(99 * (++ step) / StepCount );

    LogFlowThisFunc(("Ready for VM destruction.\n"));

    /* If we are called from Console::uninit(), then try to destroy the VM even
     * on failure (this will most likely fail too, but what to do?..) */
    if (RT_SUCCESS(vrc) || autoCaller.state() == InUninit)
    {
        /* If the machine has an USB comtroller, release all USB devices
         * (symmetric to the code in captureUSBDevices()) */
        bool fHasUSBController = false;
        {
            PPDMIBASE pBase;
            int vrc = PDMR3QueryLun(mpVM, "usb-ohci", 0, 0, &pBase);
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

        /* Set mpVM to NULL early just in case if some old code is not using
         * addVMCaller()/releaseVMCaller(). */
        PVM pVM = mpVM;
        mpVM = NULL;

        LogFlowThisFunc(("Destroying the VM...\n"));

        alock.leave();

        vrc = VMR3Destroy(pVM);

        /* take the lock again */
        alock.enter();

        /* advance percent count */
        if (aProgress)
            aProgress->SetCurrentOperationProgress(99 * (++ step) / StepCount );

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
            /* bad bad bad, but what to do? */
            mpVM = pVM;
            rc = setError(VBOX_E_VM_ERROR,
                tr("Could not destroy the machine. (Error: %Rrc)"),
                vrc);
        }

        /* Complete the detaching of the USB devices. */
        if (fHasUSBController)
            detachAllUSBDevices(true /* aDone */);

        /* advance percent count */
        if (aProgress)
            aProgress->SetCurrentOperationProgress(99 * (++ step) / StepCount );
    }
    else
    {
        rc = setError(VBOX_E_VM_ERROR,
            tr("Could not power off the machine. (Error: %Rrc)"),
            vrc);
    }

    /* Finished with destruction. Note that if something impossible happened and
     * we've failed to destroy the VM, mVMDestroying will remain true and
     * mMachineState will be something like Stopping, so most Console methods
     * will return an error to the caller. */
    if (mpVM == NULL)
        mVMDestroying = false;

    if (SUCCEEDED(rc))
    {
        /* uninit dynamically allocated members of mCallbackData */
        if (mCallbackData.mpsc.valid)
        {
            if (mCallbackData.mpsc.shape != NULL)
                RTMemFree(mCallbackData.mpsc.shape);
        }
        memset(&mCallbackData, 0, sizeof(mCallbackData));
    }

    /* complete the progress */
    if (aProgress)
        aProgress->notifyComplete(rc);

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

    AutoWriteLock alock(this);

    HRESULT rc = S_OK;

    if (mMachineState != aMachineState)
    {
        LogFlowThisFunc(("machineState=%d\n", aMachineState));
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
             * session state is already SessionState_Closed at the same time
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
            LogFlowThisFunc(("mControl->UpdateState()=%08X\n", rc));
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
HRESULT Console::findSharedFolder(CBSTR aName,
                                  ComObjPtr<SharedFolder> &aSharedFolder,
                                  bool aSetError /* = false */)
{
    /* sanity check */
    AssertReturn(isWriteLockOnCurrentThread(), E_FAIL);

    SharedFolderMap::const_iterator it = mSharedFolders.find(aName);
    if (it != mSharedFolders.end())
    {
        aSharedFolder = it->second;
        return S_OK;
    }

    if (aSetError)
        setError(VBOX_E_FILE_ERROR,
            tr("Could not find a shared folder named '%ls'."),
            aName);

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

    /* protect mpVM (if not NULL) */
    AutoVMCallerQuietWeak autoVMCaller(this);

    HRESULT rc = S_OK;

    bool online = mpVM && autoVMCaller.isOk() && mVMMDev->isShFlActive();

    if (aGlobal)
    {
        /// @todo grab & process global folders when they are done
    }
    else
    {
        SharedFolderDataMap oldFolders;
        if (online)
            oldFolders = mMachineSharedFolders;

        mMachineSharedFolders.clear();

        SafeIfaceArray<ISharedFolder> folders;
        rc = mMachine->COMGETTER(SharedFolders)(ComSafeArrayAsOutParam(folders));
        AssertComRCReturnRC(rc);

        for (size_t i = 0; i < folders.size(); ++i)
        {
            ComPtr<ISharedFolder> folder = folders[i];

            Bstr name;
            Bstr hostPath;
            BOOL writable;

            rc = folder->COMGETTER(Name)(name.asOutParam());
            CheckComRCBreakRC(rc);
            rc = folder->COMGETTER(HostPath)(hostPath.asOutParam());
            CheckComRCBreakRC(rc);
            rc = folder->COMGETTER(Writable)(&writable);

            mMachineSharedFolders.insert(std::make_pair(name, SharedFolderData(hostPath, writable)));

            /* send changes to HGCM if the VM is running */
            /// @todo report errors as runtime warnings through VMSetError
            if (online)
            {
                SharedFolderDataMap::iterator it = oldFolders.find(name);
                if (it == oldFolders.end() || it->second.mHostPath != hostPath)
                {
                    /* a new machine folder is added or
                     * the existing machine folder is changed */
                    if (mSharedFolders.find(name) != mSharedFolders.end())
                        ; /* the console folder exists, nothing to do */
                    else
                    {
                        /* remove the old machine folder (when changed)
                         * or the global folder if any (when new) */
                        if (it != oldFolders.end() ||
                            mGlobalSharedFolders.find(name) !=
                                mGlobalSharedFolders.end())
                            rc = removeSharedFolder(name);
                        /* create the new machine folder */
                        rc = createSharedFolder(name, SharedFolderData(hostPath, writable));
                    }
                }
                /* forget the processed (or identical) folder */
                if (it != oldFolders.end())
                    oldFolders.erase(it);

                rc = S_OK;
            }
        }

        AssertComRCReturnRC(rc);

        /* process outdated (removed) folders */
        /// @todo report errors as runtime warnings through VMSetError
        if (online)
        {
            for (SharedFolderDataMap::const_iterator it = oldFolders.begin();
                 it != oldFolders.end(); ++ it)
            {
                if (mSharedFolders.find(it->first) != mSharedFolders.end())
                    ; /* the console folder exists, nothing to do */
                else
                {
                    /* remove the outdated machine folder */
                    rc = removeSharedFolder(it->first);
                    /* create the global folder if there is any */
                    SharedFolderDataMap::const_iterator git =
                        mGlobalSharedFolders.find(it->first);
                    if (git != mGlobalSharedFolders.end())
                        rc = createSharedFolder(git->first, git->second);
                }
            }

            rc = S_OK;
        }
    }

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
bool Console::findOtherSharedFolder(IN_BSTR aName,
                                    SharedFolderDataMap::const_iterator &aIt)
{
    /* sanity check */
    AssertReturn(isWriteLockOnCurrentThread(), false);

    /* first, search machine folders */
    aIt = mMachineSharedFolders.find(aName);
    if (aIt != mMachineSharedFolders.end())
        return true;

    /* second, search machine folders */
    aIt = mGlobalSharedFolders.find(aName);
    if (aIt != mGlobalSharedFolders.end())
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
HRESULT Console::createSharedFolder(CBSTR aName, SharedFolderData aData)
{
    ComAssertRet(aName && *aName, E_FAIL);
    ComAssertRet(aData.mHostPath, E_FAIL);

    /* sanity checks */
    AssertReturn(mpVM, E_FAIL);
    AssertReturn(mVMMDev->isShFlActive(), E_FAIL);

    VBOXHGCMSVCPARM parms[SHFL_CPARMS_ADD_MAPPING];
    SHFLSTRING *pFolderName, *pMapName;
    size_t cbString;

    Log(("Adding shared folder '%ls' -> '%ls'\n", aName, aData.mHostPath.raw()));

    cbString = (RTUtf16Len(aData.mHostPath) + 1) * sizeof(RTUTF16);
    if (cbString >= UINT16_MAX)
        return setError(E_INVALIDARG, tr("The name is too long"));
    pFolderName = (SHFLSTRING *) RTMemAllocZ(sizeof(SHFLSTRING) + cbString);
    Assert(pFolderName);
    memcpy(pFolderName->String.ucs2, aData.mHostPath, cbString);

    pFolderName->u16Size   = (uint16_t)cbString;
    pFolderName->u16Length = (uint16_t)cbString - sizeof(RTUTF16);

    parms[0].type = VBOX_HGCM_SVC_PARM_PTR;
    parms[0].u.pointer.addr = pFolderName;
    parms[0].u.pointer.size = sizeof(SHFLSTRING) + (uint16_t)cbString;

    cbString = (RTUtf16Len(aName) + 1) * sizeof(RTUTF16);
    if (cbString >= UINT16_MAX)
    {
        RTMemFree(pFolderName);
        return setError(E_INVALIDARG, tr("The host path is too long"));
    }
    pMapName = (SHFLSTRING *) RTMemAllocZ(sizeof(SHFLSTRING) + cbString);
    Assert(pMapName);
    memcpy(pMapName->String.ucs2, aName, cbString);

    pMapName->u16Size   = (uint16_t)cbString;
    pMapName->u16Length = (uint16_t)cbString - sizeof(RTUTF16);

    parms[1].type = VBOX_HGCM_SVC_PARM_PTR;
    parms[1].u.pointer.addr = pMapName;
    parms[1].u.pointer.size = sizeof(SHFLSTRING) + (uint16_t)cbString;

    parms[2].type = VBOX_HGCM_SVC_PARM_32BIT;
    parms[2].u.uint32 = aData.mWritable;

    int vrc = mVMMDev->hgcmHostCall("VBoxSharedFolders",
                                    SHFL_FN_ADD_MAPPING,
                                    SHFL_CPARMS_ADD_MAPPING, &parms[0]);
    RTMemFree(pFolderName);
    RTMemFree(pMapName);

    if (RT_FAILURE(vrc))
        return setError(E_FAIL,
            tr("Could not create a shared folder '%ls' mapped to '%ls' (%Rrc)"),
            aName, aData.mHostPath.raw(), vrc);

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
HRESULT Console::removeSharedFolder(CBSTR aName)
{
    ComAssertRet(aName && *aName, E_FAIL);

    /* sanity checks */
    AssertReturn(mpVM, E_FAIL);
    AssertReturn(mVMMDev->isShFlActive(), E_FAIL);

    VBOXHGCMSVCPARM parms;
    SHFLSTRING *pMapName;
    size_t cbString;

    Log(("Removing shared folder '%ls'\n", aName));

    cbString = (RTUtf16Len(aName) + 1) * sizeof(RTUTF16);
    if (cbString >= UINT16_MAX)
        return setError(E_INVALIDARG, tr("The name is too long"));
    pMapName = (SHFLSTRING *) RTMemAllocZ(sizeof(SHFLSTRING) + cbString);
    Assert(pMapName);
    memcpy(pMapName->String.ucs2, aName, cbString);

    pMapName->u16Size   = (uint16_t)cbString;
    pMapName->u16Length = (uint16_t)cbString - sizeof(RTUTF16);

    parms.type = VBOX_HGCM_SVC_PARM_PTR;
    parms.u.pointer.addr = pMapName;
    parms.u.pointer.size = sizeof(SHFLSTRING) + (uint16_t)cbString;

    int vrc = mVMMDev->hgcmHostCall("VBoxSharedFolders",
                                    SHFL_FN_REMOVE_MAPPING,
                                    1, &parms);
    RTMemFree(pMapName);
    if (RT_FAILURE(vrc))
        return setError(E_FAIL,
                        tr("Could not remove the shared folder '%ls' (%Rrc)"),
                        aName, vrc);

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
    LogFlowFunc(("Changing state from %d to %d (aVM=%p)\n",
                 aOldState, aState, aVM));

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
            AutoWriteLock alock(that);

            if (that->mVMStateChangeCallbackDisabled)
                break;

            /* Do we still think that it is running? It may happen if this is a
             * VM-(guest-)initiated shutdown/poweroff.
             */
            if (   that->mMachineState != MachineState_Stopping
                && that->mMachineState != MachineState_Saving
                && that->mMachineState != MachineState_Restoring
                && that->mMachineState != MachineState_TeleportingIn
                && that->mMachineState != MachineState_LiveSnapshotting
                && that->mMachineState != MachineState_Teleporting
                && that->mMachineState != MachineState_TeleportingPausedVM
               )
            {
                LogFlowFunc(("VM has powered itself off but Console still thinks it is running. Notifying.\n"));

                /* prevent powerDown() from calling VMR3PowerOff() again */
                Assert(that->mVMPoweredOff == false);
                that->mVMPoweredOff = true;

                /* we are stopping now */
                that->setMachineState(MachineState_Stopping);

                /* Setup task object and thread to carry out the operation
                 * asynchronously (if we call powerDown() right here but there
                 * is one or more mpVM callers (added with addVMCaller()) we'll
                 * deadlock).
                 */
                std::auto_ptr<VMProgressTask> task(new VMProgressTask(that, NULL /* aProgress */,
                                                                      true /* aUsesVMPtr */));

                 /* If creating a task is falied, this can currently mean one of
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
            AutoWriteLock alock(that);

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
                    /* successfully saved (note that the machine is already in
                     * the Saved state on the server due to EndSavingState()
                     * called from saveStateThread(), so only change the local
                     * state) */
                    that->setMachineStateLocally(MachineState_Saved);
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
                    /* Teleportation failed or was cancelled.  Back to powered off. */
                    that->setMachineState(MachineState_PoweredOff);
                    break;
                case MachineState_Teleporting:
                case MachineState_TeleportingPausedVM:
                    /* Successfully teleported the VM. */
                    that->setMachineState(MachineState_Teleported);
                    break;
            }
            break;
        }

        case VMSTATE_SUSPENDED:
        {
            if (aOldState == VMSTATE_SUSPENDING)
            {
                AutoWriteLock alock(that);

                if (that->mVMStateChangeCallbackDisabled)
                    break;

                /* Change the machine state from Running to Paused. */
                AssertBreak(that->mMachineState == MachineState_Running);
                that->setMachineState(MachineState_Paused);
            }
            break;
        }

        case VMSTATE_SUSPENDED_LS:
        case VMSTATE_SUSPENDED_EXT_LS:
        {
            AutoWriteLock alock(that);
            if (that->mVMStateChangeCallbackDisabled)
                break;
            if (that->mMachineState == MachineState_Teleporting)
                that->setMachineState(MachineState_TeleportingPausedVM);
            else if (that->mMachineState == MachineState_LiveSnapshotting)
                that->setMachineState(MachineState_Saving);
            else
                AssertMsgFailed(("%s/%s -> %s\n", Global::stringifyMachineState(that->mMachineState), VMR3GetStateName(aOldState),  VMR3GetStateName(aState) ));
            break;
        }

        case VMSTATE_RUNNING:
        {
            if (   aOldState == VMSTATE_POWERING_ON
                || aOldState == VMSTATE_RESUMING)
            {
                AutoWriteLock alock(that);

                if (that->mVMStateChangeCallbackDisabled)
                    break;

                /* Change the machine state from Starting, Restoring or Paused
                 * to Running */
                Assert(   (   (   that->mMachineState == MachineState_Starting
                               || that->mMachineState == MachineState_Paused)
                           && aOldState == VMSTATE_POWERING_ON)
                       || (   (   that->mMachineState == MachineState_Restoring
                               || that->mMachineState == MachineState_TeleportingIn
                               || that->mMachineState == MachineState_Paused)
                           && aOldState == VMSTATE_RESUMING));

                that->setMachineState(MachineState_Running);
            }

            break;
        }

        case VMSTATE_RUNNING_LS:
            AssertMsg(   that->mMachineState == MachineState_LiveSnapshotting
                      || that->mMachineState == MachineState_Teleporting,
                      ("%s/%s -> %s\n", Global::stringifyMachineState(that->mMachineState), VMR3GetStateName(aOldState),  VMR3GetStateName(aState) ));
            break;

        case VMSTATE_FATAL_ERROR:
        {
            AutoWriteLock alock(that);

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
            AutoWriteLock alock(that);

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
    AutoWriteLock alock(this);

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

    /* protect mpVM */
    AutoVMCaller autoVMCaller(this);
    CheckComRCReturnRC(autoVMCaller.rc());

    LogFlowThisFunc(("Proxying USB device '%s' {%RTuuid}...\n",
                      Address.raw(), uuid.ptr()));

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

/** @todo just do everything here and only wrap the PDMR3Usb call. That'll offload some notification stuff from the EMT thread. */
    int vrc = VMR3ReqCallWait(mpVM, VMCPUID_ANY,
                              (PFNRT) usbAttachCallback, 6, this, aHostDevice, uuid.ptr(), fRemote, Address.raw(), aMaskedIfs);

    /* restore the lock */
    alock.enter();

    /* hrc is S_OK here */

    if (RT_FAILURE(vrc))
    {
        LogWarningThisFunc(("Failed to create proxy device for '%s' {%RTuuid} (%Rrc)\n",
                             Address.raw(), uuid.ptr(), vrc));

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
Console::usbAttachCallback(Console *that, IUSBDevice *aHostDevice, PCRTUUID aUuid, bool aRemote, const char *aAddress, ULONG aMaskedIfs)
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

    int vrc = PDMR3USBCreateProxyDevice(that->mpVM, aUuid, aRemote, aAddress, pvRemoteBackend,
                                        portVersion == 1 ? VUSB_STDVER_11 : VUSB_STDVER_20, aMaskedIfs);
    if (RT_SUCCESS(vrc))
    {
        /* Create a OUSBDevice and add it to the device list */
        ComObjPtr<OUSBDevice> device;
        device.createObject();
        HRESULT hrc = device->init(aHostDevice);
        AssertComRC(hrc);

        AutoWriteLock alock(that);
        that->mUSBDevices.push_back(device);
        LogFlowFunc(("Attached device {%RTuuid}\n", device->id().raw()));

        /* notify callbacks */
        that->onUSBDeviceStateChange(device, true /* aAttached */, NULL);
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
    AutoWriteLock alock(this);

    /* protect mpVM */
    AutoVMCaller autoVMCaller(this);
    CheckComRCReturnRC(autoVMCaller.rc());

    /* if the device is attached, then there must at least one USB hub. */
    AssertReturn(PDMR3USBHasHub(mpVM), E_FAIL);

    LogFlowThisFunc(("Detaching USB proxy device {%RTuuid}...\n",
                      (*aIt)->id().raw()));

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

/** @todo just do everything here and only wrap the PDMR3Usb call. That'll offload some notification stuff from the EMT thread. */
    int vrc = VMR3ReqCallWait(mpVM, VMCPUID_ANY,
                              (PFNRT) usbDetachCallback, 4, this, &aIt, (*aIt)->id().raw());
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
Console::usbDetachCallback(Console *that, USBDeviceList::iterator *aIt, PCRTUUID aUuid)
{
    LogFlowFuncEnter();
    LogFlowFunc(("that={%p}\n", that));

    AssertReturn(that && aUuid, VERR_INVALID_PARAMETER);
    ComObjPtr<OUSBDevice> device = **aIt;

    /*
     * If that was a remote device, release the backend pointer.
     * The pointer was requested in usbAttachCallback.
     */
    BOOL fRemote = FALSE;

    HRESULT hrc2 = (**aIt)->COMGETTER(Remote)(&fRemote);
    ComAssertComRC(hrc2);

    if (fRemote)
    {
        Guid guid(*aUuid);
        that->consoleVRDPServer()->USBBackendReleasePointer(&guid);
    }

    int vrc = PDMR3USBDetachDevice(that->mpVM, aUuid);

    if (RT_SUCCESS(vrc))
    {
        AutoWriteLock alock(that);

        /* Remove the device from the collection */
        that->mUSBDevices.erase(*aIt);
        LogFlowFunc(("Detached device {%RTuuid}\n", device->id().raw()));

        /* notify callbacks */
        that->onUSBDeviceStateChange(device, false /* aAttached */, NULL);
    }

    LogFlowFunc(("vrc=%Rrc\n", vrc));
    LogFlowFuncLeave();
    return vrc;
}

#endif /* VBOX_WITH_USB */
#if (defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD)) && !defined(VBOX_WITH_NETFLT)

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
        rc = networkAdapter->COMGETTER(HostInterface)(tapDeviceName.asOutParam());
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
                strcpy(IfReq.ifr_name, str.raw());
            else
                memcpy(IfReq.ifr_name, str.raw(), sizeof(IfReq.ifr_name) - 1); /** @todo bitch about names which are too long... */
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
    rc = networkAdapter->COMGETTER(HostInterface)(tapDeviceName.asOutParam());
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
        strcat(szTapdev, str.raw());
    else
        memcpy(szTapdev + strlen(szTapdev), str.raw(), sizeof(szTapdev) - strlen(szTapdev) - 1); /** @todo bitch about names which are too long... */
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
        rc = networkAdapter->COMGETTER(HostInterface)(tapDeviceName.asOutParam());
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
    HRESULT rc;
    for (ULONG slot = 0; slot < SchemaDefs::NetworkAdapterCount; slot ++)
    {
        ComPtr<INetworkAdapter> networkAdapter;
        rc = mMachine->GetNetworkAdapter(slot, networkAdapter.asOutParam());
        CheckComRCBreakRC(rc);

        BOOL enabled = FALSE;
        networkAdapter->COMGETTER(Enabled)(&enabled);
        if (!enabled)
            continue;

        NetworkAttachmentType_T attachment;
        networkAdapter->COMGETTER(AttachmentType)(&attachment);
        if (attachment == NetworkAttachmentType_Bridged)
        {
#if defined(RT_OS_LINUX) && !defined(VBOX_WITH_NETFLT)
            HRESULT rc2 = detachFromTapInterface(networkAdapter);
            if (FAILED(rc2) && SUCCEEDED(rc))
                rc = rc2;
#endif
        }
    }

    return rc;
}


/**
 * Process callback handler for VMR3LoadFromFile, VMR3LoadFromStream, VMR3Save
 * and VMR3Teleport.
 *
 * @param   pVM         The VM handle.
 * @param   uPercent    Completetion precentage (0-100).
 * @param   pvUser      Pointer to the VMProgressTask structure.
 * @return  VINF_SUCCESS.
 */
/*static*/
DECLCALLBACK(int) Console::stateProgressCallback(PVM pVM, unsigned uPercent, void *pvUser)
{
    VMProgressTask *task = static_cast<VMProgressTask *>(pvUser);
    AssertReturn(task, VERR_INVALID_PARAMETER);

    /* update the progress object */
    if (task->mProgress)
        task->mProgress->SetCurrentOperationProgress(uPercent);

    return VINF_SUCCESS;
}

/**
 * VM error callback function. Called by the various VM components.
 *
 * @param   pVM         VM handle. Can be NULL if an error occurred before
 *                      successfully creating a VM.
 * @param   pvUser      Pointer to the VMProgressTask structure.
 * @param   rc          VBox status code.
 * @param   pszFormat   Printf-like error message.
 * @param   args        Various number of arguments for the error message.
 *
 * @thread EMT, VMPowerUp...
 *
 * @note The VMProgressTask structure modified by this callback is not thread
 *       safe.
 */
/* static */ DECLCALLBACK(void)
Console::setVMErrorCallback(PVM pVM, void *pvUser, int rc, RT_SRC_POS_DECL,
                            const char *pszFormat, va_list args)
{
    VMProgressTask *task = static_cast<VMProgressTask *>(pvUser);
    AssertReturnVoid(task);

    /* we ignore RT_SRC_POS_DECL arguments to avoid confusion of end-users */
    va_list va2;
    va_copy(va2, args); /* Have to make a copy here or GCC will break. */

    /* append to the existing error message if any */
    if (task->mErrorMsg.length())
        task->mErrorMsg = Utf8StrFmt("%s.\n%N (%Rrc)", task->mErrorMsg.raw(),
                                     pszFormat, &va2, rc, rc);
    else
        task->mErrorMsg = Utf8StrFmt("%N (%Rrc)",
                                     pszFormat, &va2, rc, rc);

    va_end (va2);
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

    Utf8Str message = Utf8StrFmtVA(pszFormat, va);

    LogRel(("Console: VM runtime error: fatal=%RTbool, errorID=%s message=\"%s\"\n",
             fFatal, pszErrorId, message.raw()));

    that->onRuntimeError(BOOL(fFatal), Bstr(pszErrorId), Bstr(message));

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
        AutoWriteLock alock(this);
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
    AutoWriteLock alock(this);
    alock.leave();

    mControl->DetachAllUSBDevices(aDone);
}

/**
 * @note Locks this object for writing.
 */
void Console::processRemoteUSBDevices(uint32_t u32ClientId, VRDPUSBDEVICEDESC *pDevList, uint32_t cbDevList)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("u32ClientId = %d, pDevList=%p, cbDevList = %d\n", u32ClientId, pDevList, cbDevList));

    AutoCaller autoCaller(this);
    if (!autoCaller.isOk())
    {
        /* Console has been already uninitialized, deny request */
        AssertMsgFailed(("Console is already uninitialized\n"));
        LogFlowThisFunc(("Console is already uninitialized\n"));
        LogFlowThisFuncLeave();
        return;
    }

    AutoWriteLock alock(this);

    /*
     * Mark all existing remote USB devices as dirty.
     */
    RemoteUSBDeviceList::iterator it = mRemoteUSBDevices.begin();
    while (it != mRemoteUSBDevices.end())
    {
        (*it)->dirty(true);
        ++ it;
    }

    /*
     * Process the pDevList and add devices those are not already in the mRemoteUSBDevices list.
     */
    /** @todo (sunlover) REMOTE_USB Strict validation of the pDevList. */
    VRDPUSBDEVICEDESC *e = pDevList;

    /* The cbDevList condition must be checked first, because the function can
     * receive pDevList = NULL and cbDevList = 0 on client disconnect.
     */
    while (cbDevList >= 2 && e->oNext)
    {
        LogFlowThisFunc(("vendor %04X, product %04X, name = %s\n",
                          e->idVendor, e->idProduct,
                          e->oProduct? (char *)e + e->oProduct: ""));

        bool fNewDevice = true;

        it = mRemoteUSBDevices.begin();
        while (it != mRemoteUSBDevices.end())
        {
            if ((*it)->devId() == e->id
                && (*it)->clientId() == u32ClientId)
            {
               /* The device is already in the list. */
               (*it)->dirty(false);
               fNewDevice = false;
               break;
            }

            ++ it;
        }

        if (fNewDevice)
        {
            LogRel(("Remote USB: ++++ Vendor %04X. Product %04X. Name = [%s].\n",
                    e->idVendor, e->idProduct, e->oProduct? (char *)e + e->oProduct: ""));

            /* Create the device object and add the new device to list. */
            ComObjPtr<RemoteUSBDevice> device;
            device.createObject();
            device->init(u32ClientId, e);

            mRemoteUSBDevices.push_back(device);

            /* Check if the device is ok for current USB filters. */
            BOOL fMatched = FALSE;
            ULONG fMaskedIfs = 0;

            HRESULT hrc = mControl->RunUSBDeviceFilters(device, &fMatched, &fMaskedIfs);

            AssertComRC(hrc);

            LogFlowThisFunc(("USB filters return %d %#x\n", fMatched, fMaskedIfs));

            if (fMatched)
            {
                hrc = onUSBDeviceAttach(device, NULL, fMaskedIfs);

                /// @todo (r=dmik) warning reporting subsystem

                if (hrc == S_OK)
                {
                    LogFlowThisFunc(("Device attached\n"));
                    device->captured(true);
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

        e = (VRDPUSBDEVICEDESC *)((uint8_t *)e + e->oNext);
    }

    /*
     * Remove dirty devices, that is those which are not reported by the server anymore.
     */
    for (;;)
    {
        ComObjPtr<RemoteUSBDevice> device;

        RemoteUSBDeviceList::iterator it = mRemoteUSBDevices.begin();
        while (it != mRemoteUSBDevices.end())
        {
            if ((*it)->dirty())
            {
                device = *it;
                break;
            }

            ++ it;
        }

        if (!device)
        {
            break;
        }

        USHORT vendorId = 0;
        device->COMGETTER(VendorId)(&vendorId);

        USHORT productId = 0;
        device->COMGETTER(ProductId)(&productId);

        Bstr product;
        device->COMGETTER(Product)(product.asOutParam());

        LogRel(("Remote USB: ---- Vendor %04X. Product %04X. Name = [%ls].\n",
                vendorId, productId, product.raw()));

        /* Detach the device from VM. */
        if (device->captured())
        {
            Bstr uuid;
            device->COMGETTER(Id)(uuid.asOutParam());
            onUSBDeviceDetach(uuid, NULL);
        }

        /* And remove it from the list. */
        mRemoteUSBDevices.erase(it);
    }

    LogFlowThisFuncLeave();
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

#if defined(RT_OS_WINDOWS)
    {
        /* initialize COM */
        HRESULT hrc = CoInitializeEx(NULL,
                                     COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE |
                                     COINIT_SPEED_OVER_MEMORY);
        LogFlowFunc(("CoInitializeEx()=%08X\n", hrc));
    }
#endif

    HRESULT rc = S_OK;
    int vrc = VINF_SUCCESS;

    /* Set up a build identifier so that it can be seen from core dumps what
     * exact build was used to produce the core. */
    static char saBuildID[40];
    RTStrPrintf(saBuildID, sizeof(saBuildID), "%s%s%s%s VirtualBox %s r%u %s%s%s%s",
                "BU", "IL", "DI", "D", VBOX_VERSION_STRING, RTBldCfgRevision(), "BU", "IL", "DI", "D");

    ComObjPtr<Console> console = task->mConsole;

    /* Note: no need to use addCaller() because VMPowerUpTask does that */

    /* The lock is also used as a signal from the task initiator (which
     * releases it only after RTThreadCreate()) that we can start the job */
    AutoWriteLock alock(console);

    /* sanity */
    Assert(console->mpVM == NULL);

    try
    {
        /* wait for auto reset ops to complete so that we can successfully lock
         * the attached hard disks by calling LockMedia() below */
        for (VMPowerUpTask::ProgressList::const_iterator
             it = task->hardDiskProgresses.begin();
             it != task->hardDiskProgresses.end(); ++ it)
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
        if (!task->mTeleporterEnabled)
        {
            rc = console->mControl->LockMedia();
            CheckComRCThrowRC(rc);
        }

#ifdef VBOX_WITH_VRDP

        /* Create the VRDP server. In case of headless operation, this will
         * also create the framebuffer, required at VM creation.
         */
        ConsoleVRDPServer *server = console->consoleVRDPServer();
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
            console->mVRDPServer->COMGETTER(Ports)(bstr.asOutParam());
            Utf8Str ports = bstr;
            errMsg = Utf8StrFmt(tr("VRDP server can't bind to a port: %s"),
                                ports.raw());
            LogRel(("Warning: failed to launch VRDP server (%Rrc): '%s'\n",
                    vrc, errMsg.raw()));
        }
        else if (RT_FAILURE(vrc))
        {
            Utf8Str errMsg;
            switch (vrc)
            {
                case VERR_FILE_NOT_FOUND:
                {
                    errMsg = Utf8StrFmt(tr("Could not load the VRDP library"));
                    break;
                }
                default:
                    errMsg = Utf8StrFmt(tr("Failed to launch VRDP server (%Rrc)"),
                                        vrc);
            }
            LogRel(("Failed to launch VRDP server (%Rrc), error message: '%s'\n",
                     vrc, errMsg.raw()));
            throw setError(E_FAIL, errMsg.c_str());
        }

#endif /* VBOX_WITH_VRDP */

        ComPtr<IMachine> pMachine = console->machine();
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

        vrc = VMR3Create(cCpus, task->mSetVMErrorCallback, task.get(),
                         task->mConfigConstructor, static_cast<Console *>(console),
                         &pVM);

        alock.enter();

#ifdef VBOX_WITH_VRDP
        /* Enable client connections to the server. */
        console->consoleVRDPServer()->EnableConnections();
#endif /* VBOX_WITH_VRDP */

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
                                            static_cast<Console *>(console));
                AssertRCBreak(vrc);

                vrc = static_cast<Console *>(console)->getDisplay()->registerSSM(pVM);
                AssertRC(vrc);
                if (RT_FAILURE(vrc))
                    break;

                /*
                 * Synchronize debugger settings
                 */
                MachineDebugger *machineDebugger = console->getMachineDebugger();
                if (machineDebugger)
                {
                    machineDebugger->flushQueuedSettings();
                }

                /*
                 * Shared Folders
                 */
                if (console->getVMMDev()->isShFlActive())
                {
                    /* Does the code below call Console from the other thread?
                     * Not sure, so leave the lock just in case. */
                    alock.leave();

                    for (SharedFolderDataMap::const_iterator
                             it = task->mSharedFolders.begin();
                         it != task->mSharedFolders.end();
                         ++ it)
                    {
                        rc = console->createSharedFolder((*it).first, (*it).second);
                        CheckComRCBreakRC(rc);
                    }

                    /* enter the lock again */
                    alock.enter();

                    CheckComRCBreakRC(rc);
                }

                /*
                 * Capture USB devices.
                 */
                rc = console->captureUSBDevices(pVM);
                CheckComRCBreakRC(rc);

                /* leave the lock before a lengthy operation */
                alock.leave();

                /* Load saved state? */
                if (task->mSavedStateFile.length())
                {
                    LogFlowFunc(("Restoring saved state from '%s'...\n",
                                 task->mSavedStateFile.raw()));

                    vrc = VMR3LoadFromFile(pVM,
                                           task->mSavedStateFile.c_str(),
                                           Console::stateProgressCallback,
                                           static_cast<VMProgressTask*>(task.get()));

                    if (RT_SUCCESS(vrc))
                    {
                        if (task->mStartPaused)
                            /* done */
                            console->setMachineState(MachineState_Paused);
                        else
                        {
                            /* Start/Resume the VM execution */
                            vrc = VMR3Resume(pVM);
                            AssertRC(vrc);
                        }
                    }

                    /* Power off in case we failed loading or resuming the VM */
                    if (RT_FAILURE(vrc))
                    {
                        int vrc2 = VMR3PowerOff(pVM);
                        AssertRC(vrc2);
                    }
                }
                else if (task->mTeleporterEnabled)
                {
                    /* -> ConsoleImplTeleporter.cpp */
                    vrc = console->teleporterTrg(pVM, pMachine, task->mStartPaused, task->mProgress);
                    if (RT_FAILURE(vrc) && !task->mErrorMsg.length())
                        rc = E_FAIL;    /* Avoid the "Missing error message..." assertion. */
                }
                else if (task->mStartPaused)
                    /* done */
                    console->setMachineState(MachineState_Paused);
                else
                {
                    /* Power on the VM (i.e. start executing) */
                    vrc = VMR3PowerOn(pVM);
                    AssertRC(vrc);
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
                HRESULT rc2 = console->powerDown();
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
                VMR3AtErrorDeregister(pVM, task->mSetVMErrorCallback, task.get());
                /** @todo register another VMSetError callback? */
                alock.enter();
            }
        }
        else
        {
            /*
             * If VMR3Create() failed it has released the VM memory.
             */
            console->mpVM = NULL;
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
            throw setError(E_FAIL, task->mErrorMsg.c_str());
        }
    }
    catch (HRESULT aRC) { rc = aRC; }

    if (   console->mMachineState == MachineState_Starting
        || console->mMachineState == MachineState_Restoring
        || console->mMachineState == MachineState_TeleportingIn
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

        Assert(console->mpVM == NULL);
        vmstateChangeCallback(NULL, VMSTATE_TERMINATED, VMSTATE_CREATING,
                              console);
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

#if defined(RT_OS_WINDOWS)
    /* uninitialize COM */
    CoUninitialize();
#endif

    LogFlowFuncLeave();

    return VINF_SUCCESS;
}


/**
 * Reconfigures a medium attachment (part of taking an online snapshot).
 *
 * @param   pVM           The VM handle.
 * @param   lInstance     The instance of the controller.
 * @param   enmController The type of the controller.
 * @param   enmBus        The storage bus type of the controller.
 * @param   aMediumAtt    The medium attachment.
 * @param   phrc          Where to store com error - only valid if we return VERR_GENERAL_FAILURE.
 * @return  VBox status code.
 */
static DECLCALLBACK(int) reconfigureMedium(PVM pVM, ULONG lInstance,
                                           StorageControllerType_T enmController,
                                           StorageBus_T enmBus,
                                           IMediumAttachment *aMediumAtt,
                                           HRESULT *phrc)
{
    LogFlowFunc(("pVM=%p aMediumAtt=%p phrc=%p\n", pVM, aMediumAtt, phrc));

    int             rc;
    HRESULT         hrc;
    Bstr            bstr;
    *phrc = S_OK;
#define RC_CHECK() do { if (RT_FAILURE(rc)) { AssertMsgFailed(("rc=%Rrc\n", rc)); return rc; } } while (0)
#define H() do { if (FAILED(hrc)) { AssertMsgFailed(("hrc=%Rhrc (%#x)\n", hrc, hrc)); *phrc = hrc; return VERR_GENERAL_FAILURE; } } while (0)

    /*
     * Figure out medium and other attachment details.
     */
    ComPtr<IMedium> medium;
    hrc = aMediumAtt->COMGETTER(Medium)(medium.asOutParam());                   H();
    LONG lDev;
    hrc = aMediumAtt->COMGETTER(Device)(&lDev);                                 H();
    LONG lPort;
    hrc = aMediumAtt->COMGETTER(Port)(&lPort);                                  H();
    DeviceType_T lType;
    hrc = aMediumAtt->COMGETTER(Type)(&lType);                                  H();

    unsigned iLUN;
    const char *pcszDevice = Console::convertControllerTypeToDev(enmController);
    AssertMsgReturn(pcszDevice, ("invalid disk controller type: %d\n", enmController), VERR_GENERAL_FAILURE);
    hrc = Console::convertBusPortDeviceToLun(enmBus, lPort, lDev, iLUN);        H();

    /* Ignore attachments other than hard disks, since at the moment they are
     * not subject to snapshotting in general. */
    if (lType != DeviceType_HardDisk || medium.isNull())
        return VINF_SUCCESS;

    /** @todo this should be unified with the relevant part of
    * Console::configConstructor to avoid inconsistencies. */

    /*
     * Is there an existing LUN? If not create it.
     */
    PCFGMNODE pCfg;
    PCFGMNODE pLunL1;

    /* SCSI has an extra driver between the device and the block driver. */
    if (enmBus == StorageBus_SCSI)
        pLunL1 = CFGMR3GetChildF(CFGMR3GetRoot(pVM), "Devices/%s/%u/LUN#%u/AttachedDriver/AttachedDriver/", pcszDevice, lInstance, iLUN);
    else
        pLunL1 = CFGMR3GetChildF(CFGMR3GetRoot(pVM), "Devices/%s/%u/LUN#%u/AttachedDriver/", pcszDevice, lInstance, iLUN);

    if (!pLunL1)
    {
        PCFGMNODE pInst = CFGMR3GetChildF(CFGMR3GetRoot(pVM), "Devices/%s/%u/", pcszDevice, lInstance);
        AssertReturn(pInst, VERR_INTERNAL_ERROR);

        PCFGMNODE pLunL0;
        rc = CFGMR3InsertNodeF(pInst, &pLunL0, "LUN#%u", iLUN);                     RC_CHECK();

        if (enmBus == StorageBus_SCSI)
        {
            rc = CFGMR3InsertString(pLunL0, "Driver",              "SCSI");             RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                           RC_CHECK();

            rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL0);                 RC_CHECK();
        }

        rc = CFGMR3InsertString(pLunL0, "Driver",              "Block");            RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                           RC_CHECK();
        rc = CFGMR3InsertString(pCfg,   "Type",                "HardDisk");         RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "Mountable",            0);                 RC_CHECK();

        rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);                 RC_CHECK();
        rc = CFGMR3InsertString(pLunL1, "Driver",              "VD");               RC_CHECK();
        rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                           RC_CHECK();
    }
    else
    {
#ifdef VBOX_STRICT
        char *pszDriver;
        rc = CFGMR3QueryStringAlloc(pLunL1, "Driver", &pszDriver);                  RC_CHECK();
        Assert(!strcmp(pszDriver, "VD"));
        MMR3HeapFree(pszDriver);
#endif

        pCfg = CFGMR3GetChild(pLunL1, "Config");
        AssertReturn(pCfg, VERR_INTERNAL_ERROR);

        /* Here used to be a lot of code checking if things have changed,
         * but that's not really worth it, as with snapshots there is always
         * some change, so the code was just logging useless information in
         * a hard to analyze form. */

        /*
         * Detach the driver and replace the config node.
         */
        rc = PDMR3DeviceDetach(pVM, pcszDevice, 0, iLUN, PDM_TACH_FLAGS_NOT_HOT_PLUG); RC_CHECK();
        CFGMR3RemoveNode(pCfg);
        rc = CFGMR3InsertNode(pLunL1, "Config", &pCfg);                             RC_CHECK();
    }

    /*
     * Create the driver configuration.
     */
    hrc = medium->COMGETTER(Location)(bstr.asOutParam());                       H();
    LogFlowFunc(("LUN#%u: leaf location '%ls'\n", iLUN, bstr.raw()));
    rc = CFGMR3InsertString(pCfg, "Path", Utf8Str(bstr).c_str());                       RC_CHECK();
    hrc = medium->COMGETTER(Format)(bstr.asOutParam());                         H();
    LogFlowFunc(("LUN#%u: leaf format '%ls'\n", iLUN, bstr.raw()));
    rc = CFGMR3InsertString(pCfg, "Format", Utf8Str(bstr).c_str());                     RC_CHECK();

    /* Pass all custom parameters. */
    bool fHostIP = true;
    SafeArray<BSTR> names;
    SafeArray<BSTR> values;
    hrc = medium->GetProperties(NULL,
                                ComSafeArrayAsOutParam(names),
                                ComSafeArrayAsOutParam(values));        H();

    if (names.size() != 0)
    {
        PCFGMNODE pVDC;
        rc = CFGMR3InsertNode(pCfg, "VDConfig", &pVDC);                 RC_CHECK();
        for (size_t i = 0; i < names.size(); ++ i)
        {
            if (values[i])
            {
                Utf8Str name = names[i];
                Utf8Str value = values[i];
                rc = CFGMR3InsertString(pVDC, name.c_str(), value.c_str());
                if (    !(name.compare("HostIPStack"))
                    &&  !(value.compare("0")))
                    fHostIP = false;
            }
        }
    }

    /* Create an inversed tree of parents. */
    ComPtr<IMedium> parentMedium = medium;
    for (PCFGMNODE pParent = pCfg;;)
    {
        hrc = parentMedium->COMGETTER(Parent)(medium.asOutParam());             H();
        if (medium.isNull())
            break;

        PCFGMNODE pCur;
        rc = CFGMR3InsertNode(pParent, "Parent", &pCur);                        RC_CHECK();
        hrc = medium->COMGETTER(Location)(bstr.asOutParam());                   H();
        rc = CFGMR3InsertString(pCur,  "Path", Utf8Str(bstr).c_str());          RC_CHECK();

        hrc = medium->COMGETTER(Format)(bstr.asOutParam());                     H();
        rc = CFGMR3InsertString(pCur,  "Format", Utf8Str(bstr).c_str());        RC_CHECK();

        /* Pass all custom parameters. */
        SafeArray<BSTR> names;
        SafeArray<BSTR> values;
        hrc = medium->GetProperties(NULL,
                                    ComSafeArrayAsOutParam(names),
                                    ComSafeArrayAsOutParam(values));            H();

        if (names.size() != 0)
        {
            PCFGMNODE pVDC;
            rc = CFGMR3InsertNode(pCur, "VDConfig", &pVDC);             RC_CHECK();
            for (size_t i = 0; i < names.size(); ++ i)
            {
                if (values[i])
                {
                    Utf8Str name = names[i];
                    Utf8Str value = values[i];
                    rc = CFGMR3InsertString(pVDC, name.c_str(), value.c_str());
                    if (    !(name.compare("HostIPStack"))
                        &&  !(value.compare("0")))
                        fHostIP = false;
                }
            }
        }

        /* Custom code: put marker to not use host IP stack to driver
        * configuration node. Simplifies life of DrvVD a bit. */
        if (!fHostIP)
        {
            rc = CFGMR3InsertInteger(pCfg, "HostIPStack", 0);           RC_CHECK();
        }


        /* next */
        pParent = pCur;
        parentMedium = medium;
    }

    CFGMR3Dump(CFGMR3GetRoot(pVM));

    /*
     * Attach the new driver.
     */
    rc = PDMR3DeviceAttach(pVM, pcszDevice, 0, iLUN, PDM_TACH_FLAGS_NOT_HOT_PLUG, NULL /*ppBase*/); RC_CHECK();

    LogFlowFunc(("Returns success\n"));
    return rc;
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

    bool fBeganTakingSnapshot = false;

    AutoCaller autoCaller(pTask->mConsole);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(pTask->mConsole);

    HRESULT rc = S_OK;

    Console *that = pTask->mConsole;

    try
    {
        /* STEP 1 + 2:
         * request creating the diff images on the server and create the snapshot object
         * (this will set the machine state to Saving on the server to block
         * others from accessing this machine)
         */
        rc = pTask->mConsole->mControl->BeginTakingSnapshot(that,
                                                            pTask->bstrName,
                                                            pTask->bstrDescription,
                                                            pTask->mProgress,
                                                            pTask->fTakingSnapshotOnline,
                                                            pTask->bstrSavedStateFile.asOutParam());
        if (FAILED(rc)) throw rc;

        fBeganTakingSnapshot = true;

        /*
         * state file is non-null only when the VM is paused
         * (i.e. creating a snapshot online)
         */
        ComAssertThrow(    (!pTask->bstrSavedStateFile.isNull() && pTask->fTakingSnapshotOnline)
                        || (pTask->bstrSavedStateFile.isNull() && !pTask->fTakingSnapshotOnline),
                       rc = E_FAIL);

        /* sync the state with the server */
        that->setMachineStateLocally(MachineState_Saving);

        // STEP 3: save the VM state (if online)
        if (pTask->fTakingSnapshotOnline)
        {
            Utf8Str strSavedStateFile(pTask->bstrSavedStateFile);

            pTask->mProgress->SetNextOperation(Bstr(tr("Saving the machine state")),
                                               pTask->ulMemSize);       // operation weight, same as computed when setting up progress object

            alock.leave();

            int vrc = VMR3Save(that->mpVM,
                               strSavedStateFile.c_str(),
                               true /*fContinueAfterwards*/,
                               Console::stateProgressCallback,
                               (void*)pTask);
            if (RT_FAILURE(vrc))
                throw setError(E_FAIL,
                            tr("Failed to save the machine state to '%s' (%Rrc)"),
                            strSavedStateFile.c_str(), vrc);

            alock.enter();

            // STEP 4: reattach hard disks
            LogFlowFunc(("Reattaching new differencing hard disks...\n"));

            pTask->mProgress->SetNextOperation(Bstr(tr("Reconfiguring medium attachments")),
                                               1);       // operation weight, same as computed when setting up progress object

            com::SafeIfaceArray<IMediumAttachment> atts;
            rc = that->mMachine->COMGETTER(MediumAttachments)(ComSafeArrayAsOutParam(atts));
            if (FAILED(rc)) throw rc;

            for (size_t i = 0;
                i < atts.size();
                ++i)
            {
                ComPtr<IStorageController> controller;
                BSTR controllerName;
                ULONG lInstance;
                StorageControllerType_T enmController;
                StorageBus_T enmBus;

                /*
                * We can't pass a storage controller object directly
                * (g++ complains about not being able to pass non POD types through '...')
                * so we have to query needed values here and pass them.
                */
                rc = atts[i]->COMGETTER(Controller)(&controllerName);
                if (FAILED(rc)) throw rc;

                rc = that->mMachine->GetStorageControllerByName(controllerName, controller.asOutParam());
                if (FAILED(rc)) throw rc;

                rc = controller->COMGETTER(ControllerType)(&enmController);
                if (FAILED(rc)) throw rc;
                rc = controller->COMGETTER(Instance)(&lInstance);
                if (FAILED(rc)) throw rc;
                rc = controller->COMGETTER(Bus)(&enmBus);
                if (FAILED(rc)) throw rc;

                /*
                 * don't leave the lock since reconfigureMedium isn't going
                 * to access Console.
                 */
                int vrc = VMR3ReqCallWait(that->mpVM,
                                          VMCPUID_ANY,
                                          (PFNRT)reconfigureMedium,
                                          6,
                                          that->mpVM,
                                          lInstance,
                                          enmController,
                                          enmBus,
                                          atts[i],
                                          &rc);
                if (RT_FAILURE(vrc))
                    throw setError(E_FAIL, Console::tr("%Rrc"), vrc);
                if (FAILED(rc)) throw rc;
            }
        }

        /*
         * finalize the requested snapshot object.
         * This will reset the machine state to the state it had right
         * before calling mControl->BeginTakingSnapshot().
         */
        rc = that->mControl->EndTakingSnapshot(TRUE);        // success
        // do not throw rc here because we can't call EndTakingSnapshot() twice
    }
    catch (HRESULT rc)
    {
        /* preserve existing error info */
        ErrorInfoKeeper eik;

        if (fBeganTakingSnapshot)
            that->mControl->EndTakingSnapshot(FALSE);             // failure
    }

    pTask->mProgress->notifyComplete(rc);

    delete pTask;

    if (pTask->lastMachineState == MachineState_Running)
    {
        /* restore the paused state if appropriate */
        that->setMachineStateLocally(MachineState_Paused);
        /* restore the running state if appropriate */
        that->Resume();
    }
    else
        that->setMachineStateLocally(pTask->lastMachineState);

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
    Assert(!task->mProgress.isNull());

    const ComObjPtr<Console> &that = task->mConsole;
    Utf8Str errMsg;
    HRESULT rc = S_OK;

    LogFlowFunc(("Saving the state to '%s'...\n", task->mSavedStateFile.raw()));

    int vrc = VMR3Save(that->mpVM,
                       task->mSavedStateFile.c_str(),
                       false, /*fContinueAfterwards*/
                       Console::stateProgressCallback,
                       static_cast<VMProgressTask*>(task.get()));
    if (RT_FAILURE(vrc))
    {
        errMsg = Utf8StrFmt(Console::tr("Failed to save the machine state to '%s' (%Rrc)"),
                            task->mSavedStateFile.raw(), vrc);
        rc = E_FAIL;
    }

    /* lock the console once we're going to access it */
    AutoWriteLock thatLock(that);

    /*
     * finalize the requested save state procedure.
     * In case of success, the server will set the machine state to Saved;
     * in case of failure it will reset the it to the state it had right
     * before calling mControl->BeginSavingState().
     */
    that->mControl->EndSavingState(SUCCEEDED(rc));

    /* synchronize the state with the server */
    if (!FAILED(rc))
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

    /* notify the progress object about operation completion */
    if (SUCCEEDED(rc))
        task->mProgress->notifyComplete(S_OK);
    else
    {
        if (errMsg.length())
            task->mProgress->notifyComplete(rc,
                                            COM_IIDOF(IConsole),
                                            Console::getComponentName(),
                                            errMsg.c_str());
        else
            task->mProgress->notifyComplete(rc);
    }

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

    std::auto_ptr<VMProgressTask> task(static_cast<VMProgressTask *>(pvUser));
    AssertReturn(task.get(), VERR_INVALID_PARAMETER);

    AssertReturn(task->isOk(), VERR_GENERAL_FAILURE);

    const ComObjPtr<Console> &that = task->mConsole;

    /* Note: no need to use addCaller() to protect Console because VMTask does
     * that */

    /* wait until the method tat started us returns */
    AutoWriteLock thatLock(that);

    /* release VM caller to avoid the powerDown() deadlock */
    task->releaseVMCaller();

    that->powerDown(task->mProgress);

    LogFlowFuncLeave();
    return VINF_SUCCESS;
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
    PDRVMAINSTATUS pData = (PDRVMAINSTATUS)(void *)pInterface;
    if (iLUN >= pData->iFirstLUN && iLUN <= pData->iLastLUN)
    {
        PPDMLED pLed;
        int rc = pData->pLedPorts->pfnQueryStatusLed(pData->pLedPorts, iLUN, &pLed);
        if (RT_FAILURE(rc))
            pLed = NULL;
        ASMAtomicXchgPtr((void * volatile *)&pData->papLeds[iLUN - pData->iFirstLUN], pLed);
        Log(("drvStatus_UnitChanged: iLUN=%d pLed=%p\n", iLUN, pLed));
    }
}


/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 */
DECLCALLBACK(void *)  Console::drvStatus_QueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVMAINSTATUS pDrv = PDMINS_2_DATA(pDrvIns, PDRVMAINSTATUS);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_LED_CONNECTORS:
            return &pDrv->ILedConnectors;
        default:
            return NULL;
    }
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
    if (pData->papLeds)
    {
        unsigned iLed = pData->iLastLUN - pData->iFirstLUN + 1;
        while (iLed-- > 0)
            ASMAtomicXchgPtr((void * volatile *)&pData->papLeds[iLed], NULL);
    }
}


/**
 * Construct a status driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
DECLCALLBACK(int) Console::drvStatus_Construct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle, uint32_t fFlags)
{
    PDRVMAINSTATUS pData = PDMINS_2_DATA(pDrvIns, PDRVMAINSTATUS);
    LogFlowFunc(("iInstance=%d\n", pDrvIns->iInstance));

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "papLeds\0First\0Last\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;
    AssertMsgReturn(PDMDrvHlpNoAttach(pDrvIns) == VERR_PDM_NO_ATTACHED_DRIVER,
                    ("Configuration error: Not possible to attach anything to this driver!\n"),
                    VERR_PDM_DRVINS_NO_ATTACH);

    /*
     * Data.
     */
    pDrvIns->IBase.pfnQueryInterface        = Console::drvStatus_QueryInterface;
    pData->ILedConnectors.pfnUnitChanged    = Console::drvStatus_UnitChanged;

    /*
     * Read config.
     */
    int rc = CFGMR3QueryPtr(pCfgHandle, "papLeds", (void **)&pData->papLeds);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Failed to query the \"papLeds\" value! rc=%Rrc\n", rc));
        return rc;
    }

    rc = CFGMR3QueryU32(pCfgHandle, "First", &pData->iFirstLUN);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->iFirstLUN = 0;
    else if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Failed to query the \"First\" value! rc=%Rrc\n", rc));
        return rc;
    }

    rc = CFGMR3QueryU32(pCfgHandle, "Last", &pData->iLastLUN);
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
    pData->pLedPorts = (PPDMILEDPORTS)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_LED_PORTS);
    if (!pData->pLedPorts)
    {
        AssertMsgFailed(("Configuration error: No led ports interface above!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

    for (unsigned i = pData->iFirstLUN; i <= pData->iLastLUN; ++i)
        Console::drvStatus_UnitChanged(&pData->ILedConnectors, i);

    return VINF_SUCCESS;
}


/**
 * Keyboard driver registration record.
 */
const PDMDRVREG Console::DrvStatusReg =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "MainStatus",
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
