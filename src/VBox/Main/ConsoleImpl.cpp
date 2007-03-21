/** @file
 *
 * VBox Console COM Class implementation
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#if defined(__WIN__)
#elif defined(__LINUX__)
#   include <errno.h>
#   include <sys/ioctl.h>
#   include <sys/poll.h>
#   include <sys/fcntl.h>
#   include <net/if.h>
#   include <linux/if_tun.h>
#endif

#include "ConsoleImpl.h"
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

// generated header
#include "SchemaDefs.h"

#include "Logging.h"

#include <iprt/string.h>
#include <iprt/asm.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/process.h>
#include <iprt/ldr.h>

#include <VBox/vmapi.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/vusb.h>
#include <VBox/mm.h>
#include <VBox/ssm.h>
#include <VBox/version.h>

#include <VBox/VBoxDev.h>

#include <VBox/HostServices/VBoxClipboardSvc.h>

#include <set>
#include <algorithm>
#include <memory> // for auto_ptr


// VMTask and friends
////////////////////////////////////////////////////////////////////////////////

/**
 *  Task structure for asynchronous VM operations.
 *
 *  Once created, the task structure adds itself as a Console caller.
 *  This means:
 *
 *  1. The user must check for #rc() before using the created structure
 *     (e.g. passing it as a thread function argument). If #rc() returns a
 *     failure, the Console object may not be used by the task (see
       Console::addCaller() for more details).
 *  2. On successful initialization, the structure keeps the Console caller
 *     until destruction (to ensure Console remains in the Ready state and won't
 *     be accidentially uninitialized). Forgetting to delete the created task
 *     will lead to Console::uninit() stuck waiting for releasing all added
 *     callers.
 *
 *  If \a aUsesVMPtr parameter is true, the task structure will also add itself
 *  as a Console::mpVM caller with the same meaning as above. See
 *  Console::addVMCaller() for more info.
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
};

struct VMPowerUpTask : public VMProgressTask
{
    VMPowerUpTask (Console *aConsole, Progress *aProgress)
        : VMProgressTask (aConsole, aProgress, false /* aUsesVMPtr */)
        , mSetVMErrorCallback (NULL), mConfigConstructor (NULL) {}

    PFNVMATERROR mSetVMErrorCallback;
    PFNCFGMCONSTRUCTOR mConfigConstructor;
    Utf8Str mSavedStateFile;
    std::map <Bstr, ComPtr <ISharedFolder> > mSharedFolders;
};

struct VMSaveTask : public VMProgressTask
{
    VMSaveTask (Console *aConsole, Progress *aProgress)
        : VMProgressTask (aConsole, aProgress, true /* aUsesVMPtr */)
        , mIsSnapshot (false)
        , mLastMachineState (MachineState_InvalidMachineState) {}

    bool mIsSnapshot;
    Utf8Str mSavedStateFile;
    MachineState_T mLastMachineState;
    ComPtr <IProgress> mServerProgress;
};


// constructor / desctructor
/////////////////////////////////////////////////////////////////////////////

Console::Console()
    : mSavedStateDataLoaded (false)
    , mConsoleVRDPServer (NULL)
    , mpVM (NULL)
    , mVMCallers (0)
    , mVMZeroCallersSem (NIL_RTSEMEVENT)
    , mVMDestroying (false)
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
    memset(mapNetworkLeds, 0, sizeof(mapNetworkLeds));

#ifdef VBOX_WITH_UNIXY_TAP_NETWORKING
    Assert(ELEMENTS(maTapFD) == ELEMENTS(maTAPDeviceName));
    Assert(ELEMENTS(maTapFD) >= SchemaDefs::NetworkAdapterCount);
    for (unsigned i = 0; i < ELEMENTS(maTapFD); i++)
    {
        maTapFD[i] = NIL_RTFILE;
        maTAPDeviceName[i] = "";
    }
#endif

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
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aMachine=%p, aControl=%p\n", aMachine, aControl));

    HRESULT rc = E_FAIL;

    unconst (mMachine) = aMachine;
    unconst (mControl) = aControl;

    /* Cache essential properties and objects */

    rc = mMachine->COMGETTER(State) (&mMachineState);
    AssertComRCReturn (rc, rc);

#ifdef VBOX_VRDP
    rc = mMachine->COMGETTER(VRDPServer) (unconst (mVRDPServer).asOutParam());
    AssertComRCReturn (rc, rc);
#endif

    rc = mMachine->COMGETTER(DVDDrive) (unconst (mDVDDrive).asOutParam());
    AssertComRCReturn (rc, rc);

    rc = mMachine->COMGETTER(FloppyDrive) (unconst (mFloppyDrive).asOutParam());
    AssertComRCReturn (rc, rc);

    /* Create associated child COM objects */

    unconst (mGuest).createObject();
    rc = mGuest->init (this);
    AssertComRCReturn (rc, rc);

    unconst (mKeyboard).createObject();
    rc = mKeyboard->init (this);
    AssertComRCReturn (rc, rc);

    unconst (mMouse).createObject();
    rc = mMouse->init (this);
    AssertComRCReturn (rc, rc);

    unconst (mDisplay).createObject();
    rc = mDisplay->init (this);
    AssertComRCReturn (rc, rc);

    unconst (mRemoteDisplayInfo).createObject();
    rc = mRemoteDisplayInfo->init (this);
    AssertComRCReturn (rc, rc);

    /* Create other child objects */

    unconst (mConsoleVRDPServer) = new ConsoleVRDPServer (this);
    AssertReturn (mConsoleVRDPServer, E_FAIL);

#ifdef VRDP_MC
    m_cAudioRefs = 0;
#endif /* VRDP_MC */

    unconst (mVMMDev) = new VMMDev(this);
    AssertReturn (mVMMDev, E_FAIL);

    unconst (mAudioSniffer) = new AudioSniffer(this);
    AssertReturn (mAudioSniffer, E_FAIL);

    memset (&mCallbackData, 0, sizeof (mCallbackData));

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
     * Uninit all children that ise addDependentChild()/removeDependentChild()
     * in their init()/uninit() methods.
     */
    uninitDependentChildren();

    /* This should be the first, since this may cause detaching remote USB devices. */
    if (mConsoleVRDPServer)
    {
        delete mConsoleVRDPServer;
        unconst (mConsoleVRDPServer) = NULL;
    }

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

    unconst (mFloppyDrive).setNull();
    unconst (mDVDDrive).setNull();
#ifdef VBOX_VRDP
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

#ifdef VRDP_MC
DECLCALLBACK(int) Console::vrdp_ClientLogon (void *pvUser,
                                             uint32_t u32ClientId,
                                             const char *pszUser,
                                             const char *pszPassword,
                                             const char *pszDomain)
#else
DECLCALLBACK(int) Console::vrdp_ClientLogon (void *pvUser, const char *pszUser,
                                             const char *pszPassword,
                                             const char *pszDomain)
#endif /* VRDP_MC */
{
    LogFlowFuncEnter();
#ifdef VRDP_MC
    LogFlowFunc (("%d, %s, %s, %s\n", u32ClientId, pszUser, pszPassword, pszDomain));
#else
    LogFlowFunc (("%s, %s, %s\n", pszUser, pszPassword, pszDomain));
#endif /* VRDP_MC */

    Console *console = static_cast <Console *> (pvUser);
    AssertReturn (console, VERR_INVALID_POINTER);

    AutoCaller autoCaller (console);
    if (!autoCaller.isOk())
    {
        /* Console has been already uninitialized, deny request */
        LogRel(("VRDPAUTH: Access denied (Console uninitialized).\n"));
        LogFlowFuncLeave();
        return VERR_ACCESS_DENIED;
    }

    Guid uuid;
    HRESULT hrc = console->mMachine->COMGETTER (Id) (uuid.asOutParam());
    AssertComRCReturn (hrc, VERR_ACCESS_DENIED);

    VRDPAuthType_T authType = VRDPAuthType_VRDPAuthNull;
    hrc = console->mVRDPServer->COMGETTER(AuthType) (&authType);
    AssertComRCReturn (hrc, VERR_ACCESS_DENIED);

    ULONG authTimeout = 0;
    hrc = console->mVRDPServer->COMGETTER(AuthTimeout) (&authTimeout);
    AssertComRCReturn (hrc, VERR_ACCESS_DENIED);

    VRDPAuthResult result = VRDPAuthAccessDenied;
    VRDPAuthGuestJudgement guestJudgement = VRDPAuthGuestNotAsked;

    LogFlowFunc(("Auth type %d\n", authType));

    LogRel (("VRDPAUTH: User: [%s]. Domain: [%s]. Authentication type: [%s]\n",
                pszUser, pszDomain,
                authType == VRDPAuthType_VRDPAuthNull?
                    "null":
                    (authType == VRDPAuthType_VRDPAuthExternal?
                        "external":
                        (authType == VRDPAuthType_VRDPAuthGuest?
                            "guest":
                            "INVALID"
                        )
                    )
            ));

    switch (authType)
    {
        case VRDPAuthType_VRDPAuthNull:
        {
            result = VRDPAuthAccessGranted;
            break;
        }

        case VRDPAuthType_VRDPAuthExternal:
        {
            /* Call the external library. */
            result = console->mConsoleVRDPServer->Authenticate (uuid, guestJudgement, pszUser, pszPassword, pszDomain);

            if (result != VRDPAuthDelegateToGuest)
            {
                break;
            }

            LogRel(("VRDPAUTH: Delegated to guest.\n"));

            LogFlowFunc (("External auth asked for guest judgement\n"));
        } /* pass through */

        case VRDPAuthType_VRDPAuthGuest:
        {
            guestJudgement = VRDPAuthGuestNotReacted;

            if (console->mVMMDev)
            {
                /* Issue the request to guest. Assume that the call does not require EMT. It should not. */

                /* Ask the guest to judge these credentials. */
                uint32_t u32GuestFlags = VMMDEV_SETCREDENTIALS_JUDGE;

                int rc = console->mVMMDev->getVMMDevPort()->pfnSetCredentials (console->mVMMDev->getVMMDevPort(),
                             pszUser, pszPassword, pszDomain, u32GuestFlags);

                if (VBOX_SUCCESS (rc))
                {
                    /* Wait for guest. */
                    rc = console->mVMMDev->WaitCredentialsJudgement (authTimeout, &u32GuestFlags);

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
                        LogFlowFunc (("Wait for credentials judgement rc = %Vrc!!!\n", rc));
                    }

                    LogFlowFunc (("Guest judgement %d\n", guestJudgement));
                }
                else
                {
                    LogFlowFunc (("Could not set credentials rc = %Vrc!!!\n", rc));
                }
            }

            if (authType == VRDPAuthType_VRDPAuthExternal)
            {
                LogRel(("VRDPAUTH: Guest judgement %d.\n", guestJudgement));
                LogFlowFunc (("External auth called again with guest judgement = %d\n", guestJudgement));
                result = console->mConsoleVRDPServer->Authenticate (uuid, guestJudgement, pszUser, pszPassword, pszDomain);
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

    if (result == VRDPAuthAccessGranted)
    {
        LogRel(("VRDPAUTH: Access granted.\n"));
        return VINF_SUCCESS;
    }

    /* Reject. */
    LogRel(("VRDPAUTH: Access denied.\n"));
    return VERR_ACCESS_DENIED;
}

#ifdef VRDP_MC
DECLCALLBACK(void) Console::vrdp_ClientConnect (void *pvUser,
                                                uint32_t u32ClientId)
#else
DECLCALLBACK(void) Console::vrdp_ClientConnect (void *pvUser,
                                                uint32_t fu32SupportedOrders)
#endif /* VRDP_MC */
{
    LogFlowFuncEnter();

    Console *console = static_cast <Console *> (pvUser);
    AssertReturnVoid (console);

    AutoCaller autoCaller (console);
    AssertComRCReturnVoid (autoCaller.rc());

#ifdef VBOX_VRDP
#ifdef VRDP_MC
    NOREF(u32ClientId);
    console->mDisplay->VideoAccelVRDP (true);
#else
    console->mDisplay->VideoAccelVRDP (true, fu32SupportedOrders);
#endif /* VRDP_MC */
#endif /* VBOX_VRDP */

    LogFlowFuncLeave();
    return;
}

#ifdef VRDP_MC
DECLCALLBACK(void) Console::vrdp_ClientDisconnect (void *pvUser,
                                                   uint32_t u32ClientId,
                                                   uint32_t fu32Intercepted)
#else
DECLCALLBACK(void) Console::vrdp_ClientDisconnect (void *pvUser)
#endif /* VRDP_MC */
{
    LogFlowFuncEnter();

    Console *console = static_cast <Console *> (pvUser);
    AssertReturnVoid (console);

    AutoCaller autoCaller (console);
    AssertComRCReturnVoid (autoCaller.rc());

    AssertReturnVoid (console->mConsoleVRDPServer);

#ifdef VBOX_VRDP
#ifdef VRDP_MC
    console->mDisplay->VideoAccelVRDP (false);
#else
    console->mDisplay->VideoAccelVRDP (false, 0);
#endif /* VRDP_MC */
#endif /* VBOX_VRDP */

#ifdef VRDP_MC
    if (fu32Intercepted & VRDP_CLIENT_INTERCEPT_USB)
    {
        console->mConsoleVRDPServer->USBBackendDelete (u32ClientId);
    }
#else
    console->mConsoleVRDPServer->DeleteUSBBackend ();
#endif /* VRDP_MC */

#ifdef VBOX_VRDP
#ifdef VRDP_MC
    if (fu32Intercepted & VRDP_CLIENT_INTERCEPT_AUDIO)
    {
        console->m_cAudioRefs--;

        if (console->m_cAudioRefs <= 0)
        {
            if (console->mAudioSniffer)
            {
                PPDMIAUDIOSNIFFERPORT port = console->mAudioSniffer->getAudioSnifferPort();
                if (port)
                {
                    port->pfnSetup (port, false, false);
                }
            }
        }
    }
#else
    if (console->mAudioSniffer)
    {
        PPDMIAUDIOSNIFFERPORT port = console->mAudioSniffer->getAudioSnifferPort();
        if (port)
        {
            port->pfnSetup (port, false, false);
        }
    }
#endif /* VRDP_MC */
#endif /* VBOX_VRDP */

    LogFlowFuncLeave();
    return;
}

#ifdef VRDP_MC
DECLCALLBACK(void) Console::vrdp_InterceptAudio (void *pvUser,
                                                 uint32_t u32ClientId)
#else
DECLCALLBACK(void) Console::vrdp_InterceptAudio (void *pvUser, bool fKeepHostAudio)
#endif /* VRDP_MC */
{
    LogFlowFuncEnter();

    Console *console = static_cast <Console *> (pvUser);
    AssertReturnVoid (console);

    AutoCaller autoCaller (console);
    AssertComRCReturnVoid (autoCaller.rc());

#ifdef VRDP_MC
    LogFlowFunc (("mAudioSniffer %p, u32ClientId %d.\n",
                  console->mAudioSniffer, u32ClientId));
    NOREF(u32ClientId);
#else
    LogFlowFunc (("mAudioSniffer %p, keepHostAudio %d.\n",
                  console->mAudioSniffer, fKeepHostAudio));
#endif /* VRDP_MC */

#ifdef VBOX_VRDP
#ifdef VRDP_MC
    console->m_cAudioRefs++;

    if (console->m_cAudioRefs == 1)
    {
        if (console->mAudioSniffer)
        {
            PPDMIAUDIOSNIFFERPORT port = console->mAudioSniffer->getAudioSnifferPort();
            if (port)
            {
                port->pfnSetup (port, true, true);
            }
        }
    }
#else
    if (console->mAudioSniffer)
    {
        PPDMIAUDIOSNIFFERPORT port = console->mAudioSniffer->getAudioSnifferPort();
        if (port)
        {
            port->pfnSetup (port, true, !!fKeepHostAudio);
        }
    }
#endif /* VRDP_MC */
#endif

    LogFlowFuncLeave();
    return;
}

#ifdef VRDP_MC
DECLCALLBACK(void) Console::vrdp_InterceptUSB (void *pvUser,
                                               uint32_t u32ClientId,
                                               PFNVRDPUSBCALLBACK *ppfn,
                                               void **ppv)
#else
DECLCALLBACK(void) Console::vrdp_InterceptUSB (void *pvUser, PFNVRDPUSBCALLBACK *ppfn, void **ppv)
#endif /* VRDP_MC */
{
    LogFlowFuncEnter();

    Console *console = static_cast <Console *> (pvUser);
    AssertReturnVoid (console);

    AutoCaller autoCaller (console);
    AssertComRCReturnVoid (autoCaller.rc());

    AssertReturnVoid (console->mConsoleVRDPServer);

#ifdef VRDP_MC
    console->mConsoleVRDPServer->USBBackendCreate (u32ClientId, ppfn, ppv);
#else
    console->mConsoleVRDPServer->CreateUSBBackend (ppfn, ppv);
#endif /* VRDP_MC */

    LogFlowFuncLeave();
    return;
}

// static
VRDPSERVERCALLBACK Console::sVrdpServerCallback =
{
    vrdp_ClientLogon,
    vrdp_ClientConnect,
    vrdp_ClientDisconnect,
    vrdp_InterceptAudio,
    vrdp_InterceptUSB
};

//static
char *Console::sSSMConsoleUnit = "ConsoleData";
//static
uint32_t Console::sSSMConsoleVer = 0x00010000;

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
        if (version == sSSMConsoleVer)
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
        rc = setError (E_FAIL,
            tr ("The saved state file '%ls' is invalid (%Vrc). "
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

    AutoReaderLock alock (that);

    int vrc = SSMR3PutU32 (pSSM, that->mSharedFolders.size());
    AssertRC (vrc);

    for (SharedFolderList::const_iterator it = that->mSharedFolders.begin();
         it != that->mSharedFolders.end();
         ++ it)
    {
        ComObjPtr <SharedFolder> folder = (*it);
        AutoLock folderLock (folder);

        Utf8Str name = folder->name();
        vrc = SSMR3PutU32 (pSSM, name.length() + 1 /* term. 0 */);
        AssertRC (vrc);
        vrc = SSMR3PutStrZ (pSSM, name);
        AssertRC (vrc);

        Utf8Str hostPath = folder->hostPath();
        vrc = SSMR3PutU32 (pSSM, hostPath.length() + 1 /* term. 0 */);
        AssertRC (vrc);
        vrc = SSMR3PutStrZ (pSSM, hostPath);
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

    if (u32Version != 0 && u32Version != sSSMConsoleVer)
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

    AutoLock alock (that);

    AssertReturn (that->mSharedFolders.size() == 0, VERR_INTERNAL_ERROR);

    uint32_t size = 0;
    int vrc = SSMR3GetU32 (pSSM, &size);
    AssertRCReturn (vrc, vrc);

    for (uint32_t i = 0; i < size; ++ i)
    {
        Bstr name;
        Bstr hostPath;

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

        ComObjPtr <SharedFolder> sharedFolder;
        sharedFolder.createObject();
        HRESULT rc = sharedFolder->init (that, name, hostPath);
        AssertComRCReturn (rc, VERR_INTERNAL_ERROR);
        if (FAILED (rc))
            return rc;

        that->mSharedFolders.push_back (sharedFolder);
    }

    return VINF_SUCCESS;
}

// IConsole properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Console::COMGETTER(Machine) (IMachine **aMachine)
{
    if (!aMachine)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mMachine is constant during life time, no need to lock */
    mMachine.queryInterfaceTo (aMachine);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(State) (MachineState_T *aMachineState)
{
    if (!aMachineState)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    /* we return our local state (since it's always the same as on the server) */
    *aMachineState = mMachineState;

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(Guest) (IGuest **aGuest)
{
    if (!aGuest)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mGuest is constant during life time, no need to lock */
    mGuest.queryInterfaceTo (aGuest);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(Keyboard) (IKeyboard **aKeyboard)
{
    if (!aKeyboard)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mKeyboard is constant during life time, no need to lock */
    mKeyboard.queryInterfaceTo (aKeyboard);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(Mouse) (IMouse **aMouse)
{
    if (!aMouse)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mMouse is constant during life time, no need to lock */
    mMouse.queryInterfaceTo (aMouse);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(Display) (IDisplay **aDisplay)
{
    if (!aDisplay)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mDisplay is constant during life time, no need to lock */
    mDisplay.queryInterfaceTo (aDisplay);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(Debugger) (IMachineDebugger **aDebugger)
{
    if (!aDebugger)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* we need a write lock because of the lazy mDebugger initialization*/
    AutoLock alock (this);

    /* check if we have to create the debugger object */
    if (!mDebugger)
    {
        unconst (mDebugger).createObject();
        mDebugger->init (this);
    }

    mDebugger.queryInterfaceTo (aDebugger);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(USBDevices) (IUSBDeviceCollection **aUSBDevices)
{
    if (!aUSBDevices)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    ComObjPtr <OUSBDeviceCollection> collection;
    collection.createObject();
    collection->init (mUSBDevices);
    collection.queryInterfaceTo (aUSBDevices);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(RemoteUSBDevices) (IHostUSBDeviceCollection **aRemoteUSBDevices)
{
    if (!aRemoteUSBDevices)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    ComObjPtr <RemoteUSBDeviceCollection> collection;
    collection.createObject();
    collection->init (mRemoteUSBDevices);
    collection.queryInterfaceTo (aRemoteUSBDevices);

    return S_OK;
}

STDMETHODIMP Console::COMGETTER(RemoteDisplayInfo) (IRemoteDisplayInfo **aRemoteDisplayInfo)
{
    if (!aRemoteDisplayInfo)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mDisplay is constant during life time, no need to lock */
    mRemoteDisplayInfo.queryInterfaceTo (aRemoteDisplayInfo);

    return S_OK;
}

STDMETHODIMP
Console::COMGETTER(SharedFolders) (ISharedFolderCollection **aSharedFolders)
{
    if (!aSharedFolders)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* loadDataFromSavedState() needs a write lock */
    AutoLock alock (this);

    /* Read console data stored in the saved state file (if not yet done) */
    HRESULT rc = loadDataFromSavedState();
    CheckComRCReturnRC (rc);

    ComObjPtr <SharedFolderCollection> coll;
    coll.createObject();
    coll->init (mSharedFolders);
    coll.queryInterfaceTo (aSharedFolders);

    return S_OK;
}

// IConsole methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Console::PowerUp (IProgress **aProgress)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("mMachineState=%d\n", mMachineState));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mMachineState >= MachineState_Running)
        return setError(E_FAIL, tr ("Cannot power up the machine as it is already running.  (Machine state: %d)"), mMachineState);

    /*
     * First check whether all disks are accessible. This is not a 100%
     * bulletproof approach (race condition, it might become inaccessible
     * right after the check) but it's convenient as it will cover 99.9%
     * of the cases and here, we're able to provide meaningful error
     * information.
     */
    ComPtr<IHardDiskAttachmentCollection> coll;
    mMachine->COMGETTER(HardDiskAttachments)(coll.asOutParam());
    ComPtr<IHardDiskAttachmentEnumerator> enumerator;
    coll->Enumerate(enumerator.asOutParam());
    BOOL fHasMore;
    while (SUCCEEDED(enumerator->HasMore(&fHasMore)) && fHasMore)
    {
        ComPtr<IHardDiskAttachment> attach;
        enumerator->GetNext(attach.asOutParam());
        ComPtr<IHardDisk> hdd;
        attach->COMGETTER(HardDisk)(hdd.asOutParam());
        Assert(hdd);
        BOOL fAccessible;
        HRESULT rc = hdd->COMGETTER(AllAccessible)(&fAccessible);
        CheckComRCReturnRC (rc);
        if (!fAccessible)
        {
            Bstr loc;
            hdd->COMGETTER(Location) (loc.asOutParam());
            Bstr errMsg;
            hdd->COMGETTER(LastAccessError) (errMsg.asOutParam());
            return setError (E_FAIL,
                tr ("VM cannot start because the hard disk '%ls' is not accessible "
                    "(%ls)"),
                loc.raw(), errMsg.raw());
        }
    }

    /* now perform the same check if a ISO is mounted */
    ComPtr<IDVDDrive> dvdDrive;
    mMachine->COMGETTER(DVDDrive)(dvdDrive.asOutParam());
    ComPtr<IDVDImage> dvdImage;
    dvdDrive->GetImage(dvdImage.asOutParam());
    if (dvdImage)
    {
        BOOL fAccessible;
        HRESULT rc = dvdImage->COMGETTER(Accessible)(&fAccessible);
        CheckComRCReturnRC (rc);
        if (!fAccessible)
        {
            Bstr filePath;
            dvdImage->COMGETTER(FilePath)(filePath.asOutParam());
            /// @todo (r=dmik) grab the last access error once
            //  IDVDImage::lastAccessError is there
            return setError (E_FAIL,
                tr ("VM cannot start because the DVD image '%ls' is not accessible"),
                filePath.raw());
        }
    }

    /* now perform the same check if a floppy is mounted */
    ComPtr<IFloppyDrive> floppyDrive;
    mMachine->COMGETTER(FloppyDrive)(floppyDrive.asOutParam());
    ComPtr<IFloppyImage> floppyImage;
    floppyDrive->GetImage(floppyImage.asOutParam());
    if (floppyImage)
    {
        BOOL fAccessible;
        HRESULT rc = floppyImage->COMGETTER(Accessible)(&fAccessible);
        CheckComRCReturnRC (rc);
        if (!fAccessible)
        {
            Bstr filePath;
            floppyImage->COMGETTER(FilePath)(filePath.asOutParam());
            /// @todo (r=dmik) grab the last access error once
            //  IDVDImage::lastAccessError is there
            return setError (E_FAIL,
                tr ("VM cannot start because the floppy image '%ls' is not accessible"),
                filePath.raw());
        }
    }

    /* now the network cards will undergo a quick consistency check */
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
            case NetworkAttachmentType_HostInterfaceNetworkAttachment:
            {
#ifdef __WIN__
                /* a valid host interface must have been set */
                Bstr hostif;
                adapter->COMGETTER(HostInterface)(hostif.asOutParam());
                if (!hostif)
                {
                    return setError (E_FAIL,
                        tr ("VM cannot start because host interface networking "
                            "requires a host interface name to be set"));
                }
                ComPtr<IVirtualBox> virtualBox;
                mMachine->COMGETTER(Parent)(virtualBox.asOutParam());
                ComPtr<IHost> host;
                virtualBox->COMGETTER(Host)(host.asOutParam());
                ComPtr<IHostNetworkInterfaceCollection> coll;
                host->COMGETTER(NetworkInterfaces)(coll.asOutParam());
                ComPtr<IHostNetworkInterface> hostInterface;
                if (!SUCCEEDED(coll->FindByName(hostif, hostInterface.asOutParam())))
                {
                    return setError (E_FAIL,
                        tr ("VM cannot start because the host interface '%ls' "
                            "does not exist"),
                        hostif.raw());
                }
#endif /* __WIN__ */
                break;
            }
            default:
                break;
        }
    }

    /* Read console data stored in the saved state file (if not yet done) */
    {
        HRESULT rc = loadDataFromSavedState();
        CheckComRCReturnRC (rc);
    }

    /* Check all types of shared folders and compose a single list */
    std::map <Bstr, ComPtr <ISharedFolder> > sharedFolders;
    {
        /// @todo (dmik) check and add globally shared folders when they are
        //  done

        ComPtr <ISharedFolderCollection> coll;
        HRESULT rc = mMachine->COMGETTER(SharedFolders) (coll.asOutParam());
        CheckComRCReturnRC (rc);
        ComPtr <ISharedFolderEnumerator> en;
        rc = coll->Enumerate (en.asOutParam());
        CheckComRCReturnRC (rc);

        BOOL hasMore = FALSE;
        while (SUCCEEDED (en->HasMore (&hasMore)) && hasMore)
        {
            ComPtr <ISharedFolder> folder;
            en->GetNext (folder.asOutParam());

            Bstr name;
            rc = folder->COMGETTER(Name) (name.asOutParam());
            CheckComRCReturnRC (rc);

            BOOL accessible = FALSE;
            rc = folder->COMGETTER(Accessible) (&accessible);
            CheckComRCReturnRC (rc);

            if (!accessible)
            {
                Bstr hostPath;
                folder->COMGETTER(HostPath) (hostPath.asOutParam());
                return setError (E_FAIL,
                    tr ("Host path '%ls' of the shared folder '%ls' is not accessible"),
                    hostPath.raw(), name.raw());
            }

            sharedFolders.insert (std::make_pair (name, folder));
            /// @todo (dmik) later, do this:
//            if (!sharedFolders.insert (std::pair <name, folder>).second)
//                return setError (E_FAIL,
//                    tr ("Could not accept a permanently shared folder named '%ls' "
//                       "because a globally shared folder with the same name "
//                       "already exists"),
//                    name.raw());
        }

        for (SharedFolderList::const_iterator it = mSharedFolders.begin();
             it != mSharedFolders.end(); ++ it)
        {
            ComPtr <ISharedFolder> folder = static_cast <SharedFolder *> (*it);

            if (!sharedFolders.insert (std::make_pair ((*it)->name(), folder)).second)
                return setError (E_FAIL,
                    tr ("Could not create a transient shared folder named '%ls' "
                       "because a global or a permanent shared folder with "
                       "the same name already exists"),
                    (*it)->name().raw());
        }
    }

    Bstr savedStateFile;

    /*
     * Saved VMs will have to prove that their saved states are kosher.
     */
    if (mMachineState == MachineState_Saved)
    {
        HRESULT rc = mMachine->COMGETTER(StateFilePath) (savedStateFile.asOutParam());
        CheckComRCReturnRC (rc);
        ComAssertRet (!!savedStateFile, E_FAIL);
        int vrc = SSMR3ValidateFile (Utf8Str (savedStateFile));
        if (VBOX_FAILURE (vrc))
            return setError (E_FAIL,
                tr ("VM cannot start because the saved state file '%ls' is invalid (%Vrc). "
                    "Discard the saved state prior to starting the VM"),
                    savedStateFile.raw(), vrc);
    }

    /* create an IProgress object to track progress of this operation */
    ComObjPtr <Progress> progress;
    progress.createObject();
    Bstr progressDesc;
    if (mMachineState == MachineState_Saved)
        progressDesc = tr ("Restoring the virtual machine");
    else
        progressDesc = tr ("Starting the virtual machine");
    progress->init ((IConsole *) this, progressDesc, FALSE /* aCancelable */);

    /* pass reference to caller if requested */
    if (aProgress)
        progress.queryInterfaceTo (aProgress);

    /* setup task object and thread to carry out the operation asynchronously */
    std::auto_ptr <VMPowerUpTask> task (new VMPowerUpTask (this, progress));
    ComAssertComRCRetRC (task->rc());

    task->mSetVMErrorCallback = setVMErrorCallback;
    task->mConfigConstructor = configConstructor;
    task->mSharedFolders = sharedFolders;
    if (mMachineState == MachineState_Saved)
        task->mSavedStateFile = savedStateFile;

    int vrc = RTThreadCreate (NULL, Console::powerUpThread, (void *) task.get(),
                              0, RTTHREADTYPE_MAIN_WORKER, 0, "VMPowerUp");

    ComAssertMsgRCRet (vrc, ("Could not create VMPowerUp thread (%Vrc)\n", vrc),
                       E_FAIL);

    /* task is now owned by powerUpThread(), so release it */
    task.release();

    if (mMachineState == MachineState_Saved)
        setMachineState (MachineState_Restoring);
    else
        setMachineState (MachineState_Starting);

    LogFlowThisFunc (("mMachineState=%d\n", mMachineState));
    LogFlowThisFuncLeave();
    return S_OK;
}

STDMETHODIMP Console::PowerDown()
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("mMachineState=%d\n", mMachineState));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mMachineState != MachineState_Running &&
        mMachineState != MachineState_Paused)
    {
        /* extra nice error message for a common case */
        if (mMachineState == MachineState_Saved)
            return setError(E_FAIL, tr ("Cannot power off a saved machine"));
        else
            return setError(E_FAIL, tr ("Cannot power off the machine as it is not running or paused.  (Machine state: %d)"), mMachineState);
    }

    LogFlowThisFunc (("Sending SHUTDOWN request...\n"));

    HRESULT rc = powerDown();

    LogFlowThisFunc (("mMachineState=%d, rc=%08X\n", mMachineState, rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::Reset()
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("mMachineState=%d\n", mMachineState));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mMachineState != MachineState_Running)
        return setError(E_FAIL, tr ("Cannot reset the machine as it is not running.  (Machine state: %d)"), mMachineState);

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

    int vrc = VMR3Reset (mpVM);

    HRESULT rc = VBOX_SUCCESS (vrc) ? S_OK :
        setError (E_FAIL, tr ("Could not reset the machine.  (Error: %Vrc)"), vrc);

    LogFlowThisFunc (("mMachineState=%d, rc=%08X\n", mMachineState, rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::Pause()
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mMachineState != MachineState_Running)
        return setError (E_FAIL, tr ("Cannot pause the machine as it is not running.  (Machine state: %d)"), mMachineState);

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    LogFlowThisFunc (("Sending PAUSE request...\n"));

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

    int vrc = VMR3Suspend (mpVM);

    HRESULT rc = VBOX_SUCCESS (vrc) ? S_OK :
        setError (E_FAIL,
            tr ("Could not suspend the machine execution.  (Error: %Vrc)"), vrc);

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::Resume()
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mMachineState != MachineState_Paused)
        return setError (E_FAIL, tr ("Cannot resume the machine as it is not paused.  (Machine state: %d)"), mMachineState);

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    LogFlowThisFunc (("Sending RESUME request...\n"));

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

    int vrc = VMR3Resume (mpVM);

    HRESULT rc = VBOX_SUCCESS (vrc) ? S_OK :
        setError (E_FAIL,
            tr ("Could not resume the machine execution.  (Error: %Vrc)"), vrc);

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::PowerButton()
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock lock (this);

    if (mMachineState != MachineState_Running)
        return setError (E_FAIL, tr ("Cannot power off the machine as it is not running.  (Machine state: %d)"), mMachineState);

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
        setError (E_FAIL,
            tr ("Controlled power off failed.  (Error: %Vrc)"), vrc);

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::SaveState (IProgress **aProgress)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("mMachineState=%d\n", mMachineState));

    if (!aProgress)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mMachineState != MachineState_Running &&
        mMachineState != MachineState_Paused)
    {
        return setError (E_FAIL,
            tr ("Cannot save the execution state as the machine "
                "is not running (machine state: %d)"), mMachineState);
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
    progress->init ((IConsole *) this,
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
         *  We assign PowerDown() a higher precendence than SaveState(),
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
                    rc = setError (E_FAIL,
                        tr ("Could not create a directory '%s' to save the state to.  (Error: %Vrc)"),
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

        ComAssertMsgRCBreak (vrc, ("Could not create VMSave thread (%Vrc)\n", vrc),
                             rc = E_FAIL);

        /* task is now owned by saveStateThread(), so release it */
        task.release();

        /* return the progress to the caller */
        progress.queryInterfaceTo (aProgress);
    }
    while (0);

    if (FAILED (rc) && !taskCreationFailed)
    {
        /* fetch any existing error info */
        ErrorInfo ei;

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

        /* restore fetched error info */
        setError (ei);
    }

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::DiscardSavedState()
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mMachineState != MachineState_Saved)
        return setError (E_FAIL,
            tr ("Cannot discard the machine state as the machine is not in the saved state.  (Machine state: %d"), mMachineState);

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
    if (!aDeviceActivity)
        return E_INVALIDARG;

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
        case DeviceType_FloppyDevice:
        {
            for (unsigned i = 0; i < ELEMENTS(mapFDLeds); i++)
                SumLed.u32 |= readAndClearLed(mapFDLeds[i]);
            break;
        }

        case DeviceType_DVDDevice:
        {
            SumLed.u32 |= readAndClearLed(mapIDELeds[2]);
            break;
        }

        case DeviceType_HardDiskDevice:
        {
            SumLed.u32 |= readAndClearLed(mapIDELeds[0]);
            SumLed.u32 |= readAndClearLed(mapIDELeds[1]);
            SumLed.u32 |= readAndClearLed(mapIDELeds[3]);
            break;
        }

        case DeviceType_NetworkDevice:
        {
            for (unsigned i = 0; i < ELEMENTS(mapNetworkLeds); i++)
                SumLed.u32 |= readAndClearLed(mapNetworkLeds[i]);
            break;
        }

        case DeviceType_USBDevice:
        {
            /// @todo (r=dmik)
            //  USB_DEVICE_ACTIVITY
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
            *aDeviceActivity = DeviceActivity_DeviceIdle;
            break;
        case PDMLED_READING:
            *aDeviceActivity = DeviceActivity_DeviceReading;
            break;
        case PDMLED_WRITING:
        case PDMLED_READING | PDMLED_WRITING:
            *aDeviceActivity = DeviceActivity_DeviceWriting;
            break;
    }

    return S_OK;
}

STDMETHODIMP Console::AttachUSBDevice (INPTR GUIDPARAM aId)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    /// @todo (r=dmik) is it legal to attach USB devices when the machine is
    //  Paused, Starting, Saving, Stopping, etc? if not, we should make a
    //  stricter check (mMachineState != MachineState_Running).
    if (mMachineState < MachineState_Running)
        return setError (E_FAIL,
            tr ("Cannot attach a USB device to a machine which is not running "
                "(machine state: %d)"), mMachineState);

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    /* Don't proceed unless we've found the usb controller. */
    PPDMIBASE pBase = NULL;
    int vrc = PDMR3QueryLun (mpVM, "usb-ohci", 0, 0, &pBase);
    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL,
            tr ("The virtual machine does not have a USB controller"));

    PVUSBIRHCONFIG pRhConfig = (PVUSBIRHCONFIG) pBase->
        pfnQueryInterface (pBase, PDMINTERFACE_VUSB_RH_CONFIG);
    ComAssertRet (pRhConfig, E_FAIL);

    /// @todo (dmik) REMOTE_USB
    //  when remote USB devices are ready, first search for a device with the
    //  given UUID in mRemoteUSBDevices. If found, request a capture from
    //  a remote client. If not found, search it on the local host as done below

    /*
     *  Try attach the given host USB device (a proper errror message should
     *  be returned in case of error).
     */
    ComPtr <IUSBDevice> hostDevice;
    HRESULT hrc = mControl->CaptureUSBDevice (aId, hostDevice.asOutParam());
    CheckComRCReturnRC (hrc);

    return attachUSBDevice (hostDevice, true /* aManual */, pRhConfig);
}

STDMETHODIMP Console::DetachUSBDevice (INPTR GUIDPARAM aId, IUSBDevice **aDevice)
{
    if (!aDevice)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

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
            tr ("Cannot detach the USB device (UUID: %s) as it is not attached here."),
            Guid (aId).toString().raw());

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    PPDMIBASE pBase = NULL;
    int vrc = PDMR3QueryLun (mpVM, "usb-ohci", 0, 0, &pBase);

    /* if the device is attached, then there must be a USB controller */
    ComAssertRCRet (vrc, E_FAIL);

    PVUSBIRHCONFIG pRhConfig = (PVUSBIRHCONFIG) pBase->
        pfnQueryInterface (pBase, PDMINTERFACE_VUSB_RH_CONFIG);
    ComAssertRet (pRhConfig, E_FAIL);

    Guid Uuid(aId);

    LogFlowThisFunc (("Detaching USB proxy device {%Vuuid}...\n", Uuid.raw()));

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

    PVMREQ pReq = NULL;
    vrc = VMR3ReqCall (mpVM, &pReq, RT_INDEFINITE_WAIT,
                       (PFNRT) usbDetachCallback, 5,
                       this, &it, true /* aManual */, pRhConfig, Uuid.raw());
    if (VBOX_SUCCESS (vrc))
        vrc = pReq->iStatus;
    VMR3ReqFree (pReq);

    HRESULT hrc = S_OK;

    if (VBOX_SUCCESS (vrc))
        device.queryInterfaceTo (aDevice);
    else
        hrc = setError (E_FAIL,
            tr ("Error detaching the USB device.  (Failed to destroy the USB proxy device: %Vrc)"), vrc);

    return hrc;
}

STDMETHODIMP
Console::CreateSharedFolder (INPTR BSTR aName, INPTR BSTR aHostPath)
{
    if (!aName || !aHostPath)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mMachineState == MachineState_Saved)
        return setError (E_FAIL,
            tr ("Cannot create a transient shared folder on a "
                "machine in the saved state."));

    /// @todo (dmik) check globally shared folders when they are done

    /* check machine's shared folders */
    {
        ComPtr <ISharedFolderCollection> coll;
        HRESULT rc = mMachine->COMGETTER(SharedFolders) (coll.asOutParam());
        if (FAILED (rc))
            return rc;

        ComPtr <ISharedFolder> machineSharedFolder;
        rc = coll->FindByName (aName, machineSharedFolder.asOutParam());
        if (SUCCEEDED (rc))
            return setError (E_FAIL,
                tr ("A permanent shared folder named '%ls' already "
                    "exists."), aName);
    }

    ComObjPtr <SharedFolder> sharedFolder;
    HRESULT rc = findSharedFolder (aName, sharedFolder, false /* aSetError */);
    if (SUCCEEDED (rc))
        return setError (E_FAIL,
            tr ("A shared folder named '%ls' already exists."), aName);

    sharedFolder.createObject();
    rc = sharedFolder->init (this, aName, aHostPath);
    CheckComRCReturnRC (rc);

    BOOL accessible = FALSE;
    rc = sharedFolder->COMGETTER(Accessible) (&accessible);
    CheckComRCReturnRC (rc);

    if (!accessible)
        return setError (E_FAIL,
            tr ("The shared folder path '%ls' on the host is not accessible."), aHostPath);

    /// @todo (r=sander?) should move this into the shared folder class */
    if (mpVM && mVMMDev->getShFlClientId())
    {
        /*
         *  if the VM is online and supports shared folders, share this folder
         *  under the specified name. On error, return it to the caller.
         */

        /* protect mpVM */
        AutoVMCaller autoVMCaller (this);
        CheckComRCReturnRC (autoVMCaller.rc());

        VBOXHGCMSVCPARM  parms[2];
        SHFLSTRING      *pFolderName, *pMapName;
        int              cbString;

        Log(("Add shared folder %ls -> %ls\n", aName, aHostPath));

        cbString = (RTStrUcs2Len(aHostPath) + 1) * sizeof(RTUCS2);
        pFolderName = (SHFLSTRING *)RTMemAllocZ(sizeof(SHFLSTRING) + cbString);
        Assert(pFolderName);
        memcpy(pFolderName->String.ucs2, aHostPath, cbString);

        pFolderName->u16Size   = cbString;
        pFolderName->u16Length = cbString - sizeof(RTUCS2);

        parms[0].type = VBOX_HGCM_SVC_PARM_PTR;
        parms[0].u.pointer.addr = pFolderName;
        parms[0].u.pointer.size = sizeof(SHFLSTRING) + cbString;

        cbString = (RTStrUcs2Len(aName) + 1) * sizeof(RTUCS2);
        pMapName = (SHFLSTRING *)RTMemAllocZ(sizeof(SHFLSTRING) + cbString);
        Assert(pMapName);
        memcpy(pMapName->String.ucs2, aName, cbString);

        pMapName->u16Size   = cbString;
        pMapName->u16Length = cbString - sizeof(RTUCS2);

        parms[1].type = VBOX_HGCM_SVC_PARM_PTR;
        parms[1].u.pointer.addr = pMapName;
        parms[1].u.pointer.size = sizeof(SHFLSTRING) + cbString;

        rc = mVMMDev->hgcmHostCall("VBoxSharedFolders", SHFL_FN_ADD_MAPPING, 2, &parms[0]);
        RTMemFree(pFolderName);
        RTMemFree(pMapName);
        if (rc != VINF_SUCCESS)
            return setError (E_FAIL, tr ("Unable to add mapping %ls to %ls."), aHostPath, aName);
    }

    mSharedFolders.push_back (sharedFolder);
    return S_OK;
}

STDMETHODIMP Console::RemoveSharedFolder (INPTR BSTR aName)
{
    if (!aName)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mMachineState == MachineState_Saved)
        return setError (E_FAIL,
            tr ("Cannot remove a transient shared folder when the "
                "machine is in the saved state."));

    ComObjPtr <SharedFolder> sharedFolder;
    HRESULT rc = findSharedFolder (aName, sharedFolder, true /* aSetError */);
    CheckComRCReturnRC (rc);

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    if (mpVM && mVMMDev->getShFlClientId())
    {
        /*
         *  if the VM is online and supports shared folders, UNshare this folder.
         *  On error, return it to the caller.
         */
        VBOXHGCMSVCPARM  parms;
        SHFLSTRING      *pMapName;
        int              cbString;

        cbString = (RTStrUcs2Len(aName) + 1) * sizeof(RTUCS2);
        pMapName = (SHFLSTRING *)RTMemAllocZ(sizeof(SHFLSTRING) + cbString);
        Assert(pMapName);
        memcpy(pMapName->String.ucs2, aName, cbString);

        pMapName->u16Size   = cbString;
        pMapName->u16Length = cbString - sizeof(RTUCS2);

        parms.type = VBOX_HGCM_SVC_PARM_PTR;
        parms.u.pointer.addr = pMapName;
        parms.u.pointer.size = sizeof(SHFLSTRING) + cbString;

        rc = mVMMDev->hgcmHostCall("VBoxSharedFolders", SHFL_FN_REMOVE_MAPPING, 1, &parms);
        RTMemFree(pMapName);
        if (rc != VINF_SUCCESS)
            rc = setError (E_FAIL, tr ("Unable to remove the mapping %ls."), aName);
    }

    mSharedFolders.remove (sharedFolder);
    return rc;
}

STDMETHODIMP Console::TakeSnapshot (INPTR BSTR aName, INPTR BSTR aDescription,
                                    IProgress **aProgress)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aName='%ls' mMachineState=%08X\n", aName, mMachineState));

    if (!aName)
        return E_INVALIDARG;
    if (!aProgress)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mMachineState > MachineState_Running &&
        mMachineState != MachineState_Paused)
    {
        return setError (E_FAIL,
            tr ("Cannot take a snapshot of a machine while it is changing state.  (Machine state: %d)"), mMachineState);
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
             *  We assign PowerDown() a higher precendence than TakeSnapshot(),
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
         *  (i.e. createing a snapshot online)
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
            rc = combinedProgress->init ((IConsole *) this,
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

            ComAssertMsgRCBreak (vrc, ("Could not create VMTakeSnap thread (%Vrc)\n", vrc),
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
        /* fetch any existing error info */
        ErrorInfo ei;

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

        /* restore fetched error info */
        setError (ei);
    }

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

STDMETHODIMP Console::DiscardSnapshot (INPTR GUIDPARAM aId, IProgress **aProgress)
{
    if (Guid (aId).isEmpty())
        return E_INVALIDARG;
    if (!aProgress)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mMachineState >= MachineState_Running)
        return setError (E_FAIL,
            tr ("Cannot discard a snapshot on a running machine (Machine state: %d)"), mMachineState);

    MachineState_T machineState = MachineState_InvalidMachineState;
    HRESULT rc = mControl->DiscardSnapshot (this, aId, &machineState, aProgress);
    CheckComRCReturnRC (rc);

    setMachineStateLocally (machineState);
    return S_OK;
}

STDMETHODIMP Console::DiscardCurrentState (IProgress **aProgress)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mMachineState >= MachineState_Running)
        return setError (E_FAIL,
            tr ("Cannot discard the current state of a running machine.  (Machine state: %d)"), mMachineState);

    MachineState_T machineState = MachineState_InvalidMachineState;
    HRESULT rc = mControl->DiscardCurrentState (this, &machineState, aProgress);
    CheckComRCReturnRC (rc);

    setMachineStateLocally (machineState);
    return S_OK;
}

STDMETHODIMP Console::DiscardCurrentSnapshotAndState (IProgress **aProgress)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mMachineState >= MachineState_Running)
        return setError (E_FAIL,
            tr ("Cannot discard the current snapshot and state on a running machine.  (Machine state: %d)"), mMachineState);

    MachineState_T machineState = MachineState_InvalidMachineState;
    HRESULT rc =
        mControl->DiscardCurrentSnapshotAndState (this, &machineState, aProgress);
    CheckComRCReturnRC (rc);

    setMachineStateLocally (machineState);
    return S_OK;
}

STDMETHODIMP Console::RegisterCallback (IConsoleCallback *aCallback)
{
    if (!aCallback)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

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
    if (!aCallback)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

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
 *  Called by IInternalSessionControl::OnDVDDriveChange().
 *
 *  @note Locks this object for reading.
 */
HRESULT Console::onDVDDriveChange()
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

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
                rc = ImagePtr->COMGETTER(FilePath) (Path.asOutParam());
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
    if (FAILED (rc))
    {
        LogFlowThisFunc (("Returns %#x\n", rc));
        return rc;
    }

    return doDriveChange ("piix3ide", 0, 2, eState, &meDVDState,
                          Utf8Str (Path).raw(), fPassthrough);
}


/**
 *  Called by IInternalSessionControl::OnFloppyDriveChange().
 *
 *  @note Locks this object for reading.
 */
HRESULT Console::onFloppyDriveChange()
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

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
            ComPtr <IFloppyImage> ImagePtr;
            rc = mFloppyDrive->GetImage (ImagePtr.asOutParam());
            if (SUCCEEDED (rc))
                rc = ImagePtr->COMGETTER(FilePath) (Path.asOutParam());
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
    if (FAILED (rc))
    {
        LogFlowThisFunc (("Returns %#x\n", rc));
        return rc;
    }

    return doDriveChange ("i82078", 0, 0, eState, &meFloppyState,
                          Utf8Str (Path).raw(), false);
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
 *                          If NULL no media or drive is attached and the lun will be configured with
 *                          the default block driver with no media. This will also be the state if
 *                          mounting / capturing the specified media / drive fails.
 * @param   fPassthrough    Enables using passthrough mode of the host DVD drive if applicable.
 *
 * @note Locks this object for reading.
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

    AutoReaderLock alock (this);

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    /*
     * Call worker in EMT, that's faster and safer than doing everything
     * using VM3ReqCall. Note that we separate VMR3ReqCall from VMR3ReqWait
     * here to make requests from under the lock in order to serialize them.
     */
    PVMREQ pReq;
    int vrc = VMR3ReqCall (mpVM, &pReq, 0 /* no wait! */,
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
            tr ("Could not mount the media/drive '%s' (%Vrc)"), pszPath, vrc);

    return setError (E_FAIL,
        tr ("Could not unmount the currently mounted media/drive (%Vrc)"), vrc);
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
 *                          If NULL no media or drive is attached and the lun will be configured with
 *                          the default block driver with no media. This will also be the state if
 *                          mounting / capturing the specified media / drive fails.
 * @param   fPassthrough    Enables using passthrough mode of the host DVD drive if applicable.
 *
 * @thread  EMT
 * @note Locks the Console object for writing
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
     *  Locking the object before doing VMR3* calls is quite safe here,
     *  since we're on EMT. Write lock is necessary because we're indirectly
     *  modify the meDVDState/meFloppyState members (pointed to by peState).
     */
    AutoLock alock (pThis);

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
                AssertBreak (pIMount, rc = VERR_INVALID_POINTER);

                /*
                 * Unmount the media.
                 */
                rc = pIMount->pfnUnmount (pIMount);
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

    LogFlowFunc (("Returning %Vrc\n", rcRet));
    return rcRet;
}


/**
 *  Called by IInternalSessionControl::OnNetworkAdapterChange().
 *
 *  @note Locks this object for writing.
 */
HRESULT Console::onNetworkAdapterChange(INetworkAdapter *networkAdapter)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    /* Don't do anything if the VM isn't running */
    if (!mpVM)
        return S_OK;

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    /* Get the properties we need from the adapter */
    BOOL fCableConnected;
    HRESULT rc = networkAdapter->COMGETTER(CableConnected)(&fCableConnected);
    AssertComRC(rc);
    if (SUCCEEDED(rc))
    {
        ULONG ulInstance;
        rc = networkAdapter->COMGETTER(Slot)(&ulInstance);
        AssertComRC(rc);
        if (SUCCEEDED(rc))
        {
            /*
             * Find the pcnet instance, get the config interface and update the link state.
             */
            PPDMIBASE pBase;
            int rcVBox = PDMR3QueryDeviceLun(mpVM, "pcnet", (unsigned)ulInstance, 0, &pBase);
            ComAssertRC(rcVBox);
            if (VBOX_SUCCESS(rcVBox))
            {
                Assert(pBase);
                PPDMINETWORKCONFIG pINetCfg = (PPDMINETWORKCONFIG)pBase->pfnQueryInterface(pBase, PDMINTERFACE_NETWORK_CONFIG);
                if (pINetCfg)
                {
                    Log(("Console::onNetworkAdapterChange: setting link state to %d\n", fCableConnected));
                    rcVBox = pINetCfg->pfnSetLinkState(pINetCfg, fCableConnected ? PDMNETWORKLINKSTATE_UP : PDMNETWORKLINKSTATE_DOWN);
                    ComAssertRC(rcVBox);
                }
            }
        }
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

    AutoLock alock (this);

    HRESULT rc = S_OK;

    if (mVRDPServer && mMachineState == MachineState_Running)
    {
        BOOL vrdpEnabled = FALSE;

        rc = mVRDPServer->COMGETTER(Enabled) (&vrdpEnabled);
        ComAssertComRCRetRC (rc);

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
                mConsoleVRDPServer->SetCallback ();
            }
        }
        else
        {
            mConsoleVRDPServer->Stop ();
        }
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

    AutoLock alock (this);

    /* Ignore if no VM is running yet. */
    if (!mpVM)
        return S_OK;

/// @todo (dmik)
//  check for the Enabled state and disable virtual USB controller??
//  Anyway, if we want to query the machine's USB Controller we need to cache
//  it to to mUSBController in #init() (as it is done with mDVDDrive).
//
//  bird: While the VM supports hot-plugging, I doubt any guest can handle it at this time... :-)
//
//    /* protect mpVM */
//    AutoVMCaller autoVMCaller (this);
//    CheckComRCReturnRC (autoVMCaller.rc());

    return S_OK;
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
 *
 *  @note Locks this object for writing.
 */
HRESULT Console::onUSBDeviceAttach (IUSBDevice *aDevice)
{
    LogFlowThisFunc (("aDevice=%p\n", aDevice));

    AutoCaller autoCaller (this);
    ComAssertComRCRetRC (autoCaller.rc());

    AutoLock alock (this);

    /* VM might have been stopped when this message arrives */
    if (mMachineState < MachineState_Running)
    {
        LogFlowThisFunc (("Attach request ignored (mMachineState=%d).\n",
                          mMachineState));
        return E_FAIL;
    }

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    /* Don't proceed unless we've found the usb controller. */
    PPDMIBASE pBase = NULL;
    int vrc = PDMR3QueryLun (mpVM, "usb-ohci", 0, 0, &pBase);
    if (VBOX_FAILURE (vrc))
    {
        LogFlowThisFunc (("Attach request ignored (no USB controller).\n"));
        return E_FAIL;
    }

    PVUSBIRHCONFIG pRhConfig = (PVUSBIRHCONFIG) pBase->
        pfnQueryInterface (pBase, PDMINTERFACE_VUSB_RH_CONFIG);
    ComAssertRet (pRhConfig, E_FAIL);

    return attachUSBDevice (aDevice, false /* aManual */, pRhConfig);
}

/**
 *  Called by IInternalSessionControl::OnUSBDeviceDetach() and locally by
 *  processRemoteUSBDevices().
 *
 *  @note Locks this object for writing.
 */
HRESULT Console::onUSBDeviceDetach (INPTR GUIDPARAM aId)
{
    Guid Uuid (aId);
    LogFlowThisFunc (("aId={%Vuuid}\n", Uuid.raw()));

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    /* Find the device. */
    ComObjPtr <OUSBDevice> device;
    USBDeviceList::iterator it = mUSBDevices.begin();
    while (it != mUSBDevices.end())
    {
        LogFlowThisFunc (("it={%Vuuid}\n", (*it)->id().raw()));
        if ((*it)->id() == Uuid)
        {
            device = *it;
            break;
        }
        ++ it;
    }

    /* VM might have been stopped when this message arrives */
    if (device.isNull())
    {
        LogFlowThisFunc (("Device not found.\n"));
        if (mMachineState < MachineState_Running)
        {
            LogFlowThisFunc (("Detach request ignored (mMachineState=%d).\n",
                              mMachineState));
            return E_FAIL;
        }
        /* the device must be in the list */
        AssertFailedReturn (E_FAIL);
    }

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    PPDMIBASE pBase = NULL;
    int vrc = PDMR3QueryLun (mpVM, "usb-ohci", 0, 0, &pBase);

    /* if the device is attached, then there must be a USB controller */
    AssertRCReturn (vrc, E_FAIL);

    PVUSBIRHCONFIG pRhConfig = (PVUSBIRHCONFIG) pBase->
        pfnQueryInterface (pBase, PDMINTERFACE_VUSB_RH_CONFIG);
    AssertReturn (pRhConfig, E_FAIL);

    LogFlowThisFunc (("Detaching USB proxy device {%Vuuid}...\n", Uuid.raw()));

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

    PVMREQ pReq;
    vrc = VMR3ReqCall (mpVM, &pReq, RT_INDEFINITE_WAIT,
                       (PFNRT) usbDetachCallback, 5,
                       this, &it, false /* aManual */, pRhConfig, Uuid.raw());
    if (VBOX_SUCCESS (vrc))
        vrc = pReq->iStatus;
    VMR3ReqFree (pReq);

    AssertRC (vrc);

    return VBOX_SUCCESS (vrc) ? S_OK : E_FAIL;
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

    AutoLock alock (this);

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
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("fVisible=%d, fAlpha=%d, xHot = %d, yHot = %d, width=%d, "
                      "height=%d, shape=%p\n",
                      fVisible, fAlpha, xHot, yHot, width, height, pShape));

    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* We need a write lock because we alter the cached callback data */
    AutoLock alock (this);

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
        cb += ((cb + 3) & ~3) + width * 4 * height; /* + gap + size of the XOR mask */
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

    LogFlowThisFuncLeave();
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
    AutoLock alock (this);

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

    AutoReaderLock alock (this);

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

    AutoReaderLock alock (this);

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

    AutoReaderLock alock (this);

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
    AutoLock alock (this);

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
void Console::onRuntimeError (BOOL aFatal, INPTR BSTR aErrorID, INPTR BSTR aMessage)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoReaderLock alock (this);

    CallbackList::iterator it = mCallbacks.begin();
    while (it != mCallbacks.end())
        (*it++)->OnRuntimeError (aFatal, aErrorID, aMessage);
}

// private mehtods
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
 *  silently check for the mpVM avaliability).
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

    AutoLock alock (this);

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

    AutoLock alock (this);

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
 *  Internal power off worker routine.
 *
 *  This method may be called only at certain places with the folliwing meaning
 *  as shown below:
 *
 *  - if the machine state is either Running or Paused, a normal
 *    Console-initiated powerdown takes place (e.g. PowerDown());
 *  - if the machine state is Saving, saveStateThread() has successfully
 *    done its job;
 *  - if the machine state is Starting or Restoring, powerUpThread() has
 *    failed to start/load the VM;
 *  - if the machine state is Stopping, the VM has powered itself off
 *    (i.e. not as a result of the powerDown() call).
 *
 *  Calling it in situations other than the above will cause unexpected
 *  behavior.
 *
 *  Note that this method should be the only one that destroys mpVM and sets
 *  it to NULL.
 *
 *  @note Locks this object for writing.
 *
 *  @note Never call this method from a thread that called addVMCaller() or
 *        instantiated an AutoVMCaller object; first call releaseVMCaller() or
 *        release(). Otherwise it will deadlock.
 */
HRESULT Console::powerDown()
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    /* sanity */
    AssertReturn (mVMDestroying == false, E_FAIL);

    LogRel (("Console::powerDown(): a request to power off the VM has been issued "
             "(mMachineState=%d, InUninit=%d)\n",
             mMachineState, autoCaller.state() == InUninit));

    /* First, wait for all mpVM callers to finish their work if necessary */
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

    AssertReturn (mpVM, E_FAIL);

    AssertMsg (mMachineState == MachineState_Running ||
               mMachineState == MachineState_Paused ||
               mMachineState == MachineState_Saving ||
               mMachineState == MachineState_Starting ||
               mMachineState == MachineState_Restoring ||
               mMachineState == MachineState_Stopping,
               ("Invalid machine state: %d\n", mMachineState));

    HRESULT rc = S_OK;
    int vrc = VINF_SUCCESS;

    /*
     *  Power off the VM if not already done that. In case of Stopping, the VM
     *  has powered itself off and notified Console in vmstateChangeCallback().
     *  In case of Starting or Restoring, powerUpThread() is calling us on
     *  failure, so the VM is already off at that point.
     */
    if (mMachineState != MachineState_Stopping &&
        mMachineState != MachineState_Starting &&
        mMachineState != MachineState_Restoring)
    {
        /*
         *  don't go from Saving to Stopping, vmstateChangeCallback needs it
         *  to set the state to Saved on VMSTATE_TERMINATED.
         */
        if (mMachineState != MachineState_Saving)
            setMachineState (MachineState_Stopping);

        LogFlowThisFunc (("Powering off the VM...\n"));

        /* Leave the lock since EMT will call us back on VMR3PowerOff() */
        alock.leave();

        vrc = VMR3PowerOff (mpVM);
        /*
         *  Note that VMR3PowerOff() may fail here (invalid VMSTATE) if the
         *  VM-(guest-)initiated power off happened in parallel a ms before
         *  this call. So far, we let this error pop up on the user's side.
         */

        alock.enter();
    }

    LogFlowThisFunc (("Ready for VM destruction\n"));

    /*
     *  If we are called from Console::uninit(), then try to destroy the VM
     *  even on failure (this will most likely fail too, but what to do?..)
     */
    if (VBOX_SUCCESS (vrc) || autoCaller.state() == InUninit)
    {
        /*
         *  Stop the VRDP server and release all USB device.
         *  (When called from uninit mConsoleVRDPServer is already destroyed.)
         */
        if (mConsoleVRDPServer)
        {
            LogFlowThisFunc (("Stopping VRDP server...\n"));

            /* Leave the lock since EMT will call us back as addVMCaller in updateDisplayData(). */
            alock.leave();

            mConsoleVRDPServer->Stop();

            alock.enter();
        }

        releaseAllUSBDevices();

        /*
         *  Now we've got to destroy the VM as well. (mpVM is not valid
         *  beyond this point). We leave the lock before calling VMR3Destroy()
         *  because it will result into calling destructors of drivers
         *  associated with Console children which may in turn try to lock
         *  Console (e.g. by instantiating SafeVMPtr to access mpVM). It's safe
         *  here because mVMDestroying is set which should prevent any activity.
         */

        /*
         *  Set mpVM to NULL early just in case if some old code is not using
         *  addVMCaller()/releaseVMCaller().
         */
        PVM pVM = mpVM;
        mpVM = NULL;

        LogFlowThisFunc (("Destroying the VM...\n"));

        alock.leave();

        vrc = VMR3Destroy (pVM);

        /* take the lock again */
        alock.enter();

        if (VBOX_SUCCESS (vrc))
        {
            LogFlowThisFunc (("Machine has been destroyed (mMachineState=%d)\n",
                              mMachineState));
            /*
             *  Note: the Console-level machine state change happens on the
             *  VMSTATE_TERMINATE state change in vmstateChangeCallback(). If
             *  powerDown() is called from EMT (i.e. from vmstateChangeCallback()
             *  on receiving VM-initiated VMSTATE_OFF), VMSTATE_TERMINATE hasn't
             *  occured yet. This is okay, because mMachineState is already
             *  Stopping in this case, so any other attempt to call PowerDown()
             *  will be rejected.
             */
        }
        else
        {
            /* bad bad bad, but what to do? */
            mpVM = pVM;
            rc = setError (E_FAIL,
                tr ("Could not destroy the machine.  (Error: %Vrc)"), vrc);
        }
    }
    else
    {
        rc = setError (E_FAIL,
            tr ("Could not power off the machine.  (Error: %Vrc)"), vrc);
    }

    /*
     *  Finished with destruction. Note that if something impossible happened
     *  and we've failed to destroy the VM, mVMDestroying will remain false and
     *  mMachineState will be something like Stopping, so most Console methods
     *  will return an error to the caller.
     */
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

    AutoLock alock (this);

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
            /*
             *  Server notification MUST be done from under the lock; otherwise
             *  the machine state here and on the server might go out of sync, that
             *  can lead to various unexpected results (like the machine state being
             *  >= MachineState_Running on the server, while the session state is
             *  already SessionState_SessionClosed at the same time there).
             *
             *  Cross-lock conditions should be carefully watched out: calling
             *  UpdateState we will require Machine and SessionMachine locks
             *  (remember that here we're holding the Console lock here, and
             *  also all locks that have been entered by the thread before calling
             *  this method).
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
HRESULT Console::findSharedFolder (const BSTR aName,
                                   ComObjPtr <SharedFolder> &aSharedFolder,
                                   bool aSetError /* = false */)
{
    /* sanity check */
    AssertReturn (isLockedOnCurrentThread(), E_FAIL);

    bool found = false;
    for (SharedFolderList::const_iterator it = mSharedFolders.begin();
        !found && it != mSharedFolders.end();
        ++ it)
    {
        AutoLock alock (*it);
        found = (*it)->name() == aName;
        if (found)
            aSharedFolder = *it;
    }

    HRESULT rc = found ? S_OK : E_INVALIDARG;

    if (aSetError && !found)
        setError (rc, tr ("Could not find a shared folder named '%ls'."), aName);

    return rc;
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
    /*
     *  Note that we must let this method proceed even if Console::uninit() has
     *  been already called. In such case this VMSTATE change is a result of:
     *  1) powerDown() called from uninit() itself, or
     *  2) VM-(guest-)initiated power off.
     */
    AssertReturnVoid (autoCaller.isOk() ||
                      autoCaller.state() == InUninit);

    switch (aState)
    {
        /*
         *  The VM has terminated
         */
        case VMSTATE_OFF:
        {
            AutoLock alock (that);

            if (that->mVMStateChangeCallbackDisabled)
                break;

            /*
             *  Do we still think that it is running? It may happen if this is
             *  a VM-(guest-)initiated shutdown/poweroff.
             */
            if (that->mMachineState != MachineState_Stopping &&
                that->mMachineState != MachineState_Saving &&
                that->mMachineState != MachineState_Restoring)
            {
                LogFlowFunc (("VM has powered itself off but Console still "
                              "thinks it is running. Notifying.\n"));

                /* prevent powerDown() from calling VMR3PowerOff() again */
                that->setMachineState (MachineState_Stopping);

                /*
                 *  Setup task object and thread to carry out the operation
                 *  asynchronously (if we call powerDown() right here but there
                 *  is one or more mpVM callers (added with addVMCaller()) we'll
                 *  deadlock.
                 */
                std::auto_ptr <VMTask> task (new VMTask (that, true /* aUsesVMPtr */));
                 /*
                  *  If creating a task is falied, this can currently mean one
                  *  of two: either Console::uninit() has been called just a ms
                  *  before (so a powerDown() call is already on the way), or
                  *  powerDown() itself is being already executed. Just do
                  *  nothing .
                  */
                if (!task->isOk())
                {
                    LogFlowFunc (("Console is already being uninitialized.\n"));
                    break;
                }

                int vrc = RTThreadCreate (NULL, Console::powerDownThread,
                                          (void *) task.get(), 0,
                                          RTTHREADTYPE_MAIN_WORKER, 0,
                                          "VMPowerDowm");

                AssertMsgRC (vrc, ("Could not create VMPowerUp thread (%Vrc)\n", vrc));
                if (VBOX_FAILURE (vrc))
                    break;

                /* task is now owned by powerDownThread(), so release it */
                task.release();
            }
            break;
        }

        /*
         *  The VM has been completely destroyed.
         *
         *  Note: This state change can happen at two points:
         *        1) At the end of VMR3Destroy() if it was not called from EMT.
         *        2) At the end of vmR3EmulationThread if VMR3Destroy() was
         *           called by EMT.
         */
        case VMSTATE_TERMINATED:
        {
            AutoLock alock (that);

            if (that->mVMStateChangeCallbackDisabled)
                break;

            /*
             *  Terminate host interface networking. If aVM is NULL, we've been
             *  manually called from powerUpThread() either before calling
             *  VMR3Create() or after VMR3Create() failed, so no need to touch
             *  networking.
             */
            if (aVM)
                that->powerDownHostInterfaces();

            /*
             *  From now on the machine is officially powered down or
             *  remains in the Saved state.
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
                    /*
                     *  successfully saved (note that the machine is already
                     *  in the Saved state on the server due to EndSavingState()
                     *  called from saveStateThread(), so only change the local
                     *  state)
                     */
                    that->setMachineStateLocally (MachineState_Saved);
                    break;
                case MachineState_Starting:
                    /*
                     *  failed to start, but be patient: set back to PoweredOff
                     *  (for similarity with the below)
                     */
                    that->setMachineState (MachineState_PoweredOff);
                    break;
                case MachineState_Restoring:
                    /*
                     *  failed to load the saved state file, but be patient:
                     *  set back to Saved (to preserve the saved state file)
                     */
                    that->setMachineState (MachineState_Saved);
                    break;
            }

            break;
        }

        case VMSTATE_SUSPENDED:
        {
            if (aOldState == VMSTATE_RUNNING)
            {
                AutoLock alock (that);

                if (that->mVMStateChangeCallbackDisabled)
                    break;

                /* Change the machine state from Running to Paused */
                Assert (that->mMachineState == MachineState_Running);
                that->setMachineState (MachineState_Paused);
            }
        }

        case VMSTATE_RUNNING:
        {
            if (aOldState == VMSTATE_CREATED ||
                aOldState == VMSTATE_SUSPENDED)
            {
                AutoLock alock (that);

                if (that->mVMStateChangeCallbackDisabled)
                    break;

                /*
                 *  Change the machine state from Starting, Restoring or Paused
                 *  to Running
                 */
                Assert ((that->mMachineState == MachineState_Starting &&
                         aOldState == VMSTATE_CREATED) ||
                        ((that->mMachineState == MachineState_Restoring ||
                          that->mMachineState == MachineState_Paused) &&
                         aOldState == VMSTATE_SUSPENDED));

                that->setMachineState (MachineState_Running);
            }
        }

        default: /* shut up gcc */
            break;
    }
}

/**
 *  Sends a request to VMM to attach the given host device.
 *  After this method succeeds, the attached device will appear in the
 *  mUSBDevices collection.
 *
 *  If \a aManual is true and a failure occures, the given device
 *  will be returned back to the USB proxy manager.
 *
 *  @param aHostDevice  device to attach
 *  @param aManual      true if device is being manually attached
 *
 *  @note Locks this object for writing.
 *  @note Synchronously calls EMT.
 */
HRESULT Console::attachUSBDevice (IUSBDevice *aHostDevice, bool aManual,
                                  PVUSBIRHCONFIG aConfig)
{
    AssertReturn (aHostDevice && aConfig, E_FAIL);

    AutoLock alock (this);

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
    void *pvRemote = NULL;

    hrc = aHostDevice->COMGETTER (Remote) (&fRemote);
    ComAssertComRCRetRC (hrc);

#ifndef VRDP_MC
    if (fRemote)
    {
        pvRemote = mConsoleVRDPServer->GetUSBBackendPointer ();
        ComAssertRet (pvRemote, E_FAIL);
    }
#endif /* !VRDP_MC */

    /* protect mpVM */
    AutoVMCaller autoVMCaller (this);
    CheckComRCReturnRC (autoVMCaller.rc());

    LogFlowThisFunc (("Proxying USB device '%s' {%Vuuid}...\n",
                      Address.raw(), Uuid.ptr()));

    /* leave the lock before a VMR3* call (EMT will call us back)! */
    alock.leave();

    PVMREQ pReq = NULL;
    int vrc = VMR3ReqCall (mpVM, &pReq, RT_INDEFINITE_WAIT,
                           (PFNRT) usbAttachCallback, 7,
                           this, aHostDevice,
                           aConfig, Uuid.ptr(), fRemote, Address.raw(), pvRemote);
    if (VBOX_SUCCESS (vrc))
        vrc = pReq->iStatus;
    VMR3ReqFree (pReq);

    /* restore the lock */
    alock.enter();

    /* hrc is S_OK here */

    if (VBOX_FAILURE (vrc))
    {
        LogWarningThisFunc (("Failed to create proxy device for '%s' {%Vuuid} (%Vrc)\n",
                             Address.raw(), Uuid.ptr(), vrc));

        if (aManual)
        {
            /*
             *  Neither SessionMachine::ReleaseUSBDevice() nor Host::releaseUSBDevice()
             *  should call the Console back, so keep the lock to provide atomicity
             *  (to protect Host reapplying USB filters)
             */
            hrc = mControl->ReleaseUSBDevice (Uuid);
            AssertComRC (hrc);
        }

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
                    tr ("Failed to create a proxy device for the USB device.  (Error: %Vrc)"), vrc);
                break;
        }
    }

    return hrc;
}

/**
 *  USB device attack callback used by AttachUSBDevice().
 *  Note that AttachUSBDevice() doesn't return until this callback is executed,
 *  so we don't use AutoCaller and don't care about reference counters of
 *  interface pointers passed in.
 *
 *  @thread EMT
 *  @note Locks the console object for writing.
 */
//static
DECLCALLBACK(int)
Console::usbAttachCallback (Console *that, IUSBDevice *aHostDevice,
                            PVUSBIRHCONFIG aConfig, PCRTUUID aUuid, bool aRemote,
                            const char *aAddress, void *aRemoteBackend)
{
    LogFlowFuncEnter();
    LogFlowFunc (("that={%p}\n", that));

    AssertReturn (that && aConfig && aUuid, VERR_INVALID_PARAMETER);

#ifdef VRDP_MC
    if (aRemote)
    {
        /* @todo aRemoteBackend input parameter is not needed. */
        Assert (aRemoteBackend == NULL);

        RemoteUSBDevice *pRemoteUSBDevice = static_cast <RemoteUSBDevice *> (aHostDevice);

        Guid guid (*aUuid);

        aRemoteBackend = that->consoleVRDPServer ()->USBBackendRequestPointer (pRemoteUSBDevice->clientId (), &guid);

        if (aRemoteBackend == NULL)
        {
            /* The clientId is invalid then. */
            return VERR_INVALID_PARAMETER;
        }
    }
#endif /* VRDP_MC */

    int vrc = aConfig->pfnCreateProxyDevice (aConfig, aUuid, aRemote, aAddress,
                                             aRemoteBackend);

    if (VBOX_SUCCESS (vrc))
    {
        /* Create a OUSBDevice and add it to the device list */
        ComObjPtr <OUSBDevice> device;
        device.createObject();
        HRESULT hrc = device->init (aHostDevice);
        AssertComRC (hrc);

        AutoLock alock (that);
        that->mUSBDevices.push_back (device);
        LogFlowFunc (("Attached device {%Vuuid}\n", device->id().raw()));
    }

    LogFlowFunc (("vrc=%Vrc\n", vrc));
    LogFlowFuncLeave();
    return vrc;
}

/**
 *  USB device attack callback used by AttachUSBDevice().
 *  Note that AttachUSBDevice() doesn't return until this callback is executed,
 *  so we don't use AutoCaller and don't care about reference counters of
 *  interface pointers passed in.
 *
 *  @thread EMT
 *  @note Locks the console object for writing.
 */
//static
DECLCALLBACK(int)
Console::usbDetachCallback (Console *that, USBDeviceList::iterator *aIt,
                            bool aManual, PVUSBIRHCONFIG aConfig, PCRTUUID aUuid)
{
    LogFlowFuncEnter();
    LogFlowFunc (("that={%p}\n", that));

    AssertReturn (that && aConfig && aUuid, VERR_INVALID_PARAMETER);

#ifdef VRDP_MC
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
#endif /* VRDP_MC */

    int vrc = aConfig->pfnDestroyProxyDevice (aConfig, aUuid);

    if (VBOX_SUCCESS (vrc))
    {
        AutoLock alock (that);

        /* Remove the device from the collection */
        that->mUSBDevices.erase (*aIt);
        LogFlowFunc (("Detached device {%Vuuid}\n", (**aIt)->id().raw()));

        /// @todo (dmik) REMOTE_USB
        //  if the device is remote, notify a remote client that we have
        //  detached the device

        /* If it's a manual detach, give it back to the USB Proxy */
        if (aManual)
        {
            /*
             *  Neither SessionMachine::ReleaseUSBDevice() nor Host::releaseUSBDevice()
             *  should call the Console back, so keep the lock to provide atomicity
             *  (to protect Host reapplying USB filters)
             */
            LogFlowFunc (("Giving it back it to USB proxy...\n"));
            HRESULT hrc = that->mControl->ReleaseUSBDevice (Guid (*aUuid));
            AssertComRC (hrc);
            vrc = SUCCEEDED (hrc) ? VINF_SUCCESS : VERR_GENERAL_FAILURE;
        }
    }

    LogFlowFunc (("vrc=%Vrc\n", vrc));
    LogFlowFuncLeave();
    return vrc;
}

/**
 *  Construct the VM configuration tree (CFGM).
 *
 *  This is a callback for VMR3Create() call. It is called from CFGMR3Init()
 *  in the emulation thread (EMT). Any per thread COM/XPCOM initialization
 *  is done here.
 *
 *  @param   pVM                 VM handle.
 *  @param   pvTask              Pointer to the VMPowerUpTask object.
 *  @return  VBox status code.
 *
 *  @note Locks the Console object for writing.
 */
DECLCALLBACK(int) Console::configConstructor(PVM pVM, void *pvTask)
{
    LogFlowFuncEnter();

    /* Note: the task pointer is owned by powerUpThread() */
    VMPowerUpTask *task = static_cast <VMPowerUpTask *> (pvTask);
    AssertReturn (task, VERR_GENERAL_FAILURE);

#if defined(__WIN__)
    {
        /* initialize COM */
        HRESULT hrc = CoInitializeEx(NULL,
                                     COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE |
                                     COINIT_SPEED_OVER_MEMORY);
        LogFlow (("Console::configConstructor(): CoInitializeEx()=%08X\n", hrc));
        AssertComRCReturn (hrc, VERR_GENERAL_FAILURE);
    }
#endif

    ComObjPtr <Console> pConsole = task->mConsole;

    AutoCaller autoCaller (pConsole);
    AssertComRCReturn (autoCaller.rc(), VERR_ACCESS_DENIED);

    /* lock the console because we widely use internal fields and methods */
    AutoLock alock (pConsole);

    ComPtr <IMachine> pMachine = pConsole->machine();

    int             rc;
    HRESULT         hrc;
    char           *psz = NULL;
    BSTR            str = NULL;
    ULONG           cRamMBs;
    unsigned        i;

#define STR_CONV()  do { rc = RTStrUcs2ToUtf8(&psz, str); RC_CHECK(); } while (0)
#define STR_FREE()  do { if (str) { SysFreeString(str); str = NULL; } if (psz) { RTStrFree(psz); psz = NULL; } } while (0)
#define RC_CHECK()  do { if (VBOX_FAILURE(rc)) { AssertMsgFailed(("rc=%Vrc\n", rc)); STR_FREE(); return rc; } } while (0)
#define H() do { if (FAILED(hrc)) { AssertMsgFailed(("hrc=%#x\n", hrc)); STR_FREE(); return VERR_GENERAL_FAILURE; } } while (0)

    /* Get necessary objects */

    ComPtr<IVirtualBox> virtualBox;
    hrc = pMachine->COMGETTER(Parent)(virtualBox.asOutParam());                     H();

    ComPtr<IHost> host;
    hrc = virtualBox->COMGETTER(Host)(host.asOutParam());                           H();

    ComPtr <ISystemProperties> systemProperties;
    hrc = virtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());   H();

    ComPtr<IBIOSSettings> biosSettings;
    hrc = pMachine->COMGETTER(BIOSSettings)(biosSettings.asOutParam());             H();


    /*
     * Get root node first.
     * This is the only node in the tree.
     */
    PCFGMNODE pRoot = CFGMR3GetRoot(pVM);
    Assert(pRoot);

    /*
     * Set the root level values.
     */
    hrc = pMachine->COMGETTER(Name)(&str);                                          H();
    STR_CONV();
    rc = CFGMR3InsertString(pRoot,  "Name",                 psz);                   RC_CHECK();
    STR_FREE();
    hrc = pMachine->COMGETTER(MemorySize)(&cRamMBs);                                H();
    rc = CFGMR3InsertInteger(pRoot, "RamSize",              cRamMBs * _1M);         RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "TimerMillies",         10);                    RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "RawR3Enabled",         1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "RawR0Enabled",         1);     /* boolean */   RC_CHECK();
    /** @todo Config: RawR0, PATMEnabled and CASMEnabled needs attention later. */
    rc = CFGMR3InsertInteger(pRoot, "PATMEnabled",          1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "CSAMEnabled",          1);     /* boolean */   RC_CHECK();

    /* hardware virtualization extensions */
    TriStateBool_T hwVirtExEnabled;
    BOOL fHWVirtExEnabled;
    hrc = pMachine->COMGETTER(HWVirtExEnabled)(&hwVirtExEnabled);                   H();
    if (hwVirtExEnabled == TriStateBool_Default)
    {
        /* check the default value */
        hrc = systemProperties->COMGETTER(HWVirtExEnabled)(&fHWVirtExEnabled);      H();
    }
    else
        fHWVirtExEnabled = (hwVirtExEnabled == TriStateBool_True);
    if (fHWVirtExEnabled)
    {
        PCFGMNODE pHWVirtExt;
        rc = CFGMR3InsertNode(pRoot, "HWVirtExt", &pHWVirtExt);                     RC_CHECK();
        rc = CFGMR3InsertInteger(pHWVirtExt, "Enabled", 1);                         RC_CHECK();
    }

    BOOL fIOAPIC;
    hrc = biosSettings->COMGETTER(IOAPICEnabled)(&fIOAPIC);                          H();

    /*
     * PDM config.
     *  Load drivers in VBoxC.[so|dll]
     */
    PCFGMNODE pPDM;
    PCFGMNODE pDrivers;
    PCFGMNODE pMod;
    rc = CFGMR3InsertNode(pRoot,    "PDM", &pPDM);                                     RC_CHECK();
    rc = CFGMR3InsertNode(pPDM,     "Drivers", &pDrivers);                             RC_CHECK();
    rc = CFGMR3InsertNode(pDrivers, "VBoxC", &pMod);                                   RC_CHECK();
#ifdef VBOX_WITH_XPCOM
    // VBoxC is located in the components subdirectory
    char szPathProgram[RTPATH_MAX + sizeof("/components/VBoxC")];
    rc = RTPathProgram(szPathProgram, RTPATH_MAX);                                  AssertRC(rc);
    strcat(szPathProgram, "/components/VBoxC");
    rc = CFGMR3InsertString(pMod,   "Path",  szPathProgram);                           RC_CHECK();
#else
    rc = CFGMR3InsertString(pMod,   "Path",  "VBoxC");                                 RC_CHECK();
#endif

    /*
     * Devices
     */
    PCFGMNODE pDevices = NULL;      /* /Devices */
    PCFGMNODE pDev = NULL;          /* /Devices/Dev/ */
    PCFGMNODE pInst = NULL;         /* /Devices/Dev/0/ */
    PCFGMNODE pCfg = NULL;          /* /Devices/Dev/.../Config/ */
    PCFGMNODE pLunL0 = NULL;        /* /Devices/Dev/0/LUN#0/ */
    PCFGMNODE pLunL1 = NULL;        /* /Devices/Dev/0/LUN#0/AttachedDriver/ */
    rc = CFGMR3InsertNode(pRoot, "Devices", &pDevices);                             RC_CHECK();

    /*
     * PC Arch.
     */
    rc = CFGMR3InsertNode(pDevices, "pcarch", &pDev);                               RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();

    /*
     * PC Bios.
     */
    rc = CFGMR3InsertNode(pDevices, "pcbios", &pDev);                               RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "RamSize",              cRamMBs * _1M);         RC_CHECK();
    rc = CFGMR3InsertString(pCfg,   "HardDiskDevice",       "piix3ide");            RC_CHECK();
    rc = CFGMR3InsertString(pCfg,   "FloppyDevice",         "i82078");              RC_CHECK();

    DeviceType_T bootDevice;
    if (SchemaDefs::MaxBootPosition > 9)
    {
        AssertMsgFailed (("Too many boot devices %d\n",
                          SchemaDefs::MaxBootPosition));
        return VERR_INVALID_PARAMETER;
    }

    for (ULONG pos = 1; pos <= SchemaDefs::MaxBootPosition; pos ++)
    {
        hrc = pMachine->GetBootOrder(pos, &bootDevice);                             H();

        char szParamName[] = "BootDeviceX";
        szParamName[sizeof (szParamName) - 2] = ((char (pos - 1)) + '0');

        const char *pszBootDevice;
        switch (bootDevice)
        {
            case DeviceType_NoDevice:
                pszBootDevice = "NONE";
                break;
            case DeviceType_HardDiskDevice:
                pszBootDevice = "IDE";
                break;
            case DeviceType_DVDDevice:
                pszBootDevice = "DVD";
                break;
            case DeviceType_FloppyDevice:
                pszBootDevice = "FLOPPY";
                break;
            case DeviceType_NetworkDevice:
                pszBootDevice = "LAN";
                break;
            default:
                AssertMsgFailed(("Invalid bootDevice=%d\n", bootDevice));
                return VERR_INVALID_PARAMETER;
        }
        rc = CFGMR3InsertString(pCfg, szParamName, pszBootDevice);                 RC_CHECK();
    }

    /*
     * BIOS logo
     */
    BOOL fFadeIn;
    hrc = biosSettings->COMGETTER(LogoFadeIn)(&fFadeIn);                            H();
    rc = CFGMR3InsertInteger(pCfg,  "FadeIn",  fFadeIn ? 1 : 0);                    RC_CHECK();
    BOOL fFadeOut;
    hrc = biosSettings->COMGETTER(LogoFadeOut)(&fFadeOut);                          H();
    rc = CFGMR3InsertInteger(pCfg,  "FadeOut", fFadeOut ? 1: 0);                    RC_CHECK();
    ULONG logoDisplayTime;
    hrc = biosSettings->COMGETTER(LogoDisplayTime)(&logoDisplayTime);               H();
    rc = CFGMR3InsertInteger(pCfg,  "LogoTime", logoDisplayTime);                   RC_CHECK();
    Bstr logoImagePath;
    hrc = biosSettings->COMGETTER(LogoImagePath)(logoImagePath.asOutParam());       H();
    rc = CFGMR3InsertString(pCfg,   "LogoFile", logoImagePath ? Utf8Str(logoImagePath) : ""); RC_CHECK();

    /*
     * Boot menu
     */
    BIOSBootMenuMode_T bootMenuMode;
    int value;
    biosSettings->COMGETTER(BootMenuMode)(&bootMenuMode);
    switch (bootMenuMode)
    {
        case BIOSBootMenuMode_Disabled:
            value = 0;
            break;
        case BIOSBootMenuMode_MenuOnly:
            value = 1;
            break;
        default:
            value = 2;
    }
    rc = CFGMR3InsertInteger(pCfg, "ShowBootMenu", value);                          RC_CHECK();

    /*
     * ACPI
     */
    BOOL fACPI;
    hrc = biosSettings->COMGETTER(ACPIEnabled)(&fACPI);                             H();
    if (fACPI)
    {
        rc = CFGMR3InsertNode(pDevices, "acpi", &pDev);                             RC_CHECK();
        rc = CFGMR3InsertNode(pDev,     "0", &pInst);                               RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted", 1);              /* boolean */   RC_CHECK();
        rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                           RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "RamSize",          cRamMBs * _1M);         RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "IOAPIC", fIOAPIC);                         RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          7);                 RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                 RC_CHECK();

        rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                          RC_CHECK();
        rc = CFGMR3InsertString(pLunL0, "Driver",               "ACPIHost");        RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                           RC_CHECK();
    }

    /*
     * DMA
     */
    rc = CFGMR3InsertNode(pDevices, "8237A", &pDev);                                RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted", 1);                  /* boolean */   RC_CHECK();

    /*
     * PCI bus.
     */
    rc = CFGMR3InsertNode(pDevices, "pci", &pDev); /* piix3 */                      RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "IOAPIC", fIOAPIC);                             RC_CHECK();

    /*
     * PS/2 keyboard & mouse.
     */
    rc = CFGMR3InsertNode(pDevices, "pckbd", &pDev);                                RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();

    rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                              RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "KeyboardQueue");       RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "QueueSize",            64);                    RC_CHECK();

    rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);                     RC_CHECK();
    rc = CFGMR3InsertString(pLunL1, "Driver",               "MainKeyboard");        RC_CHECK();
    rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                               RC_CHECK();
    Keyboard *pKeyboard = pConsole->mKeyboard;
    rc = CFGMR3InsertInteger(pCfg,  "Object",     (uintptr_t)pKeyboard);            RC_CHECK();

    rc = CFGMR3InsertNode(pInst,    "LUN#1", &pLunL0);                              RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "MouseQueue");          RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "QueueSize",            128);                   RC_CHECK();

    rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);                     RC_CHECK();
    rc = CFGMR3InsertString(pLunL1, "Driver",               "MainMouse");           RC_CHECK();
    rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                               RC_CHECK();
    Mouse *pMouse = pConsole->mMouse;
    rc = CFGMR3InsertInteger(pCfg,  "Object",     (uintptr_t)pMouse);               RC_CHECK();

    /*
     * i82078 Floppy drive controller
     */
    ComPtr<IFloppyDrive> floppyDrive;
    hrc = pMachine->COMGETTER(FloppyDrive)(floppyDrive.asOutParam());               H();
    BOOL fFloppyEnabled;
    hrc = floppyDrive->COMGETTER(Enabled)(&fFloppyEnabled);                         H();
    if (fFloppyEnabled)
    {
        rc = CFGMR3InsertNode(pDevices, "i82078",    &pDev);                        RC_CHECK();
        rc = CFGMR3InsertNode(pDev,     "0",         &pInst);                       RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted",   1);                            RC_CHECK();
        rc = CFGMR3InsertNode(pInst,    "Config",    &pCfg);                        RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "IRQ",       6);                            RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "DMA",       2);                            RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "MemMapped", 0 );                           RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "IOBase",    0x3f0);                        RC_CHECK();

        /* Attach the status driver */
        rc = CFGMR3InsertNode(pInst,    "LUN#999", &pLunL0);                            RC_CHECK();
        rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");          RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapFDLeds[0]); RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "First",    0);                                 RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "Last",     0);                                 RC_CHECK();

        rc = CFGMR3InsertNode(pInst,    "LUN#0",     &pLunL0);                          RC_CHECK();

        ComPtr<IFloppyImage> floppyImage;
        hrc = floppyDrive->GetImage(floppyImage.asOutParam());                          H();
        if (floppyImage)
        {
            pConsole->meFloppyState = DriveState_ImageMounted;
            rc = CFGMR3InsertString(pLunL0, "Driver",    "Block");                      RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0,   "Config",    &pCfg);                        RC_CHECK();
            rc = CFGMR3InsertString(pCfg,   "Type",      "Floppy 1.44");                RC_CHECK();
            rc = CFGMR3InsertInteger(pCfg,  "Mountable", 1);                            RC_CHECK();

            rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);                 RC_CHECK();
            rc = CFGMR3InsertString(pLunL1, "Driver",          "RawImage");             RC_CHECK();
            rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                           RC_CHECK();
            hrc = floppyImage->COMGETTER(FilePath)(&str);                               H();
            STR_CONV();
            rc = CFGMR3InsertString(pCfg,   "Path",             psz);                   RC_CHECK();
            STR_FREE();
        }
        else
        {
            ComPtr<IHostFloppyDrive> hostFloppyDrive;
            hrc = floppyDrive->GetHostDrive(hostFloppyDrive.asOutParam());              H();
            if (hostFloppyDrive)
            {
                pConsole->meFloppyState = DriveState_HostDriveCaptured;
                rc = CFGMR3InsertString(pLunL0, "Driver",      "HostFloppy");           RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                       RC_CHECK();
                hrc = hostFloppyDrive->COMGETTER(Name)(&str);                           H();
                STR_CONV();
                rc = CFGMR3InsertString(pCfg,   "Path",         psz);                   RC_CHECK();
                STR_FREE();
            }
            else
            {
                pConsole->meFloppyState = DriveState_NotMounted;
                rc = CFGMR3InsertString(pLunL0, "Driver",    "Block");       RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,   "Config",    &pCfg);         RC_CHECK();
                rc = CFGMR3InsertString(pCfg,   "Type",      "Floppy 1.44"); RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "Mountable", 1);             RC_CHECK();
            }
        }
    }

    /*
     * i8254 Programmable Interval Timer And Dummy Speaker
     */
    rc = CFGMR3InsertNode(pDevices, "i8254", &pDev);                                RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
#ifdef DEBUG
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
#endif

    /*
     * i8259 Programmable Interrupt Controller.
     */
    rc = CFGMR3InsertNode(pDevices, "i8259", &pDev);                                RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();

    /*
     * Advanced Programmable Interrupt Controller.
     */
    rc = CFGMR3InsertNode(pDevices, "apic", &pDev);                                 RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();

    if (fIOAPIC)
    {
        /*
         * I/O Advanced Programmable Interrupt Controller.
         */
        rc = CFGMR3InsertNode(pDevices, "ioapic", &pDev);                           RC_CHECK();
        rc = CFGMR3InsertNode(pDev,     "0", &pInst);                               RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted",          1);     /* boolean */   RC_CHECK();
        rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                           RC_CHECK();
    }

    /*
     * RTC MC146818.
     */
    rc = CFGMR3InsertNode(pDevices, "mc146818", &pDev);                             RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();

#if 0
    /*
     * Serial ports
     */
    rc = CFGMR3InsertNode(pDevices, "serial", &pDev);                               RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "IRQ",       4);                                RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "IOBase",    0x3f8);                            RC_CHECK();

    rc = CFGMR3InsertNode(pDev,     "1", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "IRQ",       3);                                RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "IOBase",    0x2f8);                            RC_CHECK();
#endif

    /*
     * VGA.
     */
    rc = CFGMR3InsertNode(pDevices, "vga", &pDev);                                  RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          2);                     RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                     RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
    hrc = pMachine->COMGETTER(VRAMSize)(&cRamMBs);                                  H();
    rc = CFGMR3InsertInteger(pCfg,  "VRamSize",             cRamMBs * _1M);         RC_CHECK();

    /* Custom VESA mode list */
    unsigned cModes = 0;
    for (unsigned iMode = 1; iMode <= 16; iMode++)
    {
        char szExtraDataKey[sizeof("CustomVideoModeXX")];
        RTStrPrintf(szExtraDataKey, sizeof(szExtraDataKey), "CustomVideoMode%d", iMode);
        hrc = pMachine->GetExtraData(Bstr(szExtraDataKey), &str);                   H();
        if (!str || !*str)
            break;
        STR_CONV();
        rc = CFGMR3InsertString(pCfg, szExtraDataKey, psz);
        STR_FREE();
        cModes++;
    }
    rc = CFGMR3InsertInteger(pCfg,  "CustomVideoModes", cModes);

    /* VESA height reduction */
    ULONG ulHeightReduction;
    IFramebuffer *pFramebuffer = pConsole->getDisplay()->getFramebuffer();
    hrc = pFramebuffer->COMGETTER(HeightReduction)(&ulHeightReduction);             H();
    rc = CFGMR3InsertInteger(pCfg,  "HeightReduction", ulHeightReduction);          RC_CHECK();

    /* Attach the display. */
    rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                              RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "MainDisplay");         RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
    Display *pDisplay = pConsole->mDisplay;
    rc = CFGMR3InsertInteger(pCfg,  "Object", (uintptr_t)pDisplay);                 RC_CHECK();

    /*
     * IDE (update this when the main interface changes)
     */
    rc = CFGMR3InsertNode(pDevices, "piix3ide", &pDev); /* piix3 */                 RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          1);                     RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        1);                     RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();

    /* Attach the status driver */
    rc = CFGMR3InsertNode(pInst,    "LUN#999", &pLunL0);                            RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");          RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapIDELeds[0]);RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "First",    0);                                 RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "Last",     3);                                 RC_CHECK();

    /* Attach the harddisks */
    ComPtr<IHardDiskAttachmentCollection> hdaColl;
    hrc = pMachine->COMGETTER(HardDiskAttachments)(hdaColl.asOutParam());           H();
    ComPtr<IHardDiskAttachmentEnumerator> hdaEnum;
    hrc = hdaColl->Enumerate(hdaEnum.asOutParam());                                 H();

    BOOL fMore = FALSE;
    while (     SUCCEEDED(hrc = hdaEnum->HasMore(&fMore))
           &&   fMore)
    {
        ComPtr<IHardDiskAttachment> hda;
        hrc = hdaEnum->GetNext(hda.asOutParam());                                   H();
        ComPtr<IHardDisk> hardDisk;
        hrc = hda->COMGETTER(HardDisk)(hardDisk.asOutParam());                      H();
        DiskControllerType_T enmCtl;
        hrc = hda->COMGETTER(Controller)(&enmCtl);                                  H();
        LONG lDev;
        hrc = hda->COMGETTER(DeviceNumber)(&lDev);                                  H();

        switch (enmCtl)
        {
            case DiskControllerType_IDE0Controller:
                i = 0;
                break;
            case DiskControllerType_IDE1Controller:
                i = 2;
                break;
            default:
                AssertMsgFailed(("invalid disk controller type: %d\n", enmCtl));
                return VERR_GENERAL_FAILURE;
        }

        if (lDev < 0 || lDev >= 2)
        {
            AssertMsgFailed(("invalid controller device number: %d\n", lDev));
            return VERR_GENERAL_FAILURE;
        }

        i = i + lDev;

        char szLUN[16];
        RTStrPrintf(szLUN, sizeof(szLUN), "LUN#%d", i);
        rc = CFGMR3InsertNode(pInst,    szLUN, &pLunL0);                            RC_CHECK();
        rc = CFGMR3InsertString(pLunL0, "Driver",               "Block");           RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                           RC_CHECK();
        rc = CFGMR3InsertString(pCfg,   "Type",                 "HardDisk");        RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "Mountable",            0);                 RC_CHECK();

        HardDiskStorageType_T hddType;
        hardDisk->COMGETTER(StorageType)(&hddType);
        if (hddType == HardDiskStorageType_VirtualDiskImage)
        {
            ComPtr<IVirtualDiskImage> vdiDisk = hardDisk;
            AssertBreak (!vdiDisk.isNull(), hrc = E_FAIL);

            rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);                 RC_CHECK();
            rc = CFGMR3InsertString(pLunL1, "Driver",         "VBoxHDD");               RC_CHECK();
            rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                           RC_CHECK();
            hrc = vdiDisk->COMGETTER(FilePath)(&str);                                   H();
            STR_CONV();
            rc = CFGMR3InsertString(pCfg,   "Path",             psz);                   RC_CHECK();
            STR_FREE();

            /* Create an inversed tree of parents. */
            ComPtr<IHardDisk> parentHardDisk = hardDisk;
            for (PCFGMNODE pParent = pCfg;;)
            {
                ComPtr<IHardDisk> curHardDisk;
                hrc = parentHardDisk->COMGETTER(Parent)(curHardDisk.asOutParam());      H();
                if (!curHardDisk)
                    break;

                vdiDisk = curHardDisk;
                AssertBreak (!vdiDisk.isNull(), hrc = E_FAIL);

                PCFGMNODE pCur;
                rc = CFGMR3InsertNode(pParent, "Parent", &pCur);                        RC_CHECK();
                hrc = vdiDisk->COMGETTER(FilePath)(&str);                               H();
                STR_CONV();
                rc = CFGMR3InsertString(pCur,  "Path", psz);                            RC_CHECK();
                STR_FREE();

                /* next */
                pParent = pCur;
                parentHardDisk = curHardDisk;
            }
        }
        else if (hddType == HardDiskStorageType_ISCSIHardDisk)
        {
            ComPtr<IISCSIHardDisk> iSCSIDisk = hardDisk;
            AssertBreak (!iSCSIDisk.isNull(), hrc = E_FAIL);

            rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);                 RC_CHECK();
            rc = CFGMR3InsertString(pLunL1, "Driver",         "iSCSI");                 RC_CHECK();
            rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                           RC_CHECK();

            /* Set up the iSCSI initiator driver configuration. */
            hrc = iSCSIDisk->COMGETTER(Target)(&str);                                   H();
            STR_CONV();
            rc = CFGMR3InsertString(pCfg,   "TargetName",   psz);                       RC_CHECK();
            STR_FREE();

            // @todo currently there is no Initiator name config.
            rc = CFGMR3InsertString(pCfg,   "InitiatorName", "iqn.2006-02.de.innotek.initiator"); RC_CHECK();

            ULONG64 lun;
            hrc = iSCSIDisk->COMGETTER(Lun)(&lun);                                      H();
            rc = CFGMR3InsertInteger(pCfg,   "LUN",         lun);                       RC_CHECK();

            hrc = iSCSIDisk->COMGETTER(Server)(&str);                                   H();
            STR_CONV();
            USHORT port;
            hrc = iSCSIDisk->COMGETTER(Port)(&port);                                    H();
            if (port != 0)
            {
                char *pszTN;
                RTStrAPrintf(&pszTN, "%s:%u", psz, port);
                rc = CFGMR3InsertString(pCfg,   "TargetAddress",    pszTN);             RC_CHECK();
                RTStrFree(pszTN);
            }
            else
            {
                rc = CFGMR3InsertString(pCfg,   "TargetAddress",    psz);               RC_CHECK();
            }
            STR_FREE();

            hrc = iSCSIDisk->COMGETTER(UserName)(&str);                                 H();
            if (str)
            {
                STR_CONV();
                rc = CFGMR3InsertString(pCfg,   "InitiatorUsername",    psz);           RC_CHECK();
                STR_FREE();
            }

            hrc = iSCSIDisk->COMGETTER(Password)(&str);                                 H();
            if (str)
            {
                STR_CONV();
                rc = CFGMR3InsertString(pCfg,   "InitiatorSecret",      psz);           RC_CHECK();
                STR_FREE();
            }

            // @todo currently there is no target username config.
            //rc = CFGMR3InsertString(pCfg,   "TargetUsername",   "");                    RC_CHECK();

            // @todo currently there is no target password config.
            //rc = CFGMR3InsertString(pCfg,   "TargetSecret",     "");                    RC_CHECK();

            /* The iSCSI initiator needs an attached iSCSI transport driver. */
            PCFGMNODE pLunL2 = NULL;        /* /Devices/Dev/0/LUN#0/AttachedDriver/AttachedDriver */
            rc = CFGMR3InsertNode(pLunL1,   "AttachedDriver", &pLunL2);                 RC_CHECK();
            rc = CFGMR3InsertString(pLunL2, "Driver",           "iSCSITCP");            RC_CHECK();
            /* Currently the transport driver has no config options. */
        }
        else if (hddType == HardDiskStorageType_VMDKImage)
        {
            ComPtr<IVMDKImage> vmdkDisk = hardDisk;
            AssertBreak (!vmdkDisk.isNull(), hrc = E_FAIL);

            rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);                 RC_CHECK();
            rc = CFGMR3InsertString(pLunL1, "Driver",         "VmdkHDD");               RC_CHECK();
            rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                           RC_CHECK();
            hrc = vmdkDisk->COMGETTER(FilePath)(&str);                                  H();
            STR_CONV();
            rc = CFGMR3InsertString(pCfg,   "Path",             psz);                   RC_CHECK();
            STR_FREE();
        }
        else
           AssertFailed();
    }
    H();

    ComPtr<IDVDDrive> dvdDrive;
    hrc = pMachine->COMGETTER(DVDDrive)(dvdDrive.asOutParam());                     H();
    if (dvdDrive)
    {
        // ASSUME: DVD drive is always attached to LUN#2 (i.e. secondary IDE master)
        rc = CFGMR3InsertNode(pInst,    "LUN#2", &pLunL0);                          RC_CHECK();
        ComPtr<IHostDVDDrive> hostDvdDrive;
        hrc = dvdDrive->GetHostDrive(hostDvdDrive.asOutParam());                    H();
        if (hostDvdDrive)
        {
            pConsole->meDVDState = DriveState_HostDriveCaptured;
            rc = CFGMR3InsertString(pLunL0, "Driver",      "HostDVD");              RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                       RC_CHECK();
            hrc = hostDvdDrive->COMGETTER(Name)(&str);                              H();
            STR_CONV();
            rc = CFGMR3InsertString(pCfg,   "Path",         psz);                   RC_CHECK();
            STR_FREE();
            BOOL fPassthrough;
            hrc = dvdDrive->COMGETTER(Passthrough)(&fPassthrough);                  H();
            rc = CFGMR3InsertInteger(pCfg,  "Passthrough",  !!fPassthrough);        RC_CHECK();
        }
        else
        {
            pConsole->meDVDState = DriveState_NotMounted;
            rc = CFGMR3InsertString(pLunL0, "Driver",               "Block");       RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                       RC_CHECK();
            rc = CFGMR3InsertString(pCfg,   "Type",                 "DVD");         RC_CHECK();
            rc = CFGMR3InsertInteger(pCfg,  "Mountable",            1);             RC_CHECK();

            ComPtr<IDVDImage> dvdImage;
            hrc = dvdDrive->GetImage(dvdImage.asOutParam());                        H();
            if (dvdImage)
            {
                pConsole->meDVDState = DriveState_ImageMounted;
                rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);         RC_CHECK();
                rc = CFGMR3InsertString(pLunL1, "Driver",          "MediaISO");     RC_CHECK();
                rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                   RC_CHECK();
                hrc = dvdImage->COMGETTER(FilePath)(&str);                          H();
                STR_CONV();
                rc = CFGMR3InsertString(pCfg,   "Path",             psz);           RC_CHECK();
                STR_FREE();
            }
        }
    }

    /*
     * Network adapters
     */
    rc = CFGMR3InsertNode(pDevices, "pcnet", &pDev);                                RC_CHECK();
    //rc = CFGMR3InsertNode(pDevices, "ne2000", &pDev);                               RC_CHECK();
    for (ULONG ulInstance = 0; ulInstance < SchemaDefs::NetworkAdapterCount; ulInstance++)
    {
        ComPtr<INetworkAdapter> networkAdapter;
        hrc = pMachine->GetNetworkAdapter(ulInstance, networkAdapter.asOutParam()); H();
        BOOL fEnabled = FALSE;
        hrc = networkAdapter->COMGETTER(Enabled)(&fEnabled);                        H();
        if (!fEnabled)
            continue;

        char szInstance[4]; Assert(ulInstance <= 999);
        RTStrPrintf(szInstance, sizeof(szInstance), "%lu", ulInstance);
        rc = CFGMR3InsertNode(pDev, szInstance, &pInst);                            RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted",              1); /* boolean */   RC_CHECK();
        /* the first network card gets the PCI ID 3, the followings starting from 8 */
        rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo", !ulInstance ? 3 : ulInstance - 1 + 8); RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                 RC_CHECK();
        rc = CFGMR3InsertNode(pInst, "Config", &pCfg);                              RC_CHECK();

        /*
         * The virtual hardware type.
         */
        NetworkAdapterType_T adapterType;
        hrc = networkAdapter->COMGETTER(AdapterType)(&adapterType);                 H();
        switch (adapterType)
        {
            case NetworkAdapterType_NetworkAdapterAm79C970A:
                rc = CFGMR3InsertInteger(pCfg, "Am79C973", 0);                      RC_CHECK();
                break;
            case NetworkAdapterType_NetworkAdapterAm79C973:
                rc = CFGMR3InsertInteger(pCfg, "Am79C973", 1);                      RC_CHECK();
                break;
            default:
                AssertMsgFailed(("Invalid network adapter type '%d' for slot '%d'",
                                 adapterType, ulInstance));
                return VERR_GENERAL_FAILURE;
        }

        /*
         * Get the MAC address and convert it to binary representation
         */
        Bstr macAddr;
        hrc = networkAdapter->COMGETTER(MACAddress)(macAddr.asOutParam());          H();
        Assert(macAddr);
        Utf8Str macAddrUtf8 = macAddr;
        char *macStr = (char*)macAddrUtf8.raw();
        Assert(strlen(macStr) == 12);
        PDMMAC Mac;
        memset(&Mac, 0, sizeof(Mac));
        char *pMac = (char*)&Mac;
        for (uint32_t i = 0; i < 6; i++)
        {
            char c1 = *macStr++ - '0';
            if (c1 > 9)
                c1 -= 7;
            char c2 = *macStr++ - '0';
            if (c2 > 9)
                c2 -= 7;
            *pMac++ = ((c1 & 0x0f) << 4) | (c2 & 0x0f);
        }
        rc = CFGMR3InsertBytes(pCfg, "MAC", &Mac, sizeof(Mac));                     RC_CHECK();

        /*
         * Check if the cable is supposed to be unplugged
         */
        BOOL fCableConnected;
        hrc = networkAdapter->COMGETTER(CableConnected)(&fCableConnected);          H();
        rc = CFGMR3InsertInteger(pCfg, "CableConnected", fCableConnected ? 1 : 0);  RC_CHECK();

        /*
         * Attach the status driver.
         */
        rc = CFGMR3InsertNode(pInst,    "LUN#999", &pLunL0);                        RC_CHECK();
        rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");      RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                           RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapNetworkLeds[ulInstance]); RC_CHECK();

        /*
         * Enable the packet sniffer if requested.
         */
        BOOL fSniffer;
        hrc = networkAdapter->COMGETTER(TraceEnabled)(&fSniffer);                   H();
        if (fSniffer)
        {
            /* insert the sniffer filter driver. */
            rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);                         RC_CHECK();
            rc = CFGMR3InsertString(pLunL0, "Driver", "NetSniffer");                RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                         RC_CHECK();
            hrc = networkAdapter->COMGETTER(TraceFile)(&str);                       H();
            if (str) /* check convention for indicating default file. */
            {
                STR_CONV();
                rc = CFGMR3InsertString(pCfg, "File", psz);                         RC_CHECK();
                STR_FREE();
            }
        }

        NetworkAttachmentType_T networkAttachment;
        hrc = networkAdapter->COMGETTER(AttachmentType)(&networkAttachment);        H();
        switch (networkAttachment)
        {
            case NetworkAttachmentType_NoNetworkAttachment:
                break;

            case NetworkAttachmentType_NATNetworkAttachment:
            {
                if (fSniffer)
                {
                    rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL0);       RC_CHECK();
                }
                else
                {
                    rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);                 RC_CHECK();
                }
                rc = CFGMR3InsertString(pLunL0, "Driver", "NAT");                   RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                     RC_CHECK();
                /* (Port forwarding goes here.) */
                break;
            }

            case NetworkAttachmentType_HostInterfaceNetworkAttachment:
            {
                /*
                 * Perform the attachment if required (don't return on error!)
                 */
                hrc = pConsole->attachToHostInterface(networkAdapter);
                if (SUCCEEDED(hrc))
                {
#ifdef VBOX_WITH_UNIXY_TAP_NETWORKING
                    Assert (pConsole->maTapFD[ulInstance] >= 0);
                    if (pConsole->maTapFD[ulInstance] >= 0)
                    {
                        if (fSniffer)
                        {
                            rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL0); RC_CHECK();
                        }
                        else
                        {
                            rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);         RC_CHECK();
                        }
                        rc = CFGMR3InsertString(pLunL0, "Driver", "HostInterface"); RC_CHECK();
                        rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);             RC_CHECK();
                        rc = CFGMR3InsertInteger(pCfg, "FileHandle", pConsole->maTapFD[ulInstance]); RC_CHECK();
                    }
#elif defined(__WIN__)
                    if (fSniffer)
                    {
                        rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL0);   RC_CHECK();
                    }
                    else
                    {
                        rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);             RC_CHECK();
                    }
                    Bstr hostInterfaceName;
                    hrc = networkAdapter->COMGETTER(HostInterface)(hostInterfaceName.asOutParam()); H();
                    ComPtr<IHostNetworkInterfaceCollection> coll;
                    hrc = host->COMGETTER(NetworkInterfaces)(coll.asOutParam());    H();
                    ComPtr<IHostNetworkInterface> hostInterface;
                    rc = coll->FindByName(hostInterfaceName, hostInterface.asOutParam());
                    if (!SUCCEEDED(rc))
                    {
                        AssertMsgFailed(("Cannot get GUID for host interface '%ls'\n", hostInterfaceName));
                        hrc = networkAdapter->Detach();                              H();
                    }
                    else
                    {
                        rc = CFGMR3InsertString(pLunL0, "Driver", "HostInterface");     RC_CHECK();
                        rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                 RC_CHECK();
                        rc = CFGMR3InsertString(pCfg, "HostInterfaceName", Utf8Str(hostInterfaceName)); RC_CHECK();
                        Guid hostIFGuid;
                        hrc = hostInterface->COMGETTER(Id)(hostIFGuid.asOutParam());    H();
                        char szDriverGUID[256] = {0};
                        /* add curly brackets */
                        szDriverGUID[0] = '{';
                        strcpy(szDriverGUID + 1, hostIFGuid.toString().raw());
                        strcat(szDriverGUID, "}");
                        rc = CFGMR3InsertBytes(pCfg, "GUID", szDriverGUID, sizeof(szDriverGUID)); RC_CHECK();
                    }
#else
# error "Port me"
#endif
                }
                else
                {
                    switch (hrc)
                    {
#ifdef __LINUX__
                        case VERR_ACCESS_DENIED:
                            return VMSetError(pVM, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,  N_(
                                             "Failed to open '/dev/net/tun' for read/write access. Please check the "
                                             "permissions of that node. Either do 'chmod 0666 /dev/net/tun' or "
                                             "change the group of that node and get member of that group. Make "
                                             "sure that these changes are permanently in particular if you are "
                                             "using udev"));
#endif /* __LINUX__ */
                        default:
                            AssertMsgFailed(("Could not attach to host interface! Bad!\n"));
                            return VMSetError(pVM, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS, N_(
                                             "Failed to initialize Host Interface Networking"));
                    }
                }
                break;
            }

            case NetworkAttachmentType_InternalNetworkAttachment:
            {
                hrc = networkAdapter->COMGETTER(InternalNetwork)(&str);                 H();
                STR_CONV();
                if (psz && *psz)
                {
                    if (fSniffer)
                    {
                        rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL0);       RC_CHECK();
                    }
                    else
                    {
                        rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);                 RC_CHECK();
                    }
                    rc = CFGMR3InsertString(pLunL0, "Driver", "IntNet");                RC_CHECK();
                    rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                     RC_CHECK();
                    rc = CFGMR3InsertString(pCfg, "Network", psz);                      RC_CHECK();
                }
                STR_FREE();
                break;
            }

            default:
                AssertMsgFailed(("should not get here!\n"));
                break;
        }
    }

    /*
     * VMM Device
     */
    rc = CFGMR3InsertNode(pDevices, "VMMDev", &pDev);                               RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          4);                     RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                     RC_CHECK();

    /* the VMM device's Main driver */
    rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                              RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "MainVMMDev");          RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
    VMMDev *pVMMDev = pConsole->mVMMDev;
    rc = CFGMR3InsertInteger(pCfg,  "Object", (uintptr_t)pVMMDev);                  RC_CHECK();

    /*
     * Audio Sniffer Device
     */
    rc = CFGMR3InsertNode(pDevices, "AudioSniffer", &pDev);                         RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();

    /* the Audio Sniffer device's Main driver */
    rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                              RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "MainAudioSniffer");    RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
    AudioSniffer *pAudioSniffer = pConsole->mAudioSniffer;
    rc = CFGMR3InsertInteger(pCfg,  "Object", (uintptr_t)pAudioSniffer);            RC_CHECK();

    /*
     * AC'97 ICH audio
     */
    ComPtr<IAudioAdapter> audioAdapter;
    hrc = pMachine->COMGETTER(AudioAdapter)(audioAdapter.asOutParam());             H();
    BOOL enabled = FALSE;
    if (audioAdapter)
    {
        hrc = audioAdapter->COMGETTER(Enabled)(&enabled);                           H();
    }
    if (enabled)
    {
        rc = CFGMR3InsertNode(pDevices, "ichac97", &pDev); /* ichac97 */
        rc = CFGMR3InsertNode(pDev,     "0", &pInst);
        rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          5);                     RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                     RC_CHECK();
        rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);

        /* the Audio driver */
        rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                              RC_CHECK();
        rc = CFGMR3InsertString(pLunL0, "Driver",               "AUDIO");               RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
        AudioDriverType_T audioDriver;
        hrc = audioAdapter->COMGETTER(AudioDriver)(&audioDriver);                       H();
        switch (audioDriver)
        {
            case AudioDriverType_NullAudioDriver:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "null");                   RC_CHECK();
                break;
            }
#ifdef __WIN__
#ifdef VBOX_WITH_WINMM
            case AudioDriverType_WINMMAudioDriver:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "winmm");                  RC_CHECK();
                break;
            }
#endif
            case AudioDriverType_DSOUNDAudioDriver:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "dsound");                 RC_CHECK();
                break;
            }
#endif /* __WIN__ */
#ifdef __LINUX__
            case AudioDriverType_OSSAudioDriver:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "oss");                    RC_CHECK();
                break;
            }
# ifdef VBOX_WITH_ALSA
            case AudioDriverType_ALSAAudioDriver:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "alsa");                   RC_CHECK();
                break;
            }
# endif
#endif /* __LINUX__ */
#ifdef __DARWIN__
            case AudioDriverType_CoreAudioDriver:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "coreaudio");              RC_CHECK();
                break;
            }
#endif 
        }
    }

    /*
     * The USB Controller.
     */
    ComPtr<IUSBController> USBCtlPtr;
    hrc = pMachine->COMGETTER(USBController)(USBCtlPtr.asOutParam());
    if (USBCtlPtr)
    {
        BOOL fEnabled;
        hrc = USBCtlPtr->COMGETTER(Enabled)(&fEnabled);                                 H();
        if (fEnabled)
        {
            rc = CFGMR3InsertNode(pDevices, "usb-ohci", &pDev);                         RC_CHECK();
            rc = CFGMR3InsertNode(pDev,     "0", &pInst);                               RC_CHECK();
            rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                           RC_CHECK();
            rc = CFGMR3InsertInteger(pInst, "Trusted",              1); /* boolean */   RC_CHECK();
            rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          6);                 RC_CHECK();
            rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                 RC_CHECK();

            rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                          RC_CHECK();
            rc = CFGMR3InsertString(pLunL0, "Driver",               "VUSBRootHub");     RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                           RC_CHECK();
        }
    }

    /*
     * Clipboard
     */
    {
        ClipboardMode_T mode = ClipboardMode_ClipDisabled;
        hrc = pMachine->COMGETTER(ClipboardMode) (&mode);                               H();

        if (mode != ClipboardMode_ClipDisabled)
        {
            /* Load the service */
            rc = pConsole->mVMMDev->hgcmLoadService ("VBoxSharedClipboard", "VBoxSharedClipboard");

            if (VBOX_FAILURE (rc))
            {
                LogRel(("VBoxSharedClipboard is not available. rc = %Vrc\n", rc));
                /* That is not a fatal failure. */
                rc = VINF_SUCCESS;
            }
            else
            {
                /* Setup the service. */
                VBOXHGCMSVCPARM parm;

                parm.type = VBOX_HGCM_SVC_PARM_32BIT;

                switch (mode)
                {
                    default:
                    case ClipboardMode_ClipDisabled:
                    {
                        LogRel(("VBoxSharedClipboard mode: Off\n"));
                        parm.u.uint32 = VBOX_SHARED_CLIPBOARD_MODE_OFF;
                        break;
                    }
                    case ClipboardMode_ClipGuestToHost:
                    {
                        LogRel(("VBoxSharedClipboard mode: Guest to Host\n"));
                        parm.u.uint32 = VBOX_SHARED_CLIPBOARD_MODE_GUEST_TO_HOST;
                        break;
                    }
                    case ClipboardMode_ClipHostToGuest:
                    {
                        LogRel(("VBoxSharedClipboard mode: Host to Guest\n"));
                        parm.u.uint32 = VBOX_SHARED_CLIPBOARD_MODE_HOST_TO_GUEST;
                        break;
                    }
                    case ClipboardMode_ClipBidirectional:
                    {
                        LogRel(("VBoxSharedClipboard mode: Bidirectional\n"));
                        parm.u.uint32 = VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL;
                        break;
                    }
                }

                pConsole->mVMMDev->hgcmHostCall ("VBoxSharedClipboard", VBOX_SHARED_CLIPBOARD_HOST_FN_SET_MODE, 1, &parm);

                Log(("Set VBoxSharedClipboard mode\n"));
            }
        }
    }

    /*
     * CFGM overlay handling.
     *
     * Here we check the extra data entries for CFGM values
     * and create the nodes and insert the values on the fly. Existing
     * values will be removed and reinserted. If a value is a valid number,
     * it will be inserted as a number, otherwise as a string.
     *
     * We first perform a run on global extra data, then on the machine
     * extra data to support global settings with local overrides.
     *
     */
    Bstr strExtraDataKey;
    bool fGlobalExtraData = true;
    for (;;)
    {
        Bstr strNextExtraDataKey;
        Bstr strExtraDataValue;

        /* get the next key */
        if (fGlobalExtraData)
            hrc = virtualBox->GetNextExtraDataKey(strExtraDataKey, strNextExtraDataKey.asOutParam(),
                                                  strExtraDataValue.asOutParam());
        else
            hrc = pMachine->GetNextExtraDataKey(strExtraDataKey, strNextExtraDataKey.asOutParam(),
                                                strExtraDataValue.asOutParam());

        /* stop if for some reason there's nothing more to request */
        if (FAILED(hrc) || !strNextExtraDataKey)
        {
            /* if we're out of global keys, continue with machine, otherwise we're done */
            if (fGlobalExtraData)
            {
                fGlobalExtraData = false;
                strExtraDataKey.setNull();
                continue;
            }
            break;
        }

        strExtraDataKey = strNextExtraDataKey;
        Utf8Str strExtraDataKeyUtf8 = Utf8Str(strExtraDataKey);

        /* we only care about keys starting with "VBoxInternal/" */
        if (strncmp(strExtraDataKeyUtf8.raw(), "VBoxInternal/", 13) != 0)
            continue;
        char *pszExtraDataKey = (char*)strExtraDataKeyUtf8.raw() + 13;

        /* the key will be in the format "Node1/Node2/Value" or simply "Value". */
        PCFGMNODE pNode;
        char *pszCFGMValueName = strrchr(pszExtraDataKey, '/');
        if (pszCFGMValueName)
        {
            /* terminate the node and advance to the value */
            *pszCFGMValueName = '\0';
            pszCFGMValueName++;

            /* does the node already exist? */
            pNode = CFGMR3GetChild(pRoot, pszExtraDataKey);
            if (pNode)
            {
                /* the value might already exist, remove it to be safe */
                CFGMR3RemoveValue(pNode, pszCFGMValueName);
            }
            else
            {
                /* create the node */
                rc = CFGMR3InsertNode(pRoot, pszExtraDataKey, &pNode);
                AssertMsgRC(rc, ("failed to insert node '%s'\n", pszExtraDataKey));
                if (VBOX_FAILURE(rc) || !pNode)
                    continue;
            }
        }
        else
        {
            pNode = pRoot;
            pszCFGMValueName = pszExtraDataKey;
            pszExtraDataKey--;

            /* the value might already exist, remove it to be safe */
            CFGMR3RemoveValue(pNode, pszCFGMValueName);
        }

        /* now let's have a look at the value */
        Utf8Str strCFGMValueUtf8 = Utf8Str(strExtraDataValue);
        const char *pszCFGMValue = strCFGMValueUtf8.raw();
        /* empty value means remove value which we've already done */
        if (pszCFGMValue && *pszCFGMValue)
        {
            /* if it's a valid number, we'll insert it as such, otherwise string */
            uint64_t u64Value;
            if (RTStrToUInt64Ex(pszCFGMValue, NULL, 0, &u64Value) == VINF_SUCCESS)
            {
                rc = CFGMR3InsertInteger(pNode, pszCFGMValueName, u64Value);
            }
            else
            {
                rc = CFGMR3InsertString(pNode, pszCFGMValueName, pszCFGMValue);
            }
            AssertMsgRC(rc, ("failed to insert CFGM value '%s' to key '%s'\n", pszCFGMValue, pszExtraDataKey));
        }
    }

#undef H
#undef RC_CHECK
#undef STR_FREE
#undef STR_CONV

    /* Register VM state change handler */
    int rc2 = VMR3AtStateRegister (pVM, Console::vmstateChangeCallback, pConsole);
    AssertRC (rc2);
    if (VBOX_SUCCESS (rc))
        rc = rc2;

    /* Register VM runtime error handler */
    rc2 = VMR3AtRuntimeErrorRegister (pVM, Console::setVMRuntimeErrorCallback, pConsole);
    AssertRC (rc2);
    if (VBOX_SUCCESS (rc))
        rc = rc2;

    /* Save the VM pointer in the machine object */
    pConsole->mpVM = pVM;

    LogFlowFunc (("vrc = %Vrc\n", rc));
    LogFlowFuncLeave();

    return rc;
}

/**
 *  Helper function to handle host interface device creation and attachment.
 *
 *  @param   networkAdapter the network adapter which attachment should be reset
 *  @return  COM status code
 *
 *  @note The caller must lock this object for writing.
 */
HRESULT Console::attachToHostInterface(INetworkAdapter *networkAdapter)
{
    /* sanity check */
    AssertReturn (isLockedOnCurrentThread(), E_FAIL);

#ifdef DEBUG
    /* paranoia */
    NetworkAttachmentType_T attachment;
    networkAdapter->COMGETTER(AttachmentType)(&attachment);
    Assert(attachment == NetworkAttachmentType_HostInterfaceNetworkAttachment);
#endif /* DEBUG */

    HRESULT rc = S_OK;

#ifdef VBOX_WITH_UNIXY_TAP_NETWORKING
    ULONG slot = 0;
    rc = networkAdapter->COMGETTER(Slot)(&slot);
    AssertComRC(rc);

    /*
     * Try get the FD.
     */
    LONG        ltapFD;
    rc = networkAdapter->COMGETTER(TAPFileDescriptor)(&ltapFD);
    if (SUCCEEDED(rc))
        maTapFD[slot] = (RTFILE)ltapFD;
    else
        maTapFD[slot] = NIL_RTFILE;

    /*
     * Are we supposed to use an existing TAP interface?
     */
    if (maTapFD[slot] != NIL_RTFILE)
    {
        /* nothing to do */
        Assert(ltapFD >= 0);
        Assert((LONG)maTapFD[slot] == ltapFD);
        rc = S_OK;
    }
    else
#endif /* VBOX_WITH_UNIXY_TAP_NETWORKING */
    {
        /*
         * Allocate a host interface device
         */
#ifdef __WIN__
        /* nothing to do */
        int rcVBox = VINF_SUCCESS;
#elif defined(__LINUX__)
        int rcVBox = RTFileOpen(&maTapFD[slot], "/dev/net/tun",
                                RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_INHERIT);
        if (VBOX_SUCCESS(rcVBox))
        {
            /*
             * Set/obtain the tap interface.
             */
            struct ifreq IfReq;
            memset(&IfReq, 0, sizeof(IfReq));
            Bstr tapDeviceName;
            rc = networkAdapter->COMGETTER(HostInterface)(tapDeviceName.asOutParam());
            if (FAILED(rc) || tapDeviceName.isEmpty())
                strcpy(IfReq.ifr_name, "tap%d");
            else
            {
                Utf8Str str(tapDeviceName);
                if (str.length() <= sizeof(IfReq.ifr_name))
                    strcpy(IfReq.ifr_name, str.raw());
                else
                    memcpy(IfReq.ifr_name, str.raw(), sizeof(IfReq.ifr_name) - 1); /** @todo bitch about names which are too long... */
            }
            IfReq.ifr_flags = IFF_TAP | IFF_NO_PI;
            rcVBox = ioctl(maTapFD[slot], TUNSETIFF, &IfReq);
            if (!rcVBox)
            {
                /*
                 * Make it pollable.
                 */
                if (fcntl(maTapFD[slot], F_SETFL, O_NONBLOCK) != -1)
                {
                    tapDeviceName = IfReq.ifr_name;
                    if (tapDeviceName)
                    {
                        Log(("attachToHostInterface: %RTfile %ls\n", maTapFD[slot], tapDeviceName.raw()));

                        /*
                         * Here is the right place to communicate the TAP file descriptor and
                         * the host interface name to the server if/when it becomes really
                         * necessary.
                         */
                        maTAPDeviceName[slot] = tapDeviceName;
                        rcVBox = VINF_SUCCESS;
                        rc = S_OK;
                    }
                    else
                        rcVBox = VERR_NO_MEMORY;
                }
                else
                {
                    AssertMsgFailed(("Configuration error: Failed to configure /dev/net/tun non blocking. errno=%d\n", errno));
                    rcVBox = VERR_HOSTIF_BLOCKING;
                    rc = setError(E_FAIL, "Failed to set /dev/net/tun to non blocking. errno=%d\n", errno);
                }
            }
            else
            {
                AssertMsgFailed(("Configuration error: Failed to configure /dev/net/tun. errno=%d\n", errno));
                rcVBox = VERR_HOSTIF_IOCTL;
                rc = setError(E_FAIL, "Failed to configure /dev/net/tun. errno = %d\n", errno);
            }
        }
        else
        {
            AssertMsgFailed(("Configuration error: Failed to open /dev/net/tun rc=%Vrc\n", rcVBox));
            switch (rcVBox)
            {
                case VERR_ACCESS_DENIED:
                    /* will be handled by our caller */
                    rc = rcVBox;
                    break;
                default:
                    rc = setError(E_FAIL, "Failed to open /dev/net/tun rc = %Vrc\n", rcVBox);
                    break;
            }
        }
#elif defined(__DARWIN__)
        /** @todo Implement tap networking for Darwin. */
        int rcVBox = VERR_NOT_IMPLEMENTED;
#elif defined(VBOX_WITH_UNIXY_TAP_NETWORKING)
# error "PORTME: Implement OS specific TAP interface open/creation."
#else
# error "Unknown host OS"
#endif
        /* in case of failure, cleanup. */
        if (VBOX_FAILURE(rcVBox) && SUCCEEDED(rc))
        {
            rc = setError(E_FAIL, tr ("General failure attaching to host interface"));
        }
    }
#ifdef VBOX_WITH_UNIXY_TAP_NETWORKING
    if (SUCCEEDED(rc))
    {
        /*
         * Call the initialization program.
         *
         * The initialization program is passed the device name as the first param.
         * The second parameter is the decimal value of the file handle of the device
         * which it inherits.
         */
        Bstr tapSetupApplication;
        networkAdapter->COMGETTER(TAPSetupApplication)(tapSetupApplication.asOutParam());
        if (tapSetupApplication)
        {
            /*
             * Create the argument list.
             */
            const char *apszArgs[4];
            /* 0. The program name. */
            Utf8Str tapSetupApp(tapSetupApplication);
            apszArgs[0] = tapSetupApp.raw();

            /* 1. The file descriptor. */
            char szFD[32];
            RTStrPrintf(szFD, sizeof(szFD), "%RTfile", maTapFD[slot]);
            apszArgs[1] = szFD;

            /* 2. The device name (optional). */
            apszArgs[2] = maTAPDeviceName[slot].isEmpty() ? NULL : maTAPDeviceName[slot].raw();

            /* 3. The end. */
            apszArgs[3] = NULL;

            /*
             * Create the process and wait for it to complete.
             */
            RTPROCESS Process;
            int rcVBox = RTProcCreate(apszArgs[0], &apszArgs[0], NULL, 0, &Process);
            if (VBOX_SUCCESS(rcVBox))
            {
                /* wait for the process to exit */
                RTPROCSTATUS ProcStatus;
                rcVBox = RTProcWait(Process, RTPROCWAIT_FLAGS_BLOCK, &ProcStatus);
                AssertRC(rcVBox);
                if (VBOX_SUCCESS(rcVBox))
                {
                    if (    ProcStatus.enmReason == RTPROCEXITREASON_NORMAL
                        &&  ProcStatus.iStatus == 0)
                        rcVBox = VINF_SUCCESS;
                    else
                        rcVBox = VMSetError(mpVM, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS, N_("Failed to initialize Host Interface Networking"));
                }
            }
            else
            {
                AssertMsgFailed(("Configuration error: Failed to start init program \"%s\", rc=%Vra\n", tapSetupApp.raw(), rcVBox));
                rc = setError(E_FAIL, "Failed to start init program \"%s\", rc = %Vra\n", tapSetupApp.raw(), rcVBox);
            }

            /* in case of failure, cleanup. */
            if (VBOX_FAILURE(rcVBox) && SUCCEEDED(rc))
            {
                rc = setError(E_FAIL, tr ("General failure configuring Host Interface Networking"));
            }
        }
    }
#endif /* VBOX_WITH_UNIXY_TAP_NETWORKING */
    return rc;
}

/**
 *  Helper function to handle detachment from a host interface
 *
 *  @param   networkAdapter the network adapter which attachment should be reset
 *  @return  COM status code
 *
 *  @note The caller must lock this object for writing.
 */
HRESULT Console::detachFromHostInterface(INetworkAdapter *networkAdapter)
{
    /* sanity check */
    AssertReturn (isLockedOnCurrentThread(), E_FAIL);

    HRESULT rc = S_OK;
#ifdef DEBUG
    /* paranoia */
    NetworkAttachmentType_T attachment;
    networkAdapter->COMGETTER(AttachmentType)(&attachment);
    Assert(attachment == NetworkAttachmentType_HostInterfaceNetworkAttachment);
#endif /* DEBUG */

#ifdef VBOX_WITH_UNIXY_TAP_NETWORKING

    ULONG slot = 0;
    rc = networkAdapter->COMGETTER(Slot)(&slot);
    AssertComRC(rc);

    /* is there an open TAP device? */
    if (maTapFD[slot] != NIL_RTFILE)
    {
        /*
         * Execute term command and close the file handle.
         */
        Bstr tapTerminateApplication;
        networkAdapter->COMGETTER(TAPTerminateApplication)(tapTerminateApplication.asOutParam());
        if (tapTerminateApplication)
        {
            /*
             * Create the argument list
             */
            const char *apszArgs[4];
            /* 0. The program name. */
            Utf8Str tapTermAppUtf8(tapTerminateApplication);
            apszArgs[0] = tapTermAppUtf8.raw();

            /* 1. The file descriptor. */
            char szFD[32];
            RTStrPrintf(szFD, sizeof(szFD), "%RTfile", maTapFD[slot]);
            apszArgs[1] = szFD;

            /* 2. Device name (optional). */
            apszArgs[2] = maTAPDeviceName[slot].isEmpty() ? NULL : maTAPDeviceName[slot].raw();

            /* 3. The end. */
            apszArgs[3] = NULL;

            /*
             * Create the process and wait for it to complete.
             */
            RTPROCESS Process;
            int rcVBox = RTProcCreate(apszArgs[0], &apszArgs[0], NULL, 0, &Process);
            if (VBOX_SUCCESS(rcVBox))
            {
                /* wait for the process to exit */
                RTPROCSTATUS ProcStatus;
                rcVBox = RTProcWait(Process, RTPROCWAIT_FLAGS_BLOCK, &ProcStatus);
                AssertRC(rcVBox);
                /* ignore return code? */
            }
            else
                AssertMsgFailed(("Configuration error: Failed to start terminate program \"%s\", rc=%Vra\n", apszArgs[0], rcVBox)); /** @todo last error candidate. */
            if (VBOX_FAILURE(rcVBox))
                rc = E_FAIL;
        }

        /*
         * Now we can close the file handle.
         */
        int rcVBox = RTFileClose(maTapFD[slot]);
        AssertRC(rcVBox);
        /* the TAP device name and handle are no longer valid */
        maTapFD[slot] = NIL_RTFILE;
        maTAPDeviceName[slot] = "";
    }
#endif
    return rc;
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
    AssertReturn (isLockedOnCurrentThread(), E_FAIL);

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
        if (attachment == NetworkAttachmentType_HostInterfaceNetworkAttachment)
        {
            HRESULT rc2 = detachFromHostInterface(networkAdapter);
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
 * @param   pVM         The VM handle. Can be NULL if an error occurred before
 *                          successfully creating a VM.
 * @param   pvUser      Pointer to the VMProgressTask structure.
 * @param   rc          VBox status code.
 * @param   pszFormat   The error message.
 * @thread EMT.
 */
/* static */ DECLCALLBACK (void)
Console::setVMErrorCallback (PVM pVM, void *pvUser, int rc, RT_SRC_POS_DECL,
                             const char *pszFormat, va_list args)
{
    VMProgressTask *task = static_cast <VMProgressTask *> (pvUser);
    AssertReturnVoid (task);

    /* we ignore RT_SRC_POS_DECL arguments to avoid confusion of end-users */
    HRESULT hrc = setError (E_FAIL, tr ("%N.\n"
                                        "VBox status code: %d (%Vrc)"),
                                    tr (pszFormat), &args,
                                    rc, rc);
    task->mProgress->notifyComplete (hrc);
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

    Utf8Str message = Utf8StrFmt (pszFormat, args);

    LogRel (("Console: VM runtime error: fatal=%RTbool, "
             "errorID=%s message=\"%s\"\n",
             fFatal, pszErrorID, message.raw()));

    that->onRuntimeError (BOOL (fFatal), Bstr (pszErrorID), Bstr (message));

    LogFlowFuncLeave();
}

/**
 *  Captures and attaches USB devices to a newly created VM.
 *
 *  @param   pVM     The VM handle.
 *
 *  @note The caller must lock this object for writing.
 */
HRESULT Console::captureUSBDevices (PVM pVM)
{
    LogFlowThisFunc (("\n"));

    /* sanity check */
    ComAssertRet (isLockedOnCurrentThread(), E_FAIL);

    /*
     *  If the machine has an USB controller, capture devices and attach
     *  them to it.
     */
    PPDMIBASE pBase;
    int vrc = PDMR3QueryLun (pVM, "usb-ohci", 0, 0, &pBase);
    if (VBOX_SUCCESS (vrc))
    {
        PVUSBIRHCONFIG pRhConfig = (PVUSBIRHCONFIG) pBase->
            pfnQueryInterface (pBase, PDMINTERFACE_VUSB_RH_CONFIG);
        ComAssertRet (pRhConfig, E_FAIL);

        /*
         *  Get the list of USB devices that should be captured and attached to
         *  the newly created machine.
         */
        ComPtr <IUSBDeviceCollection> coll;
        HRESULT hrc = mControl->AutoCaptureUSBDevices (coll.asOutParam());
        ComAssertComRCRetRC (hrc);

        /*
         *  Enumerate the devices and attach them.
         *  Failing to attach an device is currently ignored and the device
         *  released.
         */
        ComPtr <IUSBDeviceEnumerator> en;
        hrc = coll->Enumerate (en.asOutParam());
        ComAssertComRCRetRC (hrc);

        BOOL hasMore = FALSE;
        while (SUCCEEDED (en->HasMore (&hasMore)) && hasMore)
        {
            ComPtr <IUSBDevice> hostDevice;
            hrc = en->GetNext (hostDevice.asOutParam());
            ComAssertComRCRetRC (hrc);
            ComAssertRet (!hostDevice.isNull(), E_FAIL);

            hrc = attachUSBDevice (hostDevice, true /* aManual */, pRhConfig);

            /// @todo (r=dmik) warning reporting subsystem
        }
    }
    else if (   vrc == VERR_PDM_DEVICE_NOT_FOUND
             || vrc == VERR_PDM_DEVICE_INSTANCE_NOT_FOUND)
        vrc = VINF_SUCCESS;
    else
        AssertRC (vrc);

    return VBOX_SUCCESS (vrc) ? S_OK : E_FAIL;
}


/**
 *  Releases all USB device which is attached to the VM for the
 *  purpose of clean up and such like.
 *
 *  @note The caller must lock this object for writing.
 */
void Console::releaseAllUSBDevices (void)
{
    LogFlowThisFunc (("\n"));

    /* sanity check */
    AssertReturnVoid (isLockedOnCurrentThread());

    mControl->ReleaseAllUSBDevices();
    mUSBDevices.clear();
}

/**
 *  @note Locks this object for writing.
 */
#ifdef VRDP_MC
void Console::processRemoteUSBDevices (uint32_t u32ClientId, VRDPUSBDEVICEDESC *pDevList, uint32_t cbDevList)
#else
void Console::processRemoteUSBDevices (VRDPUSBDEVICEDESC *pDevList, uint32_t cbDevList)
#endif /* VRDP_MC */
{
    LogFlowThisFuncEnter();
#ifdef VRDP_MC
    LogFlowThisFunc (("u32ClientId = %d, pDevList=%p, cbDevList = %d\n", u32ClientId, pDevList, cbDevList));
#else
    LogFlowThisFunc (("pDevList=%p, cbDevList = %d\n", pDevList, cbDevList));
#endif /* VRDP_MC */

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

    AutoLock alock (this);

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
#ifdef VRDP_MC
            if ((*it)->devId () == e->id
                && (*it)->clientId () == u32ClientId)
#else
            if ((*it)->devId () == e->id)
#endif /* VRDP_MC */
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
#ifdef VRDP_MC
            device->init (u32ClientId, e);
#else
            device->init (e);
#endif /* VRDP_MC */

            mRemoteUSBDevices.push_back (device);

            /* Check if the device is ok for current USB filters. */
            BOOL fMatched = FALSE;

            HRESULT hrc = mControl->RunUSBDeviceFilters(device, &fMatched);

            AssertComRC (hrc);

            LogFlowThisFunc (("USB filters return %d\n", fMatched));

            if (fMatched)
            {
                hrc = onUSBDeviceAttach(device);

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
            onUSBDeviceDetach (uuid);
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

#if defined(__WIN__)
    {
        /* initialize COM */
        HRESULT hrc = CoInitializeEx (NULL,
                                      COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE |
                                      COINIT_SPEED_OVER_MEMORY);
        LogFlowFunc (("CoInitializeEx()=%08X\n", hrc));
    }
#endif

    HRESULT hrc = S_OK;
    int vrc = VINF_SUCCESS;

    ComObjPtr <Console> console = task->mConsole;

    /* Note: no need to use addCaller() because VMPowerUpTask does that */

    AutoLock alock (console);

    /* sanity */
    Assert (console->mpVM == NULL);

    do
    {
        /*
         * Initialize the release logging facility. In case something
         * goes wrong, there will be no release logging. Maybe in the future
         * we can add some logic to use different file names in this case.
         * Note that the logic must be in sync with Machine::DeleteSettings().
         */

        Bstr logFolder;
        hrc = console->mControl->GetLogFolder (logFolder.asOutParam());
        CheckComRCBreakRC (hrc);

        Utf8Str logDir = logFolder;

        /* make sure the Logs folder exists */
        Assert (!logDir.isEmpty());
        if (!RTDirExists (logDir))
            RTDirCreateFullPath (logDir, 0777);

        Utf8Str logFile = Utf8StrFmt ("%s%cVBox.log",
                                      logDir.raw(), RTPATH_DELIMITER);

        /*
         * Age the old log files
         * Rename .2 to .3, .1 to .2 and the last log file to .1
         * Overwrite target files in case they exist;
         */
        for (int i = 2; i >= 0; i--)
        {
            Utf8Str oldName;
            if (i > 0)
                oldName = Utf8StrFmt ("%s.%d", logFile.raw(), i);
            else
                oldName = logFile;
            Utf8Str newName = Utf8StrFmt ("%s.%d", logFile.raw(), i + 1);
            RTFileRename(oldName.raw(), newName.raw(), RTFILEMOVE_FLAGS_REPLACE);
        }

        PRTLOGGER loggerRelease;
        static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
        RTUINT fFlags = RTLOGFLAGS_PREFIX_TIME_PROG;
#ifdef __WIN__
        fFlags |= RTLOGFLAGS_USECRLF;
#endif /* __WIN__ */
        vrc = RTLogCreate(&loggerRelease, fFlags, "all",
                          "VBOX_RELEASE_LOG", ELEMENTS(s_apszGroups), s_apszGroups,
                          RTLOGDEST_FILE, logFile.raw());
        if (VBOX_SUCCESS(vrc))
        {
            /* some introductory information */
            RTTIMESPEC timeSpec;
            char nowUct[64];
            RTTimeSpecToString(RTTimeNow(&timeSpec), nowUct, sizeof(nowUct));
            RTLogRelLogger(loggerRelease, 0, ~0U,
                           "VirtualBox %s (%s %s) release log\n"
                           "Log opened %s\n",
                           VBOX_VERSION_STRING, __DATE__, __TIME__,
                           nowUct);

            /* register this logger as the release logger */
            RTLogRelSetDefaultInstance(loggerRelease);
        }
        else
        {
            hrc = setError (E_FAIL,
                tr ("Failed to open release log file '%s' (%Vrc)"),
                logFile.raw(), vrc);
            break;
        }

#ifdef VBOX_VRDP
        if (VBOX_SUCCESS (vrc))
        {
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
                    default:
                        errMsg = Utf8StrFmt (tr ("Failed to launch VRDP server (%Vrc)"),
                                             vrc);
                }
                LogRel (("Failed to launch VRDP server (%Vrc), error message: '%s'\n",
                         vrc, errMsg.raw()));
                hrc = setError (E_FAIL, errMsg);
                break;
            }
        }
#endif /* VBOX_VRDP */

        /*
         * Create the VM
         */
        PVM pVM;
        /*
         *  leave the lock since EMT will call Console. It's safe because
         *  mMachineState is either Starting or Restoring state here.
         */
        alock.leave();

        vrc = VMR3Create (task->mSetVMErrorCallback, task.get(),
                          task->mConfigConstructor, task.get(),
                          &pVM);

        alock.enter();

#ifdef VBOX_VRDP
        {
            /* Enable client connections to the server. */
            ConsoleVRDPServer *server = console->consoleVRDPServer();
            server->SetCallback ();
        }
#endif /* VBOX_VRDP */

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

                if (console->getVMMDev()->getShFlClientId())
                {
                    /// @todo (dmik)
                    //      does the code below call Console from the other thread?
                    //      Not sure, so leave the lock just in case
                    alock.leave();

                    /*
                     * Shared Folders
                     */
                    for (std::map <Bstr, ComPtr <ISharedFolder> >::const_iterator
                         it = task->mSharedFolders.begin();
                         it != task->mSharedFolders.end();
                         ++ it)
                    {
                        Bstr name = (*it).first;
                        ComPtr <ISharedFolder> folder = (*it).second;

                        Bstr hostPath;
                        hrc = folder->COMGETTER(HostPath) (hostPath.asOutParam());
                        CheckComRCBreakRC (hrc);

                        LogFlowFunc (("Adding shared folder '%ls' -> '%ls'\n",
                                      name.raw(), hostPath.raw()));
                        ComAssertBreak (!name.isEmpty() && !hostPath.isEmpty(),
                                        hrc = E_FAIL);

                        /** @todo should move this into the shared folder class */
                        VBOXHGCMSVCPARM  parms[2];
                        SHFLSTRING      *pFolderName, *pMapName;
                        int              cbString;

                        cbString = (hostPath.length() + 1) * sizeof(RTUCS2);
                        pFolderName = (SHFLSTRING *)RTMemAllocZ(sizeof(SHFLSTRING) + cbString);
                        Assert(pFolderName);
                        memcpy(pFolderName->String.ucs2, hostPath.raw(), cbString);

                        pFolderName->u16Size   = cbString;
                        pFolderName->u16Length = cbString - sizeof(RTUCS2);

                        parms[0].type = VBOX_HGCM_SVC_PARM_PTR;
                        parms[0].u.pointer.addr = pFolderName;
                        parms[0].u.pointer.size = sizeof(SHFLSTRING) + cbString;

                        cbString = (name.length() + 1) * sizeof(RTUCS2);
                        pMapName = (SHFLSTRING *)RTMemAllocZ(sizeof(SHFLSTRING) + cbString);
                        Assert(pMapName);
                        memcpy(pMapName->String.ucs2, name.raw(), cbString);

                        pMapName->u16Size   = cbString;
                        pMapName->u16Length = cbString - sizeof(RTUCS2);

                        parms[1].type = VBOX_HGCM_SVC_PARM_PTR;
                        parms[1].u.pointer.addr = pMapName;
                        parms[1].u.pointer.size = sizeof(SHFLSTRING) + cbString;

                        vrc = console->getVMMDev()->hgcmHostCall("VBoxSharedFolders",
                            SHFL_FN_ADD_MAPPING, 2, &parms[0]);

                        RTMemFree(pFolderName);
                        RTMemFree(pMapName);

                        if (VBOX_FAILURE (vrc))
                        {
                            hrc = setError (E_FAIL,
                                tr ("Unable to add mapping '%ls' to '%ls' (%Vrc)"),
                                hostPath.raw(), name.raw(), vrc);
                            break;
                        }
                    }

                    /* enter the lock again */
                    alock.enter();

                    CheckComRCBreakRC (hrc);
                }

                /*
                 * Capture USB devices.
                 */
                hrc = console->captureUSBDevices (pVM);
                CheckComRCBreakRC (hrc);

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

                    /* Start/Resume the VM execution */
                    if (VBOX_SUCCESS (vrc))
                    {
                        vrc = VMR3Resume (pVM);
                        AssertRC (vrc);
                    }

                    /* Power off in case we failed loading or resuming the VM */
                    if (VBOX_FAILURE (vrc))
                    {
                        int vrc2 = VMR3PowerOff (pVM);
                        AssertRC (vrc2);
                    }
                }
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
            if (FAILED (hrc) || VBOX_FAILURE (vrc))
            {
                /* preserve the current error info */
                ErrorInfo ei;

                /*
                 *  powerDown() will call VMR3Destroy() and do all necessary
                 *  cleanup (VRDP, USB devices)
                 */
                HRESULT hrc2 = console->powerDown();
                AssertComRC (hrc2);

                setError (ei);
            }
        }
        else
        {
            /*
             * If VMR3Create() failed it has released the VM memory.
             */
            console->mpVM = NULL;
        }

        if (SUCCEEDED (hrc) && VBOX_FAILURE (vrc))
        {
            /*
             *  If VMR3Create() or one of the other calls in this function fail,
             *  an appropriate error message has been already set. However since
             *  that happens via a callback, the status code in this function is
             *  not updated.
             */
            if (!task->mProgress->completed())
            {
                /*
                 *  If the COM error info is not yet set but we've got a
                 *  failure, convert the VBox status code into a meaningful
                 *  error message. This becomes unused once all the sources of
                 *  errors set the appropriate error message themselves.
                 *  Note that we don't use VMSetError() below because pVM is
                 *  either invalid or NULL here.
                 */
                AssertMsgFailed (("Missing error message during powerup for "
                                  "status code %Vrc\n", vrc));
                hrc = setError (E_FAIL,
                    tr ("Failed to start VM execution (%Vrc)"), vrc);
            }
            else
                hrc = task->mProgress->resultCode();

            Assert (FAILED (hrc));
            break;
        }
    }
    while (0);

    if (console->mMachineState == MachineState_Starting ||
        console->mMachineState == MachineState_Restoring)
    {
        /*
         *  We are still in the Starting/Restoring state. This means one of:
         *  1) we failed before VMR3Create() was called;
         *  2) VMR3Create() failed.
         *  In both cases, there is no need to call powerDown(), but we still
         *  need to go back to the PoweredOff/Saved state. Reuse
         *  vmstateChangeCallback() for that purpose.
         */

        /* preserve the current error info */
        ErrorInfo ei;

        Assert (console->mpVM == NULL);
        vmstateChangeCallback (NULL, VMSTATE_TERMINATED, VMSTATE_CREATING,
                               console);
        setError (ei);
    }

    /*
     *  Evaluate the final result.
     *  Note that the appropriate mMachineState value is already set by
     *  vmstateChangeCallback() in all cases.
     */

    /* leave the lock, don't need it any more */
    alock.leave();

    if (SUCCEEDED (hrc))
    {
        /* Notify the progress object of the success */
        task->mProgress->notifyComplete (S_OK);
    }
    else
    {
        if (!task->mProgress->completed())
        {
            /*  The progress object will fetch the current error info. This
             *  gets the errors signalled by using setError(). The ones
             *  signalled via VMSetError() immediately notify the progress
             *  object that the operation is completed. */
            task->mProgress->notifyComplete (hrc);
        }

        LogRel (("Power up failed (vrc=%Vrc, hrc=0x%08X)\n", vrc, hrc));
    }

#if defined(__WIN__)
    /* uninitialize COM */
    CoUninitialize();
#endif

    LogFlowFuncLeave();

    return VINF_SUCCESS;
}


/**
 *  Reconfigures a VDI.
 *
 *  @param   pVM     The VM handle.
 *  @param   hda     The harddisk attachment.
 *  @param   phrc    Where to store com error - only valid if we return VERR_GENERAL_FAILURE.
 *  @return  VBox status code.
 */
static DECLCALLBACK(int) reconfigureVDI(PVM pVM, IHardDiskAttachment *hda, HRESULT *phrc)
{
    LogFlowFunc (("pVM=%p hda=%p phrc=%p\n", pVM, hda, phrc));

    int             rc;
    HRESULT         hrc;
    char           *psz = NULL;
    BSTR            str = NULL;
    *phrc = S_OK;
#define STR_CONV()  do { rc = RTStrUcs2ToUtf8(&psz, str); RC_CHECK(); } while (0)
#define STR_FREE()  do { if (str) { SysFreeString(str); str = NULL; } if (psz) { RTStrFree(psz); psz = NULL; } } while (0)
#define RC_CHECK()  do { if (VBOX_FAILURE(rc)) { AssertMsgFailed(("rc=%Vrc\n", rc)); STR_FREE(); return rc; } } while (0)
#define H() do { if (FAILED(hrc)) { AssertMsgFailed(("hrc=%#x\n", hrc)); STR_FREE(); *phrc = hrc; return VERR_GENERAL_FAILURE; } } while (0)

    /*
     * Figure out which IDE device this is.
     */
    ComPtr<IHardDisk> hardDisk;
    hrc = hda->COMGETTER(HardDisk)(hardDisk.asOutParam());                      H();
    DiskControllerType_T enmCtl;
    hrc = hda->COMGETTER(Controller)(&enmCtl);                                  H();
    LONG lDev;
    hrc = hda->COMGETTER(DeviceNumber)(&lDev);                                  H();

    int i;
    switch (enmCtl)
    {
        case DiskControllerType_IDE0Controller:
            i = 0;
            break;
        case DiskControllerType_IDE1Controller:
            i = 2;
            break;
        default:
            AssertMsgFailed(("invalid disk controller type: %d\n", enmCtl));
            return VERR_GENERAL_FAILURE;
    }

    if (lDev < 0 || lDev >= 2)
    {
        AssertMsgFailed(("invalid controller device number: %d\n", lDev));
        return VERR_GENERAL_FAILURE;
    }

    i = i + lDev;

    /*
     * Is there an existing LUN? If not create it.
     * We ASSUME that this will NEVER collide with the DVD.
     */
    PCFGMNODE pCfg;
    PCFGMNODE pLunL1 = CFGMR3GetChildF(CFGMR3GetRoot(pVM), "Devices/piix3ide/0/LUN#%d/AttachedDriver/", i);
    if (!pLunL1)
    {
        PCFGMNODE pInst = CFGMR3GetChild(CFGMR3GetRoot(pVM), "Devices/piix3ide/0/");
        AssertReturn(pInst, VERR_INTERNAL_ERROR);

        PCFGMNODE pLunL0;
        rc = CFGMR3InsertNodeF(pInst, &pLunL0, "LUN#%d", i);                        RC_CHECK();
        rc = CFGMR3InsertString(pLunL0, "Driver",              "Block");            RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                           RC_CHECK();
        rc = CFGMR3InsertString(pCfg,   "Type",                "HardDisk");         RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "Mountable",            0);                 RC_CHECK();

        rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);                 RC_CHECK();
        rc = CFGMR3InsertString(pLunL1, "Driver",              "VBoxHDD");          RC_CHECK();
        rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                           RC_CHECK();
    }
    else
    {
#ifdef VBOX_STRICT
        char *pszDriver;
        rc = CFGMR3QueryStringAlloc(pLunL1, "Driver", &pszDriver);                  RC_CHECK();
        Assert(!strcmp(pszDriver, "VBoxHDD"));
        MMR3HeapFree(pszDriver);
#endif

        /*
         * Check if things has changed.
         */
        pCfg = CFGMR3GetChild(pLunL1, "Config");
        AssertReturn(pCfg, VERR_INTERNAL_ERROR);

        /* the image */
        /// @todo (dmik) we temporarily use the location property to
        //  determine the image file name. This is subject to change
        //  when iSCSI disks are here (we should either query a
        //  storage-specific interface from IHardDisk, or "standardize"
        //  the location property)
        hrc = hardDisk->COMGETTER(Location)(&str);                                  H();
        STR_CONV();
        char *pszPath;
        rc = CFGMR3QueryStringAlloc(pCfg, "Path", &pszPath);                        RC_CHECK();
        if (!strcmp(psz, pszPath))
        {
            /* parent images. */
            ComPtr<IHardDisk> parentHardDisk = hardDisk;
            for (PCFGMNODE pParent = pCfg;;)
            {
                MMR3HeapFree(pszPath);
                pszPath = NULL;
                STR_FREE();

                /* get parent */
                ComPtr<IHardDisk> curHardDisk;
                hrc = parentHardDisk->COMGETTER(Parent)(curHardDisk.asOutParam());  H();
                PCFGMNODE pCur;
                pCur = CFGMR3GetChild(pParent, "Parent");
                if (!pCur && !curHardDisk)
                {
                    /* no change */
                    LogFlowFunc (("No change!\n"));
                    return VINF_SUCCESS;
                }
                if (!pCur || !curHardDisk)
                    break;

                /* compare paths. */
                /// @todo (dmik) we temporarily use the location property to
                //  determine the image file name. This is subject to change
                //  when iSCSI disks are here (we should either query a
                //  storage-specific interface from IHardDisk, or "standardize"
                //  the location property)
                hrc = curHardDisk->COMGETTER(Location)(&str);                       H();
                STR_CONV();
                rc = CFGMR3QueryStringAlloc(pCfg, "Path", &pszPath);                RC_CHECK();
                if (strcmp(psz, pszPath))
                    break;

                /* next */
                pParent = pCur;
                parentHardDisk = curHardDisk;
            }

        }
        else
            LogFlowFunc (("LUN#%d: old leaf image '%s'\n", i, pszPath));

        MMR3HeapFree(pszPath);
        STR_FREE();

        /*
         * Detach the driver and replace the config node.
         */
        rc = PDMR3DeviceDetach(pVM, "piix3ide", 0, i);                              RC_CHECK();
        CFGMR3RemoveNode(pCfg);
        rc = CFGMR3InsertNode(pLunL1, "Config", &pCfg);                             RC_CHECK();
    }

    /*
     * Create the driver configuration.
     */
    /// @todo (dmik) we temporarily use the location property to
    //  determine the image file name. This is subject to change
    //  when iSCSI disks are here (we should either query a
    //  storage-specific interface from IHardDisk, or "standardize"
    //  the location property)
    hrc = hardDisk->COMGETTER(Location)(&str);                                  H();
    STR_CONV();
    LogFlowFunc (("LUN#%d: leaf image '%s'\n", i, psz));
    rc = CFGMR3InsertString(pCfg, "Path", psz);                                 RC_CHECK();
    STR_FREE();
    /* Create an inversed tree of parents. */
    ComPtr<IHardDisk> parentHardDisk = hardDisk;
    for (PCFGMNODE pParent = pCfg;;)
    {
        ComPtr<IHardDisk> curHardDisk;
        hrc = parentHardDisk->COMGETTER(Parent)(curHardDisk.asOutParam());      H();
        if (!curHardDisk)
            break;

        PCFGMNODE pCur;
        rc = CFGMR3InsertNode(pParent, "Parent", &pCur);                        RC_CHECK();
        /// @todo (dmik) we temporarily use the location property to
        //  determine the image file name. This is subject to change
        //  when iSCSI disks are here (we should either query a
        //  storage-specific interface from IHardDisk, or "standardize"
        //  the location property)
        hrc = curHardDisk->COMGETTER(Location)(&str);                           H();
        STR_CONV();
        rc = CFGMR3InsertString(pCur,  "Path", psz);                            RC_CHECK();
        STR_FREE();

        /* next */
        pParent = pCur;
        parentHardDisk = curHardDisk;
    }

    /*
     * Attach the new driver.
     */
    rc = PDMR3DeviceAttach(pVM, "piix3ide", 0, i, NULL);                        RC_CHECK();

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
                Console::tr ("Failed to save the machine state to '%s' (%Vrc)"),
                task->mSavedStateFile.raw(), vrc);
            rc = E_FAIL;
        }
    }

    /* lock the console sonce we're going to access it */
    AutoLock thatLock (that);

    if (SUCCEEDED (rc))
    {
        if (task->mIsSnapshot)
        do
        {
            LogFlowFunc (("Reattaching new differencing VDIs...\n"));

            ComPtr <IHardDiskAttachmentCollection> hdaColl;
            rc = that->mMachine->COMGETTER(HardDiskAttachments) (hdaColl.asOutParam());
            if (FAILED (rc))
                break;
            ComPtr <IHardDiskAttachmentEnumerator> hdaEn;
            rc = hdaColl->Enumerate (hdaEn.asOutParam());
            if (FAILED (rc))
                break;
            BOOL more = FALSE;
            while (SUCCEEDED (rc = hdaEn->HasMore (&more)) && more)
            {
                ComPtr <IHardDiskAttachment> hda;
                rc = hdaEn->GetNext (hda.asOutParam());
                if (FAILED (rc))
                    break;

                PVMREQ pReq;
                IHardDiskAttachment *pHda = hda;
                /*
                 *  don't leave the lock since reconfigureVDI isn't going to
                 *  access Console.
                 */
                int vrc = VMR3ReqCall (that->mpVM, &pReq, RT_INDEFINITE_WAIT,
                                       (PFNRT)reconfigureVDI, 3, that->mpVM,
                                       pHda, &rc);
                if (VBOX_SUCCESS (rc))
                    rc = pReq->iStatus;
                VMR3ReqFree (pReq);
                if (FAILED (rc))
                    break;
                if (VBOX_FAILURE (vrc))
                {
                    errMsg = Utf8StrFmt (Console::tr ("%Vrc"), vrc);
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

    std::auto_ptr <VMTask> task (static_cast <VMTask *> (pvUser));
    AssertReturn (task.get(), VERR_INVALID_PARAMETER);

    AssertReturn (task->isOk(), VERR_GENERAL_FAILURE);

    const ComObjPtr <Console> &that = task->mConsole;

    /*
     *  Note: no need to use addCaller() to protect Console
     *  because VMTask does that
     */

    /* release VM caller to let powerDown() proceed */
    task->releaseVMCaller();

    HRESULT rc = that->powerDown();
    AssertComRC (rc);

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
    PDRVMAINSTATUS pDrv = PDMINS2DATA(pDrvIns, PDRVMAINSTATUS);
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
    PDRVMAINSTATUS pData = PDMINS2DATA(pDrvIns, PDRVMAINSTATUS);
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
    PDRVMAINSTATUS pData = PDMINS2DATA(pDrvIns, PDRVMAINSTATUS);
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
        AssertMsgFailed(("Configuration error: Failed to query the \"papLeds\" value! rc=%Vrc\n", rc));
        return rc;
    }

    rc = CFGMR3QueryU32(pCfgHandle, "First", &pData->iFirstLUN);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->iFirstLUN = 0;
    else if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Failed to query the \"First\" value! rc=%Vrc\n", rc));
        return rc;
    }

    rc = CFGMR3QueryU32(pCfgHandle, "Last", &pData->iLastLUN);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->iLastLUN = 0;
    else if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Failed to query the \"Last\" value! rc=%Vrc\n", rc));
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

