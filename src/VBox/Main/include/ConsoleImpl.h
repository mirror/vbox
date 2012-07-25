/* $Id$ */
/** @file
 * VBox Console COM Class definition
 */

/*
 * Copyright (C) 2005-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_CONSOLEIMPL
#define ____H_CONSOLEIMPL

#include "VirtualBoxBase.h"
#include "VBox/com/array.h"
#include "EventImpl.h"

class Guest;
class Keyboard;
class Mouse;
class Display;
class MachineDebugger;
class TeleporterStateSrc;
class OUSBDevice;
class RemoteUSBDevice;
class SharedFolder;
class VRDEServerInfo;
class AudioSniffer;
#ifdef VBOX_WITH_USB_VIDEO
class UsbWebcamInterface;
#endif
#ifdef VBOX_WITH_USB_CARDREADER
class UsbCardReader;
#endif
class ConsoleVRDPServer;
class VMMDev;
class Progress;
class BusAssignmentManager;
COM_STRUCT_OR_CLASS(IEventListener);
#ifdef VBOX_WITH_EXTPACK
class ExtPackManager;
#endif

#include <VBox/RemoteDesktop/VRDE.h>
#include <VBox/vmm/pdmdrv.h>
#ifdef VBOX_WITH_GUEST_PROPS
# include <VBox/HostServices/GuestPropertySvc.h>  /* For the property notification callback */
#endif

#ifdef RT_OS_WINDOWS
# include "../src-server/win/VBoxComEvents.h"
#endif

struct VUSBIRHCONFIG;
typedef struct VUSBIRHCONFIG *PVUSBIRHCONFIG;

#include <list>
#include <vector>

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
            return setError(E_ACCESSDENIED, tr("The console is not powered up")); \
    } while (0)

// Console
///////////////////////////////////////////////////////////////////////////////

/** IConsole implementation class */
class ATL_NO_VTABLE Console :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IConsole)
{
    Q_OBJECT

public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(Console, IConsole)

    DECLARE_NOT_AGGREGATABLE(Console)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Console)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IConsole)
    END_COM_MAP()

    Console();
    ~Console();

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializers/uninitializers for internal purposes only
    HRESULT init(IMachine *aMachine, IInternalMachineControl *aControl, LockType_T aLockType);
    void uninit();

    // IConsole properties
    STDMETHOD(COMGETTER(Machine))(IMachine **aMachine);
    STDMETHOD(COMGETTER(State))(MachineState_T *aMachineState);
    STDMETHOD(COMGETTER(Guest))(IGuest **aGuest);
    STDMETHOD(COMGETTER(Keyboard))(IKeyboard **aKeyboard);
    STDMETHOD(COMGETTER(Mouse))(IMouse **aMouse);
    STDMETHOD(COMGETTER(Display))(IDisplay **aDisplay);
    STDMETHOD(COMGETTER(Debugger))(IMachineDebugger **aDebugger);
    STDMETHOD(COMGETTER(USBDevices))(ComSafeArrayOut(IUSBDevice *, aUSBDevices));
    STDMETHOD(COMGETTER(RemoteUSBDevices))(ComSafeArrayOut(IHostUSBDevice *, aRemoteUSBDevices));
    STDMETHOD(COMGETTER(VRDEServerInfo))(IVRDEServerInfo **aVRDEServerInfo);
    STDMETHOD(COMGETTER(SharedFolders))(ComSafeArrayOut(ISharedFolder *, aSharedFolders));
    STDMETHOD(COMGETTER(EventSource)) (IEventSource ** aEventSource);
    STDMETHOD(COMGETTER(AttachedPciDevices))(ComSafeArrayOut(IPciDeviceAttachment *, aAttachments));
    STDMETHOD(COMGETTER(UseHostClipboard))(BOOL *aUseHostClipboard);
    STDMETHOD(COMSETTER(UseHostClipboard))(BOOL aUseHostClipboard);

    // IConsole methods
    STDMETHOD(PowerUp)(IProgress **aProgress);
    STDMETHOD(PowerUpPaused)(IProgress **aProgress);
    STDMETHOD(PowerDown)(IProgress **aProgress);
    STDMETHOD(Reset)();
    STDMETHOD(Pause)();
    STDMETHOD(Resume)();
    STDMETHOD(PowerButton)();
    STDMETHOD(SleepButton)();
    STDMETHOD(GetPowerButtonHandled)(BOOL *aHandled);
    STDMETHOD(GetGuestEnteredACPIMode)(BOOL *aEntered);
    STDMETHOD(SaveState)(IProgress **aProgress);
    STDMETHOD(AdoptSavedState)(IN_BSTR aSavedStateFile);
    STDMETHOD(DiscardSavedState)(BOOL aRemoveFile);
    STDMETHOD(GetDeviceActivity)(DeviceType_T aDeviceType,
                                DeviceActivity_T *aDeviceActivity);
    STDMETHOD(AttachUSBDevice)(IN_BSTR aId);
    STDMETHOD(DetachUSBDevice)(IN_BSTR aId, IUSBDevice **aDevice);
    STDMETHOD(FindUSBDeviceByAddress)(IN_BSTR aAddress, IUSBDevice **aDevice);
    STDMETHOD(FindUSBDeviceById)(IN_BSTR aId, IUSBDevice **aDevice);
    STDMETHOD(CreateSharedFolder)(IN_BSTR aName, IN_BSTR aHostPath, BOOL aWritable, BOOL aAutoMount);
    STDMETHOD(RemoveSharedFolder)(IN_BSTR aName);
    STDMETHOD(TakeSnapshot)(IN_BSTR aName, IN_BSTR aDescription,
                            IProgress **aProgress);
    STDMETHOD(DeleteSnapshot)(IN_BSTR aId, IProgress **aProgress);
    STDMETHOD(DeleteSnapshotAndAllChildren)(IN_BSTR aId, IProgress **aProgress);
    STDMETHOD(DeleteSnapshotRange)(IN_BSTR aStartId, IN_BSTR aEndId, IProgress **aProgress);
    STDMETHOD(RestoreSnapshot)(ISnapshot *aSnapshot, IProgress **aProgress);
    STDMETHOD(Teleport)(IN_BSTR aHostname, ULONG aPort, IN_BSTR aPassword, ULONG aMaxDowntime, IProgress **aProgress);

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
    AudioSniffer *getAudioSniffer() const { return mAudioSniffer; }

    const ComPtr<IMachine> &machine() const { return mMachine; }

    bool useHostClipboard() { return mfUseHostClipboard; }

    /** Method is called only from ConsoleVRDPServer */
    IVRDEServer *getVRDEServer() const { return mVRDEServer; }

    ConsoleVRDPServer *consoleVRDPServer() const { return mConsoleVRDPServer; }

    HRESULT updateMachineState(MachineState_T aMachineState);

    // events from IInternalSessionControl
    HRESULT onNetworkAdapterChange(INetworkAdapter *aNetworkAdapter, BOOL changeAdapter);
    HRESULT onSerialPortChange(ISerialPort *aSerialPort);
    HRESULT onParallelPortChange(IParallelPort *aParallelPort);
    HRESULT onStorageControllerChange();
    HRESULT onMediumChange(IMediumAttachment *aMediumAttachment, BOOL aForce);
    HRESULT onCPUChange(ULONG aCPU, BOOL aRemove);
    HRESULT onCPUExecutionCapChange(ULONG aExecutionCap);
    HRESULT onClipboardModeChange(ClipboardMode_T aClipboardMode);
    HRESULT onDragAndDropModeChange(DragAndDropMode_T aDragAndDropMode);
    HRESULT onVRDEServerChange(BOOL aRestart);
    HRESULT onUSBControllerChange();
    HRESULT onSharedFolderChange(BOOL aGlobal);
    HRESULT onUSBDeviceAttach(IUSBDevice *aDevice, IVirtualBoxErrorInfo *aError, ULONG aMaskedIfs);
    HRESULT onUSBDeviceDetach(IN_BSTR aId, IVirtualBoxErrorInfo *aError);
    HRESULT onBandwidthGroupChange(IBandwidthGroup *aBandwidthGroup);
    HRESULT onStorageDeviceChange(IMediumAttachment *aMediumAttachment, BOOL aRemove);
    HRESULT getGuestProperty(IN_BSTR aKey, BSTR *aValue, LONG64 *aTimestamp, BSTR *aFlags);
    HRESULT setGuestProperty(IN_BSTR aKey, IN_BSTR aValue, IN_BSTR aFlags);
    HRESULT enumerateGuestProperties(IN_BSTR aPatterns,
                                     ComSafeArrayOut(BSTR, aNames),
                                     ComSafeArrayOut(BSTR, aValues),
                                     ComSafeArrayOut(LONG64, aTimestamps),
                                     ComSafeArrayOut(BSTR, aFlags));
    HRESULT onlineMergeMedium(IMediumAttachment *aMediumAttachment,
                              ULONG aSourceIdx, ULONG aTargetIdx,
                              IMedium *aSource, IMedium *aTarget,
                              BOOL aMergeForward, IMedium *aParentForTarget,
                              ComSafeArrayIn(IMedium *, aChildrenToReparent),
                              IProgress *aProgress);
    VMMDev *getVMMDev() { return m_pVMMDev; }
    AudioSniffer *getAudioSniffer() { return mAudioSniffer; }
#ifdef VBOX_WITH_EXTPACK
    ExtPackManager *getExtPackManager();
#endif
    EventSource *getEventSource() { return mEventSource; }
#ifdef VBOX_WITH_USB_CARDREADER
    UsbCardReader *getUsbCardReader() { return mUsbCardReader; }
#endif

    int VRDPClientLogon(uint32_t u32ClientId, const char *pszUser, const char *pszPassword, const char *pszDomain);
    void VRDPClientStatusChange(uint32_t u32ClientId, const char *pszStatus);
    void VRDPClientConnect(uint32_t u32ClientId);
    void VRDPClientDisconnect(uint32_t u32ClientId, uint32_t fu32Intercepted);
    void VRDPInterceptAudio(uint32_t u32ClientId);
    void VRDPInterceptUSB(uint32_t u32ClientId, void **ppvIntercept);
    void VRDPInterceptClipboard(uint32_t u32ClientId);

    void processRemoteUSBDevices(uint32_t u32ClientId, VRDEUSBDEVICEDESC *pDevList, uint32_t cbDevList, bool fDescExt);
    void reportGuestStatistics(ULONG aValidStats, ULONG aCpuUser,
                               ULONG aCpuKernel, ULONG aCpuIdle,
                               ULONG aMemTotal, ULONG aMemFree,
                               ULONG aMemBalloon, ULONG aMemShared,
                               ULONG aMemCache, ULONG aPageTotal,
                               ULONG aAllocVMM, ULONG aFreeVMM,
                               ULONG aBalloonedVMM, ULONG aSharedVMM)
    {
        mControl->ReportGuestStatistics(aValidStats, aCpuUser, aCpuKernel, aCpuIdle,
                                        aMemTotal, aMemFree, aMemBalloon, aMemShared,
                                        aMemCache, aPageTotal, aAllocVMM, aFreeVMM,
                                        aBalloonedVMM, aSharedVMM);
    }
    void enableVMMStatistics(BOOL aEnable);

    // callback callers (partly; for some events console callbacks are notified
    // directly from IInternalSessionControl event handlers declared above)
    void onMousePointerShapeChange(bool fVisible, bool fAlpha,
                                   uint32_t xHot, uint32_t yHot,
                                   uint32_t width, uint32_t height,
                                   ComSafeArrayIn(uint8_t, aShape));
    void onMouseCapabilityChange(BOOL supportsAbsolute, BOOL supportsRelative, BOOL needsHostCursor);
    void onStateChange(MachineState_T aMachineState);
    void onAdditionsStateChange();
    void onAdditionsOutdated();
    void onKeyboardLedsChange(bool fNumLock, bool fCapsLock, bool fScrollLock);
    void onUSBDeviceStateChange(IUSBDevice *aDevice, bool aAttached,
                                IVirtualBoxErrorInfo *aError);
    void onRuntimeError(BOOL aFatal, IN_BSTR aErrorID, IN_BSTR aMessage);
    HRESULT onShowWindow(BOOL aCheck, BOOL *aCanShow, LONG64 *aWinId);
    void onVRDEServerInfoChange();

    static const PDMDRVREG DrvStatusReg;

    static HRESULT setErrorStatic(HRESULT aResultCode, const char *pcsz, ...);
    HRESULT setInvalidMachineStateError();

    static const char *convertControllerTypeToDev(StorageControllerType_T enmCtrlType);
    static HRESULT convertBusPortDeviceToLun(StorageBus_T enmBus, LONG port, LONG device, unsigned &uLun);
    // Called from event listener
    HRESULT onNATRedirectRuleChange(ULONG ulInstance, BOOL aNatRuleRemove,
                                 NATProtocol_T aProto, IN_BSTR aHostIp, LONG aHostPort, IN_BSTR aGuestIp, LONG aGuestPort);

private:

    /**
     *  Base template for AutoVMCaller and SaveVMPtr. Template arguments
     *  have the same meaning as arguments of Console::addVMCaller().
     */
    template <bool taQuiet = false, bool taAllowNullVM = false>
    class AutoVMCallerBase
    {
    public:
        AutoVMCallerBase(Console *aThat) : mThat(aThat), mRC(S_OK)
        {
            Assert(aThat);
            mRC = aThat->addVMCaller(taQuiet, taAllowNullVM);
        }
        ~AutoVMCallerBase()
        {
            if (SUCCEEDED(mRC))
                mThat->releaseVMCaller();
        }
        /** Decreases the number of callers before the instance is destroyed. */
        void releaseCaller()
        {
            AssertReturnVoid(SUCCEEDED(mRC));
            mThat->releaseVMCaller();
            mRC = E_FAIL;
        }
        /** Restores the number of callers after by #release(). #rc() must be
         *  rechecked to ensure the operation succeeded. */
        void addYY()
        {
            AssertReturnVoid(!SUCCEEDED(mRC));
            mRC = mThat->addVMCaller(taQuiet, taAllowNullVM);
        }
        /** Returns the result of Console::addVMCaller() */
        HRESULT rc() const { return mRC; }
        /** Shortcut to SUCCEEDED(rc()) */
        bool isOk() const { return SUCCEEDED(mRC); }
    protected:
        Console *mThat;
        HRESULT mRC;
    private:
        DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP(AutoVMCallerBase)
    };

#if 0
    /**
     *  Helper class that protects sections of code using the mpVM pointer by
     *  automatically calling addVMCaller() on construction and
     *  releaseVMCaller() on destruction. Intended for Console methods dealing
     *  with mpVM. The usage pattern is:
     *  <code>
     *      AutoVMCaller autoVMCaller(this);
     *      if (FAILED(autoVMCaller.rc())) return autoVMCaller.rc();
     *      ...
     *      VMR3ReqCall (mpVM, ...
     *  </code>
     *
     *  @note Temporarily locks the argument for writing.
     *
     *  @sa SafeVMPtr, SafeVMPtrQuiet
     *  @obsolete Use SafeVMPtr
     */
    typedef AutoVMCallerBase<false, false> AutoVMCaller;
#endif

    /**
     *  Same as AutoVMCaller but doesn't set extended error info on failure.
     *
     *  @note Temporarily locks the argument for writing.
    *  @obsolete Use SafeVMPtrQuiet
     */
    typedef AutoVMCallerBase<true, false> AutoVMCallerQuiet;

    /**
     *  Same as AutoVMCaller but allows a null VM pointer (to trigger an error
     *  instead of assertion).
     *
     *  @note Temporarily locks the argument for writing.
     *  @obsolete Use SafeVMPtr
     */
    typedef AutoVMCallerBase<false, true> AutoVMCallerWeak;

    /**
     *  Same as AutoVMCaller but doesn't set extended error info on failure
     *  and allows a null VM pointer (to trigger an error instead of
     *  assertion).
     *
     *  @note Temporarily locks the argument for writing.
     *  @obsolete Use SafeVMPtrQuiet
     */
    typedef AutoVMCallerBase<true, true> AutoVMCallerQuietWeak;

    /**
     *  Base template for SaveVMPtr and SaveVMPtrQuiet.
     */
    template<bool taQuiet = false>
    class SafeVMPtrBase : public AutoVMCallerBase<taQuiet, true>
    {
        typedef AutoVMCallerBase<taQuiet, true> Base;
    public:
        SafeVMPtrBase(Console *aThat) : Base(aThat), mpVM(NULL), mpUVM(NULL)
        {
            if (SUCCEEDED(Base::mRC))
                Base::mRC = aThat->safeVMPtrRetainer(&mpVM, &mpUVM, taQuiet);
        }
        ~SafeVMPtrBase()
        {
            if (SUCCEEDED(Base::mRC))
                release();
        }
        /** Smart SaveVMPtr to PVM cast operator */
        operator PVM() const { return mpVM; }
        /** Direct PVM access for printf()-like functions */
        PVM raw() const { return mpVM; }
        /** Direct PUVM access for printf()-like functions */
        PUVM rawUVM() const { return mpUVM; }
        /** Release the handles. */
        void release()
        {
            AssertReturnVoid(SUCCEEDED(Base::mRC));
            Base::mThat->safeVMPtrReleaser(&mpVM, &mpUVM);
            Base::releaseCaller();
        }

    private:
        PVM     mpVM;
        PUVM    mpUVM;
        DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP(SafeVMPtrBase)
    };

public:

    /**
     *  Helper class that safely manages the Console::mpVM pointer
     *  by calling addVMCaller() on construction and releaseVMCaller() on
     *  destruction. Intended for Console children. The usage pattern is:
     *  <code>
     *      Console::SaveVMPtr pVM(mParent);
     *      if (FAILED(pVM.rc())) return pVM.rc();
     *      ...
     *      VMR3ReqCall(pVM, ...
     *      ...
     *      printf("%p\n", pVM.raw());
     *  </code>
     *
     *  @note Temporarily locks the argument for writing.
     *
     *  @sa SafeVMPtrQuiet, AutoVMCaller
     */
    typedef SafeVMPtrBase<false> SafeVMPtr;

    /**
     *  A deviation of SaveVMPtr that doesn't set the error info on failure.
     *  Intended for pieces of code that don't need to return the VM access
     *  failure to the caller. The usage pattern is:
     *  <code>
     *      Console::SaveVMPtrQuiet pVM(mParent);
     *      if (pVM.rc())
     *          VMR3ReqCall(pVM, ...
     *      return S_OK;
     *  </code>
     *
     *  @note Temporarily locks the argument for writing.
     *
     *  @sa SafeVMPtr, AutoVMCaller
     */
    typedef SafeVMPtrBase<true> SafeVMPtrQuiet;

    class SharedFolderData
    {
    public:
        SharedFolderData()
        { }

        SharedFolderData(const Utf8Str &aHostPath,
                         bool aWritable,
                         bool aAutoMount)
           : m_strHostPath(aHostPath),
             m_fWritable(aWritable),
             m_fAutoMount(aAutoMount)
        { }

        // copy constructor
        SharedFolderData(const SharedFolderData& aThat)
           : m_strHostPath(aThat.m_strHostPath),
             m_fWritable(aThat.m_fWritable),
             m_fAutoMount(aThat.m_fAutoMount)
        { }

        Utf8Str m_strHostPath;
        bool m_fWritable;
        bool m_fAutoMount;
    };

    typedef std::map<Utf8Str, ComObjPtr<SharedFolder> > SharedFolderMap;
    typedef std::map<Utf8Str, SharedFolderData> SharedFolderDataMap;
    typedef std::map<Utf8Str, ComPtr<IMediumAttachment> > MediumAttachmentMap;

private:

    typedef std::list <ComObjPtr<OUSBDevice> > USBDeviceList;
    typedef std::list <ComObjPtr<RemoteUSBDevice> > RemoteUSBDeviceList;

    HRESULT addVMCaller(bool aQuiet = false, bool aAllowNullVM = false);
    void    releaseVMCaller();
    HRESULT safeVMPtrRetainer(PVM *a_ppVM, PUVM *a_ppUVM, bool aQuiet);
    void    safeVMPtrReleaser(PVM *a_ppVM, PUVM *a_ppUVM);

    HRESULT consoleInitReleaseLog(const ComPtr<IMachine> aMachine);

    HRESULT powerUp(IProgress **aProgress, bool aPaused);
    HRESULT powerDown(IProgress *aProgress = NULL);

/* Note: FreeBSD needs this whether netflt is used or not. */
#if ((defined(RT_OS_LINUX) && !defined(VBOX_WITH_NETFLT)) || defined(RT_OS_FREEBSD))
    HRESULT attachToTapInterface(INetworkAdapter *networkAdapter);
    HRESULT detachFromTapInterface(INetworkAdapter *networkAdapter);
#endif
    HRESULT powerDownHostInterfaces();

    HRESULT setMachineState(MachineState_T aMachineState, bool aUpdateServer = true);
    HRESULT setMachineStateLocally(MachineState_T aMachineState)
    {
        return setMachineState(aMachineState, false /* aUpdateServer */);
    }

    HRESULT findSharedFolder(const Utf8Str &strName,
                             ComObjPtr<SharedFolder> &aSharedFolder,
                             bool aSetError = false);

    HRESULT fetchSharedFolders(BOOL aGlobal);
    bool findOtherSharedFolder(const Utf8Str &straName,
                               SharedFolderDataMap::const_iterator &aIt);

    HRESULT createSharedFolder(const Utf8Str &strName, const SharedFolderData &aData);
    HRESULT removeSharedFolder(const Utf8Str &strName);

    static DECLCALLBACK(int) configConstructor(PVM pVM, void *pvConsole);
    int configConstructorInner(PVM pVM, AutoWriteLock *pAlock);
    int configCfgmOverlay(PVM pVM, IVirtualBox *pVirtualBox, IMachine *pMachine);

    int configMediumAttachment(PCFGMNODE pCtlInst,
                               const char *pcszDevice,
                               unsigned uInstance,
                               StorageBus_T enmBus,
                               bool fUseHostIOCache,
                               bool fBuiltinIoCache,
                               bool fSetupMerge,
                               unsigned uMergeSource,
                               unsigned uMergeTarget,
                               IMediumAttachment *pMediumAtt,
                               MachineState_T aMachineState,
                               HRESULT *phrc,
                               bool fAttachDetach,
                               bool fForceUnmount,
                               bool fHotplug,
                               PVM pVM,
                               DeviceType_T *paLedDevType);
    int configMedium(PCFGMNODE pLunL0,
                     bool fPassthrough,
                     DeviceType_T enmType,
                     bool fUseHostIOCache,
                     bool fBuiltinIoCache,
                     bool fSetupMerge,
                     unsigned uMergeSource,
                     unsigned uMergeTarget,
                     const char *pcszBwGroup,
                     bool fDiscard,
                     IMedium *pMedium,
                     MachineState_T aMachineState,
                     HRESULT *phrc);
    static DECLCALLBACK(int) reconfigureMediumAttachment(Console *pConsole,
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
                                                         HRESULT *phrc);
    static DECLCALLBACK(int) changeRemovableMedium(Console *pThis,
                                                   PVM pVM,
                                                   const char *pcszDevice,
                                                   unsigned uInstance,
                                                   StorageBus_T enmBus,
                                                   bool fUseHostIOCache,
                                                   IMediumAttachment *aMediumAtt,
                                                   bool fForce);

    HRESULT attachRawPciDevices(PVM pVM, BusAssignmentManager *BusMgr, PCFGMNODE pDevices);
    void attachStatusDriver(PCFGMNODE pCtlInst, PPDMLED *papLeds,
                            uint64_t uFirst, uint64_t uLast,
                            Console::MediumAttachmentMap *pmapMediumAttachments,
                            const char *pcszDevice, unsigned uInstance);

    int configNetwork(const char *pszDevice, unsigned uInstance, unsigned uLun,
                      INetworkAdapter *aNetworkAdapter, PCFGMNODE pCfg,
                      PCFGMNODE pLunL0, PCFGMNODE pInst,
                      bool fAttachDetach, bool fIgnoreConnectFailure);

    static DECLCALLBACK(int) configGuestProperties(void *pvConsole, PVM pVM);
    static DECLCALLBACK(int) configGuestControl(void *pvConsole);
    static DECLCALLBACK(void) vmstateChangeCallback(PVM aVM, VMSTATE aState,
                                                    VMSTATE aOldState, void *aUser);
    static DECLCALLBACK(int) unplugCpu(Console *pThis, PVM pVM, unsigned uCpu);
    static DECLCALLBACK(int) plugCpu(Console *pThis, PVM pVM, unsigned uCpu);
    HRESULT doMediumChange(IMediumAttachment *aMediumAttachment, bool fForce, PVM pVM);
    HRESULT doCPURemove(ULONG aCpu, PVM pVM);
    HRESULT doCPUAdd(ULONG aCpu, PVM pVM);

    HRESULT doNetworkAdapterChange(PVM pVM, const char *pszDevice, unsigned uInstance,
                                   unsigned uLun, INetworkAdapter *aNetworkAdapter);
    static DECLCALLBACK(int) changeNetworkAttachment(Console *pThis, PVM pVM, const char *pszDevice,
                                                     unsigned uInstance, unsigned uLun,
                                                     INetworkAdapter *aNetworkAdapter);

    void changeClipboardMode(ClipboardMode_T aClipboardMode);
    void changeDragAndDropMode(DragAndDropMode_T aDragAndDropMode);

#ifdef VBOX_WITH_USB
    HRESULT attachUSBDevice(IUSBDevice *aHostDevice, ULONG aMaskedIfs);
    HRESULT detachUSBDevice(const ComObjPtr<OUSBDevice> &aHostDevice);

    static DECLCALLBACK(int) usbAttachCallback(Console *that, PVM pVM, IUSBDevice *aHostDevice, PCRTUUID aUuid,
                       bool aRemote, const char *aAddress, void *pvRemoteBackend, USHORT aPortVersion, ULONG aMaskedIfs);
    static DECLCALLBACK(int) usbDetachCallback(Console *that, PVM pVM, PCRTUUID aUuid);
#endif

    static DECLCALLBACK(int) attachStorageDevice(Console *pThis,
                                                 PVM pVM,
                                                 const char *pcszDevice,
                                                 unsigned uInstance,
                                                 StorageBus_T enmBus,
                                                 bool fUseHostIOCache,
                                                 IMediumAttachment *aMediumAtt);
    static DECLCALLBACK(int) detachStorageDevice(Console *pThis,
                                                 PVM pVM,
                                                 const char *pcszDevice,
                                                 unsigned uInstance,
                                                 StorageBus_T enmBus,
                                                 IMediumAttachment *aMediumAtt);
    HRESULT doStorageDeviceAttach(IMediumAttachment *aMediumAttachment, PVM pVM);
    HRESULT doStorageDeviceDetach(IMediumAttachment *aMediumAttachment, PVM pVM);

    static DECLCALLBACK(int)    fntTakeSnapshotWorker(RTTHREAD Thread, void *pvUser);

    static DECLCALLBACK(int)    stateProgressCallback(PVM pVM, unsigned uPercent, void *pvUser);

    static DECLCALLBACK(void)   genericVMSetErrorCallback(PVM pVM, void *pvUser, int rc, RT_SRC_POS_DECL,
                                                          const char *pszErrorFmt, va_list va);

    static void                 setVMRuntimeErrorCallbackF(PVM pVM, void *pvUser, uint32_t fFatal,
                                                          const char *pszErrorId, const char *pszFormat, ...);
    static DECLCALLBACK(void)   setVMRuntimeErrorCallback(PVM pVM, void *pvUser, uint32_t fFatal,
                                                          const char *pszErrorId, const char *pszFormat, va_list va);

    HRESULT                     captureUSBDevices(PVM pVM);
    void                        detachAllUSBDevices(bool aDone);

    static DECLCALLBACK(int)   powerUpThread(RTTHREAD Thread, void *pvUser);
    static DECLCALLBACK(int)   saveStateThread(RTTHREAD Thread, void *pvUser);
    static DECLCALLBACK(int)   powerDownThread(RTTHREAD Thread, void *pvUser);

    static DECLCALLBACK(int)    vmm2User_SaveState(PCVMM2USERMETHODS pThis, PUVM pUVM);
    static DECLCALLBACK(void)   vmm2User_NotifyEmtInit(PCVMM2USERMETHODS pThis, PUVM pUVM, PUVMCPU pUVCpu);
    static DECLCALLBACK(void)   vmm2User_NotifyEmtTerm(PCVMM2USERMETHODS pThis, PUVM pUVM, PUVMCPU pUVCpu);
    static DECLCALLBACK(void)   vmm2User_NotifyPdmtInit(PCVMM2USERMETHODS pThis, PUVM pUVM);
    static DECLCALLBACK(void)   vmm2User_NotifyPdmtTerm(PCVMM2USERMETHODS pThis, PUVM pUVM);

    static DECLCALLBACK(void *) drvStatus_QueryInterface(PPDMIBASE pInterface, const char *pszIID);
    static DECLCALLBACK(void)   drvStatus_UnitChanged(PPDMILEDCONNECTORS pInterface, unsigned iLUN);
    static DECLCALLBACK(int)    drvStatus_MediumEjected(PPDMIMEDIANOTIFY pInterface, unsigned iLUN);
    static DECLCALLBACK(void)   drvStatus_Destruct(PPDMDRVINS pDrvIns);
    static DECLCALLBACK(int)    drvStatus_Construct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags);

    int mcAudioRefs;
    volatile uint32_t mcVRDPClients;
    uint32_t mu32SingleRDPClientId; /* The id of a connected client in the single connection mode. */
    volatile  bool mcGuestCredentialsProvided;

    static const char *sSSMConsoleUnit;
    static uint32_t sSSMConsoleVer;

    HRESULT loadDataFromSavedState();
    int loadStateFileExecInternal(PSSMHANDLE pSSM, uint32_t u32Version);

    static DECLCALLBACK(void)   saveStateFileExec(PSSMHANDLE pSSM, void *pvUser);
    static DECLCALLBACK(int)    loadStateFileExec(PSSMHANDLE pSSM, void *pvUser, uint32_t uVersion, uint32_t uPass);

#ifdef VBOX_WITH_GUEST_PROPS
    static DECLCALLBACK(int)    doGuestPropNotification(void *pvExtension, uint32_t, void *pvParms, uint32_t cbParms);
    HRESULT                     doEnumerateGuestProperties(CBSTR aPatterns,
                                                           ComSafeArrayOut(BSTR, aNames),
                                                           ComSafeArrayOut(BSTR, aValues),
                                                           ComSafeArrayOut(LONG64, aTimestamps),
                                                           ComSafeArrayOut(BSTR, aFlags));

    void guestPropertiesHandleVMReset(void);
    bool guestPropertiesVRDPEnabled(void);
    void guestPropertiesVRDPUpdateLogon(uint32_t u32ClientId, const char *pszUser, const char *pszDomain);
    void guestPropertiesVRDPUpdateActiveClient(uint32_t u32ClientId);
    void guestPropertiesVRDPUpdateClientAttach(uint32_t u32ClientId, bool fAttached);
    void guestPropertiesVRDPUpdateNameChange(uint32_t u32ClientId, const char *pszName);
    void guestPropertiesVRDPUpdateDisconnect(uint32_t u32ClientId);
#endif

    /** @name Teleporter support
     * @{ */
    static DECLCALLBACK(int)    teleporterSrcThreadWrapper(RTTHREAD hThread, void *pvUser);
    HRESULT                     teleporterSrc(TeleporterStateSrc *pState);
    HRESULT                     teleporterSrcReadACK(TeleporterStateSrc *pState, const char *pszWhich, const char *pszNAckMsg = NULL);
    HRESULT                     teleporterSrcSubmitCommand(TeleporterStateSrc *pState, const char *pszCommand, bool fWaitForAck = true);
    HRESULT                     teleporterTrg(PUVM pUVM, IMachine *pMachine, Utf8Str *pErrorMsg, bool fStartPaused,
                                              Progress *pProgress, bool *pfPowerOffOnFailure);
    static DECLCALLBACK(int)    teleporterTrgServeConnection(RTSOCKET Sock, void *pvUser);
    /** @} */

    bool mSavedStateDataLoaded : 1;

    const ComPtr<IMachine> mMachine;
    const ComPtr<IInternalMachineControl> mControl;

    const ComPtr<IVRDEServer> mVRDEServer;

    ConsoleVRDPServer * const mConsoleVRDPServer;

    const ComObjPtr<Guest> mGuest;
    const ComObjPtr<Keyboard> mKeyboard;
    const ComObjPtr<Mouse> mMouse;
    const ComObjPtr<Display> mDisplay;
    const ComObjPtr<MachineDebugger> mDebugger;
    const ComObjPtr<VRDEServerInfo> mVRDEServerInfo;
    /** This can safely be used without holding any locks.
     * An AutoCaller suffices to prevent it being destroy while in use and
     * internally there is a lock providing the necessary serialization. */
    const ComObjPtr<EventSource> mEventSource;
#ifdef VBOX_WITH_EXTPACK
    const ComObjPtr<ExtPackManager> mptrExtPackManager;
#endif

    USBDeviceList mUSBDevices;
    RemoteUSBDeviceList mRemoteUSBDevices;

    SharedFolderDataMap m_mapGlobalSharedFolders;
    SharedFolderDataMap m_mapMachineSharedFolders;
    SharedFolderMap m_mapSharedFolders;             // the console instances

    /** The user mode VM handle. */
    PUVM mpUVM;
    /** Holds the number of "readonly" mpVM callers (users) */
    uint32_t mVMCallers;
    /** Semaphore posted when the number of mpVM callers drops to zero */
    RTSEMEVENT mVMZeroCallersSem;
    /** true when Console has entered the mpVM destruction phase */
    bool mVMDestroying : 1;
    /** true when power down is initiated by vmstateChangeCallback (EMT) */
    bool mVMPoweredOff : 1;
    /** true when vmstateChangeCallback shouldn't initiate a power down.  */
    bool mVMIsAlreadyPoweringOff : 1;
    /** true if we already showed the snapshot folder size warning. */
    bool mfSnapshotFolderSizeWarningShown : 1;
    /** true if we already showed the snapshot folder ext4/xfs bug warning. */
    bool mfSnapshotFolderExt4WarningShown : 1;
    /** true if we already listed the disk type of the snapshot folder. */
    bool mfSnapshotFolderDiskTypeShown : 1;

    /** Pointer to the VMM -> User (that's us) callbacks. */
    struct MYVMM2USERMETHODS : public VMM2USERMETHODS
    {
        Console *pConsole;
    } *mpVmm2UserMethods;

    /** The current network attachment type in the VM.
     * This doesn't have to match the network attachment type maintained in the
     * NetworkAdapter. This is needed to change the network attachment
     * dynamically.
     */
    typedef std::vector<NetworkAttachmentType_T> NetworkAttachmentTypeVector;
    NetworkAttachmentTypeVector meAttachmentType;

    VMMDev * m_pVMMDev;
    AudioSniffer * const mAudioSniffer;
#ifdef VBOX_WITH_USB_VIDEO
    UsbWebcamInterface * const mUsbWebcamInterface;
#endif
#ifdef VBOX_WITH_USB_CARDREADER
    UsbCardReader * const mUsbCardReader;
#endif
    BusAssignmentManager* mBusMgr;

    enum
    {
        iLedFloppy  = 0,
        cLedFloppy  = 2,
        iLedIde     = iLedFloppy + cLedFloppy,
        cLedIde     = 4,
        iLedSata    = iLedIde + cLedIde,
        cLedSata    = 30,
        iLedScsi    = iLedSata + cLedSata,
        cLedScsi    = 16,
        iLedSas     = iLedScsi + cLedScsi,
        cLedSas     = 8,
        cLedStorage = cLedFloppy + cLedIde + cLedSata + cLedScsi + cLedSas
    };
    DeviceType_T maStorageDevType[cLedStorage];
    PPDMLED      mapStorageLeds[cLedStorage];
    PPDMLED      mapNetworkLeds[36];    /**< @todo adapt this to the maximum network card count */
    PPDMLED      mapSharedFolderLed;
    PPDMLED      mapUSBLed[2];

    MediumAttachmentMap mapMediumAttachments;

/* Note: FreeBSD needs this whether netflt is used or not. */
#if ((defined(RT_OS_LINUX) && !defined(VBOX_WITH_NETFLT)) || defined(RT_OS_FREEBSD))
    Utf8Str      maTAPDeviceName[8];
    RTFILE       maTapFD[8];
#endif

    bool mVMStateChangeCallbackDisabled;

    bool mfUseHostClipboard;

    /** Local machine state value. */
    MachineState_T mMachineState;

    /** Pointer to the progress object of a live cancelable task.
     *
     * This is currently only used by Console::Teleport(), but is intended to later
     * be used by the live snapshot code path as well.  Actions like
     * Console::PowerDown, which automatically cancels out the running snapshot /
     * teleportation operation, will cancel the teleportation / live snapshot
     * operation before starting. */
    ComObjPtr<Progress> mptrCancelableProgress;

    /* The purpose of caching of some events is probably in order to
       automatically fire them at new event listeners.  However, there is no
       (longer?) any code making use of this... */
#ifdef CONSOLE_WITH_EVENT_CACHE
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
            com::SafeArray<BYTE> shape;
        } mpsc;

        /** OnMouseCapabilityChange() cache */
        struct
        {
            bool valid;
            BOOL supportsAbsolute;
            BOOL supportsRelative;
            BOOL needsHostCursor;
        } mcc;

        /** OnKeyboardLedsChange() cache */
        struct
        {
            bool valid;
            bool numLock;
            bool capsLock;
            bool scrollLock;
        } klc;

        void clear()
        {
            RT_ZERO(mcc);
            RT_ZERO(klc);

            /* We cannot RT_ZERO mpsc because of shape's vtable. */
            mpsc.shape.setNull();
            mpsc.valid = mpsc.visible = mpsc.alpha = false;
            mpsc.xHot = mpsc.yHot = mpsc.width = mpsc.height = 0;
        }
    } mCallbackData;
#endif
    ComPtr<IEventListener> mVmListener;

    friend struct VMTask;
};

#endif // !____H_CONSOLEIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
