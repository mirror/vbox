/* $Id$ */

/** @file
 *
 * VBox Console COM Class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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
#include "Version.h"
#include "package-generated.h"

// generated header
#include "SchemaDefs.h"

#include "Logging.h"

#include <VBox/com/array.h>

#include <iprt/string.h>
#include <iprt/asm.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/process.h>
#include <iprt/ldr.h>
#include <iprt/cpputils.h>
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

#include <VBox/VBoxDev.h>

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
      Console::addCaller() for more details).
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
    VMTask (Console *aConsole, bool aUsesVMPtr)
        : mConsole (aConsole), mCallerAdded (false), mVMCallerAdded (false)
    {
        AssertReturnVoid (aConsole);
        mRC = aConsole->addCaller();
        if (SUCCEEDED (mRC))
        {
            mCallerAdded = true;
            if (aUsesVMPtr)
            {
                mRC = aConsole->addVMCaller();
                if (SUCCEEDED (mRC))
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
    bool isOk() const { return SUCCEEDED (rc()); }

    /** Releases the Console caller before destruction. Not normally necessary. */
    void releaseCaller()
    {
        AssertReturnVoid (mCallerAdded);
        mConsole->releaseCaller();
        mCallerAdded = false;
    }

    /** Releases the VM caller before destruction. Not normally necessary. */
    void releaseVMCaller()
    {
        AssertReturnVoid (mVMCallerAdded);
        mConsole->releaseVMCaller();
        mVMCallerAdded = false;
    }

    const ComObjPtr <Console> mConsole;

private:

    HRESULT mRC;
    bool mCallerAdded : 1;
    bool mVMCallerAdded : 1;
};

struct VMProgressTask : public VMTask
{
    VMProgressTask (Console *aConsole, Progress *aProgress, bool aUsesVMPtr)
        : VMTask (aConsole, aUsesVMPtr), mProgress (aProgress) {}

    const ComObjPtr <Progress> mProgress;

    Utf8Str mErrorMsg;
};

struct VMPowerUpTask : public VMProgressTask
{
    VMPowerUpTask (Console *aConsole, Progress *aProgress)
        : VMProgressTask (aConsole, aProgress, false /* aUsesVMPtr */)
        , mSetVMErrorCallback (NULL), mConfigConstructor (NULL), mStartPaused (false) {}

    PFNVMATERROR mSetVMErrorCallback;
    PFNCFGMCONSTRUCTOR mConfigConstructor;
    Utf8Str mSavedStateFile;
    Console::SharedFolderDataMap mSharedFolders;
    bool mStartPaused;

    typedef std::list <ComPtr <IHardDisk> > HardDiskList;
    HardDiskList hardDisks;

    /* array of progress objects for hard disk reset operations */
    typedef std::list <ComPtr <IProgress> > ProgressList;
    ProgressList hardDiskProgresses;
};

struct VMSaveTask : public VMProgressTask
{
    VMSaveTask (Console *aConsole, Progress *aProgress)
        : VMProgressTask (aConsole, aProgress, true /* aUsesVMPtr */)
        , mIsSnapshot (false)
        , mLastMachineState (MachineState_Null) {}

    bool mIsSnapshot;
    Utf8Str mSavedStateFile;
    MachineState_T mLastMachineState;
    ComPtr <IProgress> mServerProgress;
};

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

Console::Console()
    : mSavedStateDataLoaded (false)
    , mConsoleVRDPServer (NULL)
    , mpVM (NULL)
    , mVMCallers (0)
    , mVMZeroCallersSem (NIL_RTSEMEVENT)
    , mVMDestroying (false)
    , mVMPoweredOff (false)
    , meDVDState (DriveState_NotMounted)
    , meFloppyState (DriveState_NotMounted)
    , mVMMDev (NULL)
    , mAudioSniffer (NULL)
    , mVMStateChangeCallbackDisabled (false)
    , mMachineState (MachineState_PoweredOff)
{}

Console::~Console()
{}

HRESULT Console::FinalConstruct()
{
    LogFlowThisFunc (("\n"));

    memset(mapFDLeds, 0, sizeof(mapFDLeds));
    memset(mapIDELeds, 0, sizeof(mapIDELeds));
    memset(mapSATALeds, 0, sizeof(mapSATALeds));
    memset(mapNetworkLeds, 0, sizeof(mapNetworkLeds));
    memset(&mapUSBLed, 0, sizeof(mapUSBLed));
    memset(&mapSharedFolderLed, 0, sizeof(mapSharedFolderLed));

    return S_OK;
}

void Console::FinalRelease()
{
    LogFlowThisFunc (("\n"));

    uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

HRESULT Console::init (IMachine *aMachine, IInternalMachineControl *aControl)
{
    AssertReturn (aMachine && aControl, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aMachine=%p, aControl=%p\n", aMachine, aControl));

    HRESULT rc = E_FAIL;

    unconst (mMachine) = aMachine;
    unconst (mControl) = aControl;

    memset (&mCallbackData, 0, sizeof (mCallbackData));

    /* Cache essential properties and objects */

    rc = mMachine->COMGETTER(State) (&mMachineState);
    AssertComRCReturnRC (rc);

#ifdef VBOX_WITH_VRDP
    rc = mMachine->COMGETTER(VRDPServer) (unconst (mVRDPServer).asOutParam());
    AssertComRCReturnRC (rc);
#endif

    rc = mMachine->COMGETTER(DVDDrive) (unconst (mDVDDrive).asOutParam());
    AssertComRCReturnRC (rc);

    rc = mMachine->COMGETTER(FloppyDrive) (unconst (mFloppyDrive).asOutParam());
    AssertComRCReturnRC (rc);

    /* Create associated child COM objects */

    unconst (mGuest).createObject();
    rc = mGuest->init (this);
    AssertComRCReturnRC (rc);

    unconst (mKeyboard).createObject();
    rc = mKeyboard->init (this);
    AssertComRCReturnRC (rc);

    unconst (mMouse).createObject();
    rc = mMouse->init (this);
    AssertComRCReturnRC (rc);

    unconst (mDisplay).createObject();
    rc = mDisplay->init (this);
    AssertComRCReturnRC (rc);

    unconst (mRemoteDisplayInfo).createObject();
    rc = mRemoteDisplayInfo->init (this);
    AssertComRCReturnRC (rc);

    /* Grab global and machine shared folder lists */

    rc = fetchSharedFolders (true /* aGlobal */);
    AssertComRCReturnRC (rc);
    rc = fetchSharedFolders (false /* aGlobal */);
    AssertComRCReturnRC (rc);

    /* Create other child objects */

    unconst (mConsoleVRDPServer) = new ConsoleVRDPServer (this);
    AssertReturn (mConsoleVRDPServer, E_FAIL);

    mcAudioRefs = 0;
    mcVRDPClients = 0;
    mu32SingleRDPClientId = 0;

    unconst (mVMMDev) = new VMMDev(this);
    AssertReturn (mVMMDev, E_FAIL);

    unconst (mAudioSniffer) = new AudioSniffer(this);
    AssertReturn (mAudioSniffer, E_FAIL);

    /* Confirm a successful initialization when it's the case */
    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();

    return S_OK;
}

/**
 *  Uninitializes the Console object.
 */
void Console::uninit()
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
    {
        LogFlowThisFunc (("Already uninitialized.\n"));
        LogFlowThisFuncLeave();
        return;
    }

    LogFlowThisFunc (("initFailed()=%d\n", autoUninitSpan.initFailed()));

    /*
     * Uninit all children that use addDependentChild()/removeDependentChild()
     * in their init()/uninit() methods.
     */
    uninitDependentChildren();

    /* power down the VM if necessary */
    if (mpVM)
    {
        powerDown();
        Assert (mpVM == NULL);
    }

    if (mVMZeroCallersSem != NIL_RTSEMEVENT)
    {
        RTSemEventDestroy (mVMZeroCallersSem);
        mVMZeroCallersSem = NIL_RTSEMEVENT;
    }

    if (mAudioSniffer)
    {
        delete mAudioSniffer;
        unconst (mAudioSniffer) = NULL;
    }

    if (mVMMDev)
    {
        delete mVMMDev;
        unconst (mVMMDev) = NULL;
    }

    mGlobalSharedFolders.clear();
    mMachineSharedFolders.clear();

    mSharedFolders.clear();
    mRemoteUSBDevices.clear();
    mUSBDevices.clear();

    if (mRemoteDisplayInfo)
    {
        mRemoteDisplayInfo->uninit();
        unconst (mRemoteDisplayInfo).setNull();;
    }

    if (mDebugger)
    {
        mDebugger->uninit();
        unconst (mDebugger).setNull();
    }

    if (mDisplay)
    {
        mDisplay->uninit();
        unconst (mDisplay).setNull();
    }

    if (mMouse)
    {
        mMouse->uninit();
        unconst (mMouse).setNull();
    }

    if (mKeyboard)
    {
        mKeyboard->uninit();
        unconst (mKeyboard).setNull();;
    }

    if (mGuest)
    {
        mGuest->uninit();
        unconst (mGuest).setNull();;
    }

    if (mConsoleVRDPServer)
    {
        delete mConsoleVRDPServer;
        unconst (mConsoleVRDPServer) = NULL;
    }

    unconst (mFloppyDrive).setNull();
    unconst (mDVDDrive).setNull();
#ifdef VBOX_WITH_VRDP
    unconst (mVRDPServer).setNull();
#endif

    unconst (mControl).setNull();
    unconst (mMachine).setNull();

    /* Release all callbacks. Do this after uninitializing the components,
     * as some of them are well-behaved and unregister their callbacks.
     * These would trigger error messages complaining about trying to
     * unregister a non-registered callback. */
    mCallbacks.clear();

    /* dynamically allocated members of mCallbackData are uninitialized
     * at the end of powerDown() */
    Assert (!mCallbackData.mpsc.valid && mCallbackData.mpsc.shape == NULL);
    Assert (!mCallbackData.mcc.valid);
    Assert (!mCallbackData.klc.valid);

    LogFlowThisFuncLeave();
}

int Console::VRDPClientLogon (uint32_t u32ClientId, const char *pszUser, const char *pszPassword, const char *pszDomain)
{
    LogFlowFuncEnter();
    LogFlowFunc (("%d, %s, %s, %s\n", u32ClientId, pszUser, pszPassword, pszDomain));

    AutoCaller autoCaller (this);
    if (!autoCaller.isOk())
    {
        /* Console has been already uninitialized, deny request */
        LogRel(("VRDPAUTH: Access denied (Console uninitialized).\n"));
        LogFlowFuncLeave();
        return VERR_ACCESS_DENIED;
    }

    Guid uuid;
    HRESULT hrc = mMachine->COMGETTER (Id) (uuid.asOutParam());
    AssertComRCReturn (hrc, VERR_ACCESS_DENIED);

    VRDPAuthType_T authType = VRDPAuthType_Null;
    hrc = mVRDPServer->COMGETTER(AuthType) (&authType);
    AssertComRCReturn (hrc, VERR_ACCESS_DENIED);

    ULONG authTimeout = 0;
    hrc = mVRDPServer->COMGETTER(AuthTimeout) (&authTimeout);
    AssertComRCReturn (hrc, VERR_ACCESS_DENIED);

    VRDPAuthResult result = VRDPAuthAccessDenied;
    VRDPAuthGuestJudgement guestJudgement = VRDPAuthGuestNotAsked;

    LogFlowFunc(("Auth type %d\n", authType));

    LogRel (("VRDPAUTH: User: [%s]. Domain: [%s]. Authentication type: [%s]\n",
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
            result = mConsoleVRDPServer->Authenticate (uuid, guestJudgement, pszUser, pszPassword, pszDomain, u32ClientId);

            if (result != VRDPAuthDelegateToGuest)
            {
                break;
            }

            LogRel(("VRDPAUTH: Delegated to guest.\n"));

            LogFlowFunc (("External auth asked for guest judgement\n"));
        } /* pass through */

        case VRDPAuthType_Guest:
        {
            guestJudgement = VRDPAuthGuestNotReacted;

            if (mVMMDev)
            {
                /* Issue the request to guest. Assume that the call does not require EMT. It should not. */

                /* Ask the guest to judge these credentials. */
                uint32_t u32GuestFlags = VMMDEV_SETCREDENTIALS_JUDGE;

                int rc = mVMMDev->getVMMDevPort()->pfnSetCredentials (mVMMDev->getVMMDevPort(),
                             pszUser, pszPassword, pszDomain, u32GuestFlags);

                if (VBOX_SUCCESS (rc))
                {
                    /* Wait for guest. */
                    rc = mVMMDev->WaitCredentialsJudgement (authTimeout, &u32GuestFlags);

                    if (VBOX_SUCCESS (rc))
                    {
                        switch (u32GuestFlags & (VMMDEV_CREDENTIALS_JUDGE_OK | VMMDEV_CREDENTIALS_JUDGE_DENY | VMMDEV_CREDENTIALS_JUDGE_NOJUDGEMENT))
                        {
                            case VMMDEV_CREDENTIALS_JUDGE_DENY:        guestJudgement = VRDPAuthGuestAccessDenied;  break;
                            case VMMDEV_CREDENTIALS_JUDGE_NOJUDGEMENT: guestJudgement = VRDPAuthGuestNoJudgement;   break;
                            case VMMDEV_CREDENTIALS_JUDGE_OK:          guestJudgement = VRDPAuthGuestAccessGranted; break;
                            default:
                                LogFlowFunc (("Invalid guest flags %08X!!!\n", u32GuestFlags)); break;
                        }
                    }
                    else
                    {
                        LogFlowFunc (("Wait for credentials judgement rc = %Rrc!!!\n", rc));
                    }

                    LogFlowFunc (("Guest judgement %d\n", guestJudgement));
                }
                else
                {
                    LogFlowFunc (("Could not set credentials rc = %Rrc!!!\n", rc));
                }
            }

            if (authType == VRDPAuthType_External)
            {
                LogRel(("VRDPAUTH: Guest judgement %d.\n", guestJudgement));
                LogFlowFunc (("External auth called again with guest judgement = %d\n", guestJudgement));
                result = mConsoleVRDPServer->Authenticate (uuid, guestJudgement, pszUser, pszPassword, pszDomain, u32ClientId);
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

    LogFlowFunc (("Result = %d\n", result));
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
    hrc = mVRDPServer->COMGETTER(AllowMultiConnection) (&allowMultiConnection);
    AssertComRCReturn (hrc, VERR_ACCESS_DENIED);

    BOOL reuseSingleConnection = FALSE;
    hrc = mVRDPServer->COMGETTER(ReuseSingleConnection) (&reuseSingleConnection);
    AssertComRCReturn (hrc, VERR_ACCESS_DENIED);

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
                mConsoleVRDPServer->DisconnectClient (mu32SingleRDPClientId, false);
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

    return VINF_SUCCESS;
}

void Console::VRDPClientConnect (uint32_t u32ClientId)
{
    LogFlowFuncEnter();

    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

#ifdef VBOX_WITH_VRDP
    uint32_t u32Clients = ASMAtomicIncU32(&mcVRDPClients);

    if (u32Clients == 1)
    {
        getVMMDev()->getVMMDevPort()->
            pfnVRDPChange (getVMMDev()->getVMMDevPort(),
                           true, VRDP_EXPERIENCE_LEVEL_FULL); // @todo configurable
    }

    NOREF(u32ClientId);
    mDisplay->VideoAccelVRDP (true);
#endif /* VBOX_WITH_VRDP */

    LogFlowFuncLeave();
    return;
}

void Console::VRDPClientDisconnect (uint32_t u32ClientId,
                                    uint32_t fu32Intercepted)
{
    LogFlowFuncEnter();

    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AssertReturnVoid (mConsoleVRDPServer);

#ifdef VBOX_WITH_VRDP
    uint32_t u32Clients = ASMAtomicDecU32(&mcVRDPClients);

    if (u32Clients == 0)
    {
        getVMMDev()->getVMMDevPort()->
            pfnVRDPChange (getVMMDev()->getVMMDevPort(),
                           false, 0);
    }

    mDisplay->VideoAccelVRDP (false);
#endif /* VBOX_WITH_VRDP */

    if (fu32Intercepted & VRDP_CLIENT_INTERCEPT_USB)
    {
        mConsoleVRDPServer->USBBackendDelete (u32ClientId);
    }

#ifdef VBOX_WITH_VRDP
    if (fu32Intercepted & VRDP_CLIENT_INTERCEPT_CLIPBOARD)
    {
        mConsoleVRDPServer->ClipboardDelete (u32ClientId);
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
                    port->pfnSetup (port, false, false);
                }
            }
        }
    }
#endif /* VBOX_WITH_VRDP */

    Guid uuid;
    HRESULT hrc = mMachine->COMGETTER (Id) (uuid.asOutParam());
    AssertComRC (hrc);

    VRDPAuthType_T authType = VRDPAuthType_Null;
    hrc = mVRDPServer->COMGETTER(AuthType) (&authType);
    AssertComRC (hrc);

    if (authType == VRDPAuthType_External)
        mConsoleVRDPServer->AuthDisconnect (uuid, u32ClientId);

    LogFlowFuncLeave();
    return;
}

void Console::VRDPInterceptAudio (uint32_t u32ClientId)
{
    LogFlowFuncEnter();

    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    LogFlowFunc (("mAudioSniffer %p, u32ClientId %d.\n",
                  mAudioSniffer, u32ClientId));
    NOREF(u32ClientId);

#ifdef VBOX_WITH_VRDP
    mcAudioRefs++;

    if (mcAudioRefs == 1)
    {
        if (mAudioSniffer)
        {
            PPDMIAUDIOSNIFFERPORT port = mAudioSniffer->getAudioSnifferPort();
            if (port)
            {
                port->pfnSetup (port, true, true);
            }
        }
    }
#endif

    LogFlowFuncLeave();
    return;
}

void Console::VRDPInterceptUSB (uint32_t u32ClientId, void **ppvIntercept)
{
    LogFlowFuncEnter();

    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AssertReturnVoid (mConsoleVRDPServer);

    mConsoleVRDPServer->USBBackendCreate (u32ClientId, ppvIntercept);

    LogFlowFuncLeave();
    return;
}

void Console::VRDPInterceptClipboard (uint32_t u32ClientId)
{
    LogFlowFuncEnter();

    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AssertReturnVoid (mConsoleVRDPServer);

#ifdef VBOX_WITH_VRDP
    mConsoleVRDPServer->ClipboardCreate (u32ClientId);
#endif /* VBOX_WITH_VRDP */

    LogFlowFuncLeave();
    return;
}


//static
const char *Console::sSSMConsoleUnit = "ConsoleData";
//static
uint32_t Console::sSSMConsoleVer = 0x00010001;

/**
 *  Loads various console data stored in the saved state file.
 *  This method does validation of the state file and returns an error info
 *  when appropriate.
 *
 *  The method does nothing if the machine is not in the Saved file or if
 *  console data from it has already been loaded.
 *
 *  @note The caller must lock this object for writing.
 */
HRESULT Console::loadDataFromSavedState()
{
    if (mMachineState != MachineState_Saved || mSavedStateDataLoaded)
        return S_OK;

    Bstr savedStateFile;
    HRESULT rc = mMachine->COMGETTER(StateFilePath) (savedStateFile.asOutParam());
    if (FAILED (rc))
        return rc;

    PSSMHANDLE ssm;
    int vrc = SSMR3Open (Utf8Str(savedStateFile), 0, &ssm);
    if (VBOX_SUCCESS (vrc))
    {
        uint32_t version = 0;
        vrc = SSMR3Seek (ssm, sSSMConsoleUnit, 0 /* iInstance */, &version);
        if (SSM_VERSION_MAJOR(version)  == SSM_VERSION_MAJOR(sSSMConsoleVer))
        {
            if (VBOX_SUCCESS (vrc))
                vrc = loadStateFileExec (ssm, this, 0);
            else if (vrc == VERR_SSM_UNIT_NOT_FOUND)
                vrc = VINF_SUCCESS;
        }
        else
            vrc = VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

        SSMR3Close (ssm);
    }

    if (VBOX_FAILURE (vrc))
        rc = setError (VBOX_E_FILE_ERROR,
            tr ("The saved state file '%ls' is invalid (%Rrc). "
                "Discard the saved state and try again"),
                savedStateFile.raw(), vrc);

    mSavedStateDataLoaded = true;

    return rc;
}

/**
 *  Callback handler to save various console data to the state file,
 *  called when the user saves the VM state.
 *
 *  @param pvUser       pointer to Console
 *
 *  @note Locks the Console object for reading.
 */
//static
DECLCALLBACK(void)
Console::saveStateFileExec (PSSMHANDLE pSSM, void *pvUser)
{
    LogFlowFunc (("\n"));

    Console *that = static_cast <Console *> (pvUser);
    AssertReturnVoid (that);

    AutoCaller autoCaller (that);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoReadLock alock (that);

    int vrc = SSMR3PutU32 (pSSM, (uint32_t)that->mSharedFolders.size());
    AssertRC (vrc);

    for (SharedFolderMap::const_iterator it = that->mSharedFolders.begin();
         it != that->mSharedFolders.end();
         ++ it)
    {
        ComObjPtr <SharedFolder> folder = (*it).second;
        // don't lock the folder because methods we access are const

        Utf8Str name = folder->name();
        vrc = SSMR3PutU32 (pSSM, (uint32_t)name.length() + 1 /* term. 0 */);
        AssertRC (vrc);
        vrc = SSMR3PutStrZ (pSSM, name);
        AssertRC (vrc);

        Utf8Str hostPath = folder->hostPath();
        vrc = SSMR3PutU32 (pSSM, (uint32_t)hostPath.length() + 1 /* term. 0 */);
        AssertRC (vrc);
        vrc = SSMR3PutStrZ (pSSM, hostPath);
        AssertRC (vrc);

        vrc = SSMR3PutBool (pSSM, !!folder->writable());
        AssertRC (vrc);
    }

    return;
}

/**
 *  Callback handler to load various console data from the state file.
 *  When \a u32Version is 0, this method is called from #loadDataFromSavedState,
 *  otherwise it is called when the VM is being restored from the saved state.
 *
 *  @param pvUser       pointer to Console
 *  @param u32Version   Console unit version.
 *                      When not 0, should match sSSMConsoleVer.
 *
 *  @note Locks the Console object for writing.
 */
//static
DECLCALLBACK(int)
Console::loadStateFileExec (PSSMHANDLE pSSM, void *pvUser, uint32_t u32Version)
{
    LogFlowFunc (("\n"));

    if (u32Version != 0 && SSM_VERSION_MAJOR_CHANGED(u32Version, sSSMConsoleVer))
        return VERR_VERSION_MISMATCH;

    if (u32Version != 0)
    {
        /* currently, nothing to do when we've been called from VMR3Load */
        return VINF_SUCCESS;
    }

    Console *that = static_cast <Console *> (pvUser);
    AssertReturn (that, VERR_INVALID_PARAMETER);

    AutoCaller autoCaller (that);
    AssertComRCReturn (autoCaller.rc(), VERR_ACCESS_DENIED);

    AutoWriteLock alock (that);

    AssertReturn (that->mSharedFolders.size() == 0, VERR_INTERNAL_ERROR);

    uint32_t size = 0;
    int vrc = SSMR3GetU32 (pSSM, &size);
    AssertRCReturn (vrc, vrc);

    for (uint32_t i = 0; i < size; ++ i)
    {
        Bstr name;
        Bstr hostPath;
        bool writable = true;

        uint32_t szBuf = 0;
        char *buf = NULL;

        vrc = SSMR3GetU32 (pSSM, &szBuf);
        AssertRCReturn (vrc, vrc);
        buf = new char [szBuf];
        vrc = SSMR3GetStrZ (pSSM, buf, szBuf);
        AssertRC (vrc);
        name = buf;
        delete[] buf;

        vrc = SSMR3GetU32 (pSSM, &szBuf);
        AssertRCReturn (vrc, vrc);
        buf = new char [szBuf];
        vrc = SSMR3GetStrZ (pSSM, buf, szBuf);
        AssertRC (vrc);
        hostPath = buf;
        delete[] buf;

        if (u32Version > 0x00010000)
            SSMR3GetBool (pSSM, &writable);

        ComObjPtr <SharedFolder> sharedFolder;
        sharedFolder.createObject();
        HRESULT rc = sharedFolder->init (that, name, hostPath, writable);
        AssertComRCReturn (rc, VERR_INTERNAL_ERROR);

        that->mSharedFolders.insert (std::make_pair (name, sharedFolder));
    }

    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_GUEST_PROPS
// static
DECLCALLBACK(int)
Console::doGuestPropNotification (void *pvExtension, uint32_t,
                                  void *pvParms, uint32_t cbParms)
{
    using namespace guestProp;

    LogFlowFunc (("pvExtension=%p, pvParms=%p, cbParms=%u\n", pvExtension, pvParms, cbParms));
    int rc = VINF_SUCCESS;
    /* No locking, as this is purely a notification which does not make any
     * changes to the object state. */
    PHOSTCALLBACKDATA pCBData = reinterpret_cast<PHOSTCALLBACKDATA>(pvParms);
    AssertReturn(sizeof(HOSTCALLBACKDATA) == cbParms, VERR_INVALID_PARAMETER);
    AssertReturn(HOSTCALLBACKMAGIC == pCBData->u32Magic, VERR_INVALID_PARAMETER);
    ComObjPtr <Console> pConsole = reinterpret_cast <Console *> (pvExtension);
    LogFlowFunc (("pCBData->pcszName=%s, pCBData->pcszValue=%s, pCBData->pcszFlags=%s\n", pCBData->pcszName, pCBData->pcszValue, pCBData->pcszFlags));
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
        if (FAILED (hrc))
        {
            LogFunc (("pConsole->mControl->PushGuestProperty failed, hrc=0x%x\n", hrc));
            LogFunc (("pCBData->pcszName=%s\n", pCBData->pcszName));
            LogFunc (("pCBData->pcszValue=%s\n", pCBData->pcszValue));
            LogFunc (("pCBData->pcszFlags=%s\n", pCBData->pcszFlags));
            rc = VERR_UNRESOLVED_ERROR;  /** @todo translate error code */
        }
    }
    LogFlowFunc (("rc=%Rrc\n", rc));
    return rc;
}

HRESULT Console::doEnumerateGuestProperties (CBSTR aPatterns,
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
    parm[0].u.pointer.size = utf8Patterns.length() + 1;

    /*
     * Now things get slightly complicated.  Due to a race with the guest adding
     * properties, there is no good way to know how much to enlarge a buffer for
     * the service to enumerate into.  We choose a decent starting size and loop a
     * few times, each time retrying with the size suggested by the service plus
     * one Kb.
     */
    size_t cchBuf = 4096;
    Utf8Str Utf8Buf;
    int vrc = VERR_BUFFER_OVERFLOW;
    for (unsigned i = 0; i < 10 && (VERR_BUFFER_OVERFLOW == vrc); ++i)
    {
        Utf8Buf.alloc(cchBuf + 1024);
        if (Utf8Buf.isNull())
            return E_OUTOFMEMORY;
        parm[1].type = VBOX_HGCM_SVC_PARM_PTR;
        parm[1].u.pointer.addr = Utf8Buf.mutableRaw();
        parm[1].u.pointer.size = cchBuf + 1024;
        vrc = mVMMDev->hgcmHostCall ("VBoxGuestPropSvc", ENUM_PROPS_HOST, 3,
                                     &parm[0]);
        if (parm[2].type != VBOX_HGCM_SVC_PARM_32BIT)
            return setError (E_FAIL, tr ("Internal application error"));
        cchBuf = parm[2].u.uint32;
    }
    if (VERR_BUFFER_OVERFLOW == vrc)
        return setError (E_UNEXPECTED, tr ("Temporary failure due to guest activity, please retry"));

    /*
     * Finally we have to unpack the data returned by the service into the safe
     * arrays supplied by the caller.  We start by counting the number of entries.
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
    com::SafeArray <BSTR> names(cEntries);
    com::SafeArray <BSTR> values(cEntries);
    com::SafeArray <ULONG64> timestamps(cEntries);
    com::SafeArray <BSTR> flags(cEntries);
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
    names.detachTo(ComSafeArrayOutArg (aNames));
    values.detachTo(ComSafeArrayOutArg (aValues));
    timestamps.detachTo(ComSafeArrayOutArg (aTimestamps));
    flags.detachTo(ComSafeArrayOutArg (aFlags));
    return S_OK;
}
#endif


// IConsole properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Console::COMGETTER(Machine) (IMachine **aMachine)
{
    CheckComArgOutPointerValid(aMachine);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mMachine is constant during life time, no need to lock */
    mMachine.queryInterfaceTo (aMachine);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(State) (MachineState_T *aMachineState)
{
    CheckComArgOutPointerValid(aMachineState);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    /* we return our local state (since it's always the same as on the server) */
    *aMachineState = mMachineState;

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(Guest) (IGuest **aGuest)
{
    CheckComArgOutPointerValid(aGuest);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mGuest is constant during life time, no need to lock */
    mGuest.queryInterfaceTo (aGuest);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(Keyboard) (IKeyboard **aKeyboard)
{
    CheckComArgOutPointerValid(aKeyboard);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mKeyboard is constant during life time, no need to lock */
    mKeyboard.queryInterfaceTo (aKeyboard);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(Mouse) (IMouse **aMouse)
{
    CheckComArgOutPointerValid(aMouse);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mMouse is constant during life time, no need to lock */
    mMouse.queryInterfaceTo (aMouse);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(Display) (IDisplay **aDisplay)
{
    CheckComArgOutPointerValid(aDisplay);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mDisplay is constant during life time, no need to lock */
    mDisplay.queryInterfaceTo (aDisplay);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(Debugger) (IMachineDebugger **aDebugger)
{
    CheckComArgOutPointerValid(aDebugger);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* we need a write lock because of the lazy mDebugger initialization*/
    AutoWriteLock alock (this);

    /* check if we have to create the debugger object */
    if (!mDebugger)
    {
        unconst (mDebugger).createObject();
        mDebugger->init (this);
    }

    mDebugger.queryInterfaceTo (aDebugger);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(USBDevices) (ComSafeArrayOut (IUSBDevice *, aUSBDevices))
{
    CheckComArgOutSafeArrayPointerValid(aUSBDevices);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    SafeIfaceArray <IUSBDevice> collection (mUSBDevices);
    collection.detachTo (ComSafeArrayOutArg(aUSBDevices));

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(RemoteUSBDevices) (ComSafeArrayOut (IHostUSBDevice *, aRemoteUSBDevices))
{
    CheckComArgOutSafeArrayPointerValid(aRemoteUSBDevices);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    SafeIfaceArray <IHostUSBDevice> collection (mRemoteUSBDevices);
    collection.detachTo (ComSafeArrayOutArg(aRemoteUSBDevices));

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(RemoteDisplayInfo) (IRemoteDisplayInfo **aRemoteDisplayInfo)
{
    CheckComArgOutPointerValid(aRemoteDisplayInfo);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mDisplay is constant during life time, no need to lock */
    mRemoteDisplayInfo.queryInterfaceTo (aRemoteDisplayInfo);

    return S_OK;
}

STDMETHODIMP
Console::COMGETTER(SharedFolders) (ComSafeArrayOut (ISharedFolder *, aSharedFolders))
{
    CheckComArgOutSafeArrayPointerValid(aSharedFolders);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* loadDataFromSavedState() needs a write lock */
    AutoWriteLock alock (this);

    /* Read console data stored in the saved state file (if not yet done) */
    HRESULT rc = loadDataFromSavedState();
    CheckComRCReturnRC (rc);

    SafeIfaceArray <ISharedFolder> sf (mSharedFolders);
    sf.detachTo (ComSafeArrayOutArg(aSharedFolders));

    return S_OK;
}


// IConsole methods
/////////////////////////////////////////////////////////////////////////////


STDMETHODIMP Console::PowerUp (IProgress **aProgress)
{
    return powerUp (aProgress, false /* aPaused */);
}

STDMETHODIMP Console::PowerUpPaused (IProgress **aProgress)
{
    return powerUp (aProgress, true /* aPaused */);
}

STDMETHODIMP Console::PowerDown()
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("mMachineState=%d\n", mMachineState));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (!Global::IsActive (mMachineState))
    {
        /* extra nice error message for a common case */
        if (mMachineState == MachineState_Saved)
            return setError (VBOX_E_INVALID_VM_STATE,
                tr ("Cannot power down a saved virtual machine"));
        else if (mMachineState == MachineState_Stopping)
            return setError (VBOX_E_INVALID_VM_STATE,
                tr ("Virtual machine is being powered down"));
        else
            return setError(VBOX_E_INVALID_VM_STATE,
                tr ("Invalid machine state: %d (must be Running, Paused "
                    "or Stuck)"),
                mMachineState);
    }

    LogFlowThisFunc (("Sending SHUTDOWN request...\n"));

    HRESULT rc = powerDown();

    LogFlowThisFunc (("mMachineState=%d, rc=%08X\n", mMachineState, rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::PowerDownAsync (IProgress **aProgress)
{
    if (aProgress == NULL)
        return E_POINTER;

    LogFlowThisFuncEnter();
    LogFlowThisFunc (("mMachineState=%d\n", mMachineState));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (!Global::IsActive (mMachineState))
    {
        /* extra nice error message for a common case */
        if (mMachineState == MachineState_Saved)
            return setError (VBOX_E_INVALID_VM_STATE,
                tr ("Cannot power down a saved virtual machine"));
        else if (mMachineState == MachineState_Stopping)
            return setError (VBOX_E_INVALID_VM_STATE,
                tr ("Virtual machine is being powered down."));
        else
            return setError(VBOX_E_INVALID_VM_STATE,
                tr ("Invalid machine state: %d (must be Running, Paused "
                    "or Stuck)"),
                mMachineState);
    }

    LogFlowThisFunc (("Initiating SHUTDOWN request...\n"));

    /* create an IProgress object to track progress of this operation */
    ComObjPtr <Progress> progress;
    progress.createObject();
    progress->init (static_cast <IConsole *> (this),
                    Bstr (tr ("Stopping virtual machine")),
                    FALSE /* aCancelable */);

    /* setup task object and thread to carry out the operation asynchronously */
    std::auto_ptr <VMProgressTask> task (
        new VMProgressTask (this, progress, true /* aUsesVMPtr */));
    AssertReturn (task->isOk(), E_FAIL);

    int vrc = RTThreadCreate (NULL, Console::powerDownThread,
                              (void *) task.get(), 0,
                              RTTHREADTYPE_MAIN_WORKER, 0,
                              "VMPowerDown");
    ComAssertMsgRCRet (vrc,
         ("Could not create VMPowerDown thread (%Rrc)", vrc), E_FAIL);

    /* task is now owned by powerDownThread(), so release it */
    task.release();

    /* go to Stopping state to forbid state-dependant operations */
    setMachineState (MachineState_Stopping);

    /* pass the progress to the caller */
    progress.queryInterfaceTo (aProgress);

    LogFlowThisFuncLeave();

    return S_OK;
}

STDMETHODIMP Console::Reset()
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("mMachineState=%d\n", mMachineState));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (mMachineState != MachineState_Running)
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Invalid machine state: %d)"), mMachineState);

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

    int vrc = VMR3Reset (mpVM);

    HRESULT rc = VBOX_SUCCESS (vrc) ? S_OK :
        setError (VBOX_E_VM_ERROR, tr ("Could not reset the machine (%Rrc)"), vrc);

    LogFlowThisFunc (("mMachineState=%d, rc=%08X\n", mMachineState, rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::Pause()
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (mMachineState != MachineState_Running)
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Invalid machine state: %d)"), mMachineState);

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    LogFlowThisFunc (("Sending PAUSE request...\n"));

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

    int vrc = VMR3Suspend (mpVM);

    HRESULT rc = VBOX_SUCCESS (vrc) ? S_OK :
        setError (VBOX_E_VM_ERROR,
            tr ("Could not suspend the machine execution (%Rrc)"), vrc);

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::Resume()
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (mMachineState != MachineState_Paused)
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Cannot resume the machine as it is not paused "
                "(machine state: %d)"), mMachineState);

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    LogFlowThisFunc (("Sending RESUME request...\n"));

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

    int vrc;
    if (VMR3GetState(mpVM) == VMSTATE_CREATED)
        vrc = VMR3PowerOn (mpVM); /* (PowerUpPaused) */
    else
        vrc = VMR3Resume (mpVM);

    HRESULT rc = VBOX_SUCCESS (vrc) ? S_OK :
        setError (VBOX_E_VM_ERROR,
            tr ("Could not resume the machine execution (%Rrc)"), vrc);

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::PowerButton()
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (mMachineState != MachineState_Running)
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Invalid machine state: %d)"), mMachineState);

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    PPDMIBASE pBase;
    int vrc = PDMR3QueryDeviceLun (mpVM, "acpi", 0, 0, &pBase);
    if (VBOX_SUCCESS (vrc))
    {
        Assert (pBase);
        PPDMIACPIPORT pPort =
            (PPDMIACPIPORT) pBase->pfnQueryInterface(pBase, PDMINTERFACE_ACPI_PORT);
        vrc = pPort ? pPort->pfnPowerButtonPress(pPort) : VERR_INVALID_POINTER;
    }

    HRESULT rc = VBOX_SUCCESS (vrc) ? S_OK :
        setError (VBOX_E_PDM_ERROR,
            tr ("Controlled power off failed (%Rrc)"), vrc);

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::GetPowerButtonHandled(BOOL *aHandled)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aHandled);

    *aHandled = FALSE;

    AutoCaller autoCaller (this);

    AutoWriteLock alock (this);

    if (mMachineState != MachineState_Running)
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Invalid machine state: %d)"), mMachineState);

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    PPDMIBASE pBase;
    int vrc = PDMR3QueryDeviceLun (mpVM, "acpi", 0, 0, &pBase);
    bool handled = false;
    if (VBOX_SUCCESS (vrc))
    {
        Assert (pBase);
        PPDMIACPIPORT pPort =
            (PPDMIACPIPORT) pBase->pfnQueryInterface(pBase, PDMINTERFACE_ACPI_PORT);
        vrc = pPort ? pPort->pfnGetPowerButtonHandled(pPort, &handled) : VERR_INVALID_POINTER;
    }

    HRESULT rc = VBOX_SUCCESS (vrc) ? S_OK :
        setError (VBOX_E_PDM_ERROR,
            tr ("Checking if the ACPI Power Button event was handled by the "
                "guest OS failed (%Rrc)"), vrc);

    *aHandled = handled;

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::GetGuestEnteredACPIMode(BOOL *aEntered)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aEntered);

    *aEntered = FALSE;

    AutoCaller autoCaller (this);

    AutoWriteLock alock (this);

    if (mMachineState != MachineState_Running)
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Invalid machine state: %d)"), mMachineState);

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    PPDMIBASE pBase;
    int vrc = PDMR3QueryDeviceLun (mpVM, "acpi", 0, 0, &pBase);
    bool entered = false;
    if (RT_SUCCESS (vrc))
    {
        Assert (pBase);
        PPDMIACPIPORT pPort =
            (PPDMIACPIPORT) pBase->pfnQueryInterface(pBase, PDMINTERFACE_ACPI_PORT);
        vrc = pPort ? pPort->pfnGetGuestEnteredACPIMode(pPort, &entered) : VERR_INVALID_POINTER;
    }

    *aEntered = RT_SUCCESS (vrc) ? entered : false;

    LogFlowThisFuncLeave();
    return S_OK;
}

STDMETHODIMP Console::SleepButton()
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (mMachineState != MachineState_Running)
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Invalid machine state: %d)"), mMachineState);

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    PPDMIBASE pBase;
    int vrc = PDMR3QueryDeviceLun (mpVM, "acpi", 0, 0, &pBase);
    if (VBOX_SUCCESS (vrc))
    {
        Assert (pBase);
        PPDMIACPIPORT pPort =
            (PPDMIACPIPORT) pBase->pfnQueryInterface(pBase, PDMINTERFACE_ACPI_PORT);
        vrc = pPort ? pPort->pfnSleepButtonPress(pPort) : VERR_INVALID_POINTER;
    }

    HRESULT rc = VBOX_SUCCESS (vrc) ? S_OK :
        setError (VBOX_E_PDM_ERROR,
            tr ("Sending sleep button event failed (%Rrc)"), vrc);

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::SaveState (IProgress **aProgress)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("mMachineState=%d\n", mMachineState));

    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (mMachineState != MachineState_Running &&
        mMachineState != MachineState_Paused)
    {
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Cannot save the execution state as the machine "
                "is not running or paused (machine state: %d)"), mMachineState);
    }

    /* memorize the current machine state */
    MachineState_T lastMachineState = mMachineState;

    if (mMachineState == MachineState_Running)
    {
        HRESULT rc = Pause();
        CheckComRCReturnRC (rc);
    }

    HRESULT rc = S_OK;

    /* create a progress object to track operation completion */
    ComObjPtr <Progress> progress;
    progress.createObject();
    progress->init (static_cast <IConsole *> (this),
                    Bstr (tr ("Saving the execution state of the virtual machine")),
                    FALSE /* aCancelable */);

    bool beganSavingState = false;
    bool taskCreationFailed = false;

    do
    {
        /* create a task object early to ensure mpVM protection is successful */
        std::auto_ptr <VMSaveTask> task (new VMSaveTask (this, progress));
        rc = task->rc();
        /*
         *  If we fail here it means a PowerDown() call happened on another
         *  thread while we were doing Pause() (which leaves the Console lock).
         *  We assign PowerDown() a higher precedence than SaveState(),
         *  therefore just return the error to the caller.
         */
        if (FAILED (rc))
        {
            taskCreationFailed = true;
            break;
        }

        Bstr stateFilePath;

        /*
         *  request a saved state file path from the server
         *  (this will set the machine state to Saving on the server to block
         *  others from accessing this machine)
         */
        rc = mControl->BeginSavingState (progress, stateFilePath.asOutParam());
        CheckComRCBreakRC (rc);

        beganSavingState = true;

        /* sync the state with the server */
        setMachineStateLocally (MachineState_Saving);

        /* ensure the directory for the saved state file exists */
        {
            Utf8Str dir = stateFilePath;
            RTPathStripFilename (dir.mutableRaw());
            if (!RTDirExists (dir))
            {
                int vrc = RTDirCreateFullPath (dir, 0777);
                if (VBOX_FAILURE (vrc))
                {
                    rc = setError (VBOX_E_FILE_ERROR,
                        tr ("Could not create a directory '%s' to save the state to (%Rrc)"),
                        dir.raw(), vrc);
                    break;
                }
            }
        }

        /* setup task object and thread to carry out the operation asynchronously */
        task->mIsSnapshot = false;
        task->mSavedStateFile = stateFilePath;
        /* set the state the operation thread will restore when it is finished */
        task->mLastMachineState = lastMachineState;

        /* create a thread to wait until the VM state is saved */
        int vrc = RTThreadCreate (NULL, Console::saveStateThread, (void *) task.get(),
                                  0, RTTHREADTYPE_MAIN_WORKER, 0, "VMSave");

        ComAssertMsgRCBreak (vrc, ("Could not create VMSave thread (%Rrc)", vrc),
                             rc = E_FAIL);

        /* task is now owned by saveStateThread(), so release it */
        task.release();

        /* return the progress to the caller */
        progress.queryInterfaceTo (aProgress);
    }
    while (0);

    if (FAILED (rc) && !taskCreationFailed)
    {
        /* preserve existing error info */
        ErrorInfoKeeper eik;

        if (beganSavingState)
        {
            /*
             *  cancel the requested save state procedure.
             *  This will reset the machine state to the state it had right
             *  before calling mControl->BeginSavingState().
             */
            mControl->EndSavingState (FALSE);
        }

        if (lastMachineState == MachineState_Running)
        {
            /* restore the paused state if appropriate */
            setMachineStateLocally (MachineState_Paused);
            /* restore the running state if appropriate */
            Resume();
        }
        else
            setMachineStateLocally (lastMachineState);
    }

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::AdoptSavedState (IN_BSTR aSavedStateFile)
{
    CheckComArgNotNull(aSavedStateFile);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (mMachineState != MachineState_PoweredOff &&
        mMachineState != MachineState_Aborted)
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Cannot adopt the saved machine state as the machine is "
                "not in Powered Off or Aborted state (machine state: %d)"),
            mMachineState);

    return mControl->AdoptSavedState (aSavedStateFile);
}

STDMETHODIMP Console::DiscardSavedState()
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (mMachineState != MachineState_Saved)
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Cannot discard the machine state as the machine is "
                "not in the saved state (machine state: %d)"),
            mMachineState);

    /*
     *  Saved -> PoweredOff transition will be detected in the SessionMachine
     *  and properly handled.
     */
    setMachineState (MachineState_PoweredOff);

    return S_OK;
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

STDMETHODIMP Console::GetDeviceActivity (DeviceType_T aDeviceType,
                                         DeviceActivity_T *aDeviceActivity)
{
    CheckComArgNotNull(aDeviceActivity);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /*
     *  Note: we don't lock the console object here because
     *  readAndClearLed() should be thread safe.
     */

    /* Get LED array to read */
    PDMLEDCORE  SumLed = {0};
    switch (aDeviceType)
    {
        case DeviceType_Floppy:
        {
            for (unsigned i = 0; i < RT_ELEMENTS(mapFDLeds); i++)
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
            for (unsigned i = 0; i < RT_ELEMENTS(mapSATALeds); i++)
                SumLed.u32 |= readAndClearLed(mapSATALeds[i]);
            break;
        }

        case DeviceType_Network:
        {
            for (unsigned i = 0; i < RT_ELEMENTS(mapNetworkLeds); i++)
                SumLed.u32 |= readAndClearLed(mapNetworkLeds[i]);
            break;
        }

        case DeviceType_USB:
        {
            for (unsigned i = 0; i < RT_ELEMENTS(mapUSBLed); i++)
                SumLed.u32 |= readAndClearLed(mapUSBLed[i]);
            break;
        }

        case DeviceType_SharedFolder:
        {
            SumLed.u32 |= readAndClearLed(mapSharedFolderLed);
            break;
        }

        default:
            return setError (E_INVALIDARG,
                tr ("Invalid device type: %d"), aDeviceType);
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

STDMETHODIMP Console::AttachUSBDevice (IN_GUID aId)
{
#ifdef VBOX_WITH_USB
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (mMachineState != MachineState_Running &&
        mMachineState != MachineState_Paused)
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Cannot attach a USB device to the machine which is not "
                "running or paused (machine state: %d)"), mMachineState);

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    /* Don't proceed unless we've found the usb controller. */
    PPDMIBASE pBase = NULL;
    int vrc = PDMR3QueryLun (mpVM, "usb-ohci", 0, 0, &pBase);
    if (VBOX_FAILURE (vrc))
        return setError (VBOX_E_PDM_ERROR,
            tr ("The virtual machine does not have a USB controller"));

    /* leave the lock because the USB Proxy service may call us back
     * (via onUSBDeviceAttach()) */
    alock.leave();

    /* Request the device capture */
    HRESULT rc = mControl->CaptureUSBDevice (aId);
    CheckComRCReturnRC (rc);

    return rc;

#else   /* !VBOX_WITH_USB */
    return setError (VBOX_E_PDM_ERROR,
        tr ("The virtual machine does not have a USB controller"));
#endif  /* !VBOX_WITH_USB */
}

STDMETHODIMP Console::DetachUSBDevice (IN_GUID aId, IUSBDevice **aDevice)
{
#ifdef VBOX_WITH_USB
    CheckComArgOutPointerValid(aDevice);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /* Find it. */
    ComObjPtr <OUSBDevice> device;
    USBDeviceList::iterator it = mUSBDevices.begin();
    while (it != mUSBDevices.end())
    {
        if ((*it)->id() == aId)
        {
            device = *it;
            break;
        }
        ++ it;
    }

    if (!device)
        return setError (E_INVALIDARG,
            tr ("USB device with UUID {%RTuuid} is not attached to this machine"),
            Guid (aId).raw());

    /*
     * Inform the USB device and USB proxy about what's cooking.
     */
    alock.leave();
    HRESULT rc2 = mControl->DetachUSBDevice (aId, false /* aDone */);
    if (FAILED (rc2))
        return rc2;
    alock.enter();

    /* Request the PDM to detach the USB device. */
    HRESULT rc = detachUSBDevice (it);

    if (SUCCEEDED (rc))
    {
        /* leave the lock since we don't need it any more (note though that
         * the USB Proxy service must not call us back here) */
        alock.leave();

        /* Request the device release. Even if it fails, the device will
         * remain as held by proxy, which is OK for us (the VM process). */
        rc = mControl->DetachUSBDevice (aId, true /* aDone */);
    }

    return rc;


#else   /* !VBOX_WITH_USB */
    return setError (VBOX_E_PDM_ERROR,
        tr ("The virtual machine does not have a USB controller"));
#endif  /* !VBOX_WITH_USB */
}

STDMETHODIMP Console::FindUSBDeviceByAddress(IN_BSTR aAddress, IUSBDevice **aDevice)
{
#ifdef VBOX_WITH_USB
    CheckComArgNotNull(aAddress);
    CheckComArgOutPointerValid(aDevice);

    *aDevice = NULL;

    SafeIfaceArray <IUSBDevice> devsvec;
    HRESULT rc = COMGETTER(USBDevices) (ComSafeArrayAsOutParam(devsvec));
    CheckComRCReturnRC (rc);

    for (size_t i = 0; i < devsvec.size(); ++i)
    {
        Bstr address;
        rc = devsvec[i]->COMGETTER(Address) (address.asOutParam());
        CheckComRCReturnRC (rc);
        if (address == aAddress)
        {
            ComObjPtr<OUSBDevice> found;
            found.createObject();
            found->init (devsvec[i]);
            return found.queryInterfaceTo (aDevice);
        }
    }

    return setErrorNoLog (VBOX_E_OBJECT_NOT_FOUND, tr (
        "Could not find a USB device with address '%ls'"),
        aAddress);

#else   /* !VBOX_WITH_USB */
    return E_NOTIMPL;
#endif  /* !VBOX_WITH_USB */
}

STDMETHODIMP Console::FindUSBDeviceById(IN_GUID aId, IUSBDevice **aDevice)
{
#ifdef VBOX_WITH_USB
    CheckComArgExpr(aId, Guid (aId).isEmpty() == false);
    CheckComArgOutPointerValid(aDevice);

    *aDevice = NULL;

    SafeIfaceArray <IUSBDevice> devsvec;
    HRESULT rc = COMGETTER(USBDevices) (ComSafeArrayAsOutParam(devsvec));
    CheckComRCReturnRC (rc);

    for (size_t i = 0; i < devsvec.size(); ++i)
    {
        Guid id;
        rc = devsvec[i]->COMGETTER(Id) (id.asOutParam());
        CheckComRCReturnRC (rc);
        if (id == aId)
        {
            ComObjPtr<OUSBDevice> found;
            found.createObject();
            found->init(devsvec[i]);
            return found.queryInterfaceTo (aDevice);
        }
    }

    return setErrorNoLog (VBOX_E_OBJECT_NOT_FOUND, tr (
        "Could not find a USB device with uuid {%RTuuid}"),
        Guid (aId).raw());

#else   /* !VBOX_WITH_USB */
    return E_NOTIMPL;
#endif  /* !VBOX_WITH_USB */
}

STDMETHODIMP
Console::CreateSharedFolder (IN_BSTR aName, IN_BSTR aHostPath, BOOL aWritable)
{
    CheckComArgNotNull(aName);
    CheckComArgNotNull(aHostPath);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /// @todo see @todo in AttachUSBDevice() about the Paused state
    if (mMachineState == MachineState_Saved)
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Cannot create a transient shared folder on the "
                "machine in the saved state"));
    if (mMachineState > MachineState_Paused)
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Cannot create a transient shared folder on the "
                "machine while it is changing the state (machine state: %d)"),
            mMachineState);

    ComObjPtr <SharedFolder> sharedFolder;
    HRESULT rc = findSharedFolder (aName, sharedFolder, false /* aSetError */);
    if (SUCCEEDED (rc))
        return setError (VBOX_E_FILE_ERROR,
            tr ("Shared folder named '%ls' already exists"), aName);

    sharedFolder.createObject();
    rc = sharedFolder->init (this, aName, aHostPath, aWritable);
    CheckComRCReturnRC (rc);

    /* protect mpVM (if not NULL) */
    AutoVMCallerQuietWeak autoVMCaller (this);

    if (mpVM && autoVMCaller.isOk() && mVMMDev->isShFlActive())
    {
        /* If the VM is online and supports shared folders, share this folder
         * under the specified name. */

        /* first, remove the machine or the global folder if there is any */
        SharedFolderDataMap::const_iterator it;
        if (findOtherSharedFolder (aName, it))
        {
            rc = removeSharedFolder (aName);
            CheckComRCReturnRC (rc);
        }

        /* second, create the given folder */
        rc = createSharedFolder (aName, SharedFolderData (aHostPath, aWritable));
        CheckComRCReturnRC (rc);
    }

    mSharedFolders.insert (std::make_pair (aName, sharedFolder));

    /* notify console callbacks after the folder is added to the list */
    {
        CallbackList::iterator it = mCallbacks.begin();
        while (it != mCallbacks.end())
            (*it++)->OnSharedFolderChange (Scope_Session);
    }

    return rc;
}

STDMETHODIMP Console::RemoveSharedFolder (IN_BSTR aName)
{
    CheckComArgNotNull(aName);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /// @todo see @todo in AttachUSBDevice() about the Paused state
    if (mMachineState == MachineState_Saved)
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Cannot remove a transient shared folder from the "
                "machine in the saved state"));
    if (mMachineState > MachineState_Paused)
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Cannot remove a transient shared folder from the "
                "machine while it is changing the state (machine state: %d)"),
            mMachineState);

    ComObjPtr <SharedFolder> sharedFolder;
    HRESULT rc = findSharedFolder (aName, sharedFolder, true /* aSetError */);
    CheckComRCReturnRC (rc);

    /* protect mpVM (if not NULL) */
    AutoVMCallerQuietWeak autoVMCaller (this);

    if (mpVM && autoVMCaller.isOk() && mVMMDev->isShFlActive())
    {
        /* if the VM is online and supports shared folders, UNshare this
         * folder. */

        /* first, remove the given folder */
        rc = removeSharedFolder (aName);
        CheckComRCReturnRC (rc);

        /* first, remove the machine or the global folder if there is any */
        SharedFolderDataMap::const_iterator it;
        if (findOtherSharedFolder (aName, it))
        {
            rc = createSharedFolder (aName, it->second);
            /* don't check rc here because we need to remove the console
             * folder from the collection even on failure */
        }
    }

    mSharedFolders.erase (aName);

    /* notify console callbacks after the folder is removed to the list */
    {
        CallbackList::iterator it = mCallbacks.begin();
        while (it != mCallbacks.end())
            (*it++)->OnSharedFolderChange (Scope_Session);
    }

    return rc;
}

STDMETHODIMP Console::TakeSnapshot (IN_BSTR aName, IN_BSTR aDescription,
                                    IProgress **aProgress)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aName='%ls' mMachineState=%08X\n", aName, mMachineState));

    CheckComArgNotNull(aName);
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (Global::IsTransient (mMachineState))
    {
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Cannot take a snapshot of the machine "
                "while it is changing the state (machine state: %d)"),
            mMachineState);
    }

    /* memorize the current machine state */
    MachineState_T lastMachineState = mMachineState;

    if (mMachineState == MachineState_Running)
    {
        HRESULT rc = Pause();
        CheckComRCReturnRC (rc);
    }

    HRESULT rc = S_OK;

    bool takingSnapshotOnline = mMachineState == MachineState_Paused;

    /*
     *  create a descriptionless VM-side progress object
     *  (only when creating a snapshot online)
     */
    ComObjPtr <Progress> saveProgress;
    if (takingSnapshotOnline)
    {
        saveProgress.createObject();
        rc = saveProgress->init (FALSE, 1, Bstr (tr ("Saving the execution state")));
        AssertComRCReturn (rc, rc);
    }

    bool beganTakingSnapshot = false;
    bool taskCreationFailed = false;

    do
    {
        /* create a task object early to ensure mpVM protection is successful */
        std::auto_ptr <VMSaveTask> task;
        if (takingSnapshotOnline)
        {
            task.reset (new VMSaveTask (this, saveProgress));
            rc = task->rc();
            /*
             *  If we fail here it means a PowerDown() call happened on another
             *  thread while we were doing Pause() (which leaves the Console lock).
             *  We assign PowerDown() a higher precedence than TakeSnapshot(),
             *  therefore just return the error to the caller.
             */
            if (FAILED (rc))
            {
                taskCreationFailed = true;
                break;
            }
        }

        Bstr stateFilePath;
        ComPtr <IProgress> serverProgress;

        /*
         *  request taking a new snapshot object on the server
         *  (this will set the machine state to Saving on the server to block
         *  others from accessing this machine)
         */
        rc = mControl->BeginTakingSnapshot (this, aName, aDescription,
                                            saveProgress, stateFilePath.asOutParam(),
                                            serverProgress.asOutParam());
        if (FAILED (rc))
            break;

        /*
         *  state file is non-null only when the VM is paused
         *  (i.e. creating a snapshot online)
         */
        ComAssertBreak (
            (!stateFilePath.isNull() && takingSnapshotOnline) ||
            (stateFilePath.isNull() && !takingSnapshotOnline),
            rc = E_FAIL);

        beganTakingSnapshot = true;

        /* sync the state with the server */
        setMachineStateLocally (MachineState_Saving);

        /*
         *  create a combined VM-side progress object and start the save task
         *  (only when creating a snapshot online)
         */
        ComObjPtr <CombinedProgress> combinedProgress;
        if (takingSnapshotOnline)
        {
            combinedProgress.createObject();
            rc = combinedProgress->init (static_cast <IConsole *> (this),
                                         Bstr (tr ("Taking snapshot of virtual machine")),
                                         serverProgress, saveProgress);
            AssertComRCBreakRC (rc);

            /* setup task object and thread to carry out the operation asynchronously */
            task->mIsSnapshot = true;
            task->mSavedStateFile = stateFilePath;
            task->mServerProgress = serverProgress;
            /* set the state the operation thread will restore when it is finished */
            task->mLastMachineState = lastMachineState;

            /* create a thread to wait until the VM state is saved */
            int vrc = RTThreadCreate (NULL, Console::saveStateThread, (void *) task.get(),
                                      0, RTTHREADTYPE_MAIN_WORKER, 0, "VMTakeSnap");

            ComAssertMsgRCBreak (vrc, ("Could not create VMTakeSnap thread (%Rrc)", vrc),
                                 rc = E_FAIL);

            /* task is now owned by saveStateThread(), so release it */
            task.release();
        }

        if (SUCCEEDED (rc))
        {
            /* return the correct progress to the caller */
            if (combinedProgress)
                combinedProgress.queryInterfaceTo (aProgress);
            else
                serverProgress.queryInterfaceTo (aProgress);
        }
    }
    while (0);

    if (FAILED (rc) && !taskCreationFailed)
    {
        /* preserve existing error info */
        ErrorInfoKeeper eik;

        if (beganTakingSnapshot && takingSnapshotOnline)
        {
            /*
             *  cancel the requested snapshot (only when creating a snapshot
             *  online, otherwise the server will cancel the snapshot itself).
             *  This will reset the machine state to the state it had right
             *  before calling mControl->BeginTakingSnapshot().
             */
            mControl->EndTakingSnapshot (FALSE);
        }

        if (lastMachineState == MachineState_Running)
        {
            /* restore the paused state if appropriate */
            setMachineStateLocally (MachineState_Paused);
            /* restore the running state if appropriate */
            Resume();
        }
        else
            setMachineStateLocally (lastMachineState);
    }

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::DiscardSnapshot (IN_GUID aId, IProgress **aProgress)
{
    CheckComArgExpr(aId, Guid (aId).isEmpty() == false);
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (Global::IsOnlineOrTransient (mMachineState))
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Cannot discard a snapshot of the running machine "
                "(machine state: %d)"),
            mMachineState);

    MachineState_T machineState = MachineState_Null;
    HRESULT rc = mControl->DiscardSnapshot (this, aId, &machineState, aProgress);
    CheckComRCReturnRC (rc);

    setMachineStateLocally (machineState);
    return S_OK;
}

STDMETHODIMP Console::DiscardCurrentState (IProgress **aProgress)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (Global::IsOnlineOrTransient (mMachineState))
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Cannot discard the current state of the running machine "
                "(nachine state: %d)"),
            mMachineState);

    MachineState_T machineState = MachineState_Null;
    HRESULT rc = mControl->DiscardCurrentState (this, &machineState, aProgress);
    CheckComRCReturnRC (rc);

    setMachineStateLocally (machineState);
    return S_OK;
}

STDMETHODIMP Console::DiscardCurrentSnapshotAndState (IProgress **aProgress)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (Global::IsOnlineOrTransient (mMachineState))
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Cannot discard the current snapshot and state of the "
                "running machine (machine state: %d)"),
            mMachineState);

    MachineState_T machineState = MachineState_Null;
    HRESULT rc =
        mControl->DiscardCurrentSnapshotAndState (this, &machineState, aProgress);
    CheckComRCReturnRC (rc);

    setMachineStateLocally (machineState);
    return S_OK;
}

STDMETHODIMP Console::RegisterCallback (IConsoleCallback *aCallback)
{
    CheckComArgNotNull(aCallback);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    mCallbacks.push_back (CallbackList::value_type (aCallback));

    /* Inform the callback about the current status (for example, the new
     * callback must know the current mouse capabilities and the pointer
     * shape in order to properly integrate the mouse pointer). */

    if (mCallbackData.mpsc.valid)
        aCallback->OnMousePointerShapeChange (mCallbackData.mpsc.visible,
                                              mCallbackData.mpsc.alpha,
                                              mCallbackData.mpsc.xHot,
                                              mCallbackData.mpsc.yHot,
                                              mCallbackData.mpsc.width,
                                              mCallbackData.mpsc.height,
                                              mCallbackData.mpsc.shape);
    if (mCallbackData.mcc.valid)
        aCallback->OnMouseCapabilityChange (mCallbackData.mcc.supportsAbsolute,
                                            mCallbackData.mcc.needsHostCursor);

    aCallback->OnAdditionsStateChange();

    if (mCallbackData.klc.valid)
        aCallback->OnKeyboardLedsChange (mCallbackData.klc.numLock,
                                         mCallbackData.klc.capsLock,
                                         mCallbackData.klc.scrollLock);

    /* Note: we don't call OnStateChange for new callbacks because the
     * machine state is a) not actually changed on callback registration
     * and b) can be always queried from Console. */

    return S_OK;
}

STDMETHODIMP Console::UnregisterCallback (IConsoleCallback *aCallback)
{
    CheckComArgNotNull(aCallback);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    CallbackList::iterator it;
    it = std::find (mCallbacks.begin(),
                    mCallbacks.end(),
                    CallbackList::value_type (aCallback));
    if (it == mCallbacks.end())
        return setError (E_INVALIDARG,
            tr ("The given callback handler is not registered"));

    mCallbacks.erase (it);
    return S_OK;
}

// Non-interface public methods
/////////////////////////////////////////////////////////////////////////////

/**
 * Called by IInternalSessionControl::OnDVDDriveChange().
 *
 * @note Locks this object for writing.
 */
HRESULT Console::onDVDDriveChange()
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    /* doDriveChange() needs a write lock */
    AutoWriteLock alock (this);

    /* Ignore callbacks when there's no VM around */
    if (!mpVM)
        return S_OK;

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    /* Get the current DVD state */
    HRESULT rc;
    DriveState_T eState;

    rc = mDVDDrive->COMGETTER (State) (&eState);
    ComAssertComRCRetRC (rc);

    /* Paranoia */
    if (    eState     == DriveState_NotMounted
        &&  meDVDState == DriveState_NotMounted)
    {
        LogFlowThisFunc (("Returns (NotMounted -> NotMounted)\n"));
        return S_OK;
    }

    /* Get the path string and other relevant properties */
    Bstr Path;
    bool fPassthrough = false;
    switch (eState)
    {
        case DriveState_ImageMounted:
        {
            ComPtr <IDVDImage> ImagePtr;
            rc = mDVDDrive->GetImage (ImagePtr.asOutParam());
            if (SUCCEEDED (rc))
                rc = ImagePtr->COMGETTER(Location) (Path.asOutParam());
            break;
        }

        case DriveState_HostDriveCaptured:
        {
            ComPtr <IHostDVDDrive> DrivePtr;
            BOOL enabled;
            rc = mDVDDrive->GetHostDrive (DrivePtr.asOutParam());
            if (SUCCEEDED (rc))
                rc = DrivePtr->COMGETTER (Name) (Path.asOutParam());
            if (SUCCEEDED (rc))
                rc = mDVDDrive->COMGETTER (Passthrough) (&enabled);
            if (SUCCEEDED (rc))
                fPassthrough = !!enabled;
            break;
        }

        case DriveState_NotMounted:
            break;

        default:
            AssertMsgFailed (("Invalid DriveState: %d\n", eState));
            rc = E_FAIL;
            break;
    }

    AssertComRC (rc);
    if (SUCCEEDED (rc))
    {
        rc = doDriveChange ("piix3ide", 0, 2, eState, &meDVDState,
                            Utf8Str (Path).raw(), fPassthrough);

        /* notify console callbacks on success */
        if (SUCCEEDED (rc))
        {
            CallbackList::iterator it = mCallbacks.begin();
            while (it != mCallbacks.end())
                (*it++)->OnDVDDriveChange();
        }
    }

    LogFlowThisFunc (("Returns %Rhrc (%#x)\n", rc, rc));
    LogFlowThisFuncLeave();
    return rc;
}


/**
 * Called by IInternalSessionControl::OnFloppyDriveChange().
 *
 * @note Locks this object for writing.
 */
HRESULT Console::onFloppyDriveChange()
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    /* doDriveChange() needs a write lock */
    AutoWriteLock alock (this);

    /* Ignore callbacks when there's no VM around */
    if (!mpVM)
        return S_OK;

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    /* Get the current floppy state */
    HRESULT rc;
    DriveState_T eState;

    /* If the floppy drive is disabled, we're not interested */
    BOOL fEnabled;
    rc = mFloppyDrive->COMGETTER (Enabled) (&fEnabled);
    ComAssertComRCRetRC (rc);

    if (!fEnabled)
        return S_OK;

    rc = mFloppyDrive->COMGETTER (State) (&eState);
    ComAssertComRCRetRC (rc);

    Log2 (("onFloppyDriveChange: eState=%d meFloppyState=%d\n", eState, meFloppyState));


    /* Paranoia */
    if (    eState     == DriveState_NotMounted
        &&  meFloppyState == DriveState_NotMounted)
    {
        LogFlowThisFunc (("Returns (NotMounted -> NotMounted)\n"));
        return S_OK;
    }

    /* Get the path string and other relevant properties */
    Bstr Path;
    switch (eState)
    {
        case DriveState_ImageMounted:
        {
            ComPtr<IFloppyImage> ImagePtr;
            rc = mFloppyDrive->GetImage (ImagePtr.asOutParam());
            if (SUCCEEDED (rc))
                rc = ImagePtr->COMGETTER(Location) (Path.asOutParam());
            break;
        }

        case DriveState_HostDriveCaptured:
        {
            ComPtr <IHostFloppyDrive> DrivePtr;
            rc = mFloppyDrive->GetHostDrive (DrivePtr.asOutParam());
            if (SUCCEEDED (rc))
                rc = DrivePtr->COMGETTER (Name) (Path.asOutParam());
            break;
        }

        case DriveState_NotMounted:
            break;

        default:
            AssertMsgFailed (("Invalid DriveState: %d\n", eState));
            rc = E_FAIL;
            break;
    }

    AssertComRC (rc);
    if (SUCCEEDED (rc))
    {
        rc = doDriveChange ("i82078", 0, 0, eState, &meFloppyState,
                            Utf8Str (Path).raw(), false);

        /* notify console callbacks on success */
        if (SUCCEEDED (rc))
        {
            CallbackList::iterator it = mCallbacks.begin();
            while (it != mCallbacks.end())
                (*it++)->OnFloppyDriveChange();
        }
    }

    LogFlowThisFunc (("Returns %Rhrc (%#x)\n", rc, rc));
    LogFlowThisFuncLeave();
    return rc;
}


/**
 * Process a floppy or dvd change.
 *
 * @returns COM status code.
 *
 * @param   pszDevice       The PDM device name.
 * @param   uInstance       The PDM device instance.
 * @param   uLun            The PDM LUN number of the drive.
 * @param   eState          The new state.
 * @param   peState         Pointer to the variable keeping the actual state of the drive.
 *                          This will be both read and updated to eState or other appropriate state.
 * @param   pszPath         The path to the media / drive which is now being mounted / captured.
 *                          If NULL no media or drive is attached and the LUN will be configured with
 *                          the default block driver with no media. This will also be the state if
 *                          mounting / capturing the specified media / drive fails.
 * @param   fPassthrough    Enables using passthrough mode of the host DVD drive if applicable.
 *
 * @note Locks this object for writing.
 */
HRESULT Console::doDriveChange (const char *pszDevice, unsigned uInstance, unsigned uLun, DriveState_T eState,
                                DriveState_T *peState, const char *pszPath, bool fPassthrough)
{
    LogFlowThisFunc (("pszDevice=%p:{%s} uInstance=%u uLun=%u eState=%d "
                      "peState=%p:{%d} pszPath=%p:{%s} fPassthrough=%d\n",
                      pszDevice, pszDevice, uInstance, uLun, eState,
                      peState, *peState, pszPath, pszPath, fPassthrough));

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    /* We will need to release the write lock before calling EMT */
    AutoWriteLock alock (this);

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    /*
     * Call worker in EMT, that's faster and safer than doing everything
     * using VM3ReqCall. Note that we separate VMR3ReqCall from VMR3ReqWait
     * here to make requests from under the lock in order to serialize them.
     */
    PVMREQ pReq;
    int vrc = VMR3ReqCall (mpVM, VMREQDEST_ANY, &pReq, 0 /* no wait! */,
                           (PFNRT) Console::changeDrive, 8,
                           this, pszDevice, uInstance, uLun, eState, peState,
                           pszPath, fPassthrough);
    /// @todo (r=dmik) bird, it would be nice to have a special VMR3Req method
    //  for that purpose, that doesn't return useless VERR_TIMEOUT
    if (vrc == VERR_TIMEOUT)
        vrc = VINF_SUCCESS;

    /* leave the lock before waiting for a result (EMT will call us back!) */
    alock.leave();

    if (VBOX_SUCCESS (vrc))
    {
        vrc = VMR3ReqWait (pReq, RT_INDEFINITE_WAIT);
        AssertRC (vrc);
        if (VBOX_SUCCESS (vrc))
            vrc = pReq->iStatus;
    }
    VMR3ReqFree (pReq);

    if (VBOX_SUCCESS (vrc))
    {
        LogFlowThisFunc (("Returns S_OK\n"));
        return S_OK;
    }

    if (pszPath)
        return setError (E_FAIL,
            tr ("Could not mount the media/drive '%s' (%Rrc)"), pszPath, vrc);

    return setError (E_FAIL,
        tr ("Could not unmount the currently mounted media/drive (%Rrc)"), vrc);
}


/**
 * Performs the Floppy/DVD change in EMT.
 *
 * @returns VBox status code.
 *
 * @param   pThis           Pointer to the Console object.
 * @param   pszDevice       The PDM device name.
 * @param   uInstance       The PDM device instance.
 * @param   uLun            The PDM LUN number of the drive.
 * @param   eState          The new state.
 * @param   peState         Pointer to the variable keeping the actual state of the drive.
 *                          This will be both read and updated to eState or other appropriate state.
 * @param   pszPath         The path to the media / drive which is now being mounted / captured.
 *                          If NULL no media or drive is attached and the LUN will be configured with
 *                          the default block driver with no media. This will also be the state if
 *                          mounting / capturing the specified media / drive fails.
 * @param   fPassthrough    Enables using passthrough mode of the host DVD drive if applicable.
 *
 * @thread  EMT
 * @note Locks the Console object for writing.
 */
DECLCALLBACK(int) Console::changeDrive (Console *pThis, const char *pszDevice, unsigned uInstance, unsigned uLun,
                                        DriveState_T eState, DriveState_T *peState,
                                        const char *pszPath, bool fPassthrough)
{
    LogFlowFunc (("pThis=%p pszDevice=%p:{%s} uInstance=%u uLun=%u eState=%d "
                  "peState=%p:{%d} pszPath=%p:{%s} fPassthrough=%d\n",
                  pThis, pszDevice, pszDevice, uInstance, uLun, eState,
                  peState, *peState, pszPath, pszPath, fPassthrough));

    AssertReturn (pThis, VERR_INVALID_PARAMETER);

    AssertMsg (    (!strcmp (pszDevice, "i82078")  && uLun == 0 && uInstance == 0)
               ||  (!strcmp (pszDevice, "piix3ide") && uLun == 2 && uInstance == 0),
               ("pszDevice=%s uLun=%d uInstance=%d\n", pszDevice, uLun, uInstance));

    AutoCaller autoCaller (pThis);
    AssertComRCReturn (autoCaller.rc(), VERR_ACCESS_DENIED);

    /*
     * Locking the object before doing VMR3* calls is quite safe here, since
     * we're on EMT. Write lock is necessary because we indirectly modify the
     * meDVDState/meFloppyState members (pointed to by peState).
     */
    AutoWriteLock alock (pThis);

    /* protect mpVM */
    AutoVMCaller autoVMCaller (pThis);
    CheckComRCReturnRC (autoVMCaller.rc());

    PVM pVM = pThis->mpVM;

    /*
     * Suspend the VM first.
     *
     * The VM must not be running since it might have pending I/O to
     * the drive which is being changed.
     */
    bool fResume;
    VMSTATE enmVMState = VMR3GetState (pVM);
    switch (enmVMState)
    {
        case VMSTATE_RESETTING:
        case VMSTATE_RUNNING:
        {
            LogFlowFunc (("Suspending the VM...\n"));
            /* disable the callback to prevent Console-level state change */
            pThis->mVMStateChangeCallbackDisabled = true;
            int rc = VMR3Suspend (pVM);
            pThis->mVMStateChangeCallbackDisabled = false;
            AssertRCReturn (rc, rc);
            fResume = true;
            break;
        }

        case VMSTATE_SUSPENDED:
        case VMSTATE_CREATED:
        case VMSTATE_OFF:
            fResume = false;
            break;

        default:
            AssertMsgFailedReturn (("enmVMState=%d\n", enmVMState), VERR_ACCESS_DENIED);
    }

    int rc = VINF_SUCCESS;
    int rcRet = VINF_SUCCESS;

    do
    {
        /*
         * Unmount existing media / detach host drive.
         */
        PPDMIMOUNT  pIMount = NULL;
        switch (*peState)
        {

            case DriveState_ImageMounted:
            {
                /*
                 * Resolve the interface.
                 */
                PPDMIBASE   pBase;
                rc = PDMR3QueryLun (pVM, pszDevice, uInstance, uLun, &pBase);
                if (VBOX_FAILURE (rc))
                {
                    if (rc == VERR_PDM_LUN_NOT_FOUND)
                        rc = VINF_SUCCESS;
                    AssertRC (rc);
                    break;
                }

                pIMount = (PPDMIMOUNT) pBase->pfnQueryInterface (pBase, PDMINTERFACE_MOUNT);
                AssertBreakStmt (pIMount, rc = VERR_INVALID_POINTER);

                /*
                 * Unmount the media.
                 */
                rc = pIMount->pfnUnmount (pIMount, false);
                if (rc == VERR_PDM_MEDIA_NOT_MOUNTED)
                    rc = VINF_SUCCESS;
                break;
            }

            case DriveState_HostDriveCaptured:
            {
                rc = PDMR3DeviceDetach (pVM, pszDevice, uInstance, uLun);
                if (rc == VINF_PDM_NO_DRIVER_ATTACHED_TO_LUN)
                    rc = VINF_SUCCESS;
                AssertRC (rc);
                break;
            }

            case DriveState_NotMounted:
                break;

            default:
                AssertMsgFailed (("Invalid *peState: %d\n", peState));
                break;
        }

        if (VBOX_FAILURE (rc))
        {
            rcRet = rc;
            break;
        }

        /*
         * Nothing is currently mounted.
         */
        *peState = DriveState_NotMounted;


        /*
         * Process the HostDriveCaptured state first, as the fallback path
         * means mounting the normal block driver without media.
         */
        if (eState == DriveState_HostDriveCaptured)
        {
            /*
             * Detach existing driver chain (block).
             */
            int rc = PDMR3DeviceDetach (pVM, pszDevice, uInstance, uLun);
            if (VBOX_FAILURE (rc))
            {
                if (rc == VERR_PDM_LUN_NOT_FOUND)
                    rc = VINF_SUCCESS;
                AssertReleaseRC (rc);
                break; /* we're toast */
            }
            pIMount = NULL;

            /*
             * Construct a new driver configuration.
             */
            PCFGMNODE pInst = CFGMR3GetChildF (CFGMR3GetRoot (pVM), "Devices/%s/%d/", pszDevice, uInstance);
            AssertRelease (pInst);
            /* nuke anything which might have been left behind. */
            CFGMR3RemoveNode (CFGMR3GetChildF (pInst, "LUN#%d", uLun));

            /* create a new block driver config */
            PCFGMNODE pLunL0;
            PCFGMNODE pCfg;
            if (    VBOX_SUCCESS (rc = CFGMR3InsertNodeF (pInst, &pLunL0, "LUN#%u", uLun))
                &&  VBOX_SUCCESS (rc = CFGMR3InsertString (pLunL0, "Driver", !strcmp (pszDevice, "i82078") ? "HostFloppy" : "HostDVD"))
                &&  VBOX_SUCCESS (rc = CFGMR3InsertNode (pLunL0,   "Config", &pCfg))
                &&  VBOX_SUCCESS (rc = CFGMR3InsertString (pCfg,   "Path", pszPath))
                &&  VBOX_SUCCESS (rc = !strcmp (pszDevice, "i82078") ? VINF_SUCCESS : CFGMR3InsertInteger(pCfg, "Passthrough", fPassthrough)))
            {
                /*
                 * Attempt to attach the driver.
                 */
                rc = PDMR3DeviceAttach (pVM, pszDevice, uInstance, uLun, NULL);
                AssertRC (rc);
            }
            if (VBOX_FAILURE (rc))
                rcRet = rc;
        }

        /*
         * Process the ImageMounted, NotMounted and failed HostDriveCapture cases.
         */
        rc = VINF_SUCCESS;
        switch (eState)
        {
#define RC_CHECK()  do { if (VBOX_FAILURE (rc)) { AssertReleaseRC (rc); break; } } while (0)

            case DriveState_HostDriveCaptured:
                if (VBOX_SUCCESS (rcRet))
                    break;
                /* fallback: umounted block driver. */
                pszPath = NULL;
                eState = DriveState_NotMounted;
                /* fallthru */
            case DriveState_ImageMounted:
            case DriveState_NotMounted:
            {
                /*
                 * Resolve the drive interface / create the driver.
                 */
                if (!pIMount)
                {
                    PPDMIBASE   pBase;
                    rc = PDMR3QueryLun (pVM, pszDevice, uInstance, uLun, &pBase);
                    if (rc == VERR_PDM_NO_DRIVER_ATTACHED_TO_LUN)
                    {
                        /*
                         * We have to create it, so we'll do the full config setup and everything.
                         */
                        PCFGMNODE pIdeInst = CFGMR3GetChildF (CFGMR3GetRoot (pVM), "Devices/%s/%d/", pszDevice, uInstance);
                        AssertRelease (pIdeInst);

                        /* nuke anything which might have been left behind. */
                        CFGMR3RemoveNode (CFGMR3GetChildF (pIdeInst, "LUN#%d", uLun));

                        /* create a new block driver config */
                        PCFGMNODE pLunL0;
                        rc = CFGMR3InsertNodeF (pIdeInst, &pLunL0, "LUN#%d", uLun); RC_CHECK();
                        rc = CFGMR3InsertString (pLunL0, "Driver",      "Block");   RC_CHECK();
                        PCFGMNODE pCfg;
                        rc = CFGMR3InsertNode (pLunL0,   "Config",      &pCfg);     RC_CHECK();
                        rc = CFGMR3InsertString (pCfg,   "Type",        !strcmp (pszDevice, "i82078") ? "Floppy 1.44" : "DVD");
                                                                                    RC_CHECK();
                        rc = CFGMR3InsertInteger (pCfg,  "Mountable",   1);         RC_CHECK();

                        /*
                         * Attach the driver.
                         */
                        rc = PDMR3DeviceAttach (pVM, pszDevice, uInstance, uLun, &pBase);
                        RC_CHECK();
                    }
                    else if (VBOX_FAILURE(rc))
                    {
                        AssertRC (rc);
                        return rc;
                    }

                    pIMount = (PPDMIMOUNT) pBase->pfnQueryInterface (pBase, PDMINTERFACE_MOUNT);
                    if (!pIMount)
                    {
                        AssertFailed();
                        return rc;
                    }
                }

                /*
                 * If we've got an image, let's mount it.
                 */
                if (pszPath && *pszPath)
                {
                    rc = pIMount->pfnMount (pIMount, pszPath, strcmp (pszDevice, "i82078") ? "MediaISO" : "RawImage");
                    if (VBOX_FAILURE (rc))
                        eState = DriveState_NotMounted;
                }
                break;
            }

            default:
                AssertMsgFailed (("Invalid eState: %d\n", eState));
                break;

#undef RC_CHECK
        }

        if (VBOX_FAILURE (rc) && VBOX_SUCCESS (rcRet))
            rcRet = rc;

        *peState = eState;
    }
    while (0);

    /*
     * Resume the VM if necessary.
     */
    if (fResume)
    {
        LogFlowFunc (("Resuming the VM...\n"));
        /* disable the callback to prevent Console-level state change */
        pThis->mVMStateChangeCallbackDisabled = true;
        rc = VMR3Resume (pVM);
        pThis->mVMStateChangeCallbackDisabled = false;
        AssertRC (rc);
        if (VBOX_FAILURE (rc))
        {
            /* too bad, we failed. try to sync the console state with the VMM state */
            vmstateChangeCallback (pVM, VMSTATE_SUSPENDED, enmVMState, pThis);
        }
        /// @todo (r=dmik) if we failed with drive mount, then the VMR3Resume
        //  error (if any) will be hidden from the caller. For proper reporting
        //  of such multiple errors to the caller we need to enhance the
        //  IVurtualBoxError interface. For now, give the first error the higher
        //  priority.
        if (VBOX_SUCCESS (rcRet))
            rcRet = rc;
    }

    LogFlowFunc (("Returning %Rrc\n", rcRet));
    return rcRet;
}


/**
 *  Called by IInternalSessionControl::OnNetworkAdapterChange().
 *
 *  @note Locks this object for writing.
 */
HRESULT Console::onNetworkAdapterChange (INetworkAdapter *aNetworkAdapter)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /* Don't do anything if the VM isn't running */
    if (!mpVM)
        return S_OK;

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    /* Get the properties we need from the adapter */
    BOOL fCableConnected;
    HRESULT rc = aNetworkAdapter->COMGETTER(CableConnected) (&fCableConnected);
    AssertComRC(rc);
    if (SUCCEEDED(rc))
    {
        ULONG ulInstance;
        rc = aNetworkAdapter->COMGETTER(Slot) (&ulInstance);
        AssertComRC (rc);
        if (SUCCEEDED (rc))
        {
            /*
             * Find the pcnet instance, get the config interface and update
             * the link state.
             */
            PPDMIBASE pBase;
            const char *cszAdapterName = "pcnet";
#ifdef VBOX_WITH_E1000
            /*
             * Perhaps it would be much wiser to wrap both 'pcnet' and 'e1000'
             * into generic 'net' device.
             */
            NetworkAdapterType_T adapterType;
            rc = aNetworkAdapter->COMGETTER(AdapterType)(&adapterType);
            AssertComRC(rc);
            if (adapterType == NetworkAdapterType_I82540EM ||
                adapterType == NetworkAdapterType_I82543GC)
                cszAdapterName = "e1000";
#endif
            int vrc = PDMR3QueryDeviceLun (mpVM, cszAdapterName,
                                           (unsigned) ulInstance, 0, &pBase);
            ComAssertRC (vrc);
            if (VBOX_SUCCESS (vrc))
            {
                Assert(pBase);
                PPDMINETWORKCONFIG pINetCfg = (PPDMINETWORKCONFIG) pBase->
                    pfnQueryInterface(pBase, PDMINTERFACE_NETWORK_CONFIG);
                if (pINetCfg)
                {
                    Log (("Console::onNetworkAdapterChange: setting link state to %d\n",
                          fCableConnected));
                    vrc = pINetCfg->pfnSetLinkState (pINetCfg,
                                                     fCableConnected ? PDMNETWORKLINKSTATE_UP
                                                                     : PDMNETWORKLINKSTATE_DOWN);
                    ComAssertRC (vrc);
                }
            }

            if (VBOX_FAILURE (vrc))
                rc = E_FAIL;
        }
    }

    /* notify console callbacks on success */
    if (SUCCEEDED (rc))
    {
        CallbackList::iterator it = mCallbacks.begin();
        while (it != mCallbacks.end())
            (*it++)->OnNetworkAdapterChange (aNetworkAdapter);
    }

    LogFlowThisFunc (("Leaving rc=%#x\n", rc));
    return rc;
}

/**
 *  Called by IInternalSessionControl::OnSerialPortChange().
 *
 *  @note Locks this object for writing.
 */
HRESULT Console::onSerialPortChange (ISerialPort *aSerialPort)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /* Don't do anything if the VM isn't running */
    if (!mpVM)
        return S_OK;

    HRESULT rc = S_OK;

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    /* nothing to do so far */

    /* notify console callbacks on success */
    if (SUCCEEDED (rc))
    {
        CallbackList::iterator it = mCallbacks.begin();
        while (it != mCallbacks.end())
            (*it++)->OnSerialPortChange (aSerialPort);
    }

    LogFlowThisFunc (("Leaving rc=%#x\n", rc));
    return rc;
}

/**
 *  Called by IInternalSessionControl::OnParallelPortChange().
 *
 *  @note Locks this object for writing.
 */
HRESULT Console::onParallelPortChange (IParallelPort *aParallelPort)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /* Don't do anything if the VM isn't running */
    if (!mpVM)
        return S_OK;

    HRESULT rc = S_OK;

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    /* nothing to do so far */

    /* notify console callbacks on success */
    if (SUCCEEDED (rc))
    {
        CallbackList::iterator it = mCallbacks.begin();
        while (it != mCallbacks.end())
            (*it++)->OnParallelPortChange (aParallelPort);
    }

    LogFlowThisFunc (("Leaving rc=%#x\n", rc));
    return rc;
}

/**
 *  Called by IInternalSessionControl::OnStorageControllerChange().
 *
 *  @note Locks this object for writing.
 */
HRESULT Console::onStorageControllerChange ()
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /* Don't do anything if the VM isn't running */
    if (!mpVM)
        return S_OK;

    HRESULT rc = S_OK;

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    /* nothing to do so far */

    /* notify console callbacks on success */
    if (SUCCEEDED (rc))
    {
        CallbackList::iterator it = mCallbacks.begin();
        while (it != mCallbacks.end())
            (*it++)->OnStorageControllerChange ();
    }

    LogFlowThisFunc (("Leaving rc=%#x\n", rc));
    return rc;
}

/**
 *  Called by IInternalSessionControl::OnVRDPServerChange().
 *
 *  @note Locks this object for writing.
 */
HRESULT Console::onVRDPServerChange()
{
    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = S_OK;

    if (mVRDPServer && mMachineState == MachineState_Running)
    {
        BOOL vrdpEnabled = FALSE;

        rc = mVRDPServer->COMGETTER(Enabled) (&vrdpEnabled);
        ComAssertComRCRetRC (rc);

        /* VRDP server may call this Console object back from other threads (VRDP INPUT or OUTPUT). */
        alock.leave();

        if (vrdpEnabled)
        {
            // If there was no VRDP server started the 'stop' will do nothing.
            // However if a server was started and this notification was called,
            // we have to restart the server.
            mConsoleVRDPServer->Stop ();

            if (VBOX_FAILURE(mConsoleVRDPServer->Launch ()))
            {
                rc = E_FAIL;
            }
            else
            {
                mConsoleVRDPServer->EnableConnections ();
            }
        }
        else
        {
            mConsoleVRDPServer->Stop ();
        }

        alock.enter();
    }

    /* notify console callbacks on success */
    if (SUCCEEDED (rc))
    {
        CallbackList::iterator it = mCallbacks.begin();
        while (it != mCallbacks.end())
            (*it++)->OnVRDPServerChange();
    }

    return rc;
}

/**
 *  Called by IInternalSessionControl::OnUSBControllerChange().
 *
 *  @note Locks this object for writing.
 */
HRESULT Console::onUSBControllerChange()
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /* Ignore if no VM is running yet. */
    if (!mpVM)
        return S_OK;

    HRESULT rc = S_OK;

/// @todo (dmik)
//  check for the Enabled state and disable virtual USB controller??
//  Anyway, if we want to query the machine's USB Controller we need to cache
//  it to mUSBController in #init() (as it is done with mDVDDrive).
//
//  bird: While the VM supports hot-plugging, I doubt any guest can handle it at this time... :-)
//
//    /* protect mpVM */
//    AutoVMCaller autoVMCaller (this);
//    CheckComRCReturnRC (autoVMCaller.rc());

    /* notify console callbacks on success */
    if (SUCCEEDED (rc))
    {
        CallbackList::iterator it = mCallbacks.begin();
        while (it != mCallbacks.end())
            (*it++)->OnUSBControllerChange();
    }

    return rc;
}

/**
 *  Called by IInternalSessionControl::OnSharedFolderChange().
 *
 *  @note Locks this object for writing.
 */
HRESULT Console::onSharedFolderChange (BOOL aGlobal)
{
    LogFlowThisFunc (("aGlobal=%RTbool\n", aGlobal));

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = fetchSharedFolders (aGlobal);

    /* notify console callbacks on success */
    if (SUCCEEDED (rc))
    {
        CallbackList::iterator it = mCallbacks.begin();
        while (it != mCallbacks.end())
            (*it++)->OnSharedFolderChange (aGlobal ? (Scope_T) Scope_Global
                                                   : (Scope_T) Scope_Machine);
    }

    return rc;
}

/**
 *  Called by IInternalSessionControl::OnUSBDeviceAttach() or locally by
 *  processRemoteUSBDevices() after IInternalMachineControl::RunUSBDeviceFilters()
 *  returns TRUE for a given remote USB device.
 *
 *  @return S_OK if the device was attached to the VM.
 *  @return failure if not attached.
 *
 *  @param aDevice
 *      The device in question.
 *  @param aMaskedIfs
 *      The interfaces to hide from the guest.
 *
 *  @note Locks this object for writing.
 */
HRESULT Console::onUSBDeviceAttach (IUSBDevice *aDevice, IVirtualBoxErrorInfo *aError, ULONG aMaskedIfs)
{
#ifdef VBOX_WITH_USB
    LogFlowThisFunc (("aDevice=%p aError=%p\n", aDevice, aError));

    AutoCaller autoCaller (this);
    ComAssertComRCRetRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /* protect mpVM (we don't need error info, since it's a callback) */
    AutoVMCallerQuiet autoVMCaller (this);
    if (FAILED (autoVMCaller.rc()))
    {
        /* The VM may be no more operational when this message arrives
         * (e.g. it may be Saving or Stopping or just PoweredOff) --
         * autoVMCaller.rc() will return a failure in this case. */
        LogFlowThisFunc (("Attach request ignored (mMachineState=%d).\n",
                          mMachineState));
        return autoVMCaller.rc();
    }

    if (aError != NULL)
    {
        /* notify callbacks about the error */
        onUSBDeviceStateChange (aDevice, true /* aAttached */, aError);
        return S_OK;
    }

    /* Don't proceed unless there's at least one USB hub. */
    if (!PDMR3USBHasHub (mpVM))
    {
        LogFlowThisFunc (("Attach request ignored (no USB controller).\n"));
        return E_FAIL;
    }

    HRESULT rc = attachUSBDevice (aDevice, aMaskedIfs);
    if (FAILED (rc))
    {
        /* take the current error info */
        com::ErrorInfoKeeper eik;
        /* the error must be a VirtualBoxErrorInfo instance */
        ComPtr <IVirtualBoxErrorInfo> error = eik.takeError();
        Assert (!error.isNull());
        if (!error.isNull())
        {
            /* notify callbacks about the error */
            onUSBDeviceStateChange (aDevice, true /* aAttached */, error);
        }
    }

    return rc;

#else   /* !VBOX_WITH_USB */
    return E_FAIL;
#endif  /* !VBOX_WITH_USB */
}

/**
 *  Called by IInternalSessionControl::OnUSBDeviceDetach() and locally by
 *  processRemoteUSBDevices().
 *
 *  @note Locks this object for writing.
 */
HRESULT Console::onUSBDeviceDetach (IN_GUID aId,
                                    IVirtualBoxErrorInfo *aError)
{
#ifdef VBOX_WITH_USB
    Guid Uuid (aId);
    LogFlowThisFunc (("aId={%RTuuid} aError=%p\n", Uuid.raw(), aError));

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /* Find the device. */
    ComObjPtr <OUSBDevice> device;
    USBDeviceList::iterator it = mUSBDevices.begin();
    while (it != mUSBDevices.end())
    {
        LogFlowThisFunc (("it={%RTuuid}\n", (*it)->id().raw()));
        if ((*it)->id() == Uuid)
        {
            device = *it;
            break;
        }
        ++ it;
    }


    if (device.isNull())
    {
        LogFlowThisFunc (("USB device not found.\n"));

        /* The VM may be no more operational when this message arrives
         * (e.g. it may be Saving or Stopping or just PoweredOff). Use
         * AutoVMCaller to detect it -- AutoVMCaller::rc() will return a
         * failure in this case. */

        AutoVMCallerQuiet autoVMCaller (this);
        if (FAILED (autoVMCaller.rc()))
        {
            LogFlowThisFunc (("Detach request ignored (mMachineState=%d).\n",
                              mMachineState));
            return autoVMCaller.rc();
        }

        /* the device must be in the list otherwise */
        AssertFailedReturn (E_FAIL);
    }

    if (aError != NULL)
    {
        /* notify callback about an error */
        onUSBDeviceStateChange (device, false /* aAttached */, aError);
        return S_OK;
    }

    HRESULT rc = detachUSBDevice (it);

    if (FAILED (rc))
    {
        /* take the current error info */
        com::ErrorInfoKeeper eik;
        /* the error must be a VirtualBoxErrorInfo instance */
        ComPtr <IVirtualBoxErrorInfo> error = eik.takeError();
        Assert (!error.isNull());
        if (!error.isNull())
        {
            /* notify callbacks about the error */
            onUSBDeviceStateChange (device, false /* aAttached */, error);
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
HRESULT Console::getGuestProperty (IN_BSTR aName, BSTR *aValue,
                                   ULONG64 *aTimestamp, BSTR *aFlags)
{
#if !defined (VBOX_WITH_GUEST_PROPS)
    ReturnComNotImplemented();
#else
    if (!VALID_PTR (aName))
        return E_INVALIDARG;
    if (!VALID_PTR (aValue))
        return E_POINTER;
    if ((aTimestamp != NULL) && !VALID_PTR (aTimestamp))
        return E_POINTER;
    if ((aFlags != NULL) && !VALID_PTR (aFlags))
        return E_POINTER;

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    /* protect mpVM (if not NULL) */
    AutoVMCallerWeak autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    /* Note: validity of mVMMDev which is bound to uninit() is guaranteed by
     * autoVMCaller, so there is no need to hold a lock of this */

    HRESULT rc = E_UNEXPECTED;
    using namespace guestProp;

    VBOXHGCMSVCPARM parm[4];
    Utf8Str Utf8Name = aName;
    AssertReturn(!Utf8Name.isNull(), E_OUTOFMEMORY);
    char pszBuffer[MAX_VALUE_LEN + MAX_FLAGS_LEN];

    parm[0].type = VBOX_HGCM_SVC_PARM_PTR;
    /* To save doing a const cast, we use the mutableRaw() member. */
    parm[0].u.pointer.addr = Utf8Name.mutableRaw();
    /* The + 1 is the null terminator */
    parm[0].u.pointer.size = Utf8Name.length() + 1;
    parm[1].type = VBOX_HGCM_SVC_PARM_PTR;
    parm[1].u.pointer.addr = pszBuffer;
    parm[1].u.pointer.size = sizeof(pszBuffer);
    int vrc = mVMMDev->hgcmHostCall ("VBoxGuestPropSvc", GET_PROP_HOST,
                                     4, &parm[0]);
    /* The returned string should never be able to be greater than our buffer */
    AssertLogRel (vrc != VERR_BUFFER_OVERFLOW);
    AssertLogRel (RT_FAILURE(vrc) || VBOX_HGCM_SVC_PARM_64BIT == parm[2].type);
    if (RT_SUCCESS (vrc) || (VERR_NOT_FOUND == vrc))
    {
        rc = S_OK;
        if (vrc != VERR_NOT_FOUND)
        {
            size_t iFlags = strlen(pszBuffer) + 1;
            Utf8Str(pszBuffer).cloneTo (aValue);
            *aTimestamp = parm[2].u.uint64;
            Utf8Str(pszBuffer + iFlags).cloneTo (aFlags);
        }
        else
            aValue = NULL;
    }
    else
        rc = setError (E_UNEXPECTED,
            tr ("The service call failed with the error %Rrc"), vrc);
    return rc;
#endif /* else !defined (VBOX_WITH_GUEST_PROPS) */
}

/**
 * @note Temporarily locks this object for writing.
 */
HRESULT Console::setGuestProperty (IN_BSTR aName, IN_BSTR aValue, IN_BSTR aFlags)
{
#if !defined (VBOX_WITH_GUEST_PROPS)
    ReturnComNotImplemented();
#else
    if (!VALID_PTR (aName))
        return E_INVALIDARG;
    if ((aValue != NULL) && !VALID_PTR (aValue))
        return E_INVALIDARG;
    if ((aFlags != NULL) && !VALID_PTR (aFlags))
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    /* protect mpVM (if not NULL) */
    AutoVMCallerWeak autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    /* Note: validity of mVMMDev which is bound to uninit() is guaranteed by
     * autoVMCaller, so there is no need to hold a lock of this */

    HRESULT rc = E_UNEXPECTED;
    using namespace guestProp;

    VBOXHGCMSVCPARM parm[3];
    Utf8Str Utf8Name = aName;
    int vrc = VINF_SUCCESS;

    parm[0].type = VBOX_HGCM_SVC_PARM_PTR;
    /* To save doing a const cast, we use the mutableRaw() member. */
    parm[0].u.pointer.addr = Utf8Name.mutableRaw();
    /* The + 1 is the null terminator */
    parm[0].u.pointer.size = Utf8Name.length() + 1;
    Utf8Str Utf8Value = aValue;
    if (aValue != NULL)
    {
        parm[1].type = VBOX_HGCM_SVC_PARM_PTR;
        /* To save doing a const cast, we use the mutableRaw() member. */
        parm[1].u.pointer.addr = Utf8Value.mutableRaw();
        /* The + 1 is the null terminator */
        parm[1].u.pointer.size = Utf8Value.length() + 1;
    }
    Utf8Str Utf8Flags = aFlags;
    if (aFlags != NULL)
    {
        parm[2].type = VBOX_HGCM_SVC_PARM_PTR;
        /* To save doing a const cast, we use the mutableRaw() member. */
        parm[2].u.pointer.addr = Utf8Flags.mutableRaw();
        /* The + 1 is the null terminator */
        parm[2].u.pointer.size = Utf8Flags.length() + 1;
    }
    if ((aValue != NULL) && (aFlags != NULL))
        vrc = mVMMDev->hgcmHostCall ("VBoxGuestPropSvc", SET_PROP_HOST,
                                     3, &parm[0]);
    else if (aValue != NULL)
        vrc = mVMMDev->hgcmHostCall ("VBoxGuestPropSvc", SET_PROP_VALUE_HOST,
                                     2, &parm[0]);
    else
        vrc = mVMMDev->hgcmHostCall ("VBoxGuestPropSvc", DEL_PROP_HOST,
                                     1, &parm[0]);
    if (RT_SUCCESS (vrc))
        rc = S_OK;
    else
        rc = setError (E_UNEXPECTED,
            tr ("The service call failed with the error %Rrc"), vrc);
    return rc;
#endif /* else !defined (VBOX_WITH_GUEST_PROPS) */
}


/**
 * @note Temporarily locks this object for writing.
 */
HRESULT Console::enumerateGuestProperties (IN_BSTR aPatterns,
                                           ComSafeArrayOut(BSTR, aNames),
                                           ComSafeArrayOut(BSTR, aValues),
                                           ComSafeArrayOut(ULONG64, aTimestamps),
                                           ComSafeArrayOut(BSTR, aFlags))
{
#if !defined (VBOX_WITH_GUEST_PROPS)
    ReturnComNotImplemented();
#else
    if (!VALID_PTR (aPatterns) && (aPatterns != NULL))
        return E_POINTER;
    if (ComSafeArrayOutIsNull (aNames))
        return E_POINTER;
    if (ComSafeArrayOutIsNull (aValues))
        return E_POINTER;
    if (ComSafeArrayOutIsNull (aTimestamps))
        return E_POINTER;
    if (ComSafeArrayOutIsNull (aFlags))
        return E_POINTER;

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    /* protect mpVM (if not NULL) */
    AutoVMCallerWeak autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    /* Note: validity of mVMMDev which is bound to uninit() is guaranteed by
     * autoVMCaller, so there is no need to hold a lock of this */

    return doEnumerateGuestProperties (aPatterns, ComSafeArrayOutArg(aNames),
                                       ComSafeArrayOutArg(aValues),
                                       ComSafeArrayOutArg(aTimestamps),
                                       ComSafeArrayOutArg(aFlags));
#endif /* else !defined (VBOX_WITH_GUEST_PROPS) */
}

/**
 *  Gets called by Session::UpdateMachineState()
 *  (IInternalSessionControl::updateMachineState()).
 *
 *  Must be called only in certain cases (see the implementation).
 *
 *  @note Locks this object for writing.
 */
HRESULT Console::updateMachineState (MachineState_T aMachineState)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    AssertReturn (mMachineState == MachineState_Saving ||
                  mMachineState == MachineState_Discarding,
                  E_FAIL);

    return setMachineStateLocally (aMachineState);
}

/**
 *  @note Locks this object for writing.
 */
void Console::onMousePointerShapeChange(bool fVisible, bool fAlpha,
                                        uint32_t xHot, uint32_t yHot,
                                        uint32_t width, uint32_t height,
                                        void *pShape)
{
#if 0
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("fVisible=%d, fAlpha=%d, xHot = %d, yHot = %d, width=%d, "
                      "height=%d, shape=%p\n",
                      fVisible, fAlpha, xHot, yHot, width, height, pShape));
#endif

    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* We need a write lock because we alter the cached callback data */
    AutoWriteLock alock (this);

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
            RTMemFree (mCallbackData.mpsc.shape);
            mCallbackData.mpsc.shape = NULL;
        }
        if (mCallbackData.mpsc.shape == NULL)
        {
            mCallbackData.mpsc.shape = (BYTE *) RTMemAllocZ (cb);
            AssertReturnVoid (mCallbackData.mpsc.shape);
        }
        mCallbackData.mpsc.shapeSize = cb;
        memcpy (mCallbackData.mpsc.shape, pShape, cb);
    }
    else
    {
        if (wasValid && mCallbackData.mpsc.shape != NULL)
            RTMemFree (mCallbackData.mpsc.shape);
        mCallbackData.mpsc.shape = NULL;
        mCallbackData.mpsc.shapeSize = 0;
    }

    mCallbackData.mpsc.valid = true;

    CallbackList::iterator it = mCallbacks.begin();
    while (it != mCallbacks.end())
        (*it++)->OnMousePointerShapeChange (fVisible, fAlpha, xHot, yHot,
                                            width, height, (BYTE *) pShape);

#if 0
    LogFlowThisFuncLeave();
#endif
}

/**
 *  @note Locks this object for writing.
 */
void Console::onMouseCapabilityChange (BOOL supportsAbsolute, BOOL needsHostCursor)
{
    LogFlowThisFunc (("supportsAbsolute=%d needsHostCursor=%d\n",
                      supportsAbsolute, needsHostCursor));

    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* We need a write lock because we alter the cached callback data */
    AutoWriteLock alock (this);

    /* save the callback arguments */
    mCallbackData.mcc.supportsAbsolute = supportsAbsolute;
    mCallbackData.mcc.needsHostCursor = needsHostCursor;
    mCallbackData.mcc.valid = true;

    CallbackList::iterator it = mCallbacks.begin();
    while (it != mCallbacks.end())
    {
        Log2(("Console::onMouseCapabilityChange: calling %p\n", (void*)*it));
        (*it++)->OnMouseCapabilityChange (supportsAbsolute, needsHostCursor);
    }
}

/**
 *  @note Locks this object for reading.
 */
void Console::onStateChange (MachineState_T machineState)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoReadLock alock (this);

    CallbackList::iterator it = mCallbacks.begin();
    while (it != mCallbacks.end())
        (*it++)->OnStateChange (machineState);
}

/**
 *  @note Locks this object for reading.
 */
void Console::onAdditionsStateChange()
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoReadLock alock (this);

    CallbackList::iterator it = mCallbacks.begin();
    while (it != mCallbacks.end())
        (*it++)->OnAdditionsStateChange();
}

/**
 *  @note Locks this object for reading.
 */
void Console::onAdditionsOutdated()
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoReadLock alock (this);

    /** @todo Use the On-Screen Display feature to report the fact.
     *  The user should be told to install additions that are
     *  provided with the current VBox build:
     *  VBOX_VERSION_MAJOR.VBOX_VERSION_MINOR.VBOX_VERSION_BUILD
     */
}

/**
 *  @note Locks this object for writing.
 */
void Console::onKeyboardLedsChange(bool fNumLock, bool fCapsLock, bool fScrollLock)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* We need a write lock because we alter the cached callback data */
    AutoWriteLock alock (this);

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
 *  @note Locks this object for reading.
 */
void Console::onUSBDeviceStateChange (IUSBDevice *aDevice, bool aAttached,
                                      IVirtualBoxErrorInfo *aError)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoReadLock alock (this);

    CallbackList::iterator it = mCallbacks.begin();
    while (it != mCallbacks.end())
        (*it++)->OnUSBDeviceStateChange (aDevice, aAttached, aError);
}

/**
 *  @note Locks this object for reading.
 */
void Console::onRuntimeError (BOOL aFatal, IN_BSTR aErrorID, IN_BSTR aMessage)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoReadLock alock (this);

    CallbackList::iterator it = mCallbacks.begin();
    while (it != mCallbacks.end())
        (*it++)->OnRuntimeError (aFatal, aErrorID, aMessage);
}

/**
 *  @note Locks this object for reading.
 */
HRESULT Console::onShowWindow (BOOL aCheck, BOOL *aCanShow, ULONG64 *aWinId)
{
    AssertReturn (aCanShow, E_POINTER);
    AssertReturn (aWinId, E_POINTER);

    *aCanShow = FALSE;
    *aWinId = 0;

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    HRESULT rc = S_OK;
    CallbackList::iterator it = mCallbacks.begin();

    if (aCheck)
    {
        while (it != mCallbacks.end())
        {
            BOOL canShow = FALSE;
            rc = (*it++)->OnCanShowWindow (&canShow);
            AssertComRC (rc);
            if (FAILED (rc) || !canShow)
                return rc;
        }
        *aCanShow = TRUE;
    }
    else
    {
        while (it != mCallbacks.end())
        {
            ULONG64 winId = 0;
            rc = (*it++)->OnShowWindow (&winId);
            AssertComRC (rc);
            if (FAILED (rc))
                return rc;
            /* only one callback may return non-null winId */
            Assert (*aWinId == 0 || winId == 0);
            if (*aWinId == 0)
                *aWinId = winId;
        }
    }

    return S_OK;
}

// private methods
////////////////////////////////////////////////////////////////////////////////

/**
 *  Increases the usage counter of the mpVM pointer. Guarantees that
 *  VMR3Destroy() will not be called on it at least until releaseVMCaller()
 *  is called.
 *
 *  If this method returns a failure, the caller is not allowed to use mpVM
 *  and may return the failed result code to the upper level. This method sets
 *  the extended error info on failure if \a aQuiet is false.
 *
 *  Setting \a aQuiet to true is useful for methods that don't want to return
 *  the failed result code to the caller when this method fails (e.g. need to
 *  silently check for the mpVM availability).
 *
 *  When mpVM is NULL but \a aAllowNullVM is true, a corresponding error will be
 *  returned instead of asserting. Having it false is intended as a sanity check
 *  for methods that have checked mMachineState and expect mpVM *NOT* to be NULL.
 *
 *  @param aQuiet       true to suppress setting error info
 *  @param aAllowNullVM true to accept mpVM being NULL and return a failure
 *                      (otherwise this method will assert if mpVM is NULL)
 *
 *  @note Locks this object for writing.
 */
HRESULT Console::addVMCaller (bool aQuiet /* = false */,
                              bool aAllowNullVM /* = false */)
{
    AutoCaller  autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (mVMDestroying)
    {
        /* powerDown() is waiting for all callers to finish */
        return aQuiet ? E_ACCESSDENIED : setError (E_ACCESSDENIED,
            tr ("Virtual machine is being powered down"));
    }

    if (mpVM == NULL)
    {
        Assert (aAllowNullVM == true);

        /* The machine is not powered up */
        return aQuiet ? E_ACCESSDENIED : setError (E_ACCESSDENIED,
            tr ("Virtual machine is not powered up"));
    }

    ++ mVMCallers;

    return S_OK;
}

/**
 *  Decreases the usage counter of the mpVM pointer. Must always complete
 *  the addVMCaller() call after the mpVM pointer is no more necessary.
 *
 *  @note Locks this object for writing.
 */
void Console::releaseVMCaller()
{
    AutoCaller  autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock (this);

    AssertReturnVoid (mpVM != NULL);

    Assert (mVMCallers > 0);
    -- mVMCallers;

    if (mVMCallers == 0 && mVMDestroying)
    {
        /* inform powerDown() there are no more callers */
        RTSemEventSignal (mVMZeroCallersSem);
    }
}

/**
 * Initialize the release logging facility. In case something
 * goes wrong, there will be no release logging. Maybe in the future
 * we can add some logic to use different file names in this case.
 * Note that the logic must be in sync with Machine::DeleteSettings().
 */
HRESULT Console::consoleInitReleaseLog (const ComPtr <IMachine> aMachine)
{
    HRESULT hrc = S_OK;

    Bstr logFolder;
    hrc = aMachine->COMGETTER(LogFolder) (logFolder.asOutParam());
    CheckComRCReturnRC (hrc);

    Utf8Str logDir = logFolder;

    /* make sure the Logs folder exists */
    Assert (!logDir.isEmpty());
    if (!RTDirExists (logDir))
        RTDirCreateFullPath (logDir, 0777);

    Utf8Str logFile = Utf8StrFmt ("%s%cVBox.log",
                                  logDir.raw(), RTPATH_DELIMITER);
    Utf8Str pngFile = Utf8StrFmt ("%s%cVBox.png",
                                  logDir.raw(), RTPATH_DELIMITER);

    /*
     * Age the old log files
     * Rename .(n-1) to .(n), .(n-2) to .(n-1), ..., and the last log file to .1
     * Overwrite target files in case they exist.
     */
    ComPtr<IVirtualBox> virtualBox;
    aMachine->COMGETTER(Parent)(virtualBox.asOutParam());
    ComPtr <ISystemProperties> systemProperties;
    virtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());
    ULONG uLogHistoryCount = 3;
    systemProperties->COMGETTER(LogHistoryCount)(&uLogHistoryCount);
    if (uLogHistoryCount)
    {
        for (int i = uLogHistoryCount-1; i >= 0; i--)
        {
            Utf8Str *files[] = { &logFile, &pngFile };
            Utf8Str oldName, newName;

            for (unsigned int j = 0; j < RT_ELEMENTS (files); ++ j)
            {
                if (i > 0)
                    oldName = Utf8StrFmt ("%s.%d", files [j]->raw(), i);
                else
                    oldName = *files [j];
                newName = Utf8StrFmt ("%s.%d", files [j]->raw(), i + 1);
                /* If the old file doesn't exist, delete the new file (if it
                 * exists) to provide correct rotation even if the sequence is
                 * broken */
                if (   RTFileRename (oldName, newName, RTFILEMOVE_FLAGS_REPLACE)
                    == VERR_FILE_NOT_FOUND)
                    RTFileDelete (newName);
            }
        }
    }

    PRTLOGGER loggerRelease;
    static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
    RTUINT fFlags = RTLOGFLAGS_PREFIX_TIME_PROG;
#if defined (RT_OS_WINDOWS) || defined (RT_OS_OS2)
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
                       "VirtualBox %s r%d %s (%s %s) release log\n"
                       "Log opened %s\n",
                       VBOX_VERSION_STRING, VBoxSVNRev (), VBOX_BUILD_TARGET,
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
        /* the package type is interesting for Linux distributions */
        RTLogRelLogger    (loggerRelease, 0, ~0U, "Package type: %s"
#ifdef VBOX_OSE
                       " (OSE)"
#endif
                       "\n",
                       VBOX_PACKAGE_STRING);

        /* register this logger as the release logger */
        RTLogRelSetDefaultInstance(loggerRelease);
        hrc = S_OK;
    }
    else
        hrc = setError (E_FAIL,
                        tr ("Failed to open release log (%s, %Rrc)"), szError, vrc);

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
HRESULT Console::powerUp (IProgress **aProgress, bool aPaused)
{
    if (aProgress == NULL)
        return E_POINTER;

    LogFlowThisFuncEnter();
    LogFlowThisFunc (("mMachineState=%d\n", mMachineState));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (Global::IsOnlineOrTransient (mMachineState))
        return setError(VBOX_E_INVALID_VM_STATE,
            tr ("Virtual machine is already running or busy "
                "(machine state: %d)"), mMachineState);

    HRESULT rc = S_OK;

    /* the network cards will undergo a quick consistency check */
    for (ULONG slot = 0; slot < SchemaDefs::NetworkAdapterCount; slot ++)
    {
        ComPtr<INetworkAdapter> adapter;
        mMachine->GetNetworkAdapter (slot, adapter.asOutParam());
        BOOL enabled = FALSE;
        adapter->COMGETTER(Enabled) (&enabled);
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
                    return setError (VBOX_E_HOST_ERROR,
                        tr ("VM cannot start because host interface networking "
                            "requires a host interface name to be set"));
                }
                ComPtr<IVirtualBox> virtualBox;
                mMachine->COMGETTER(Parent)(virtualBox.asOutParam());
                ComPtr<IHost> host;
                virtualBox->COMGETTER(Host)(host.asOutParam());
                ComPtr<IHostNetworkInterface> hostInterface;
                if (!SUCCEEDED(host->FindHostNetworkInterfaceByName(hostif, hostInterface.asOutParam())))
                {
                    return setError (VBOX_E_HOST_ERROR,
                        tr ("VM cannot start because the host interface '%ls' "
                            "does not exist"),
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
    CheckComRCReturnRC (rc);

    /* Check all types of shared folders and compose a single list */
    SharedFolderDataMap sharedFolders;
    {
        /* first, insert global folders */
        for (SharedFolderDataMap::const_iterator it = mGlobalSharedFolders.begin();
             it != mGlobalSharedFolders.end(); ++ it)
            sharedFolders [it->first] = it->second;

        /* second, insert machine folders */
        for (SharedFolderDataMap::const_iterator it = mMachineSharedFolders.begin();
             it != mMachineSharedFolders.end(); ++ it)
            sharedFolders [it->first] = it->second;

        /* third, insert console folders */
        for (SharedFolderMap::const_iterator it = mSharedFolders.begin();
             it != mSharedFolders.end(); ++ it)
            sharedFolders [it->first] = SharedFolderData(it->second->hostPath(), it->second->writable());
    }

    Bstr savedStateFile;

    /*
     * Saved VMs will have to prove that their saved states are kosher.
     */
    if (mMachineState == MachineState_Saved)
    {
        rc = mMachine->COMGETTER(StateFilePath) (savedStateFile.asOutParam());
        CheckComRCReturnRC (rc);
        ComAssertRet (!!savedStateFile, E_FAIL);
        int vrc = SSMR3ValidateFile (Utf8Str (savedStateFile));
        if (VBOX_FAILURE (vrc))
            return setError (VBOX_E_FILE_ERROR,
                tr ("VM cannot start because the saved state file '%ls' is invalid (%Rrc). "
                    "Discard the saved state prior to starting the VM"),
                    savedStateFile.raw(), vrc);
    }

    /* create a progress object to track progress of this operation */
    ComObjPtr <Progress> powerupProgress;
    powerupProgress.createObject();
    Bstr progressDesc;
    if (mMachineState == MachineState_Saved)
        progressDesc = tr ("Restoring virtual machine");
    else
        progressDesc = tr ("Starting virtual machine");
    rc = powerupProgress->init (static_cast <IConsole *> (this),
                                progressDesc, FALSE /* aCancelable */);
    CheckComRCReturnRC (rc);

    /* setup task object and thread to carry out the operation
     * asynchronously */

    std::auto_ptr <VMPowerUpTask> task (new VMPowerUpTask (this, powerupProgress));
    ComAssertComRCRetRC (task->rc());

    task->mSetVMErrorCallback = setVMErrorCallback;
    task->mConfigConstructor = configConstructor;
    task->mSharedFolders = sharedFolders;
    task->mStartPaused = aPaused;
    if (mMachineState == MachineState_Saved)
        task->mSavedStateFile = savedStateFile;

    /* Reset differencing hard disks for which autoReset is true */
    {
        com::SafeIfaceArray <IHardDiskAttachment> atts;
        rc = mMachine->
            COMGETTER(HardDiskAttachments) (ComSafeArrayAsOutParam (atts));
        CheckComRCReturnRC (rc);

        for (size_t i = 0; i < atts.size(); ++ i)
        {
            ComPtr <IHardDisk> hardDisk;
            rc = atts [i]->COMGETTER(HardDisk) (hardDisk.asOutParam());
            CheckComRCReturnRC (rc);

            /* save for later use on the powerup thread */
            task->hardDisks.push_back (hardDisk);

            /* needs autoreset? */
            BOOL autoReset = FALSE;
            rc = hardDisk->COMGETTER(AutoReset)(&autoReset);
            CheckComRCReturnRC (rc);

            if (autoReset)
            {
                ComPtr <IProgress> resetProgress;
                rc = hardDisk->Reset (resetProgress.asOutParam());
                CheckComRCReturnRC (rc);

                /* save for later use on the powerup thread */
                task->hardDiskProgresses.push_back (resetProgress);
            }
        }
    }

    rc = consoleInitReleaseLog (mMachine);
    CheckComRCReturnRC (rc);

    /* pass the progress object to the caller if requested */
    if (aProgress)
    {
        if (task->hardDiskProgresses.size() == 0)
        {
            /* there are no other operations to track, return the powerup
             * progress only */
            powerupProgress.queryInterfaceTo (aProgress);
        }
        else
        {
            /* create a combined progress object */
            ComObjPtr <CombinedProgress> progress;
            progress.createObject();
            VMPowerUpTask::ProgressList progresses (task->hardDiskProgresses);
            progresses.push_back (ComPtr <IProgress> (powerupProgress));
            rc = progress->init (static_cast <IConsole *> (this),
                                 progressDesc, progresses.begin(),
                                 progresses.end());
            AssertComRCReturnRC (rc);
            progress.queryInterfaceTo (aProgress);
        }
    }

    int vrc = RTThreadCreate (NULL, Console::powerUpThread, (void *) task.get(),
                              0, RTTHREADTYPE_MAIN_WORKER, 0, "VMPowerUp");

    ComAssertMsgRCRet (vrc, ("Could not create VMPowerUp thread (%Rrc)", vrc),
                       E_FAIL);

    /* task is now owned by powerUpThread(), so release it */
    task.release();

    /* finally, set the state: no right to fail in this method afterwards
     * since we've already started the thread and it is now responsible for
     * any error reporting and appropriate state change! */

    if (mMachineState == MachineState_Saved)
        setMachineState (MachineState_Restoring);
    else
        setMachineState (MachineState_Starting);

    LogFlowThisFunc (("mMachineState=%d\n", mMachineState));
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
HRESULT Console::powerDown (Progress *aProgress /*= NULL*/)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /* Total # of steps for the progress object. Must correspond to the
     * number of "advance percent count" comments in this method! */
    enum { StepCount = 7 };
    /* current step */
    size_t step = 0;

    HRESULT rc = S_OK;
    int vrc = VINF_SUCCESS;

    /* sanity */
    Assert (mVMDestroying == false);

    Assert (mpVM != NULL);

    AssertMsg (mMachineState == MachineState_Running ||
               mMachineState == MachineState_Paused ||
               mMachineState == MachineState_Stuck ||
               mMachineState == MachineState_Saving ||
               mMachineState == MachineState_Starting ||
               mMachineState == MachineState_Restoring ||
               mMachineState == MachineState_Stopping,
               ("Invalid machine state: %d\n", mMachineState));

    LogRel (("Console::powerDown(): A request to power off the VM has been "
             "issued (mMachineState=%d, InUninit=%d)\n",
             mMachineState, autoCaller.state() == InUninit));

    /* Check if we need to power off the VM. In case of mVMPoweredOff=true, the
     * VM has already powered itself off in vmstateChangeCallback() and is just
     * notifying Console about that. In case of Starting or Restoring,
     * powerUpThread() is calling us on failure, so the VM is already off at
     * that point. */
    if (!mVMPoweredOff &&
        (mMachineState == MachineState_Starting ||
         mMachineState == MachineState_Restoring))
        mVMPoweredOff = true;

    /* go to Stopping state if not already there. Note that we don't go from
     * Saving/Restoring to Stopping because vmstateChangeCallback() needs it to
     * set the state to Saved on VMSTATE_TERMINATED. In terms of protecting from
     * inappropriate operations while leaving the lock below, Saving or
     * Restoring should be fine too */
    if (mMachineState != MachineState_Saving &&
        mMachineState != MachineState_Restoring &&
        mMachineState != MachineState_Stopping)
        setMachineState (MachineState_Stopping);

    /* ----------------------------------------------------------------------
     * DONE with necessary state changes, perform the power down actions (it's
     * safe to leave the object lock now if needed)
     * ---------------------------------------------------------------------- */

    /* Stop the VRDP server to prevent new clients connection while VM is being
     * powered off. */
    if (mConsoleVRDPServer)
    {
        LogFlowThisFunc (("Stopping VRDP server...\n"));

        /* Leave the lock since EMT will call us back as addVMCaller()
         * in updateDisplayData(). */
        alock.leave();

        mConsoleVRDPServer->Stop();

        alock.enter();
    }

    /* advance percent count */
    if (aProgress)
        aProgress->notifyProgress (99 * (++ step) / StepCount );

#ifdef VBOX_WITH_HGCM

# ifdef VBOX_WITH_GUEST_PROPS

    /* Save all guest property store entries to the machine XML file */
    com::SafeArray <BSTR> namesOut;
    com::SafeArray <BSTR> valuesOut;
    com::SafeArray <ULONG64> timestampsOut;
    com::SafeArray <BSTR> flagsOut;
    Bstr pattern("");
    if (pattern.isNull())
        rc = E_OUTOFMEMORY;
    else
        rc = doEnumerateGuestProperties (Bstr (""), ComSafeArrayAsOutParam (namesOut),
                                        ComSafeArrayAsOutParam (valuesOut),
                                        ComSafeArrayAsOutParam (timestampsOut),
                                        ComSafeArrayAsOutParam (flagsOut));
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
                guestProp::validateFlags (Utf8Str(flagsOut[i]).raw(), &fFlags);
                if (   !( fFlags & guestProp::TRANSIENT)
                    || (mMachineState == MachineState_Saving)
                  )
                {
                    names.push_back(namesOut[i]);
                    values.push_back(valuesOut[i]);
                    timestamps.push_back(timestampsOut[i]);
                    flags.push_back(flagsOut[i]);
                }
            }
            com::SafeArray <BSTR> namesIn (names);
            com::SafeArray <BSTR> valuesIn (values);
            com::SafeArray <ULONG64> timestampsIn (timestamps);
            com::SafeArray <BSTR> flagsIn (flags);
            if (   namesIn.isNull()
                || valuesIn.isNull()
                || timestampsIn.isNull()
                || flagsIn.isNull()
                )
                throw std::bad_alloc();
            /* PushGuestProperties() calls DiscardSettings(), which calls us back */
            alock.leave();
            mControl->PushGuestProperties (ComSafeArrayAsInParam (namesIn),
                                          ComSafeArrayAsInParam (valuesIn),
                                          ComSafeArrayAsInParam (timestampsIn),
                                          ComSafeArrayAsInParam (flagsIn));
            alock.enter();
        }
        catch (std::bad_alloc)
        {
            rc = E_OUTOFMEMORY;
        }
    }

    /* advance percent count */
    if (aProgress)
        aProgress->notifyProgress (99 * (++ step) / StepCount );

# endif /* VBOX_WITH_GUEST_PROPS defined */

    /* Shutdown HGCM services before stopping the guest, because they might
     * need a cleanup. */
    if (mVMMDev)
    {
        LogFlowThisFunc (("Shutdown HGCM...\n"));

        /* Leave the lock since EMT will call us back as addVMCaller() */
        alock.leave();

        mVMMDev->hgcmShutdown ();

        alock.enter();
    }

    /* advance percent count */
    if (aProgress)
        aProgress->notifyProgress (99 * (++ step) / StepCount );

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
            RTSemEventCreate (&mVMZeroCallersSem);

        LogFlowThisFunc (("Waiting for mpVM callers (%d) to drop to zero...\n",
                          mVMCallers));

        alock.leave();

        RTSemEventWait (mVMZeroCallersSem, RT_INDEFINITE_WAIT);

        alock.enter();
    }

    /* advance percent count */
    if (aProgress)
        aProgress->notifyProgress (99 * (++ step) / StepCount );

    vrc = VINF_SUCCESS;

    /* Power off the VM if not already done that */
    if (!mVMPoweredOff)
    {
        LogFlowThisFunc (("Powering off the VM...\n"));

        /* Leave the lock since EMT will call us back on VMR3PowerOff() */
        alock.leave();

        vrc = VMR3PowerOff (mpVM);

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
        aProgress->notifyProgress (99 * (++ step) / StepCount );

    LogFlowThisFunc (("Ready for VM destruction.\n"));

    /* If we are called from Console::uninit(), then try to destroy the VM even
     * on failure (this will most likely fail too, but what to do?..) */
    if (VBOX_SUCCESS (vrc) || autoCaller.state() == InUninit)
    {
        /* If the machine has an USB comtroller, release all USB devices
         * (symmetric to the code in captureUSBDevices()) */
        bool fHasUSBController = false;
        {
            PPDMIBASE pBase;
            int vrc = PDMR3QueryLun (mpVM, "usb-ohci", 0, 0, &pBase);
            if (VBOX_SUCCESS (vrc))
            {
                fHasUSBController = true;
                detachAllUSBDevices (false /* aDone */);
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

        LogFlowThisFunc (("Destroying the VM...\n"));

        alock.leave();

        vrc = VMR3Destroy (pVM);

        /* take the lock again */
        alock.enter();

        /* advance percent count */
        if (aProgress)
            aProgress->notifyProgress (99 * (++ step) / StepCount );

        if (VBOX_SUCCESS (vrc))
        {
            LogFlowThisFunc (("Machine has been destroyed (mMachineState=%d)\n",
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
            rc = setError (VBOX_E_VM_ERROR,
                tr ("Could not destroy the machine.  (Error: %Rrc)"), vrc);
        }

        /* Complete the detaching of the USB devices. */
        if (fHasUSBController)
            detachAllUSBDevices (true /* aDone */);

        /* advance percent count */
        if (aProgress)
            aProgress->notifyProgress (99 * (++ step) / StepCount );
    }
    else
    {
        rc = setError (VBOX_E_VM_ERROR,
            tr ("Could not power off the machine.  (Error: %Rrc)"), vrc);
    }

    /* Finished with destruction. Note that if something impossible happened and
     * we've failed to destroy the VM, mVMDestroying will remain true and
     * mMachineState will be something like Stopping, so most Console methods
     * will return an error to the caller. */
    if (mpVM == NULL)
        mVMDestroying = false;

    if (SUCCEEDED (rc))
    {
        /* uninit dynamically allocated members of mCallbackData */
        if (mCallbackData.mpsc.valid)
        {
            if (mCallbackData.mpsc.shape != NULL)
                RTMemFree (mCallbackData.mpsc.shape);
        }
        memset (&mCallbackData, 0, sizeof (mCallbackData));
    }

    /* complete the progress */
    if (aProgress)
        aProgress->notifyComplete (rc);

    LogFlowThisFuncLeave();
    return rc;
}

/**
 *  @note Locks this object for writing.
 */
HRESULT Console::setMachineState (MachineState_T aMachineState,
                                  bool aUpdateServer /* = true */)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = S_OK;

    if (mMachineState != aMachineState)
    {
        LogFlowThisFunc (("machineState=%d\n", aMachineState));
        mMachineState = aMachineState;

        /// @todo (dmik)
        //      possibly, we need to redo onStateChange() using the dedicated
        //      Event thread, like it is done in VirtualBox. This will make it
        //      much safer (no deadlocks possible if someone tries to use the
        //      console from the callback), however, listeners will lose the
        //      ability to synchronously react to state changes (is it really
        //      necessary??)
        LogFlowThisFunc (("Doing onStateChange()...\n"));
        onStateChange (aMachineState);
        LogFlowThisFunc (("Done onStateChange()\n"));

        if (aUpdateServer)
        {
            /* Server notification MUST be done from under the lock; otherwise
             * the machine state here and on the server might go out of sync
             * whihc can lead to various unexpected results (like the machine
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
            LogFlowThisFunc (("Doing mControl->UpdateState()...\n"));
            rc = mControl->UpdateState (aMachineState);
            LogFlowThisFunc (("mControl->UpdateState()=%08X\n", rc));
        }
    }

    return rc;
}

/**
 *  Searches for a shared folder with the given logical name
 *  in the collection of shared folders.
 *
 *  @param aName            logical name of the shared folder
 *  @param aSharedFolder    where to return the found object
 *  @param aSetError        whether to set the error info if the folder is
 *                          not found
 *  @return
 *      S_OK when found or E_INVALIDARG when not found
 *
 *  @note The caller must lock this object for writing.
 */
HRESULT Console::findSharedFolder (CBSTR aName,
                                   ComObjPtr <SharedFolder> &aSharedFolder,
                                   bool aSetError /* = false */)
{
    /* sanity check */
    AssertReturn (isWriteLockOnCurrentThread(), E_FAIL);

    SharedFolderMap::const_iterator it = mSharedFolders.find (aName);
    if (it != mSharedFolders.end())
    {
        aSharedFolder = it->second;
        return S_OK;
    }

    if (aSetError)
        setError (VBOX_E_FILE_ERROR,
                  tr ("Could not find a shared folder named '%ls'."), aName);

    return VBOX_E_FILE_ERROR;
}

/**
 *  Fetches the list of global or machine shared folders from the server.
 *
 *  @param aGlobal true to fetch global folders.
 *
 *  @note The caller must lock this object for writing.
 */
HRESULT Console::fetchSharedFolders (BOOL aGlobal)
{
    /* sanity check */
    AssertReturn (AutoCaller (this).state() == InInit ||
                  isWriteLockOnCurrentThread(), E_FAIL);

    /* protect mpVM (if not NULL) */
    AutoVMCallerQuietWeak autoVMCaller (this);

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

        SafeIfaceArray <ISharedFolder> folders;
        rc = mMachine->COMGETTER(SharedFolders) (ComSafeArrayAsOutParam(folders));
        AssertComRCReturnRC (rc);

        for (size_t i = 0; i < folders.size(); ++i)
        {
            ComPtr <ISharedFolder> folder = folders[i];

            Bstr name;
            Bstr hostPath;
            BOOL writable;

            rc = folder->COMGETTER(Name) (name.asOutParam());
            CheckComRCBreakRC (rc);
            rc = folder->COMGETTER(HostPath) (hostPath.asOutParam());
            CheckComRCBreakRC (rc);
            rc = folder->COMGETTER(Writable) (&writable);

            mMachineSharedFolders.insert (std::make_pair (name, SharedFolderData (hostPath, writable)));

            /* send changes to HGCM if the VM is running */
            /// @todo report errors as runtime warnings through VMSetError
            if (online)
            {
                SharedFolderDataMap::iterator it = oldFolders.find (name);
                if (it == oldFolders.end() || it->second.mHostPath != hostPath)
                {
                    /* a new machine folder is added or
                     * the existing machine folder is changed */
                    if (mSharedFolders.find (name) != mSharedFolders.end())
                        ; /* the console folder exists, nothing to do */
                    else
                    {
                        /* remove the old machine folder (when changed)
                         * or the global folder if any (when new) */
                        if (it != oldFolders.end() ||
                            mGlobalSharedFolders.find (name) !=
                                mGlobalSharedFolders.end())
                            rc = removeSharedFolder (name);
                        /* create the new machine folder */
                        rc = createSharedFolder (name, SharedFolderData (hostPath, writable));
                    }
                }
                /* forget the processed (or identical) folder */
                if (it != oldFolders.end())
                    oldFolders.erase (it);

                rc = S_OK;
            }
        }

        AssertComRCReturnRC (rc);

        /* process outdated (removed) folders */
        /// @todo report errors as runtime warnings through VMSetError
        if (online)
        {
            for (SharedFolderDataMap::const_iterator it = oldFolders.begin();
                 it != oldFolders.end(); ++ it)
            {
                if (mSharedFolders.find (it->first) != mSharedFolders.end())
                    ; /* the console folder exists, nothing to do */
                else
                {
                    /* remove the outdated machine folder */
                    rc = removeSharedFolder (it->first);
                    /* create the global folder if there is any */
                    SharedFolderDataMap::const_iterator git =
                        mGlobalSharedFolders.find (it->first);
                    if (git != mGlobalSharedFolders.end())
                        rc = createSharedFolder (git->first, git->second);
                }
            }

            rc = S_OK;
        }
    }

    return rc;
}

/**
 *  Searches for a shared folder with the given name in the list of machine
 *  shared folders and then in the list of the global shared folders.
 *
 *  @param aName    Name of the folder to search for.
 *  @param aIt      Where to store the pointer to the found folder.
 *  @return         @c true if the folder was found and @c false otherwise.
 *
 *  @note The caller must lock this object for reading.
 */
bool Console::findOtherSharedFolder (IN_BSTR aName,
                                     SharedFolderDataMap::const_iterator &aIt)
{
    /* sanity check */
    AssertReturn (isWriteLockOnCurrentThread(), false);

    /* first, search machine folders */
    aIt = mMachineSharedFolders.find (aName);
    if (aIt != mMachineSharedFolders.end())
        return true;

    /* second, search machine folders */
    aIt = mGlobalSharedFolders.find (aName);
    if (aIt != mGlobalSharedFolders.end())
        return true;

    return false;
}

/**
 *  Calls the HGCM service to add a shared folder definition.
 *
 *  @param aName        Shared folder name.
 *  @param aHostPath    Shared folder path.
 *
 *  @note Must be called from under AutoVMCaller and when mpVM != NULL!
 *  @note Doesn't lock anything.
 */
HRESULT Console::createSharedFolder (CBSTR aName, SharedFolderData aData)
{
    ComAssertRet (aName && *aName, E_FAIL);
    ComAssertRet (aData.mHostPath, E_FAIL);

    /* sanity checks */
    AssertReturn (mpVM, E_FAIL);
    AssertReturn (mVMMDev->isShFlActive(), E_FAIL);

    VBOXHGCMSVCPARM  parms[SHFL_CPARMS_ADD_MAPPING];
    SHFLSTRING      *pFolderName, *pMapName;
    size_t           cbString;

    Log (("Adding shared folder '%ls' -> '%ls'\n", aName, aData.mHostPath.raw()));

    cbString = (RTUtf16Len (aData.mHostPath) + 1) * sizeof (RTUTF16);
    if (cbString >= UINT16_MAX)
        return setError (E_INVALIDARG, tr ("The name is too long"));
    pFolderName = (SHFLSTRING *) RTMemAllocZ (sizeof (SHFLSTRING) + cbString);
    Assert (pFolderName);
    memcpy (pFolderName->String.ucs2, aData.mHostPath, cbString);

    pFolderName->u16Size   = (uint16_t)cbString;
    pFolderName->u16Length = (uint16_t)cbString - sizeof(RTUTF16);

    parms[0].type = VBOX_HGCM_SVC_PARM_PTR;
    parms[0].u.pointer.addr = pFolderName;
    parms[0].u.pointer.size = sizeof (SHFLSTRING) + (uint16_t)cbString;

    cbString = (RTUtf16Len (aName) + 1) * sizeof (RTUTF16);
    if (cbString >= UINT16_MAX)
    {
        RTMemFree (pFolderName);
        return setError (E_INVALIDARG, tr ("The host path is too long"));
    }
    pMapName = (SHFLSTRING *) RTMemAllocZ (sizeof(SHFLSTRING) + cbString);
    Assert (pMapName);
    memcpy (pMapName->String.ucs2, aName, cbString);

    pMapName->u16Size   = (uint16_t)cbString;
    pMapName->u16Length = (uint16_t)cbString - sizeof (RTUTF16);

    parms[1].type = VBOX_HGCM_SVC_PARM_PTR;
    parms[1].u.pointer.addr = pMapName;
    parms[1].u.pointer.size = sizeof (SHFLSTRING) + (uint16_t)cbString;

    parms[2].type = VBOX_HGCM_SVC_PARM_32BIT;
    parms[2].u.uint32 = aData.mWritable;

    int vrc = mVMMDev->hgcmHostCall ("VBoxSharedFolders",
                                     SHFL_FN_ADD_MAPPING,
                                     SHFL_CPARMS_ADD_MAPPING, &parms[0]);
    RTMemFree (pFolderName);
    RTMemFree (pMapName);

    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL,
                         tr ("Could not create a shared folder '%ls' "
                             "mapped to '%ls' (%Rrc)"),
                         aName, aData.mHostPath.raw(), vrc);

    return S_OK;
}

/**
 *  Calls the HGCM service to remove the shared folder definition.
 *
 *  @param aName        Shared folder name.
 *
 *  @note Must be called from under AutoVMCaller and when mpVM != NULL!
 *  @note Doesn't lock anything.
 */
HRESULT Console::removeSharedFolder (CBSTR aName)
{
    ComAssertRet (aName && *aName, E_FAIL);

    /* sanity checks */
    AssertReturn (mpVM, E_FAIL);
    AssertReturn (mVMMDev->isShFlActive(), E_FAIL);

    VBOXHGCMSVCPARM  parms;
    SHFLSTRING      *pMapName;
    size_t           cbString;

    Log (("Removing shared folder '%ls'\n", aName));

    cbString = (RTUtf16Len (aName) + 1) * sizeof (RTUTF16);
    if (cbString >= UINT16_MAX)
        return setError (E_INVALIDARG, tr ("The name is too long"));
    pMapName = (SHFLSTRING *) RTMemAllocZ (sizeof (SHFLSTRING) + cbString);
    Assert (pMapName);
    memcpy (pMapName->String.ucs2, aName, cbString);

    pMapName->u16Size   = (uint16_t)cbString;
    pMapName->u16Length = (uint16_t)cbString - sizeof (RTUTF16);

    parms.type = VBOX_HGCM_SVC_PARM_PTR;
    parms.u.pointer.addr = pMapName;
    parms.u.pointer.size = sizeof (SHFLSTRING) + (uint16_t)cbString;

    int vrc = mVMMDev->hgcmHostCall ("VBoxSharedFolders",
                                     SHFL_FN_REMOVE_MAPPING,
                                     1, &parms);
    RTMemFree(pMapName);
    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL,
                         tr ("Could not remove the shared folder '%ls' (%Rrc)"),
                         aName, vrc);

    return S_OK;
}

/**
 *  VM state callback function. Called by the VMM
 *  using its state machine states.
 *
 *  Primarily used to handle VM initiated power off, suspend and state saving,
 *  but also for doing termination completed work (VMSTATE_TERMINATE).
 *
 *  In general this function is called in the context of the EMT.
 *
 *  @param   aVM         The VM handle.
 *  @param   aState      The new state.
 *  @param   aOldState   The old state.
 *  @param   aUser       The user argument (pointer to the Console object).
 *
 *  @note Locks the Console object for writing.
 */
DECLCALLBACK(void)
Console::vmstateChangeCallback (PVM aVM, VMSTATE aState, VMSTATE aOldState,
                                void *aUser)
{
    LogFlowFunc (("Changing state from %d to %d (aVM=%p)\n",
                  aOldState, aState, aVM));

    Console *that = static_cast <Console *> (aUser);
    AssertReturnVoid (that);

    AutoCaller autoCaller (that);

    /* Note that we must let this method proceed even if Console::uninit() has
     * been already called. In such case this VMSTATE change is a result of:
     * 1) powerDown() called from uninit() itself, or
     * 2) VM-(guest-)initiated power off. */
    AssertReturnVoid (autoCaller.isOk() ||
                      autoCaller.state() == InUninit);

    switch (aState)
    {
        /*
         * The VM has terminated
         */
        case VMSTATE_OFF:
        {
            AutoWriteLock alock (that);

            if (that->mVMStateChangeCallbackDisabled)
                break;

            /* Do we still think that it is running? It may happen if this is a
             * VM-(guest-)initiated shutdown/poweroff.
             */
            if (that->mMachineState != MachineState_Stopping &&
                that->mMachineState != MachineState_Saving &&
                that->mMachineState != MachineState_Restoring)
            {
                LogFlowFunc (("VM has powered itself off but Console still "
                              "thinks it is running. Notifying.\n"));

                /* prevent powerDown() from calling VMR3PowerOff() again */
                Assert (that->mVMPoweredOff == false);
                that->mVMPoweredOff = true;

                /* we are stopping now */
                that->setMachineState (MachineState_Stopping);

                /* Setup task object and thread to carry out the operation
                 * asynchronously (if we call powerDown() right here but there
                 * is one or more mpVM callers (added with addVMCaller()) we'll
                 * deadlock).
                 */
                std::auto_ptr <VMProgressTask> task (
                    new VMProgressTask (that, NULL /* aProgress */,
                                        true /* aUsesVMPtr */));

                 /* If creating a task is falied, this can currently mean one of
                  * two: either Console::uninit() has been called just a ms
                  * before (so a powerDown() call is already on the way), or
                  * powerDown() itself is being already executed. Just do
                  * nothing.
                  */
                if (!task->isOk())
                {
                    LogFlowFunc (("Console is already being uninitialized.\n"));
                    break;
                }

                int vrc = RTThreadCreate (NULL, Console::powerDownThread,
                                          (void *) task.get(), 0,
                                          RTTHREADTYPE_MAIN_WORKER, 0,
                                          "VMPowerDown");

                AssertMsgRCBreak (vrc,
                    ("Could not create VMPowerDown thread (%Rrc)\n", vrc));

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
            AutoWriteLock alock (that);

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
                    that->setMachineState (MachineState_PoweredOff);
                    break;
                case MachineState_Saving:
                    /* successfully saved (note that the machine is already in
                     * the Saved state on the server due to EndSavingState()
                     * called from saveStateThread(), so only change the local
                     * state) */
                    that->setMachineStateLocally (MachineState_Saved);
                    break;
                case MachineState_Starting:
                    /* failed to start, but be patient: set back to PoweredOff
                     * (for similarity with the below) */
                    that->setMachineState (MachineState_PoweredOff);
                    break;
                case MachineState_Restoring:
                    /* failed to load the saved state file, but be patient: set
                     * back to Saved (to preserve the saved state file) */
                    that->setMachineState (MachineState_Saved);
                    break;
            }

            break;
        }

        case VMSTATE_SUSPENDED:
        {
            if (aOldState == VMSTATE_RUNNING)
            {
                AutoWriteLock alock (that);

                if (that->mVMStateChangeCallbackDisabled)
                    break;

                /* Change the machine state from Running to Paused */
                Assert (that->mMachineState == MachineState_Running);
                that->setMachineState (MachineState_Paused);
            }

            break;
        }

        case VMSTATE_RUNNING:
        {
            if (aOldState == VMSTATE_CREATED ||
                aOldState == VMSTATE_SUSPENDED)
            {
                AutoWriteLock alock (that);

                if (that->mVMStateChangeCallbackDisabled)
                    break;

                /* Change the machine state from Starting, Restoring or Paused
                 * to Running */
                Assert (   (   (   that->mMachineState == MachineState_Starting
                                || that->mMachineState == MachineState_Paused)
                            && aOldState == VMSTATE_CREATED)
                        || (   (   that->mMachineState == MachineState_Restoring
                                || that->mMachineState == MachineState_Paused)
                            && aOldState == VMSTATE_SUSPENDED));

                that->setMachineState (MachineState_Running);
            }

            break;
        }

        case VMSTATE_GURU_MEDITATION:
        {
            AutoWriteLock alock (that);

            if (that->mVMStateChangeCallbackDisabled)
                break;

            /* Guru respects only running VMs */
            Assert (Global::IsOnline (that->mMachineState));

            that->setMachineState (MachineState_Stuck);

            break;
        }

        default: /* shut up gcc */
            break;
    }
}

#ifdef VBOX_WITH_USB

/**
 *  Sends a request to VMM to attach the given host device.
 *  After this method succeeds, the attached device will appear in the
 *  mUSBDevices collection.
 *
 *  @param aHostDevice  device to attach
 *
 *  @note Synchronously calls EMT.
 *  @note Must be called from under this object's lock.
 */
HRESULT Console::attachUSBDevice (IUSBDevice *aHostDevice, ULONG aMaskedIfs)
{
    AssertReturn (aHostDevice, E_FAIL);
    AssertReturn (isWriteLockOnCurrentThread(), E_FAIL);

    /* still want a lock object because we need to leave it */
    AutoWriteLock alock (this);

    HRESULT hrc;

    /*
     *  Get the address and the Uuid, and call the pfnCreateProxyDevice roothub
     *  method in EMT (using usbAttachCallback()).
     */
    Bstr BstrAddress;
    hrc = aHostDevice->COMGETTER (Address) (BstrAddress.asOutParam());
    ComAssertComRCRetRC (hrc);

    Utf8Str Address (BstrAddress);

    Guid Uuid;
    hrc = aHostDevice->COMGETTER (Id) (Uuid.asOutParam());
    ComAssertComRCRetRC (hrc);

    BOOL fRemote = FALSE;
    hrc = aHostDevice->COMGETTER (Remote) (&fRemote);
    ComAssertComRCRetRC (hrc);

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    LogFlowThisFunc (("Proxying USB device '%s' {%RTuuid}...\n",
                      Address.raw(), Uuid.ptr()));

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

/** @todo just do everything here and only wrap the PDMR3Usb call. That'll offload some notification stuff from the EMT thread. */
    PVMREQ pReq = NULL;
    int vrc = VMR3ReqCall (mpVM, VMREQDEST_ANY, &pReq, RT_INDEFINITE_WAIT,
                           (PFNRT) usbAttachCallback, 6, this, aHostDevice, Uuid.ptr(), fRemote, Address.raw(), aMaskedIfs);
    if (VBOX_SUCCESS (vrc))
        vrc = pReq->iStatus;
    VMR3ReqFree (pReq);

    /* restore the lock */
    alock.enter();

    /* hrc is S_OK here */

    if (VBOX_FAILURE (vrc))
    {
        LogWarningThisFunc (("Failed to create proxy device for '%s' {%RTuuid} (%Rrc)\n",
                             Address.raw(), Uuid.ptr(), vrc));

        switch (vrc)
        {
            case VERR_VUSB_NO_PORTS:
                hrc = setError (E_FAIL,
                    tr ("Failed to attach the USB device.  (No available ports on the USB controller)."));
                break;
            case VERR_VUSB_USBFS_PERMISSION:
                hrc = setError (E_FAIL,
                    tr ("Not permitted to open the USB device, check usbfs options"));
                break;
            default:
                hrc = setError (E_FAIL,
                    tr ("Failed to create a proxy device for the USB device.  (Error: %Rrc)"), vrc);
                break;
        }
    }

    return hrc;
}

/**
 *  USB device attach callback used by AttachUSBDevice().
 *  Note that AttachUSBDevice() doesn't return until this callback is executed,
 *  so we don't use AutoCaller and don't care about reference counters of
 *  interface pointers passed in.
 *
 *  @thread EMT
 *  @note Locks the console object for writing.
 */
//static
DECLCALLBACK(int)
Console::usbAttachCallback (Console *that, IUSBDevice *aHostDevice, PCRTUUID aUuid, bool aRemote, const char *aAddress, ULONG aMaskedIfs)
{
    LogFlowFuncEnter();
    LogFlowFunc (("that={%p}\n", that));

    AssertReturn (that && aUuid, VERR_INVALID_PARAMETER);

    void *pvRemoteBackend = NULL;
    if (aRemote)
    {
        RemoteUSBDevice *pRemoteUSBDevice = static_cast <RemoteUSBDevice *> (aHostDevice);
        Guid guid (*aUuid);

        pvRemoteBackend = that->consoleVRDPServer ()->USBBackendRequestPointer (pRemoteUSBDevice->clientId (), &guid);
        if (!pvRemoteBackend)
            return VERR_INVALID_PARAMETER;  /* The clientId is invalid then. */
    }

    USHORT portVersion = 1;
    HRESULT hrc = aHostDevice->COMGETTER(PortVersion)(&portVersion);
    AssertComRCReturn(hrc, VERR_GENERAL_FAILURE);
    Assert(portVersion == 1 || portVersion == 2);

    int vrc = PDMR3USBCreateProxyDevice (that->mpVM, aUuid, aRemote, aAddress, pvRemoteBackend,
                                         portVersion == 1 ? VUSB_STDVER_11 : VUSB_STDVER_20, aMaskedIfs);
    if (VBOX_SUCCESS (vrc))
    {
        /* Create a OUSBDevice and add it to the device list */
        ComObjPtr <OUSBDevice> device;
        device.createObject();
        HRESULT hrc = device->init (aHostDevice);
        AssertComRC (hrc);

        AutoWriteLock alock (that);
        that->mUSBDevices.push_back (device);
        LogFlowFunc (("Attached device {%RTuuid}\n", device->id().raw()));

        /* notify callbacks */
        that->onUSBDeviceStateChange (device, true /* aAttached */, NULL);
    }

    LogFlowFunc (("vrc=%Rrc\n", vrc));
    LogFlowFuncLeave();
    return vrc;
}

/**
 *  Sends a request to VMM to detach the given host device.  After this method
 *  succeeds, the detached device will disappear from the mUSBDevices
 *  collection.
 *
 *  @param aIt  Iterator pointing to the device to detach.
 *
 *  @note Synchronously calls EMT.
 *  @note Must be called from under this object's lock.
 */
HRESULT Console::detachUSBDevice (USBDeviceList::iterator &aIt)
{
    AssertReturn (isWriteLockOnCurrentThread(), E_FAIL);

    /* still want a lock object because we need to leave it */
    AutoWriteLock alock (this);

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    /* if the device is attached, then there must at least one USB hub. */
    AssertReturn (PDMR3USBHasHub (mpVM), E_FAIL);

    LogFlowThisFunc (("Detaching USB proxy device {%RTuuid}...\n",
                      (*aIt)->id().raw()));

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

    PVMREQ pReq;
/** @todo just do everything here and only wrap the PDMR3Usb call. That'll offload some notification stuff from the EMT thread. */
    int vrc = VMR3ReqCall (mpVM, VMREQDEST_ANY, &pReq, RT_INDEFINITE_WAIT,
                           (PFNRT) usbDetachCallback, 4,
                           this, &aIt, (*aIt)->id().raw());
    if (VBOX_SUCCESS (vrc))
        vrc = pReq->iStatus;
    VMR3ReqFree (pReq);

    ComAssertRCRet (vrc, E_FAIL);

    return S_OK;
}

/**
 *  USB device detach callback used by DetachUSBDevice().
 *  Note that DetachUSBDevice() doesn't return until this callback is executed,
 *  so we don't use AutoCaller and don't care about reference counters of
 *  interface pointers passed in.
 *
 *  @thread EMT
 *  @note Locks the console object for writing.
 */
//static
DECLCALLBACK(int)
Console::usbDetachCallback (Console *that, USBDeviceList::iterator *aIt, PCRTUUID aUuid)
{
    LogFlowFuncEnter();
    LogFlowFunc (("that={%p}\n", that));

    AssertReturn (that && aUuid, VERR_INVALID_PARAMETER);
    ComObjPtr <OUSBDevice> device = **aIt;

    /*
     * If that was a remote device, release the backend pointer.
     * The pointer was requested in usbAttachCallback.
     */
    BOOL fRemote = FALSE;

    HRESULT hrc2 = (**aIt)->COMGETTER (Remote) (&fRemote);
    ComAssertComRC (hrc2);

    if (fRemote)
    {
        Guid guid (*aUuid);
        that->consoleVRDPServer ()->USBBackendReleasePointer (&guid);
    }

    int vrc = PDMR3USBDetachDevice (that->mpVM, aUuid);

    if (VBOX_SUCCESS (vrc))
    {
        AutoWriteLock alock (that);

        /* Remove the device from the collection */
        that->mUSBDevices.erase (*aIt);
        LogFlowFunc (("Detached device {%RTuuid}\n", device->id().raw()));

        /* notify callbacks */
        that->onUSBDeviceStateChange (device, false /* aAttached */, NULL);
    }

    LogFlowFunc (("vrc=%Rrc\n", vrc));
    LogFlowFuncLeave();
    return vrc;
}

#endif /* VBOX_WITH_USB */


/**
 *  Helper function to handle host interface device creation and attachment.
 *
 *  @param   networkAdapter the network adapter which attachment should be reset
 *  @return  COM status code
 *
 *  @note The caller must lock this object for writing.
 */
HRESULT Console::attachToBridgedInterface(INetworkAdapter *networkAdapter)
{
#if !defined(RT_OS_LINUX) || defined(VBOX_WITH_NETFLT)
    /*
     * Nothing to do here.
     *
     * Note, the reason for this method in the first place a memory / fork
     * bug on linux. All this code belongs in DrvTAP and similar places.
     */
    NOREF(networkAdapter);
    return S_OK;

#else /* RT_OS_LINUX && !VBOX_WITH_NETFLT */

    LogFlowThisFunc(("\n"));
    /* sanity check */
    AssertReturn (isWriteLockOnCurrentThread(), E_FAIL);

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

    /*
     * Allocate a host interface device
     */
    int rcVBox = RTFileOpen(&maTapFD[slot], "/dev/net/tun",
                            RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_INHERIT);
    if (VBOX_SUCCESS(rcVBox))
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
            tapDeviceName.setNull();  /* Is this necessary? */
        if (tapDeviceName.isEmpty())
        {
            LogRel(("No TAP device name was supplied.\n"));
            rc = setError(E_FAIL, tr ("No TAP device name was supplied for the host networking interface"));
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
                rc = setError(E_FAIL, tr ("Failed to open the host network interface %ls"),
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
                Log(("attachToBridgedInterface: %RTfile %ls\n", maTapFD[slot], tapDeviceName.raw()));
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
                rc = setError(E_FAIL, tr ("could not set up the host networking device for non blocking access: %s"),
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
                rc = setError(E_FAIL, tr ("Could not set up the host networking device: %Rrc"), rcVBox);
                break;
        }
    }
    /* in case of failure, cleanup. */
    if (VBOX_FAILURE(rcVBox) && SUCCEEDED(rc))
    {
        LogRel(("General failure attaching to host interface\n"));
        rc = setError(E_FAIL, tr ("General failure attaching to host interface"));
    }
    LogFlowThisFunc(("rc=%d\n", rc));
    return rc;
#endif /* RT_OS_LINUX */
}

/**
 *  Helper function to handle detachment from a host interface
 *
 *  @param   networkAdapter the network adapter which attachment should be reset
 *  @return  COM status code
 *
 *  @note The caller must lock this object for writing.
 */
HRESULT Console::detachFromBridgedInterface(INetworkAdapter *networkAdapter)
{
#if !defined(RT_OS_LINUX) || defined(VBOX_WITH_NETFLT)
    /*
     * Nothing to do here.
     */
    NOREF(networkAdapter);
    return S_OK;

#else /* RT_OS_LINUX */

    /* sanity check */
    LogFlowThisFunc(("\n"));
    AssertReturn (isWriteLockOnCurrentThread(), E_FAIL);

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
               so that the termination script can remove the interface.  Otherwise we still
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
#endif /* RT_OS_LINUX */
}


/**
 *  Called at power down to terminate host interface networking.
 *
 *  @note The caller must lock this object for writing.
 */
HRESULT Console::powerDownHostInterfaces()
{
    LogFlowThisFunc (("\n"));

    /* sanity check */
    AssertReturn (isWriteLockOnCurrentThread(), E_FAIL);

    /*
     * host interface termination handling
     */
    HRESULT rc;
    for (ULONG slot = 0; slot < SchemaDefs::NetworkAdapterCount; slot ++)
    {
        ComPtr<INetworkAdapter> networkAdapter;
        rc = mMachine->GetNetworkAdapter(slot, networkAdapter.asOutParam());
        CheckComRCBreakRC (rc);

        BOOL enabled = FALSE;
        networkAdapter->COMGETTER(Enabled) (&enabled);
        if (!enabled)
            continue;

        NetworkAttachmentType_T attachment;
        networkAdapter->COMGETTER(AttachmentType)(&attachment);
        if (attachment == NetworkAttachmentType_Bridged)
        {
            HRESULT rc2 = detachFromBridgedInterface(networkAdapter);
            if (FAILED(rc2) && SUCCEEDED(rc))
                rc = rc2;
        }
    }

    return rc;
}


/**
 *  Process callback handler for VMR3Load and VMR3Save.
 *
 *  @param   pVM         The VM handle.
 *  @param   uPercent    Completetion precentage (0-100).
 *  @param   pvUser      Pointer to the VMProgressTask structure.
 *  @return  VINF_SUCCESS.
 */
/*static*/ DECLCALLBACK (int)
Console::stateProgressCallback (PVM pVM, unsigned uPercent, void *pvUser)
{
    VMProgressTask *task = static_cast <VMProgressTask *> (pvUser);
    AssertReturn (task, VERR_INVALID_PARAMETER);

    /* update the progress object */
    if (task->mProgress)
        task->mProgress->notifyProgress (uPercent);

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
/* static */ DECLCALLBACK (void)
Console::setVMErrorCallback (PVM pVM, void *pvUser, int rc, RT_SRC_POS_DECL,
                             const char *pszFormat, va_list args)
{
    VMProgressTask *task = static_cast <VMProgressTask *> (pvUser);
    AssertReturnVoid (task);

    /* we ignore RT_SRC_POS_DECL arguments to avoid confusion of end-users */
    va_list va2;
    va_copy (va2, args); /* Have to make a copy here or GCC will break. */

    /* append to the existing error message if any */
    if (!task->mErrorMsg.isEmpty())
        task->mErrorMsg = Utf8StrFmt ("%s.\n%N (%Rrc)", task->mErrorMsg.raw(),
                                      pszFormat, &va2, rc, rc);
    else
        task->mErrorMsg = Utf8StrFmt ("%N (%Rrc)",
                                      pszFormat, &va2, rc, rc);

    va_end (va2);
}

/**
 * VM runtime error callback function.
 * See VMSetRuntimeError for the detailed description of parameters.
 *
 * @param   pVM             The VM handle.
 * @param   pvUser          The user argument.
 * @param   fFatal          Whether it is a fatal error or not.
 * @param   pszErrorID      Error ID string.
 * @param   pszFormat       Error message format string.
 * @param   args            Error message arguments.
 * @thread EMT.
 */
/* static */ DECLCALLBACK(void)
Console::setVMRuntimeErrorCallback (PVM pVM, void *pvUser, bool fFatal,
                                    const char *pszErrorID,
                                    const char *pszFormat, va_list args)
{
    LogFlowFuncEnter();

    Console *that = static_cast <Console *> (pvUser);
    AssertReturnVoid (that);

    Utf8Str message = Utf8StrFmtVA (pszFormat, args);

    LogRel (("Console: VM runtime error: fatal=%RTbool, "
             "errorID=%s message=\"%s\"\n",
             fFatal, pszErrorID, message.raw()));

    that->onRuntimeError (BOOL (fFatal), Bstr (pszErrorID), Bstr (message));

    LogFlowFuncLeave();
}

/**
 *  Captures USB devices that match filters of the VM.
 *  Called at VM startup.
 *
 *  @param   pVM     The VM handle.
 *
 *  @note The caller must lock this object for writing.
 */
HRESULT Console::captureUSBDevices (PVM pVM)
{
    LogFlowThisFunc (("\n"));

    /* sanity check */
    ComAssertRet (isWriteLockOnCurrentThread(), E_FAIL);

    /* If the machine has an USB controller, ask the USB proxy service to
     * capture devices */
    PPDMIBASE pBase;
    int vrc = PDMR3QueryLun (pVM, "usb-ohci", 0, 0, &pBase);
    if (VBOX_SUCCESS (vrc))
    {
        /* leave the lock before calling Host in VBoxSVC since Host may call
         * us back from under its lock (e.g. onUSBDeviceAttach()) which would
         * produce an inter-process dead-lock otherwise. */
        AutoWriteLock alock (this);
        alock.leave();

        HRESULT hrc = mControl->AutoCaptureUSBDevices();
        ComAssertComRCRetRC (hrc);
    }
    else if (   vrc == VERR_PDM_DEVICE_NOT_FOUND
             || vrc == VERR_PDM_DEVICE_INSTANCE_NOT_FOUND)
        vrc = VINF_SUCCESS;
    else
        AssertRC (vrc);

    return VBOX_SUCCESS (vrc) ? S_OK : E_FAIL;
}


/**
 *  Detach all USB device which are attached to the VM for the
 *  purpose of clean up and such like.
 *
 *  @note The caller must lock this object for writing.
 */
void Console::detachAllUSBDevices (bool aDone)
{
    LogFlowThisFunc (("aDone=%RTbool\n", aDone));

    /* sanity check */
    AssertReturnVoid (isWriteLockOnCurrentThread());

    mUSBDevices.clear();

    /* leave the lock before calling Host in VBoxSVC since Host may call
     * us back from under its lock (e.g. onUSBDeviceAttach()) which would
     * produce an inter-process dead-lock otherwise. */
    AutoWriteLock alock (this);
    alock.leave();

    mControl->DetachAllUSBDevices (aDone);
}

/**
 *  @note Locks this object for writing.
 */
void Console::processRemoteUSBDevices (uint32_t u32ClientId, VRDPUSBDEVICEDESC *pDevList, uint32_t cbDevList)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("u32ClientId = %d, pDevList=%p, cbDevList = %d\n", u32ClientId, pDevList, cbDevList));

    AutoCaller autoCaller (this);
    if (!autoCaller.isOk())
    {
        /* Console has been already uninitialized, deny request */
        AssertMsgFailed (("Temporary assertion to prove that it happens, "
                          "please report to dmik\n"));
        LogFlowThisFunc (("Console is already uninitialized\n"));
        LogFlowThisFuncLeave();
        return;
    }

    AutoWriteLock alock (this);

    /*
     * Mark all existing remote USB devices as dirty.
     */
    RemoteUSBDeviceList::iterator it = mRemoteUSBDevices.begin();
    while (it != mRemoteUSBDevices.end())
    {
        (*it)->dirty (true);
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
        LogFlowThisFunc (("vendor %04X, product %04X, name = %s\n",
                          e->idVendor, e->idProduct,
                          e->oProduct? (char *)e + e->oProduct: ""));

        bool fNewDevice = true;

        it = mRemoteUSBDevices.begin();
        while (it != mRemoteUSBDevices.end())
        {
            if ((*it)->devId () == e->id
                && (*it)->clientId () == u32ClientId)
            {
               /* The device is already in the list. */
               (*it)->dirty (false);
               fNewDevice = false;
               break;
            }

            ++ it;
        }

        if (fNewDevice)
        {
            LogRel(("Remote USB: ++++ Vendor %04X. Product %04X. Name = [%s].\n",
                        e->idVendor, e->idProduct, e->oProduct? (char *)e + e->oProduct: ""
                    ));

            /* Create the device object and add the new device to list. */
            ComObjPtr <RemoteUSBDevice> device;
            device.createObject();
            device->init (u32ClientId, e);

            mRemoteUSBDevices.push_back (device);

            /* Check if the device is ok for current USB filters. */
            BOOL fMatched = FALSE;
            ULONG fMaskedIfs = 0;

            HRESULT hrc = mControl->RunUSBDeviceFilters(device, &fMatched, &fMaskedIfs);

            AssertComRC (hrc);

            LogFlowThisFunc (("USB filters return %d %#x\n", fMatched, fMaskedIfs));

            if (fMatched)
            {
                hrc = onUSBDeviceAttach (device, NULL, fMaskedIfs);

                /// @todo (r=dmik) warning reporting subsystem

                if (hrc == S_OK)
                {
                    LogFlowThisFunc (("Device attached\n"));
                    device->captured (true);
                }
            }
        }

        if (cbDevList < e->oNext)
        {
            LogWarningThisFunc (("cbDevList %d > oNext %d\n",
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
        ComObjPtr <RemoteUSBDevice> device;

        RemoteUSBDeviceList::iterator it = mRemoteUSBDevices.begin();
        while (it != mRemoteUSBDevices.end())
        {
            if ((*it)->dirty ())
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
        device->COMGETTER(VendorId) (&vendorId);

        USHORT productId = 0;
        device->COMGETTER(ProductId) (&productId);

        Bstr product;
        device->COMGETTER(Product) (product.asOutParam());

        LogRel(("Remote USB: ---- Vendor %04X. Product %04X. Name = [%ls].\n",
                    vendorId, productId, product.raw ()
                ));

        /* Detach the device from VM. */
        if (device->captured ())
        {
            Guid uuid;
            device->COMGETTER (Id) (uuid.asOutParam());
            onUSBDeviceDetach (uuid, NULL);
        }

        /* And remove it from the list. */
        mRemoteUSBDevices.erase (it);
    }

    LogFlowThisFuncLeave();
}

/**
 *  Thread function which starts the VM (also from saved state) and
 *  track progress.
 *
 *  @param   Thread      The thread id.
 *  @param   pvUser      Pointer to a VMPowerUpTask structure.
 *  @return  VINF_SUCCESS (ignored).
 *
 *  @note Locks the Console object for writing.
 */
/*static*/
DECLCALLBACK (int) Console::powerUpThread (RTTHREAD Thread, void *pvUser)
{
    LogFlowFuncEnter();

    std::auto_ptr <VMPowerUpTask> task (static_cast <VMPowerUpTask *> (pvUser));
    AssertReturn (task.get(), VERR_INVALID_PARAMETER);

    AssertReturn (!task->mConsole.isNull(), VERR_INVALID_PARAMETER);
    AssertReturn (!task->mProgress.isNull(), VERR_INVALID_PARAMETER);

#if defined(RT_OS_WINDOWS)
    {
        /* initialize COM */
        HRESULT hrc = CoInitializeEx (NULL,
                                      COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE |
                                      COINIT_SPEED_OVER_MEMORY);
        LogFlowFunc (("CoInitializeEx()=%08X\n", hrc));
    }
#endif

    HRESULT rc = S_OK;
    int vrc = VINF_SUCCESS;

    /* Set up a build identifier so that it can be seen from core dumps what
     * exact build was used to produce the core. */
    static char saBuildID[40];
    RTStrPrintf(saBuildID, sizeof(saBuildID), "%s%s%s%s VirtualBox %s r%d %s%s%s%s",
                "BU", "IL", "DI", "D", VBOX_VERSION_STRING, VBoxSVNRev (), "BU", "IL", "DI", "D");

    ComObjPtr <Console> console = task->mConsole;

    /* Note: no need to use addCaller() because VMPowerUpTask does that */

    /* The lock is also used as a signal from the task initiator (which
     * releases it only after RTThreadCreate()) that we can start the job */
    AutoWriteLock alock (console);

    /* sanity */
    Assert (console->mpVM == NULL);

    try
    {
        /* wait for auto reset ops to complete so that we can successfully lock
         * the attached hard disks by calling LockMedia() below */
        for (VMPowerUpTask::ProgressList::const_iterator
             it = task->hardDiskProgresses.begin();
             it != task->hardDiskProgresses.end(); ++ it)
        {
            HRESULT rc2 = (*it)->WaitForCompletion (-1);
            AssertComRC (rc2);
        }

        /* lock attached media. This method will also check their
         * accessibility. Note that the media will be unlocked automatically
         * by SessionMachine::setMachineState() when the VM is powered down. */
        rc = console->mControl->LockMedia();
        CheckComRCThrowRC (rc);

#ifdef VBOX_WITH_VRDP

        /* Create the VRDP server. In case of headless operation, this will
         * also create the framebuffer, required at VM creation.
         */
        ConsoleVRDPServer *server = console->consoleVRDPServer();
        Assert (server);

        /// @todo (dmik)
        //      does VRDP server call Console from the other thread?
        //      Not sure, so leave the lock just in case
        alock.leave();
        vrc = server->Launch();
        alock.enter();

        if (VBOX_FAILURE (vrc))
        {
            Utf8Str errMsg;
            switch (vrc)
            {
                case VERR_NET_ADDRESS_IN_USE:
                {
                    ULONG port = 0;
                    console->mVRDPServer->COMGETTER(Port) (&port);
                    errMsg = Utf8StrFmt (tr ("VRDP server port %d is already in use"),
                                         port);
                    break;
                }
                case VERR_FILE_NOT_FOUND:
                {
                    errMsg = Utf8StrFmt (tr ("Could not load the VRDP library"));
                    break;
                }
                default:
                    errMsg = Utf8StrFmt (tr ("Failed to launch VRDP server (%Rrc)"),
                                         vrc);
            }
            LogRel (("Failed to launch VRDP server (%Rrc), error message: '%s'\n",
                     vrc, errMsg.raw()));
            throw setError (E_FAIL, errMsg);
        }

#endif /* VBOX_WITH_VRDP */

        ULONG cCpus = 1;
#ifdef VBOX_WITH_SMP_GUESTS
        pMachine->COMGETTER(CPUCount)(&cCpus);
#endif

        /*
         * Create the VM
         */
        PVM pVM;
        /*
         *  leave the lock since EMT will call Console. It's safe because
         *  mMachineState is either Starting or Restoring state here.
         */
        alock.leave();

        vrc = VMR3Create (cCpus, task->mSetVMErrorCallback, task.get(),
                          task->mConfigConstructor, static_cast <Console *> (console),
                          &pVM);

        alock.enter();

#ifdef VBOX_WITH_VRDP
        /* Enable client connections to the server. */
        console->consoleVRDPServer()->EnableConnections ();
#endif /* VBOX_WITH_VRDP */

        if (VBOX_SUCCESS (vrc))
        {
            do
            {
                /*
                 *  Register our load/save state file handlers
                 */
                vrc = SSMR3RegisterExternal (pVM,
                    sSSMConsoleUnit, 0 /* iInstance */, sSSMConsoleVer,
                    0 /* cbGuess */,
                    NULL, saveStateFileExec, NULL, NULL, loadStateFileExec, NULL,
                    static_cast <Console *> (console));
                AssertRC (vrc);
                if (VBOX_FAILURE (vrc))
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
                    /// @todo (dmik)
                    //      does the code below call Console from the other thread?
                    //      Not sure, so leave the lock just in case
                    alock.leave();

                    for (SharedFolderDataMap::const_iterator
                             it = task->mSharedFolders.begin();
                         it != task->mSharedFolders.end();
                         ++ it)
                    {
                        rc = console->createSharedFolder ((*it).first, (*it).second);
                        CheckComRCBreakRC (rc);
                    }

                    /* enter the lock again */
                    alock.enter();

                    CheckComRCBreakRC (rc);
                }

                /*
                 * Capture USB devices.
                 */
                rc = console->captureUSBDevices (pVM);
                CheckComRCBreakRC (rc);

                /* leave the lock before a lengthy operation */
                alock.leave();

                /* Load saved state? */
                if (!!task->mSavedStateFile)
                {
                    LogFlowFunc (("Restoring saved state from '%s'...\n",
                                  task->mSavedStateFile.raw()));

                    vrc = VMR3Load (pVM, task->mSavedStateFile,
                                    Console::stateProgressCallback,
                                    static_cast <VMProgressTask *> (task.get()));

                    if (VBOX_SUCCESS (vrc))
                    {
                        if (task->mStartPaused)
                            /* done */
                            console->setMachineState (MachineState_Paused);
                        else
                        {
                            /* Start/Resume the VM execution */
                            vrc = VMR3Resume (pVM);
                            AssertRC (vrc);
                        }
                    }

                    /* Power off in case we failed loading or resuming the VM */
                    if (VBOX_FAILURE (vrc))
                    {
                        int vrc2 = VMR3PowerOff (pVM);
                        AssertRC (vrc2);
                    }
                }
                else if (task->mStartPaused)
                    /* done */
                    console->setMachineState (MachineState_Paused);
                else
                {
                    /* Power on the VM (i.e. start executing) */
                    vrc = VMR3PowerOn(pVM);
                    AssertRC (vrc);
                }

                /* enter the lock again */
                alock.enter();
            }
            while (0);

            /*  On failure, destroy the VM */
            if (FAILED (rc) || VBOX_FAILURE (vrc))
            {
                /* preserve existing error info */
                ErrorInfoKeeper eik;

                /* powerDown() will call VMR3Destroy() and do all necessary
                 * cleanup (VRDP, USB devices) */
                HRESULT rc2 = console->powerDown();
                AssertComRC (rc2);
            }
        }
        else
        {
            /*
             * If VMR3Create() failed it has released the VM memory.
             */
            console->mpVM = NULL;
        }

        if (SUCCEEDED (rc) && VBOX_FAILURE (vrc))
        {
            /* If VMR3Create() or one of the other calls in this function fail,
             * an appropriate error message has been set in task->mErrorMsg.
             * However since that happens via a callback, the rc status code in
             * this function is not updated.
             */
            if (task->mErrorMsg.isNull())
            {
                /* If the error message is not set but we've got a failure,
                 * convert the VBox status code into a meaningfulerror message.
                 * This becomes unused once all the sources of errors set the
                 * appropriate error message themselves.
                 */
                AssertMsgFailed (("Missing error message during powerup for "
                                  "status code %Rrc\n", vrc));
                task->mErrorMsg = Utf8StrFmt (
                    tr ("Failed to start VM execution (%Rrc)"), vrc);
            }

            /* Set the error message as the COM error.
             * Progress::notifyComplete() will pick it up later. */
            throw setError (E_FAIL, task->mErrorMsg);
        }
    }
    catch (HRESULT aRC) { rc = aRC; }

    if (console->mMachineState == MachineState_Starting ||
        console->mMachineState == MachineState_Restoring)
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

        Assert (console->mpVM == NULL);
        vmstateChangeCallback (NULL, VMSTATE_TERMINATED, VMSTATE_CREATING,
                               console);
    }

    /*
     * Evaluate the final result. Note that the appropriate mMachineState value
     * is already set by vmstateChangeCallback() in all cases.
     */

    /* leave the lock, don't need it any more */
    alock.leave();

    if (SUCCEEDED (rc))
    {
        /* Notify the progress object of the success */
        task->mProgress->notifyComplete (S_OK);
    }
    else
    {
        /* The progress object will fetch the current error info */
        task->mProgress->notifyComplete (rc);

        LogRel (("Power up failed (vrc=%Rrc, rc=%Rhrc (%#08X))\n", vrc, rc, rc));
    }

#if defined(RT_OS_WINDOWS)
    /* uninitialize COM */
    CoUninitialize();
#endif

    LogFlowFuncLeave();

    return VINF_SUCCESS;
}


/**
 *  Reconfigures a VDI.
 *
 *  @param   pVM           The VM handle.
 *  @param   lInstance     The instance of the controller.
 *  @param   enmController The type of the controller.
 *  @param   hda           The harddisk attachment.
 *  @param   phrc          Where to store com error - only valid if we return VERR_GENERAL_FAILURE.
 *  @return  VBox status code.
 */
static DECLCALLBACK(int) reconfigureHardDisks(PVM pVM, ULONG lInstance,
                                              StorageControllerType_T enmController,
                                              IHardDiskAttachment *hda,
                                              HRESULT *phrc)
{
    LogFlowFunc (("pVM=%p hda=%p phrc=%p\n", pVM, hda, phrc));

    int             rc;
    HRESULT         hrc;
    Bstr            bstr;
    *phrc = S_OK;
#define RC_CHECK()  do { if (VBOX_FAILURE(rc)) { AssertMsgFailed(("rc=%Rrc\n", rc)); return rc; } } while (0)
#define H() do { if (FAILED(hrc)) { AssertMsgFailed(("hrc=%Rhrc (%#x)\n", hrc, hrc)); *phrc = hrc; return VERR_GENERAL_FAILURE; } } while (0)

    /*
     * Figure out which IDE device this is.
     */
    ComPtr<IHardDisk> hardDisk;
    hrc = hda->COMGETTER(HardDisk)(hardDisk.asOutParam());                      H();
    LONG lDev;
    hrc = hda->COMGETTER(Device)(&lDev);                                        H();
    LONG lPort;
    hrc = hda->COMGETTER(Port)(&lPort);                                         H();

    int         iLUN;
    const char *pcszDevice = NULL;
    bool        fSCSI = false;

    switch (enmController)
    {
        case StorageControllerType_PIIX3:
        case StorageControllerType_PIIX4:
        case StorageControllerType_ICH6:
        {
            if (lPort >= 2 || lPort < 0)
            {
                AssertMsgFailed(("invalid controller channel number: %d\n", lPort));
                return VERR_GENERAL_FAILURE;
            }

            if (lDev >= 2 || lDev < 0)
            {
                AssertMsgFailed(("invalid controller device number: %d\n", lDev));
                return VERR_GENERAL_FAILURE;
            }

            iLUN = 2*lPort + lDev;
            pcszDevice = "piix3ide";
            break;
        }
        case StorageControllerType_IntelAhci:
        {
            iLUN = lPort;
            pcszDevice = "ahci";
            break;
        }
        case StorageControllerType_BusLogic:
        {
            iLUN = lPort;
            pcszDevice = "buslogic";
            fSCSI = true;
            break;
        }
        case StorageControllerType_LsiLogic:
        {
            iLUN = lPort;
            pcszDevice = "lsilogicscsi";
            fSCSI = true;
            break;
        }
        default:
        {
            AssertMsgFailed(("invalid disk controller type: %d\n", enmController));
            return VERR_GENERAL_FAILURE;
        }
    }

    /** @todo this should be unified with the relevant part of
    * Console::configConstructor to avoid inconsistencies. */

    /*
     * Is there an existing LUN? If not create it.
     * We ASSUME that this will NEVER collide with the DVD.
     */
    PCFGMNODE pCfg;
    PCFGMNODE pLunL1;
    PCFGMNODE pLunL2;

    /* SCSI has an extra driver between the device and the block driver. */
    if (fSCSI)
        pLunL1 = CFGMR3GetChildF(CFGMR3GetRoot(pVM), "Devices/%s/%u/LUN#%d/AttachedDriver/AttachedDriver/", pcszDevice, lInstance, iLUN);
    else
        pLunL1 = CFGMR3GetChildF(CFGMR3GetRoot(pVM), "Devices/%s/%u/LUN#%d/AttachedDriver/", pcszDevice, lInstance, iLUN);

    if (!pLunL1)
    {
        PCFGMNODE pInst = CFGMR3GetChildF(CFGMR3GetRoot(pVM), "Devices/%s/%u/", pcszDevice, lInstance);
        AssertReturn(pInst, VERR_INTERNAL_ERROR);

        PCFGMNODE pLunL0;
        rc = CFGMR3InsertNodeF(pInst, &pLunL0, "LUN#%d", iLUN);                     RC_CHECK();

        if (fSCSI)
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
        rc = PDMR3DeviceDetach(pVM, pcszDevice, 0, iLUN);                            RC_CHECK();
        CFGMR3RemoveNode(pCfg);
        rc = CFGMR3InsertNode(pLunL1, "Config", &pCfg);                             RC_CHECK();
    }

    /*
     * Create the driver configuration.
     */
    hrc = hardDisk->COMGETTER(Location)(bstr.asOutParam());                     H();
    LogFlowFunc (("LUN#%d: leaf location '%ls'\n", iLUN, bstr.raw()));
    rc = CFGMR3InsertString(pCfg, "Path", Utf8Str(bstr));                       RC_CHECK();
    hrc = hardDisk->COMGETTER(Format)(bstr.asOutParam());                       H();
    LogFlowFunc (("LUN#%d: leaf format '%ls'\n", iLUN, bstr.raw()));
    rc = CFGMR3InsertString(pCfg, "Format", Utf8Str(bstr));                     RC_CHECK();

#if defined(VBOX_WITH_PDM_ASYNC_COMPLETION)
    if (bstr == L"VMDK")
    {
        /* Create cfgm nodes for async transport driver because VMDK is
         * currently the only one which may support async I/O. This has
         * to be made generic based on the capabiliy flags when the new
         * HardDisk interface is merged.
         */
        rc = CFGMR3InsertNode (pLunL1, "AttachedDriver", &pLunL2);      RC_CHECK();
        rc = CFGMR3InsertString (pLunL2, "Driver", "TransportAsync");   RC_CHECK();
        /* The async transport driver has no config options yet. */
    }
#endif

    /* Pass all custom parameters. */
    bool fHostIP = true;
    SafeArray <BSTR> names;
    SafeArray <BSTR> values;
    hrc = hardDisk->GetProperties (NULL,
                                   ComSafeArrayAsOutParam (names),
                                   ComSafeArrayAsOutParam (values));    H();

    if (names.size() != 0)
    {
        PCFGMNODE pVDC;
        rc = CFGMR3InsertNode (pCfg, "VDConfig", &pVDC);                RC_CHECK();
        for (size_t i = 0; i < names.size(); ++ i)
        {
            if (values [i])
            {
                Utf8Str name = names [i];
                Utf8Str value = values [i];
                rc = CFGMR3InsertString (pVDC, name, value);
                if (    !(name.compare("HostIPStack"))
                    &&  !(value.compare("0")))
                    fHostIP = false;
            }
        }
    }

    /* Create an inversed tree of parents. */
    ComPtr<IHardDisk> parentHardDisk = hardDisk;
    for (PCFGMNODE pParent = pCfg;;)
    {
        hrc = parentHardDisk->COMGETTER(Parent)(hardDisk.asOutParam());     H();
        if (hardDisk.isNull())
            break;

        PCFGMNODE pCur;
        rc = CFGMR3InsertNode(pParent, "Parent", &pCur);                        RC_CHECK();
        hrc = hardDisk->COMGETTER(Location)(bstr.asOutParam());                 H();
        rc = CFGMR3InsertString(pCur,  "Path", Utf8Str(bstr));                  RC_CHECK();

        hrc = hardDisk->COMGETTER(Format)(bstr.asOutParam());                   H();
        rc = CFGMR3InsertString(pCur,  "Format", Utf8Str(bstr));                RC_CHECK();

        /* Pass all custom parameters. */
        SafeArray <BSTR> names;
        SafeArray <BSTR> values;
        hrc = hardDisk->GetProperties (NULL,
                                       ComSafeArrayAsOutParam (names),
                                       ComSafeArrayAsOutParam (values));H();

        if (names.size() != 0)
        {
            PCFGMNODE pVDC;
            rc = CFGMR3InsertNode (pCur, "VDConfig", &pVDC);            RC_CHECK();
            for (size_t i = 0; i < names.size(); ++ i)
            {
                if (values [i])
                {
                    Utf8Str name = names [i];
                    Utf8Str value = values [i];
                    rc = CFGMR3InsertString (pVDC, name, value);
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
            rc = CFGMR3InsertInteger (pCfg, "HostIPStack", 0);          RC_CHECK();
        }


        /* next */
        pParent = pCur;
        parentHardDisk = hardDisk;
    }

    CFGMR3Dump(CFGMR3GetRoot(pVM));

    /*
     * Attach the new driver.
     */
    rc = PDMR3DeviceAttach(pVM, pcszDevice, 0, iLUN, NULL);                      RC_CHECK();

    LogFlowFunc (("Returns success\n"));
    return rc;
}


/**
 *  Thread for executing the saved state operation.
 *
 *  @param   Thread      The thread handle.
 *  @param   pvUser      Pointer to a VMSaveTask structure.
 *  @return  VINF_SUCCESS (ignored).
 *
 *  @note Locks the Console object for writing.
 */
/*static*/
DECLCALLBACK (int) Console::saveStateThread (RTTHREAD Thread, void *pvUser)
{
    LogFlowFuncEnter();

    std::auto_ptr <VMSaveTask> task (static_cast <VMSaveTask *> (pvUser));
    AssertReturn (task.get(), VERR_INVALID_PARAMETER);

    Assert (!task->mSavedStateFile.isNull());
    Assert (!task->mProgress.isNull());

    const ComObjPtr <Console> &that = task->mConsole;

    /*
     *  Note: no need to use addCaller() to protect Console or addVMCaller() to
     *  protect mpVM because VMSaveTask does that
     */

    Utf8Str errMsg;
    HRESULT rc = S_OK;

    if (task->mIsSnapshot)
    {
        Assert (!task->mServerProgress.isNull());
        LogFlowFunc (("Waiting until the server creates differencing VDIs...\n"));

        rc = task->mServerProgress->WaitForCompletion (-1);
        if (SUCCEEDED (rc))
        {
            HRESULT result = S_OK;
            rc = task->mServerProgress->COMGETTER(ResultCode) (&result);
            if (SUCCEEDED (rc))
                rc = result;
        }
    }

    if (SUCCEEDED (rc))
    {
        LogFlowFunc (("Saving the state to '%s'...\n", task->mSavedStateFile.raw()));

        int vrc = VMR3Save (that->mpVM, task->mSavedStateFile,
                            Console::stateProgressCallback,
                            static_cast <VMProgressTask *> (task.get()));
        if (VBOX_FAILURE (vrc))
        {
            errMsg = Utf8StrFmt (
                Console::tr ("Failed to save the machine state to '%s' (%Rrc)"),
                task->mSavedStateFile.raw(), vrc);
            rc = E_FAIL;
        }
    }

    /* lock the console once we're going to access it */
    AutoWriteLock thatLock (that);

    if (SUCCEEDED (rc))
    {
        if (task->mIsSnapshot)
        do
        {
            LogFlowFunc (("Reattaching new differencing hard disks...\n"));

            com::SafeIfaceArray <IHardDiskAttachment> atts;
            rc = that->mMachine->
                COMGETTER(HardDiskAttachments) (ComSafeArrayAsOutParam (atts));
            if (FAILED (rc))
                break;
            for (size_t i = 0; i < atts.size(); ++ i)
            {
                PVMREQ pReq;
                ComPtr<IStorageController> controller;
                BSTR controllerName;
                ULONG lInstance;
                StorageControllerType_T enmController;

                /*
                 * We can't pass a storage controller object directly
                 * (g++ complains about not being able to pass non POD types through '...')
                 * so we have to query needed values here and pass them.
                 */
                rc = atts[i]->COMGETTER(Controller)(&controllerName);
                if (FAILED (rc))
                    break;

                rc = that->mMachine->GetStorageControllerByName(controllerName, controller.asOutParam());
                if (FAILED (rc))
                    break;

                rc = controller->COMGETTER(ControllerType)(&enmController);
                rc = controller->COMGETTER(Instance)(&lInstance);
                /*
                 *  don't leave the lock since reconfigureHardDisks isn't going
                 *  to access Console.
                 */
                int vrc = VMR3ReqCall (that->mpVM, VMREQDEST_ANY, &pReq, RT_INDEFINITE_WAIT,
                                       (PFNRT)reconfigureHardDisks, 5, that->mpVM, lInstance,
                                       enmController, atts [i], &rc);
                if (VBOX_SUCCESS (rc))
                    rc = pReq->iStatus;
                VMR3ReqFree (pReq);
                if (FAILED (rc))
                    break;
                if (VBOX_FAILURE (vrc))
                {
                    errMsg = Utf8StrFmt (Console::tr ("%Rrc"), vrc);
                    rc = E_FAIL;
                    break;
                }
            }
        }
        while (0);
    }

    /* finalize the procedure regardless of the result */
    if (task->mIsSnapshot)
    {
        /*
         *  finalize the requested snapshot object.
         *  This will reset the machine state to the state it had right
         *  before calling mControl->BeginTakingSnapshot().
         */
        that->mControl->EndTakingSnapshot (SUCCEEDED (rc));
    }
    else
    {
        /*
         *  finalize the requested save state procedure.
         *  In case of success, the server will set the machine state to Saved;
         *  in case of failure it will reset the it to the state it had right
         *  before calling mControl->BeginSavingState().
         */
        that->mControl->EndSavingState (SUCCEEDED (rc));
    }

    /* synchronize the state with the server */
    if (task->mIsSnapshot || FAILED (rc))
    {
        if (task->mLastMachineState == MachineState_Running)
        {
            /* restore the paused state if appropriate */
            that->setMachineStateLocally (MachineState_Paused);
            /* restore the running state if appropriate */
            that->Resume();
        }
        else
            that->setMachineStateLocally (task->mLastMachineState);
    }
    else
    {
        /*
         *  The machine has been successfully saved, so power it down
         *  (vmstateChangeCallback() will set state to Saved on success).
         *  Note: we release the task's VM caller, otherwise it will
         *  deadlock.
         */
        task->releaseVMCaller();

        rc = that->powerDown();
    }

    /* notify the progress object about operation completion */
    if (SUCCEEDED (rc))
        task->mProgress->notifyComplete (S_OK);
    else
    {
        if (!errMsg.isNull())
            task->mProgress->notifyComplete (rc,
                COM_IIDOF(IConsole), Console::getComponentName(), errMsg);
        else
            task->mProgress->notifyComplete (rc);
    }

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

/**
 *  Thread for powering down the Console.
 *
 *  @param   Thread      The thread handle.
 *  @param   pvUser      Pointer to the VMTask structure.
 *  @return  VINF_SUCCESS (ignored).
 *
 *  @note Locks the Console object for writing.
 */
/*static*/
DECLCALLBACK (int) Console::powerDownThread (RTTHREAD Thread, void *pvUser)
{
    LogFlowFuncEnter();

    std::auto_ptr <VMProgressTask> task (static_cast <VMProgressTask *> (pvUser));
    AssertReturn (task.get(), VERR_INVALID_PARAMETER);

    AssertReturn (task->isOk(), VERR_GENERAL_FAILURE);

    const ComObjPtr <Console> &that = task->mConsole;

    /* Note: no need to use addCaller() to protect Console because VMTask does
     * that */

    /* wait until the method tat started us returns */
    AutoWriteLock thatLock (that);

    /* release VM caller to avoid the powerDown() deadlock */
    task->releaseVMCaller();

    that->powerDown (task->mProgress);

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
        if (VBOX_FAILURE(rc))
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
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
DECLCALLBACK(int) Console::drvStatus_Construct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVMAINSTATUS pData = PDMINS_2_DATA(pDrvIns, PDRVMAINSTATUS);
    LogFlowFunc(("iInstance=%d\n", pDrvIns->iInstance));

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "papLeds\0First\0Last\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;
    PPDMIBASE pBaseIgnore;
    int rc = pDrvIns->pDrvHlp->pfnAttach(pDrvIns, &pBaseIgnore);
    if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
    {
        AssertMsgFailed(("Configuration error: Not possible to attach anything to this driver!\n"));
        return VERR_PDM_DRVINS_NO_ATTACH;
    }

    /*
     * Data.
     */
    pDrvIns->IBase.pfnQueryInterface        = Console::drvStatus_QueryInterface;
    pData->ILedConnectors.pfnUnitChanged    = Console::drvStatus_UnitChanged;

    /*
     * Read config.
     */
    rc = CFGMR3QueryPtr(pCfgHandle, "papLeds", (void **)&pData->papLeds);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Failed to query the \"papLeds\" value! rc=%Rrc\n", rc));
        return rc;
    }

    rc = CFGMR3QueryU32(pCfgHandle, "First", &pData->iFirstLUN);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->iFirstLUN = 0;
    else if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Failed to query the \"First\" value! rc=%Rrc\n", rc));
        return rc;
    }

    rc = CFGMR3QueryU32(pCfgHandle, "Last", &pData->iLastLUN);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->iLastLUN = 0;
    else if (VBOX_FAILURE(rc))
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

    for (unsigned i = pData->iFirstLUN; i <= pData->iLastLUN; i++)
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
    /* pfnDetach */
    NULL
};
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
