/** @file
 *
 * VBox Console COM Class definition
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
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
 *  Intended to used only within Console children (i,e. Keyboard, Mouse,
 *  Display, etc.).
 *
 *  @param drv  driver pointer to check (compare it with NULL)
 */
#define CHECK_CONSOLE_DRV(drv) \
    do { \
        if (!(drv)) \
            return setError (E_ACCESSDENIED, tr ("The console is not powered up")); \
    } while (0)

/** @def VBOX_WITH_UNIXY_TAP_NETWORKING
 *  Unixy style TAP networking. This is defined in the Makefile since it's also
 *  used by NetworkAdapterImpl.h/cpp.
 */
#ifdef __DOXYGEN__
# define VBOX_WITH_UNIXY_TAP_NETWORKING
#endif

// Console
///////////////////////////////////////////////////////////////////////////////

/** IConsole implementation class */
class ATL_NO_VTABLE Console :
    public VirtualBoxBaseWithChildrenNEXT,
    public VirtualBoxSupportErrorInfoImpl <Console, IConsole>,
    public VirtualBoxSupportTranslation <Console>,
    public IConsole
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
    STDMETHOD(COMGETTER(USBDevices)) (IUSBDeviceCollection **aUSBDevices);
    STDMETHOD(COMGETTER(RemoteUSBDevices)) (IHostUSBDeviceCollection **aRemoteUSBDevices);
    STDMETHOD(COMGETTER(RemoteDisplayInfo)) (IRemoteDisplayInfo **aRemoteDisplayInfo);
    STDMETHOD(COMGETTER(SharedFolders)) (ISharedFolderCollection **aSharedFolders);

    // IConsole methods
    STDMETHOD(PowerUp) (IProgress **aProgress);
    STDMETHOD(PowerDown)();
    STDMETHOD(Reset)();
    STDMETHOD(Pause)();
    STDMETHOD(Resume)();
    STDMETHOD(PowerButton)();
    STDMETHOD(SaveState) (IProgress **aProgress);
    STDMETHOD(AdoptSavedState) (INPTR BSTR aSavedStateFile);
    STDMETHOD(DiscardSavedState)();
    STDMETHOD(GetDeviceActivity) (DeviceType_T aDeviceType,
                                 DeviceActivity_T *aDeviceActivity);
    STDMETHOD(AttachUSBDevice) (INPTR GUIDPARAM aId);
    STDMETHOD(DetachUSBDevice) (INPTR GUIDPARAM aId, IUSBDevice **aDevice);
    STDMETHOD(CreateSharedFolder) (INPTR BSTR aName, INPTR BSTR aHostPath);
    STDMETHOD(RemoveSharedFolder) (INPTR BSTR aName);
    STDMETHOD(TakeSnapshot) (INPTR BSTR aName, INPTR BSTR aDescription,
                             IProgress **aProgress);
    STDMETHOD(DiscardSnapshot) (INPTR GUIDPARAM aId, IProgress **aProgress);
    STDMETHOD(DiscardCurrentState) (IProgress **aProgress);
    STDMETHOD(DiscardCurrentSnapshotAndState) (IProgress **aProgress);
    STDMETHOD(RegisterCallback) (IConsoleCallback *aCallback);
    STDMETHOD(UnregisterCallback)(IConsoleCallback *aCallback);

    // public methods for internal purposes only

    /*
     *  Note: the following methods do not increase refcount. intended to be
     *  called only by the VM execution thread.
     */

    Guest *getGuest() { return mGuest; }
    Keyboard *getKeyboard() { return mKeyboard; }
    Mouse *getMouse() { return mMouse; }
    Display *getDisplay() { return mDisplay; }
    MachineDebugger *getMachineDebugger() { return mDebugger; }

    const ComPtr <IMachine> &machine() { return mMachine; }

    /** Method is called only from ConsoleVRDPServer */
    IVRDPServer *getVRDPServer() { return mVRDPServer; }

    ConsoleVRDPServer *consoleVRDPServer() { return mConsoleVRDPServer; }

    HRESULT updateMachineState (MachineState_T aMachineState);

    // events from IInternalSessionControl
    HRESULT onDVDDriveChange();
    HRESULT onFloppyDriveChange();
    HRESULT onNetworkAdapterChange (INetworkAdapter *aNetworkAdapter);
    HRESULT onSerialPortChange (ISerialPort *aSerialPort);
    HRESULT onParallelPortChange (IParallelPort *aParallelPort);
    HRESULT onVRDPServerChange();
    HRESULT onUSBControllerChange();
    HRESULT onSharedFolderChange (BOOL aGlobal);
    HRESULT onUSBDeviceAttach (IUSBDevice *aDevice, IVirtualBoxErrorInfo *aError);
    HRESULT onUSBDeviceDetach (INPTR GUIDPARAM aId, IVirtualBoxErrorInfo *aError);

    VMMDev *getVMMDev() { return mVMMDev; }
    AudioSniffer *getAudioSniffer () { return mAudioSniffer; }

#ifdef VRDP_NO_COM
    int VRDPClientLogon (uint32_t u32ClientId, const char *pszUser, const char *pszPassword, const char *pszDomain);
    void VRDPClientConnect (uint32_t u32ClientId);
    void VRDPClientDisconnect (uint32_t u32ClientId, uint32_t fu32Intercepted);
    void VRDPInterceptAudio (uint32_t u32ClientId);
    void VRDPInterceptUSB (uint32_t u32ClientId, void **ppvIntercept);
    void VRDPInterceptClipboard (uint32_t u32ClientId);
#else
    static VRDPSERVERCALLBACK *getVrdpServerCallback () { return &sVrdpServerCallback; };
#endif /* VRDP_NO_COM */

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
    void onRuntimeError (BOOL aFatal, INPTR BSTR aErrorID, INPTR BSTR aMessage);
    HRESULT onShowWindow (BOOL aCheck, BOOL *aCanShow, ULONG64 *aWinId);

    static const PDMDRVREG DrvStatusReg;

    void reportAuthLibraryError (const char *filename, int rc)
    {
        setError (E_FAIL, tr("Could not load the external authentication library '%s' (%Vrc)"), filename, rc);
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
     *  @sa SafeVMPtr, SafeVMPtrQuiet
     */
    typedef AutoVMCallerBase <false, false> AutoVMCaller;

    /**
     *  Same as AutoVMCaller but doesn't set extended error info on failure.
     */
    typedef AutoVMCallerBase <true, false> AutoVMCallerQuiet;

    /**
     *  Same as AutoVMCaller but allows a null VM pointer (to trigger an error
     *  instead of assertion).
     */
    typedef AutoVMCallerBase <false, true> AutoVMCallerWeak;

    /**
     *  Same as AutoVMCaller but doesn't set extended error info on failure
     *  and allows a null VM pointer (to trigger an error instead of
     *  assertion).
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
     *  @sa SafeVMPtrQuiet, AutoVMCaller
     */
    typedef SafeVMPtrBase <false> SafeVMPtr;

    /**
     *  A deviation of SaveVMPtr that doesn't set the error info on failure.
     *  Intenede for pieces of code that don't need to return the VM access
     *  failure to the caller. The usage pattern is:
     *  <code>
     *      Console::SaveVMPtrQuiet pVM (mParent);
     *      if (pVM.rc())
     *          VMR3ReqCall (pVM, ...
     *      return S_OK;
     *  </code>
     *
     *  @sa SafeVMPtr, AutoVMCaller
     */
    typedef SafeVMPtrBase <true> SafeVMPtrQuiet;

    typedef std::map <Bstr, ComObjPtr <SharedFolder> > SharedFolderMap;
    typedef std::map <Bstr, Bstr> SharedFolderDataMap;

private:

    typedef std::list <ComObjPtr <OUSBDevice> > USBDeviceList;
    typedef std::list <ComObjPtr <RemoteUSBDevice> > RemoteUSBDeviceList;

    HRESULT addVMCaller (bool aQuiet = false, bool aAllowNullVM = false);
    void releaseVMCaller();

    HRESULT powerDown();

    HRESULT callTapSetupApplication(bool isStatic, RTFILE tapFD, Bstr &tapDevice,
                                    Bstr &tapSetupApplication);
    HRESULT attachToHostInterface(INetworkAdapter *networkAdapter);
    HRESULT detachFromHostInterface(INetworkAdapter *networkAdapter);
    HRESULT powerDownHostInterfaces();

    HRESULT setMachineState (MachineState_T aMachineState, bool aUpdateServer = true);
    HRESULT setMachineStateLocally (MachineState_T aMachineState)
    {
        return setMachineState (aMachineState, false /* aUpdateServer */);
    }

    HRESULT findSharedFolder (const BSTR aName,
                              ComObjPtr <SharedFolder> &aSharedFolder,
                              bool aSetError = false);

    HRESULT fetchSharedFolders (BOOL aGlobal);
    bool findOtherSharedFolder (INPTR BSTR aName,
                                SharedFolderDataMap::const_iterator &aIt);

    HRESULT createSharedFolder (INPTR BSTR aName, INPTR BSTR aHostPath);
    HRESULT removeSharedFolder (INPTR BSTR aName);

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

#ifndef VBOX_WITH_PDMUSB
    HRESULT attachUSBDevice (IUSBDevice *aHostDevice, PVUSBIRHCONFIG aConfig);
    HRESULT detachUSBDevice (USBDeviceList::iterator &aIt);

    static DECLCALLBACK(int)
    usbAttachCallback (Console *that, IUSBDevice *aHostDevice,
                       PVUSBIRHCONFIG aConfig, PCRTUUID aUuid, bool aRemote,
                       const char *aAddress, void *aRemoteBackend);
    static DECLCALLBACK(int)
    usbDetachCallback (Console *that, USBDeviceList::iterator *aIt,
                       PVUSBIRHCONFIG aConfig, PCRTUUID aUuid);
#else /* PDMUsb coding. */
    HRESULT attachUSBDevice (IUSBDevice *aHostDevice);
    HRESULT detachUSBDevice (USBDeviceList::iterator &aIt);

    static DECLCALLBACK(int)
    usbAttachCallback (Console *that, IUSBDevice *aHostDevice, PCRTUUID aUuid,
                       bool aRemote, const char *aAddress);
    static DECLCALLBACK(int)
    usbDetachCallback (Console *that, USBDeviceList::iterator *aIt, PCRTUUID aUuid);
#endif /* PDMUsb coding. */

    static DECLCALLBACK (int)
    stateProgressCallback (PVM pVM, unsigned uPercent, void *pvUser);

    static DECLCALLBACK(void)
    setVMErrorCallback (PVM pVM, void *pvUser, int rc, RT_SRC_POS_DECL,
                        const char *pszFormat, va_list args);

    static DECLCALLBACK(void)
    setVMRuntimeErrorCallback (PVM pVM, void *pvUser, bool fFatal,
                               const char *pszErrorID,
                               const char *pszFormat, va_list args);

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

#ifdef VRDP_NO_COM
#else
    static DECLCALLBACK(int)    vrdp_ClientLogon (void *pvUser, uint32_t u32ClientId, const char *pszUser, const char *pszPassword, const char *pszDomain);
    static DECLCALLBACK(void)   vrdp_ClientConnect (void *pvUser, uint32_t u32ClientId);
    static DECLCALLBACK(void)   vrdp_ClientDisconnect (void *pvUser, uint32_t u32ClientId, uint32_t fu32Intercepted);
    static DECLCALLBACK(void)   vrdp_InterceptAudio (void *pvUser, uint32_t u32ClientId);
    static DECLCALLBACK(void)   vrdp_InterceptUSB (void *pvUser, uint32_t u32ClientId, PFNVRDPUSBCALLBACK *ppfn, void **ppv);
    static DECLCALLBACK(void)   vrdp_InterceptClipboard (void *pvUser, uint32_t u32ClientId, PFNVRDPCLIPBOARDCALLBACK *ppfn, void **ppv);

    static VRDPSERVERCALLBACK   sVrdpServerCallback;
#endif /* VRDP_NO_COM */

    static const char *sSSMConsoleUnit;
    static uint32_t sSSMConsoleVer;

    HRESULT loadDataFromSavedState();

    static DECLCALLBACK(void)   saveStateFileExec (PSSMHANDLE pSSM, void *pvUser);
    static DECLCALLBACK(int)    loadStateFileExec (PSSMHANDLE pSSM, void *pvUser, uint32_t u32Version);

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
    PPDMLED     mapNetworkLeds[8];
    PPDMLED     mapSharedFolderLed;
    PPDMLED     mapUSBLed;
#ifdef VBOX_WITH_UNIXY_TAP_NETWORKING
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
