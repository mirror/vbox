/* $Id$ */
/** @file
 * VirtualBox IHostUSBDevice COM interface implementation.
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

#ifndef ____H_HOSTUSBDEVICEIMPL
#define ____H_HOSTUSBDEVICEIMPL

#include "VirtualBoxBase.h"
#include "USBDeviceFilterImpl.h"
/* #include "USBProxyService.h" circular on Host/HostUSBDevice, the includer
 * must include this. */

#include <VBox/usb.h>
#include "Logging.h"

class SessionMachine;
class USBProxyService;

/**
 * The unified state machine of HostUSBDevice.
 *
 * This is a super set of USBDEVICESTATE / USBDeviceState_T that
 * includes additional states for tracking state transitions.
 *
 * @remarks
 *  The CapturingForVM and CapturingForProxy states have been merged
 *  into Capturing with a destination state (AttachingToVM or HeldByProxy).
 *
 *  The DetachingFromVM state is a merge of DetachingFromVMToProxy and
 *  DetachingFromVMToHost and uses the destination state (HeldByProxy
 *  or ReleasingToHost) like Capturing.
 *
 *  The *AwaitingDetach and *AwaitingReattach substates (optionally used
 *  in Capturing, AttachingToVM, DetachingFromVM and ReleasingToHost) are
 *  implemented via a substate kHostUSBDeviceSubState.
 */
typedef enum
{
    /** The device is unsupported (HUB).
     * Next Host: PhysDetached.
     * Next VBox: No change permitted.
     */
    kHostUSBDeviceState_Unsupported = USBDEVICESTATE_UNSUPPORTED,
    /** The device is used exclusivly by the host or is inaccessible for some other reason.
     * Next Host: Capturable, Unused, PhysDetached.
     *            Run filters.
     * Next VBox: No change permitted.
     */
    kHostUSBDeviceState_UsedByHost = USBDEVICESTATE_USED_BY_HOST,
    /** The device is used by the host but can be captured.
     * Next Host: Unsupported, UsedByHost, Unused, PhysDetached.
     *            Run filters if Unused (for wildcard filters).
     * Next VBox: CapturingForVM, CapturingForProxy.
     */
    kHostUSBDeviceState_Capturable = USBDEVICESTATE_USED_BY_HOST_CAPTURABLE,
    /** The device is not used by the host and can be captured.
     * Next Host: UsedByHost, Capturable, PhysDetached
     *            Don't run any filters (done on state entry).
     * Next VBox: CapturingForVM, CapturingForProxy.
     */
    kHostUSBDeviceState_Unused = USBDEVICESTATE_UNUSED,
    /** The device is held captive by the proxy.
     * Next Host: PhysDetached
     * Next VBox: ReleasingHeld, AttachingToVM
     */
    kHostUSBDeviceState_HeldByProxy = USBDEVICESTATE_HELD_BY_PROXY,
    /** The device is in use by a VM.
     * Next Host: PhysDetachingFromVM
     * Next VBox: DetachingFromVM
     */
    kHostUSBDeviceState_UsedByVM = USBDEVICESTATE_USED_BY_GUEST,
    /** The device has been detach from both the host and VMs.
     * This is the final state. */
    kHostUSBDeviceState_PhysDetached = 9,


    /** The start of the transitional states. */
    kHostUSBDeviceState_FirstTransitional,

    /** The device is being seized from the host, either for HeldByProxy or for AttachToVM.
     *
     * On some hosts we will need to re-enumerate the in which case the sub-state
     * is employed to track this progress. On others, this is synchronous or faked, and
     * will will then leave the device in this state and poke the service thread to do
     * the completion state change.
     *
     * Next Host: PhysDetached.
     * Next VBox: HeldByProxy or AttachingToVM on success,
     *            previous state (Unused or Capturable) or UsedByHost on failure.
     */
    kHostUSBDeviceState_Capturing = kHostUSBDeviceState_FirstTransitional,

    /** The device is being released back to the host, following VM or Proxy usage.
     * Most hosts needs to re-enumerate the device and will therefore employ the
     * sub-state as during capturing. On the others we'll just leave it to the usb
     * service thread to advance the device state.
     *
     * Next Host: Unused, UsedByHost, Capturable.
     *            No filters.
     * Next VBox: PhysDetached (timeout), HeldByProxy (failure).
     */
    kHostUSBDeviceState_ReleasingToHost,

    /** The device is being attached to a VM.
     *
     * This requires IPC to the VM and we will not advance the state until
     * that completes.
     *
     * Next Host: PhysDetachingFromVM.
     * Next VBox: UsedByGuest, HeldByProxy (failure).
     */
    kHostUSBDeviceState_AttachingToVM,

    /** The device is being detached from a VM and will be returned to the proxy or host.
     *
     * This involves IPC and may or may not also require re-enumeration of the
     * device. Which means that it might transition directly into the ReleasingToHost state
     * because the client (VM) will do the actual re-enumeration.
     *
     * Next Host: PhysDetachingFromVM (?) or just PhysDetached.
     * Next VBox: ReleasingToHost, HeldByProxy.
     */
    kHostUSBDeviceState_DetachingFromVM,

    /** The device has been physically removed while a VM used it.
     *
     * This is the device state while VBoxSVC is doing IPC to the client (VM) telling it
     * to detach it.
     *
     * Next Host: None.
     * Next VBox: PhysDetached
     */
    kHostUSBDeviceState_PhysDetachingFromVM,

    /** Just an invalid state value for use as default for some methods. */
    kHostUSBDeviceState_Invalid = 0x7fff
} HostUSBDeviceState;


/**
 * Sub-state for dealing with device re-enumeration.
 */
typedef enum
{
    /** Not in any sub-state. */
    kHostUSBDeviceSubState_Default = 0,
    /** Awaiting a logical device detach following a device re-enumeration. */
    kHostUSBDeviceSubState_AwaitingDetach,
    /** Awaiting a logical device re-attach following a device re-enumeration. */
    kHostUSBDeviceSubState_AwaitingReAttach
} HostUSBDeviceSubState;


/**
 * Object class used to hold Host USB Device properties.
 */
class ATL_NO_VTABLE HostUSBDevice :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IHostUSBDevice)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(HostUSBDevice, IHostUSBDevice)

    DECLARE_NOT_AGGREGATABLE(HostUSBDevice)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(HostUSBDevice)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IHostUSBDevice)
        COM_INTERFACE_ENTRY(IUSBDevice)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (HostUSBDevice)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(PUSBDEVICE aUsb, USBProxyService *aUSBProxyService);
    void uninit();

    // IUSBDevice properties
    STDMETHOD(COMGETTER(Id))(BSTR *aId);
    STDMETHOD(COMGETTER(VendorId))(USHORT *aVendorId);
    STDMETHOD(COMGETTER(ProductId))(USHORT *aProductId);
    STDMETHOD(COMGETTER(Revision))(USHORT *aRevision);
    STDMETHOD(COMGETTER(Manufacturer))(BSTR *aManufacturer);
    STDMETHOD(COMGETTER(Product))(BSTR *aProduct);
    STDMETHOD(COMGETTER(SerialNumber))(BSTR *aSerialNumber);
    STDMETHOD(COMGETTER(Address))(BSTR *aAddress);
    STDMETHOD(COMGETTER(Port))(USHORT *aPort);
    STDMETHOD(COMGETTER(Version))(USHORT *aVersion);
    STDMETHOD(COMGETTER(PortVersion))(USHORT *aPortVersion);
    STDMETHOD(COMGETTER(Remote))(BOOL *aRemote);

    // IHostUSBDevice properties
    STDMETHOD(COMGETTER(State))(USBDeviceState_T *aState);

    // public methods only for internal purposes

    /** @note Must be called from under the object read lock. */
    const Guid& getId() const { return mId; }

    /** @note Must be called from under the object read lock. */
    HostUSBDeviceState getUnistate() const { return mUniState; }

    /** @note Must be called from under the object read lock. */
    const char *getStateName() { return stateName (mUniState, mPendingUniState, mUniSubState); }

    /** @note Must be called from under the object read lock. */
    bool isCapturableOrHeld()
    {
        return mUniState == kHostUSBDeviceState_Unused
            || mUniState == kHostUSBDeviceState_Capturable
            || mUniState == kHostUSBDeviceState_HeldByProxy;
    }

    /** @note Must be called from under the object read lock. */
    ComObjPtr<SessionMachine> &getMachine() { return mMachine; }

    /** @note Must be called from under the object read lock. */
    PCUSBDEVICE getUsbData() const { return mUsb; }

    Utf8Str getName();

    HRESULT requestCaptureForVM(SessionMachine *aMachine, bool aSetError, ULONG aMaskedIfs = 0);
    HRESULT onDetachFromVM(SessionMachine *aMachine, bool aDone, bool *aRunFilters, bool aAbnormal = false);
    HRESULT requestReleaseToHost();
    HRESULT requestHold();
    bool wasActuallyDetached();
    void onPhysicalDetached();

    bool isMatch(const USBDeviceFilter::Data &aData);
    int compare(PCUSBDEVICE aDev2);
    static int compare(PCUSBDEVICE aDev1, PCUSBDEVICE aDev2, bool aIsAwaitingReAttach = false);

    bool updateState(PCUSBDEVICE aDev, bool *aRunFilters, SessionMachine **aIgnoreMachine);
    bool updateStateFake(PCUSBDEVICE aDev, bool *aRunFilters, SessionMachine **aIgnoreMachine);

    static const char *stateName(HostUSBDeviceState aState,
                                 HostUSBDeviceState aPendingState = kHostUSBDeviceState_Invalid,
                                 HostUSBDeviceSubState aSubState = kHostUSBDeviceSubState_Default);

protected:
    HRESULT attachToVM(SessionMachine *aMachine, ULONG aMaskedIfs = 0);
    void detachFromVM(HostUSBDeviceState aFinalState);
    void onPhysicalDetachedInternal();
    bool hasAsyncOperationTimedOut() const;

    bool setState (HostUSBDeviceState aNewState, HostUSBDeviceState aNewPendingState = kHostUSBDeviceState_Invalid,
                   HostUSBDeviceSubState aNewSubState = kHostUSBDeviceSubState_Default);
    bool startTransition (HostUSBDeviceState aNewState, HostUSBDeviceState aFinalState,
                          HostUSBDeviceSubState aNewSubState = kHostUSBDeviceSubState_Default);
    bool advanceTransition(bool aSkipReAttach = false);
    bool failTransition();
    USBDeviceState_T canonicalState() const;

private:

    const Guid mId;

    /** @name The state machine variables
     * Only setState(), init() and uninit() will modify these members!
     * @{ */
    /** The RTTimeNanoTS() corresponding to the last state change.
     *
     * Old state machine: RTTimeNanoTS() of when mIsStatePending was set or mDetaching changed
     * from kNotDetaching. For operations that cannot be canceled it's 0. */
    uint64_t mLastStateChangeTS;
    /** Current state. */
    HostUSBDeviceState mUniState;
    /** Sub-state for tracking re-enumeration. */
    HostUSBDeviceSubState mUniSubState;
    /** The final state of an pending transition.
     * This is mainly a measure to reduce the number of HostUSBDeviceState values. */
    HostUSBDeviceState mPendingUniState;
    /** Previous state.
     * This is used for bailing out when a transition like capture fails. */
    HostUSBDeviceState mPrevUniState;
    /** Indicator set by onDetachedPhys and check when advancing a transitional state. */
    bool mIsPhysicallyDetached;
    /** @} */

    /** The machine the usb device is (being) attached to. */
    ComObjPtr<SessionMachine> mMachine;
    /** Pointer to the USB Proxy Service instance. */
    USBProxyService *mUSBProxyService;
    /** Pointer to the USB Device structure owned by this device.
     * Only used for host devices. */
    PUSBDEVICE mUsb;
    /** The interface mask to be used in the pending capture.
     * This is a filter property. */
    ULONG mMaskedIfs;
    /** The name of this device. */
    Utf8Str mNameObj;
    /** The name of this device (for logging purposes).
     * This points to the string in mNameObj. */
    const char *mName;

    friend class USBProxyService;
#ifdef RT_OS_SOLARIS
    friend class USBProxyServiceSolaris;

    /** One-shot filter id only for new code */
    void *mOneShotId;
#endif
#ifdef RT_OS_LINUX
    friend class USBProxyServiceLinux;
#endif
#ifdef RT_OS_DARWIN
    /** One-shot filter id. */
    void *mOneShotId;

    friend class USBProxyServiceDarwin;
#endif
#ifdef RT_OS_FreeBSD
    friend class USBProxyServiceFreeBSD;
#endif
};

#endif // ____H_HOSTUSBDEVICEIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
