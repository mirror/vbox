/* $Id$ */

/** @file
 *
 * VBox Console COM Class definition
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

#ifndef ____H_CONSOLEIMPL
#define ____H_CONSOLEIMPL

#include "VirtualBoxBase.h"
#include "ProgressImpl.h"

class Guest;
class Keyboard;
class Mouse;
class Display;
class MachineDebugger;
class OUSBDevice;
class RemoteUSBDevice;
class SharedFolder;
class RemoteDisplayInfo;
class AudioSniffer;
class ConsoleVRDPServer;
class VMMDev;

#include <VBox/vrdpapi.h>
#include <VBox/pdmdrv.h>
#ifdef VBOX_WITH_GUEST_PROPS
# include <VBox/HostServices/GuestPropertySvc.h>  /* For the property notification callback */
#endif

struct VUSBIRHCONFIG;
typedef struct VUSBIRHCONFIG *PVUSBIRHCONFIG;

#include <list>

// defines
///////////////////////////////////////////////////////////////////////////////

/**
 *  Checks the availability of the underlying VM device driver corresponding
 *  to the COM interface (IKeyboard, IMouse, IDisplay, etc.). When the driver is
 *  not available (NULL), sets error info and returns returns E_ACCESSDENIED.
 *  The translatable error message is defined in null context.
 *
 *  Intended to used only within Console children (i.e. Keyboard, Mouse,
 *  Display, etc.).
 *
 *  @param drv  driver pointer to check (compare it with NULL)
 */
#define CHECK_CONSOLE_DRV(drv) \
    do { \
        if (!(drv)) \
            return setError (E_ACCESSDENIED, tr ("The console is not powered up")); \
    } while (0)

// Console
///////////////////////////////////////////////////////////////////////////////

/** IConsole implementation class */
class ATL_NO_VTABLE Console :
    public VirtualBoxBaseWithChildrenNEXT,
    public VirtualBoxSupportErrorInfoImpl <Console, IConsole>,
    public VirtualBoxSupportTranslation <Console>,
    VBOX_SCRIPTABLE_IMPL(IConsole)
{
    Q_OBJECT

public:

    DECLARE_NOT_AGGREGATABLE(Console)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Console)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IConsole)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    Console();
    ~Console();

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializers/uninitializers for internal purposes only
    HRESULT init (IMachine *aMachine, IInternalMachineControl *aControl);
    void uninit();

    // IConsole properties
    STDMETHOD(COMGETTER(Machine)) (IMachine **aMachine);
    STDMETHOD(COMGETTER(State)) (MachineState_T *aMachineState);
    STDMETHOD(COMGETTER(Guest)) (IGuest **aGuest);
    STDMETHOD(COMGETTER(Keyboard)) (IKeyboard **aKeyboard);
    STDMETHOD(COMGETTER(Mouse)) (IMouse **aMouse);
    STDMETHOD(COMGETTER(Display)) (IDisplay **aDisplay);
    STDMETHOD(COMGETTER(Debugger)) (IMachineDebugger **aDebugger);
    STDMETHOD(COMGETTER(USBDevices)) (ComSafeArrayOut (IUSBDevice *, aUSBDevices));
    STDMETHOD(COMGETTER(RemoteUSBDevices)) (ComSafeArrayOut (IHostUSBDevice *, aRemoteUSBDevices));
    STDMETHOD(COMGETTER(RemoteDisplayInfo)) (IRemoteDisplayInfo **aRemoteDisplayInfo);
    STDMETHOD(COMGETTER(SharedFolders)) (ComSafeArrayOut (ISharedFolder *, aSharedFolders));

    // IConsole methods
    STDMETHOD(PowerUp) (IProgress **aProgress);
    STDMETHOD(PowerUpPaused) (IProgress **aProgress);
    STDMETHOD(PowerDown)();
    STDMETHOD(PowerDownAsync) (IProgress **aProgress);
    STDMETHOD(Reset)();
    STDMETHOD(Pause)();
    STDMETHOD(Resume)();
    STDMETHOD(PowerButton)();
    STDMETHOD(SleepButton)();
    STDMETHOD(GetPowerButtonHandled)(BOOL *aHandled);
    STDMETHOD(GetGuestEnteredACPIMode)(BOOL *aEntered);
    STDMETHOD(SaveState) (IProgress **aProgress);
    STDMETHOD(AdoptSavedState) (IN_BSTR aSavedStateFile);
    STDMETHOD(DiscardSavedState)();
    STDMETHOD(GetDeviceActivity) (DeviceType_T aDeviceType,
                                 DeviceActivity_T *aDeviceActivity);
    STDMETHOD(AttachUSBDevice) (IN_GUID aId);
    STDMETHOD(DetachUSBDevice) (IN_GUID aId, IUSBDevice **aDevice);
    STDMETHOD(FindUSBDeviceByAddress) (IN_BSTR aAddress, IUSBDevice **aDevice);
    STDMETHOD(FindUSBDeviceById) (IN_GUID aId, IUSBDevice **aDevice);
    STDMETHOD(CreateSharedFolder) (IN_BSTR aName, IN_BSTR aHostPath, BOOL aWritable);
    STDMETHOD(RemoveSharedFolder) (IN_BSTR aName);
    STDMETHOD(TakeSnapshot) (IN_BSTR aName, IN_BSTR aDescription,
                             IProgress **aProgress);
    STDMETHOD(DiscardSnapshot) (IN_GUID aId, IProgress **aProgress);
    STDMETHOD(DiscardCurrentState) (IProgress **aProgress);
    STDMETHOD(DiscardCurrentSnapshotAndState) (IProgress **aProgress);
    STDMETHOD(RegisterCallback) (IConsoleCallback *aCallback);
    STDMETHOD(UnregisterCallback)(IConsoleCallback *aCallback);

    // public methods for internal purposes only

    /*
     *  Note: the following methods do not increase refcount. intended to be
     *  called only by the VM execution thread.
     */

    Guest *getGuest() const { return mGuest; }
    Keyboard *getKeyboard() const { return mKeyboard; }
    Mouse *getMouse() const { return mMouse; }
    Display *getDisplay() const { return mDisplay; }
    MachineDebugger *getMachineDebugger() const { return mDebugger; }

    const ComPtr <IMachine> &machine() const { return mMachine; }

    /** Method is called only from ConsoleVRDPServer */
    IVRDPServer *getVRDPServer() const { return mVRDPServer; }

    ConsoleVRDPServer *consoleVRDPServer() const { return mConsoleVRDPServer; }

    HRESULT updateMachineState (MachineState_T aMachineState);

    // events from IInternalSessionControl
    HRESULT onDVDDriveChange();
    HRESULT onFloppyDriveChange();
    HRESULT onNetworkAdapterChange (INetworkAdapter *aNetworkAdapter);
    HRESULT onSerialPortChange (ISerialPort *aSerialPort);
    HRESULT onParallelPortChange (IParallelPort *aParallelPort);
    HRESULT onStorageControllerChange ();
    HRESULT onVRDPServerChange();
    HRESULT onUSBControllerChange();
    HRESULT onSharedFolderChange (BOOL aGlobal);
    HRESULT onUSBDeviceAttach (IUSBDevice *aDevice, IVirtualBoxErrorInfo *aError, ULONG aMaskedIfs);
    HRESULT onUSBDeviceDetach (IN_GUID aId, IVirtualBoxErrorInfo *aError);
    HRESULT getGuestProperty (IN_BSTR aKey, BSTR *aValue, ULONG64 *aTimestamp, BSTR *aFlags);
    HRESULT setGuestProperty (IN_BSTR aKey, IN_BSTR aValue, IN_BSTR aFlags);
    HRESULT enumerateGuestProperties (IN_BSTR aPatterns, ComSafeArrayOut(BSTR, aNames), ComSafeArrayOut(BSTR, aValues), ComSafeArrayOut(ULONG64, aTimestamps), ComSafeArrayOut(BSTR, aFlags));
    VMMDev *getVMMDev() { return mVMMDev; }
    AudioSniffer *getAudioSniffer () { return mAudioSniffer; }

    int VRDPClientLogon (uint32_t u32ClientId, const char *pszUser, const char *pszPassword, const char *pszDomain);
    void VRDPClientConnect (uint32_t u32ClientId);
    void VRDPClientDisconnect (uint32_t u32ClientId, uint32_t fu32Intercepted);
    void VRDPInterceptAudio (uint32_t u32ClientId);
    void VRDPInterceptUSB (uint32_t u32ClientId, void **ppvIntercept);
    void VRDPInterceptClipboard (uint32_t u32ClientId);

    void processRemoteUSBDevices (uint32_t u32ClientId, VRDPUSBDEVICEDESC *pDevList, uint32_t cbDevList);

    // callback callers (partly; for some events console callbacks are notified
    // directly from IInternalSessionControl event handlers declared above)
    void onMousePointerShapeChange(bool fVisible, bool fAlpha,
                                   uint32_t xHot, uint32_t yHot,
                                   uint32_t width, uint32_t height,
                                   void *pShape);
    void onMouseCapabilityChange (BOOL supportsAbsolute, BOOL needsHostCursor);
    void onStateChange (MachineState_T aMachineState);
    void onAdditionsStateChange();
    void onAdditionsOutdated();
    void onKeyboardLedsChange (bool fNumLock, bool fCapsLock, bool fScrollLock);
    void onUSBDeviceStateChange (IUSBDevice *aDevice, bool aAttached,
                                 IVirtualBoxErrorInfo *aError);
    void onRuntimeError (BOOL aFatal, IN_BSTR aErrorID, IN_BSTR aMessage);
    HRESULT onShowWindow (BOOL aCheck, BOOL *aCanShow, ULONG64 *aWinId);

    static const PDMDRVREG DrvStatusReg;

    void reportAuthLibraryError (const char *filename, int rc)
    {
        setError (E_FAIL, tr("Could not load the external authentication library '%s' (%Rrc)"), filename, rc);
    }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"Console"; }

private:

    /**
     *  Base template for AutoVMCaller and SaveVMPtr. Template arguments
     *  have the same meaning as arguments of Console::addVMCaller().
     */
    template <bool taQuiet = false, bool taAllowNullVM = false>
    class AutoVMCallerBase
    {
    public:
        AutoVMCallerBase (Console *aThat) : mThat (aThat), mRC (S_OK)
        {
            Assert (aThat);
            mRC = aThat->addVMCaller (taQuiet, taAllowNullVM);
        }
        ~AutoVMCallerBase()
        {
            if (SUCCEEDED (mRC))
                mThat->releaseVMCaller();
        }
        /** Decreases the number of callers before the instance is destroyed. */
        void release()
        {
            AssertReturnVoid (SUCCEEDED (mRC));
            mThat->releaseVMCaller();
            mRC = E_FAIL;
        }
        /** Restores the number of callers after by #release(). #rc() must be
         *  rechecked to ensure the operation succeeded. */
        void add()
        {
            AssertReturnVoid (!SUCCEEDED (mRC));
            mRC = mThat->addVMCaller (taQuiet, taAllowNullVM);
        }
        /** Returns the result of Console::addVMCaller() */
        HRESULT rc() const { return mRC; }
        /** Shortcut to SUCCEEDED (rc()) */
        bool isOk() const { return SUCCEEDED (mRC); }
    protected:
        Console *mThat;
        HRESULT mRC;
    private:
        DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (AutoVMCallerBase)
        DECLARE_CLS_NEW_DELETE_NOOP (AutoVMCallerBase)
    };

    /**
     *  Helper class that protects sections of code using the mpVM pointer by
     *  automatically calling addVMCaller() on construction and
     *  releaseVMCaller() on destruction. Intended for Console methods dealing
     *  with mpVM. The usage pattern is:
     *  <code>
     *      AutoVMCaller autoVMCaller (this);
     *      CheckComRCReturnRC (autoVMCaller.rc());
     *      ...
     *      VMR3ReqCall (mpVM, ...
     *  </code>
     *
     *  @note Temporarily locks the argument for writing.
     *
     *  @sa SafeVMPtr, SafeVMPtrQuiet
     */
    typedef AutoVMCallerBase <false, false> AutoVMCaller;

    /**
     *  Same as AutoVMCaller but doesn't set extended error info on failure.
     *
     *  @note Temporarily locks the argument for writing.
     */
    typedef AutoVMCallerBase <true, false> AutoVMCallerQuiet;

    /**
     *  Same as AutoVMCaller but allows a null VM pointer (to trigger an error
     *  instead of assertion).
     *
     *  @note Temporarily locks the argument for writing.
     */
    typedef AutoVMCallerBase <false, true> AutoVMCallerWeak;

    /**
     *  Same as AutoVMCaller but doesn't set extended error info on failure
     *  and allows a null VM pointer (to trigger an error instead of
     *  assertion).
     *
     *  @note Temporarily locks the argument for writing.
     */
    typedef AutoVMCallerBase <true, true> AutoVMCallerQuietWeak;

    /**
     *  Base template for SaveVMPtr and SaveVMPtrQuiet.
     */
    template <bool taQuiet = false>
    class SafeVMPtrBase : public AutoVMCallerBase <taQuiet, true>
    {
        typedef AutoVMCallerBase <taQuiet, true> Base;
    public:
        SafeVMPtrBase (Console *aThat) : Base (aThat), mpVM (NULL)
        {
            if (SUCCEEDED (Base::mRC))
                mpVM = aThat->mpVM;
        }
        /** Smart SaveVMPtr to PVM cast operator */
        operator PVM() const { return mpVM; }
        /** Direct PVM access for printf()-like functions */
        PVM raw() const { return mpVM; }
    private:
        PVM mpVM;
        DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (SafeVMPtrBase)
        DECLARE_CLS_NEW_DELETE_NOOP (SafeVMPtrBase)
    };

public:

    /**
     *  Helper class that safely manages the Console::mpVM pointer
     *  by calling addVMCaller() on construction and releaseVMCaller() on
     *  destruction. Intended for Console children. The usage pattern is:
     *  <code>
     *      Console::SaveVMPtr pVM (mParent);
     *      CheckComRCReturnRC (pVM.rc());
     *      ...
     *      VMR3ReqCall (pVM, ...
     *      ...
     *      printf ("%p\n", pVM.raw());
     *  </code>
     *
     *  @note Temporarily locks the argument for writing.
     *
     *  @sa SafeVMPtrQuiet, AutoVMCaller
     */
    typedef SafeVMPtrBase <false> SafeVMPtr;

    /**
     *  A deviation of SaveVMPtr that doesn't set the error info on failure.
     *  Intended for pieces of code that don't need to return the VM access
     *  failure to the caller. The usage pattern is:
     *  <code>
     *      Console::SaveVMPtrQuiet pVM (mParent);
     *      if (pVM.rc())
     *          VMR3ReqCall (pVM, ...
     *      return S_OK;
     *  </code>
     *
     *  @note Temporarily locks the argument for writing.
     *
     *  @sa SafeVMPtr, AutoVMCaller
     */
    typedef SafeVMPtrBase <true> SafeVMPtrQuiet;

    class SharedFolderData
    {
    public:
        SharedFolderData() {}
        SharedFolderData(Bstr aHostPath, BOOL aWritable)
           : mHostPath (aHostPath)
           , mWritable (aWritable) {}
        SharedFolderData(const SharedFolderData& aThat)
           : mHostPath (aThat.mHostPath)
           , mWritable (aThat.mWritable) {}
        Bstr mHostPath;
        BOOL mWritable;
    };
    typedef std::map <Bstr, ComObjPtr <SharedFolder> > SharedFolderMap;
    typedef std::map <Bstr, SharedFolderData> SharedFolderDataMap;

private:

    typedef std::list <ComObjPtr <OUSBDevice> > USBDeviceList;
    typedef std::list <ComObjPtr <RemoteUSBDevice> > RemoteUSBDeviceList;

    HRESULT addVMCaller (bool aQuiet = false, bool aAllowNullVM = false);
    void releaseVMCaller();

    HRESULT consoleInitReleaseLog (const ComPtr <IMachine> aMachine);

    HRESULT powerUp (IProgress **aProgress, bool aPaused);
    HRESULT powerDown (Progress *aProgress = NULL);

    HRESULT callTapSetupApplication(bool isStatic, RTFILE tapFD, Bstr &tapDevice,
                                    Bstr &tapSetupApplication);
    HRESULT attachToBridgedInterface(INetworkAdapter *networkAdapter);
    HRESULT detachFromBridgedInterface(INetworkAdapter *networkAdapter);
    HRESULT powerDownHostInterfaces();

    HRESULT setMachineState (MachineState_T aMachineState, bool aUpdateServer = true);
    HRESULT setMachineStateLocally (MachineState_T aMachineState)
    {
        return setMachineState (aMachineState, false /* aUpdateServer */);
    }

    HRESULT findSharedFolder (CBSTR aName,
                              ComObjPtr <SharedFolder> &aSharedFolder,
                              bool aSetError = false);

    HRESULT fetchSharedFolders (BOOL aGlobal);
    bool findOtherSharedFolder (IN_BSTR aName,
                                SharedFolderDataMap::const_iterator &aIt);

    HRESULT createSharedFolder (CBSTR aName, SharedFolderData aData);
    HRESULT removeSharedFolder (CBSTR aName);

    static DECLCALLBACK(int) configConstructor(PVM pVM, void *pvConsole);
    static DECLCALLBACK(void) vmstateChangeCallback(PVM aVM, VMSTATE aState,
                                                    VMSTATE aOldState, void *aUser);
    HRESULT doDriveChange (const char *pszDevice, unsigned uInstance,
                           unsigned uLun, DriveState_T eState,
                           DriveState_T *peState, const char *pszPath,
                           bool fPassthrough);
    static DECLCALLBACK(int) changeDrive (Console *pThis, const char *pszDevice,
                                          unsigned uInstance, unsigned uLun,
                                          DriveState_T eState, DriveState_T *peState,
                                          const char *pszPath, bool fPassthrough);

#ifdef VBOX_WITH_USB
    HRESULT attachUSBDevice (IUSBDevice *aHostDevice, ULONG aMaskedIfs);
    HRESULT detachUSBDevice (USBDeviceList::iterator &aIt);

    static DECLCALLBACK(int)
    usbAttachCallback (Console *that, IUSBDevice *aHostDevice, PCRTUUID aUuid,
                       bool aRemote, const char *aAddress, ULONG aMaskedIfs);
    static DECLCALLBACK(int)
    usbDetachCallback (Console *that, USBDeviceList::iterator *aIt, PCRTUUID aUuid);
#endif

    static DECLCALLBACK (int)
    stateProgressCallback (PVM pVM, unsigned uPercent, void *pvUser);

    static DECLCALLBACK(void)
    setVMErrorCallback (PVM pVM, void *pvUser, int rc, RT_SRC_POS_DECL,
                        const char *pszFormat, va_list args);

    static DECLCALLBACK(void)
    setVMRuntimeErrorCallback (PVM pVM, void *pvUser, uint32_t fFatal,
                               const char *pszErrorId,
                               const char *pszFormat, va_list va);

    HRESULT                     captureUSBDevices (PVM pVM);
    void                        detachAllUSBDevices (bool aDone);

    static DECLCALLBACK (int)   powerUpThread (RTTHREAD Thread, void *pvUser);
    static DECLCALLBACK (int)   saveStateThread (RTTHREAD Thread, void *pvUser);
    static DECLCALLBACK (int)   powerDownThread (RTTHREAD Thread, void *pvUser);

    static DECLCALLBACK(void *) drvStatus_QueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface);
    static DECLCALLBACK(void)   drvStatus_UnitChanged(PPDMILEDCONNECTORS pInterface, unsigned iLUN);
    static DECLCALLBACK(void)   drvStatus_Destruct(PPDMDRVINS pDrvIns);
    static DECLCALLBACK(int)    drvStatus_Construct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle);

    int mcAudioRefs;
    volatile uint32_t mcVRDPClients;
    uint32_t mu32SingleRDPClientId; /* The id of a connected client in the single connection mode. */

    static const char *sSSMConsoleUnit;
    static uint32_t sSSMConsoleVer;

    HRESULT loadDataFromSavedState();

    static DECLCALLBACK(void)   saveStateFileExec (PSSMHANDLE pSSM, void *pvUser);
    static DECLCALLBACK(int)    loadStateFileExec (PSSMHANDLE pSSM, void *pvUser, uint32_t u32Version);

#ifdef VBOX_WITH_GUEST_PROPS
    static DECLCALLBACK(int)    doGuestPropNotification (void *pvExtension, uint32_t,
                                                         void *pvParms, uint32_t cbParms);
    HRESULT doEnumerateGuestProperties (CBSTR aPatterns,
                                        ComSafeArrayOut(BSTR, aNames),
                                        ComSafeArrayOut(BSTR, aValues),
                                        ComSafeArrayOut(ULONG64, aTimestamps),
                                        ComSafeArrayOut(BSTR, aFlags));
#endif

    bool mSavedStateDataLoaded : 1;

    const ComPtr <IMachine> mMachine;
    const ComPtr <IInternalMachineControl> mControl;

    const ComPtr <IVRDPServer> mVRDPServer;
    const ComPtr <IDVDDrive> mDVDDrive;
    const ComPtr <IFloppyDrive> mFloppyDrive;

    ConsoleVRDPServer * const mConsoleVRDPServer;

    const ComObjPtr <Guest> mGuest;
    const ComObjPtr <Keyboard> mKeyboard;
    const ComObjPtr <Mouse> mMouse;
    const ComObjPtr <Display> mDisplay;
    const ComObjPtr <MachineDebugger> mDebugger;
    const ComObjPtr <RemoteDisplayInfo> mRemoteDisplayInfo;

    USBDeviceList mUSBDevices;
    RemoteUSBDeviceList mRemoteUSBDevices;

    SharedFolderMap mSharedFolders;
    SharedFolderDataMap mMachineSharedFolders;
    SharedFolderDataMap mGlobalSharedFolders;

    /** The VM instance handle. */
    PVM mpVM;
    /** Holds the number of "readonly" mpVM callers (users) */
    uint32_t mVMCallers;
    /** Semaphore posted when the number of mpVM callers drops to zero */
    RTSEMEVENT mVMZeroCallersSem;
    /** true when Console has entered the mpVM destruction phase */
    bool mVMDestroying : 1;
    /** true when power down is initiated by vmstateChangeCallback (EMT) */
    bool mVMPoweredOff : 1;

    /** The current DVD drive state in the VM.
     * This does not have to match the state maintained in the DVD. */
    DriveState_T meDVDState;
    /** The current Floppy drive state in the VM.
     * This does not have to match the state maintained in the Floppy. */
    DriveState_T meFloppyState;

    VMMDev * const mVMMDev;
    AudioSniffer * const mAudioSniffer;

    PPDMLED     mapFDLeds[2];
    PPDMLED     mapIDELeds[4];
    PPDMLED     mapSATALeds[30];
    PPDMLED     mapSCSILeds[16];
    PPDMLED     mapNetworkLeds[8];
    PPDMLED     mapSharedFolderLed;
    PPDMLED     mapUSBLed[2];
#if !defined(VBOX_WITH_NETFLT) && defined(RT_OS_LINUX)
    Utf8Str     maTAPDeviceName[8];
    RTFILE      maTapFD[8];
#endif

    bool mVMStateChangeCallbackDisabled;

    /* Local machine state value */
    MachineState_T mMachineState;

    typedef std::list <ComPtr <IConsoleCallback> > CallbackList;
    CallbackList mCallbacks;

    struct
    {
        /** OnMousePointerShapeChange() cache */
        struct
        {
            bool valid;
            bool visible;
            bool alpha;
            uint32_t xHot;
            uint32_t yHot;
            uint32_t width;
            uint32_t height;
            BYTE *shape;
            size_t shapeSize;
        }
        mpsc;

        /** OnMouseCapabilityChange() cache */
        struct
        {
            bool valid;
            BOOL supportsAbsolute;
            BOOL needsHostCursor;
        }
        mcc;

        /** OnKeyboardLedsChange() cache */
        struct
        {
            bool valid;
            bool numLock;
            bool capsLock;
            bool scrollLock;
        }
        klc;
    }
    mCallbackData;

    friend struct VMTask;
};

#endif // ____H_CONSOLEIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
