/** @file
 * PDM - Pluggable Device Manager, USB Devices.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_pdmusb_h
#define ___VBox_pdmusb_h

#include <VBox/pdmqueue.h>
#include <VBox/pdmcritsect.h>
#include <VBox/pdmthread.h>
#include <VBox/pdmifs.h>
#include <VBox/tm.h>
#include <VBox/ssm.h>
#include <VBox/cfgm.h>
#include <VBox/dbgf.h>
#include <VBox/mm.h>
#include <VBox/err.h>
#include <VBox/vusb.h>
#include <iprt/stdarg.h>

__BEGIN_DECLS

/** @defgroup grp_pdm_usbdev    USB Devices
 * @ingroup grp_pdm
 * @{
 */

/**
 * USB descriptor cache.
 *
 * This structure is owned by the USB device but provided to the PDM/VUSB layer
 * thru the PDMUSBREG::pfnGetDescriptorCache method. PDM/VUSB will use the
 * information here to map addresses to endpoints, perform SET_CONFIGURATION
 * requests, and optionally perform GET_DESCRIPTOR requests (see flag).
 *
 * Currently, only device and configuration descriptors are cached.
 */
typedef struct PDMUSBDESCCACHE
{
    /** USB device descriptor */
    PCVUSBDESCDEVICE    pDevice;
    /** USB Descriptor arrays (pDev->bNumConfigurations) */
    PCVUSBDESCCONFIGEX  paConfigs;
    /** Use the cached descriptors for GET_DESCRIPTOR requests. */
    bool                fUseCachedDescriptors;
} PDMUSBDESCCACHE;
/** Pointer to an USB descriptor cache. */
typedef PDMUSBDESCCACHE *PPDMUSBDESCCACHE;
/** Pointer to a const USB descriptor cache. */
typedef const PDMUSBDESCCACHE *PCPDMUSBDESCCACHE;



/** PDM USB Device Registration Structure,
 *
 * This structure is used when registering a device from VBoxUsbRegister() in HC Ring-3.
 * The PDM will make use of this structure untill the VM is destroyed.
 */
typedef struct PDMUSBREG
{
    /** Structure version. PDM_DEVREG_VERSION defines the current version. */
    uint32_t            u32Version;
    /** Device name. */
    char                szDeviceName[32];
    /** The description of the device. The UTF-8 string pointed to shall, like this structure,
     * remain unchanged from registration till VM destruction. */
    const char         *pszDescription;

    /** Flags, combination of the PDM_USBREG_FLAGS_* \#defines. */
    RTUINT              fFlags;
    /** Maximum number of instances (per VM). */
    RTUINT              cMaxInstances;
    /** Size of the instance data. */
    RTUINT              cbInstance;


    /**
     * Construct an USB device instance for a VM.
     *
     * @returns VBox status.
     * @param   pUsbIns     The USB device instance data.
     *                      If the registration structure is needed, pUsbDev->pDevReg points to it.
     * @param   iInstance   Instance number. Use this to figure out which registers and such to use.
     *                      The instance number is also found in pUsbDev->iInstance, but since it's
     *                      likely to be freqently used PDM passes it as parameter.
     * @param   pCfg        Configuration node handle for the device. Use this to obtain the configuration
     *                      of the device instance. It's also found in pUsbDev->pCfg, but since it's
     *                      primary usage will in this function it's passed as a parameter.
     * @param   pCfgGlobal  Handle to the global device configuration. Also found in pUsbDev->pCfgGlobal.
     * @remarks This callback is required.
     */
    DECLR3CALLBACKMEMBER(int, pfnConstruct,(PPDMUSBINS pUsbIns, int iInstance, PCFGMNODE pCfg, PCFGMNODE pCfgGlobal));

    /**
     * Destruct an USB device instance.
     *
     * Most VM resources are freed by the VM. This callback is provided so that any non-VM
     * resources can be freed correctly.
     *
     * This method will be called regardless of the pfnConstruc result to avoid
     * complicated failure paths.
     *
     * @param   pUsbIns     The USB device instance data.
     * @remarks Optional.
     */
    DECLR3CALLBACKMEMBER(void, pfnDestruct,(PPDMUSBINS pUsbIns));


    /**
     * Init complete notification.
     *
     * This can be done to do communication with other devices and other
     * initialization which requires everything to be in place.
     *
     * @returns VBOX status code.
     * @param   pUsbIns     The USB device instance data.
     * @remarks Optional.
     * @remarks Not called when hotplugged.
     */
    DECLR3CALLBACKMEMBER(int, pfnVMInitComplete,(PPDMUSBINS pUsbIns));

    /**
     * VM Power On notification.
     *
     * @returns VBox status.
     * @param   pUsbIns     The USB device instance data.
     * @remarks Optional.
     */
    DECLR3CALLBACKMEMBER(void, pfnVMPowerOn,(PPDMUSBINS pUsbIns));

    /**
     * VM Reset notification.
     *
     * @returns VBox status.
     * @param   pUsbIns     The USB device instance data.
     * @remarks Optional.
     */
    DECLR3CALLBACKMEMBER(void, pfnVMReset,(PPDMUSBINS pUsbIns));

    /**
     * VM Suspend notification.
     *
     * @returns VBox status.
     * @param   pUsbIns     The USB device instance data.
     * @remarks Optional.
     */
    DECLR3CALLBACKMEMBER(void, pfnVMSuspend,(PPDMUSBINS pUsbIns));

    /**
     * VM Resume notification.
     *
     * @returns VBox status.
     * @param   pUsbIns     The USB device instance data.
     * @remarks Optional.
     */
    DECLR3CALLBACKMEMBER(void, pfnVMResume,(PPDMUSBINS pUsbIns));

    /**
     * VM Power Off notification.
     *
     * @param   pUsbIns     The USB device instance data.
     */
    DECLR3CALLBACKMEMBER(void, pfnVMPowerOff,(PPDMUSBINS pUsbIns));

    /**
     * Called after the constructor when attaching a device at run time.
     *
     * This can be used to do tasks normally assigned to pfnInitComplete and/or pfnVMPowerOn.
     *
     * @returns VBox status.
     * @param   pUsbIns     The USB device instance data.
     * @remarks Optional.
     */
    DECLR3CALLBACKMEMBER(void, pfnHotPlugged,(PPDMUSBINS pUsbIns));

    /**
     * Called before the destructor when a device is unplugged at run time.
     *
     * This can be used to do tasks normally assigned to pfnVMSuspend and/or pfnVMPowerOff.
     *
     * @returns VBox status.
     * @param   pUsbIns     The USB device instance data.
     * @remarks Optional.
     */
    DECLR3CALLBACKMEMBER(void, pfnHotUnplugged,(PPDMUSBINS pUsbIns));
    /**
     * Driver Attach command.
     *
     * This is called to let the USB device attach to a driver for a specified LUN
     * at runtime. This is not called during VM construction, the device constructor
     * have to attach to all the available drivers.
     *
     * @returns VBox status code.
     * @param   pUsbIns     The USB device instance data.
     * @param   iLUN        The logical unit which is being detached.
     * @remarks Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnDriverAttach,(PPDMUSBINS pUsbIns, unsigned iLUN));

    /**
     * Driver Detach notification.
     *
     * This is called when a driver is detaching itself from a LUN of the device.
     * The device should adjust it's state to reflect this.
     *
     * @param   pUsbIns     The USB device instance data.
     * @param   iLUN        The logical unit which is being detached.
     * @remarks Optional.
     */
    DECLR3CALLBACKMEMBER(void, pfnDriverDetach,(PPDMUSBINS pUsbIns, unsigned iLUN));

    /**
     * Query the base interface of a logical unit.
     *
     * @returns VBOX status code.
     * @param   pUsbIns     The USB device instance data.
     * @param   iLUN        The logicial unit to query.
     * @param   ppBase      Where to store the pointer to the base interface of the LUN.
     * @remarks Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryInterface,(PPDMUSBINS pUsbIns, unsigned iLUN, PPDMIBASE *ppBase));

    /**
     * Requests the USB device to reset.
     *
     * @returns VBox status code.
     * @param   pUsbIns         The USB device instance.
     * @param   fResetOnLinux   A hint to the usb proxy.
     *                          Don't use this unless you're the linux proxy device.
     * @thread  Any thread.
     * @remarks Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnUsbReset,(PPDMUSBINS pUsbIns, bool fResetOnLinux));

    /**
     * Query device and configuration descriptors for the caching and servicing
     * relevant GET_DESCRIPTOR requests.
     *
     * @returns Pointer to the descriptor cache (read-only).
     * @param   pUsbIns             The USB device instance.
     * @remarks Mandatory.
     */
    DECLR3CALLBACKMEMBER(PCPDMUSBDESCCACHE, pfnUsbGetDescriptorCache,(PPDMUSBINS pUsbIns));

    /**
     * SET_CONFIGURATION request.
     *
     * @returns VBox status code.
     * @param   pUsbIns             The USB device instance.
     * @param   bConfigurationValue The bConfigurationValue of the new configuration.
     * @param   pvOldCfgDesc        Internal - for the device proxy.
     * @param   pvOldIfState        Internal - for the device proxy.
     * @param   pvNewCfgDesc        Internal - for the device proxy.
     * @remarks Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnUsbSetConfiguration,(PPDMUSBINS pUsbIns, uint8_t bConfigurationValue,
                                                      const void *pvOldCfgDesc, const void *pvOldIfState, const void *pvNewCfgDesc));

    /**
     * SET_INTERFACE request.
     *
     * @returns VBox status code.
     * @param   pUsbIns             The USB device instance.
     * @param   bInterfaceNumber    The interface number.
     * @param   bAlternateSetting   The alternate setting.
     * @remarks Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnUsbSetInterface,(PPDMUSBINS pUsbIns, uint8_t bInterfaceNumber, uint8_t bAlternateSetting));

    /**
     * Clears the halted state of an endpoint. (Optional)
     *
     * This called when VUSB sees a CLEAR_FEATURE(ENDPOINT_HALT) on request
     * on the zero pipe.
     *
     * @returns VBox status code.
     * @param   pUsbIns             The USB device instance.
     * @param   uEndpoint           The endpoint to clear.
     * @remarks Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnUsbClearHaltedEndpoint,(PPDMUSBINS pUsbIns, unsigned uEndpoint));

    /**
     * Allocates an URB.
     *
     * This can be used to make use of shared user/kernel mode buffers.
     *
     * @returns VBox status code.
     * @param   pUsbIns             The USB device instance.
     * @param   cbData              The size of the data buffer.
     * @param   cTds                The number of TDs.
     * @param   enmType             The type of URB.
     * @param   ppUrb               Where to store the allocated URB.
     * @remarks Optional.
     * @remarks Not implemented yet.
     */
    DECLR3CALLBACKMEMBER(int, pfnUrbNew,(PPDMUSBINS pUsbIns, size_t cbData, size_t cTds, VUSBXFERTYPE enmType, PVUSBURB *ppUrb));

    /**
     * Queues an URB for processing.
     *
     * @returns VBox status code.
     * @retval  VINF_SUCCESS on success.
     * @retval  VERR_VUSB_DEVICE_NOT_ATTACHED if the device has been disconnected.
     * @retval  VERR_VUSB_FAILED_TO_QUEUE_URB as a general failure kind of thing.
     * @retval  TBD - document new stuff!
     *
     * @param   pUsbIns             The USB device instance.
     * @param   pUrb                The URB to process.
     * @remarks Mandatory.
     */
    DECLR3CALLBACKMEMBER(int, pfnUrbQueue,(PPDMUSBINS pUsbIns, PVUSBURB pUrb));

    /**
     * Cancels an URB.
     *
     * @returns VBox status code.
     * @param   pUsbIns             The USB device instance.
     * @param   pUrb                The URB to cancel.
     * @remarks Mandatory.
     */
    DECLR3CALLBACKMEMBER(int, pfnUrbCancel,(PPDMUSBINS pUsbIns, PVUSBURB pUrb));

    /**
     * Reaps an URB.
     *
     * @returns A ripe URB, NULL if none.
     * @param   pUsbIns             The USB device instance.
     * @param   cMillies            How log to wait for an URB to become ripe.
     * @remarks Mandatory.
     */
    DECLR3CALLBACKMEMBER(PVUSBURB, pfnUrbReap,(PPDMUSBINS pUsbIns, unsigned cMillies));


    /** Just some init precaution. Must be set to PDM_USBREG_VERSION. */
    uint32_t            u32TheEnd;
} PDMUSBREG;
/** Pointer to a PDM USB Device Structure. */
typedef PDMUSBREG *PPDMUSBREG;
/** Const pointer to a PDM USB Device Structure. */
typedef PDMUSBREG const *PCPDMUSBREG;

/** Current USBREG version number. */
#define PDM_USBREG_VERSION  0xed010000

/** PDM USB Device Flags.
 * @{ */
/* none yet */
/** @} */

#ifdef IN_RING3
/**
 * PDM USB Device API.
 */
typedef struct PDMUSBHLP
{
    /** Structure version. PDM_USBHLP_VERSION defines the current version. */
    uint32_t                    u32Version;

    /**
     * Attaches a driver (chain) to the USB device.
     *
     * The first call for a LUN this will serve as a registartion of the LUN. The pBaseInterface and
     * the pszDesc string will be registered with that LUN and kept around for PDMR3QueryUSBDeviceLun().
     *
     * @returns VBox status code.
     * @param   pUsbIns             The USB device instance.
     * @param   iLun                The logical unit to attach.
     * @param   pBaseInterface      Pointer to the base interface for that LUN. (device side / down)
     * @param   ppBaseInterface     Where to store the pointer to the base interface. (driver side / up)
     * @param   pszDesc             Pointer to a string describing the LUN. This string must remain valid
     *                              for the live of the device instance.
     */
    DECLR3CALLBACKMEMBER(int, pfnDriverAttach,(PPDMUSBINS pUsbIns, RTUINT iLun, PPDMIBASE pBaseInterface, PPDMIBASE *ppBaseInterface, const char *pszDesc));

    /**
     * Assert that the current thread is the emulation thread.
     *
     * @returns True if correct.
     * @returns False if wrong.
     * @param   pUsbIns             The USB device instance.
     * @param   pszFile             Filename of the assertion location.
     * @param   iLine               Linenumber of the assertion location.
     * @param   pszFunction         Function of the assertion location.
     */
    DECLR3CALLBACKMEMBER(bool, pfnAssertEMT,(PPDMUSBINS pUsbIns, const char *pszFile, unsigned iLine, const char *pszFunction));

    /**
     * Assert that the current thread is NOT the emulation thread.
     *
     * @returns True if correct.
     * @returns False if wrong.
     * @param   pUsbIns             The USB device instance.
     * @param   pszFile             Filename of the assertion location.
     * @param   iLine               Linenumber of the assertion location.
     * @param   pszFunction         Function of the assertion location.
     */
    DECLR3CALLBACKMEMBER(bool, pfnAssertOther,(PPDMUSBINS pUsbIns, const char *pszFile, unsigned iLine, const char *pszFunction));

    /**
     * Stops the VM and enters the debugger to look at the guest state.
     *
     * Use the PDMUsbDBGFStop() inline function with the RT_SRC_POS macro instead of
     * invoking this function directly.
     *
     * @returns VBox status code which must be passed up to the VMM.
     * @param   pUsbIns             The USB device instance.
     * @param   pszFile             Filename of the assertion location.
     * @param   iLine               The linenumber of the assertion location.
     * @param   pszFunction         Function of the assertion location.
     * @param   pszFormat           Message. (optional)
     * @param   va                  Message parameters.
     */
    DECLR3CALLBACKMEMBER(int, pfnDBGFStopV,(PPDMUSBINS pUsbIns, const char *pszFile, unsigned iLine, const char *pszFunction, const char *pszFormat, va_list va));

    /**
     * Register a info handler with DBGF,
     *
     * @returns VBox status code.
     * @param   pUsbIns             The USB device instance.
     * @param   pszName             The identifier of the info.
     * @param   pszDesc             The description of the info and any arguments the handler may take.
     * @param   pfnHandler          The handler function to be called to display the info.
     */
/** @todo    DECLR3CALLBACKMEMBER(int, pfnDBGFInfoRegister,(PPDMUSBINS pUsbIns, const char *pszName, const char *pszDesc, PFNDBGFHANDLERUSB pfnHandler)); */

    /**
     * Allocate memory which is associated with current VM instance
     * and automatically freed on it's destruction.
     *
     * @returns Pointer to allocated memory. The memory is *NOT* zero-ed.
     * @param   pUsbIns             The USB device instance.
     * @param   cb                  Number of bytes to allocate.
     */
    DECLR3CALLBACKMEMBER(void *, pfnMMHeapAlloc,(PPDMUSBINS pUsbIns, size_t cb));

    /**
     * Allocate memory which is associated with current VM instance
     * and automatically freed on it's destruction. The memory is ZEROed.
     *
     * @returns Pointer to allocated memory. The memory is *NOT* zero-ed.
     * @param   pUsbIns             The USB device instance.
     * @param   cb                  Number of bytes to allocate.
     */
    DECLR3CALLBACKMEMBER(void *, pfnMMHeapAllocZ,(PPDMUSBINS pUsbIns, size_t cb));

    /**
     * Create a queue.
     *
     * @returns VBox status code.
     * @param   pUsbIns             The USB device instance.
     * @param   cbItem              Size a queue item.
     * @param   cItems              Number of items in the queue.
     * @param   cMilliesInterval    Number of milliseconds between polling the queue.
     *                              If 0 then the emulation thread will be notified whenever an item arrives.
     * @param   pfnCallback         The consumer function.
     * @param   ppQueue             Where to store the queue handle on success.
     * @thread  The emulation thread.
     */
/** @todo    DECLR3CALLBACKMEMBER(int, pfnPDMQueueCreate,(PPDMUSBINS pUsbIns, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval, PFNPDMQUEUEUSB pfnCallback, PPDMQUEUE *ppQueue)); */

    /**
     * Register a save state data unit.
     *
     * @returns VBox status.
     * @param   pUsbIns             The USB device instance.
     * @param   pszName         Data unit name.
     * @param   u32Instance     The instance identifier of the data unit.
     *                          This must together with the name be unique.
     * @param   u32Version      Data layout version number.
     * @param   cbGuess         The approximate amount of data in the unit.
     *                          Only for progress indicators.
     * @param   pfnSavePrep     Prepare save callback, optional.
     * @param   pfnSaveExec     Execute save callback, optional.
     * @param   pfnSaveDone     Done save callback, optional.
     * @param   pfnLoadPrep     Prepare load callback, optional.
     * @param   pfnLoadExec     Execute load callback, optional.
     * @param   pfnLoadDone     Done load callback, optional.
     */
/** @todo    DECLR3CALLBACKMEMBER(int, pfnSSMRegister,(PPDMUSBINS pUsbIns, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
                                              PFNSSMUSBSAVEPREP pfnSavePrep, PFNSSMUSBSAVEEXEC pfnSaveExec, PFNSSMUSBSAVEDONE pfnSaveDone,
                                              PFNSSMUSBLOADPREP pfnLoadPrep, PFNSSMUSBLOADEXEC pfnLoadExec, PFNSSMUSBLOADDONE pfnLoadDone)); */

    /**
     * Register a STAM sample.
     *
     * Use the PDMUsbHlpSTAMRegister wrapper.
     *
     * @returns VBox status.
     * @param   pUsbIns             The USB device instance.
     * @param   pvSample            Pointer to the sample.
     * @param   enmType             Sample type. This indicates what pvSample is pointing at.
     * @param   enmVisibility       Visibility type specifying whether unused statistics should be visible or not.
     * @param   enmUnit             Sample unit.
     * @param   pszDesc             Sample description.
     * @param   pszName             The sample name format string.
     * @param   va                  Arguments to the format string.
     */
    DECLR3CALLBACKMEMBER(void, pfnSTAMRegisterV,(PPDMUSBINS pUsbIns, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility,
                                                 STAMUNIT enmUnit, const char *pszDesc, const char *pszName, va_list va));

    /**
     * Creates a timer.
     *
     * @returns VBox status.
     * @param   pUsbIns             The USB device instance.
     * @param   enmClock            The clock to use on this timer.
     * @param   pfnCallback         Callback function.
     * @param   pszDesc             Pointer to description string which must stay around
     *                              until the timer is fully destroyed (i.e. a bit after TMTimerDestroy()).
     * @param   ppTimer             Where to store the timer on success.
     */
/** @todo    DECLR3CALLBACKMEMBER(int, pfnTMTimerCreate,(PPDMUSBINS pUsbIns, TMCLOCK enmClock, PFNTMTIMERUSB pfnCallback, const char *pszDesc, PPTMTIMERHC ppTimer)); */

    /**
     * Set the VM error message
     *
     * @returns rc.
     * @param   pUsbIns             The USB device instance.
     * @param   rc                  VBox status code.
     * @param   RT_SRC_POS_DECL     Use RT_SRC_POS.
     * @param   pszFormat           Error message format string.
     * @param   va                  Error message arguments.
     */
    DECLR3CALLBACKMEMBER(int, pfnVMSetErrorV,(PPDMUSBINS pUsbIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va));

    /**
     * Set the VM runtime error message
     *
     * @returns VBox status code.
     * @param   pUsbIns             The USB device instance.
     * @param   fFatal              Whether it is a fatal error or not.
     * @param   pszErrorID          Error ID string.
     * @param   pszFormat           Error message format string.
     * @param   va                  Error message arguments.
     */
    DECLR3CALLBACKMEMBER(int, pfnVMSetRuntimeErrorV,(PPDMUSBINS pUsbIns, bool fFatal, const char *pszErrorID, const char *pszFormat, va_list va));

    /** Just a safety precaution. */
    uint32_t                        u32TheEnd;
} PDMUSBHLP;
/** Pointer PDM USB Device API. */
typedef PDMUSBHLP *PPDMUSBHLP;
/** Pointer const PDM USB Device API. */
typedef const PDMUSBHLP *PCPDMUSBHLP;

/** Current USBHLP version number. */
#define PDM_USBHLP_VERSION  0xec020000

#endif /* IN_RING3 */

/**
 * PDM USB Device Instance.
 */
typedef struct PDMUSBINS
{
    /** Structure version. PDM_USBINS_VERSION defines the current version. */
    uint32_t                    u32Version;
    /** USB device instance number. */
    RTUINT                      iInstance;
    /** The base interface of the device.
     * The device constructor initializes this if it has any device level
     * interfaces to export. To obtain this interface call PDMR3QueryUSBDevice(). */
    PDMIBASE                    IBase;
#if HC_ARCH_BITS == 32
    uint32_t                    u32Alignment; /**< Alignment padding. */
#endif

    /** Internal data. */
    union
    {
#ifdef PDMUSBINSINT_DECLARED
        PDMUSBINSINT            s;
#endif
        uint8_t                 padding[HC_ARCH_BITS == 32 ? 64 : 96];
    } Internal;

    /** Pointer the PDM USB Device API. */
    R3PTRTYPE(PCPDMUSBHLP)      pUsbHlp;
    /** Pointer to the USB device registration structure.  */
    R3PTRTYPE(PCPDMUSBREG)      pUsbReg;
    /** Configuration handle. */
    R3PTRTYPE(PCFGMNODE)        pCfg;
    /** The (device) global configuration handle. */
    R3PTRTYPE(PCFGMNODE)        pCfgGlobal;
    /** Pointer to device instance data. */
    R3PTRTYPE(void *)           pvInstanceDataR3;
    /** Pointer to the VUSB Device structure.
     * Internal to VUSB, don't touch.
     * @todo Moved this to PDMUSBINSINT. */
    R3PTRTYPE(void *)           pvVUsbDev2;
    /** Device name for using when logging.
     * The constructor sets this and the destructor frees it. */
    R3PTRTYPE(char *)           pszName;
    /** Padding to make achInstanceData aligned at 32 byte boundrary. */
    uint32_t                    au32Padding[HC_ARCH_BITS == 32 ? 5 : 2];
    /** Device instance data. The size of this area is defined
     * in the PDMUSBREG::cbInstanceData field. */
    char                        achInstanceData[8];
} PDMUSBINS;

/** Current USBINS version number. */
#define PDM_USBINS_VERSION  0xf3010000

/** Converts a pointer to the PDMUSBINS::IBase to a pointer to PDMUSBINS. */
#define PDMIBASE_2_PDMUSB(pInterface) ( (PPDMUSBINS)((char *)(pInterface) - RT_OFFSETOF(PDMUSBINS, IBase)) )


/** @def PDMUSB_ASSERT_EMT
 * Assert that the current thread is the emulation thread.
 */
#ifdef VBOX_STRICT
# define PDMUSB_ASSERT_EMT(pUsbIns)  pUsbIns->pUsbHlp->pfnAssertEMT(pUsbIns, __FILE__, __LINE__, __FUNCTION__)
#else
# define PDMUSB_ASSERT_EMT(pUsbIns)  do { } while (0)
#endif

/** @def PDMUSB_ASSERT_OTHER
 * Assert that the current thread is NOT the emulation thread.
 */
#ifdef VBOX_STRICT
# define PDMUSB_ASSERT_OTHER(pUsbIns)  pUsbIns->pUsbHlp->pfnAssertOther(pUsbIns, __FILE__, __LINE__, __FUNCTION__)
#else
# define PDMUSB_ASSERT_OTHER(pUsbIns)  do { } while (0)
#endif

/** @def PDMUSB_SET_ERROR
 * Set the VM error. See PDMDevHlpVMSetError() for printf like message formatting.
 */
#define PDMUSB_SET_ERROR(pUsbIns, rc, pszError) \
    PDMDevHlpVMSetError(pUsbIns, rc, RT_SRC_POS, "%s", pszError)

/** @def PDMUSB_SET_RUNTIME_ERROR
 * Set the VM runtime error. See PDMDevHlpVMSetRuntimeError() for printf like message formatting.
 */
#define PDMUSB_SET_RUNTIME_ERROR(pUsbIns, fFatal, pszErrorID, pszError) \
    PDMDevHlpVMSetRuntimeError(pUsbIns, fFatal, pszErrorID, "%s", pszError)


#ifdef IN_RING3

/**
 * VBOX_STRICT wrapper for pUsbHlp->pfnDBGFStopV.
 *
 * @returns VBox status code which must be passed up to the VMM.
 * @param   pUsbIns             Device instance.
 * @param   RT_SRC_POS_DECL     Use RT_SRC_POS.
 * @param   pszFormat           Message. (optional)
 * @param   ...                 Message parameters.
 */
DECLINLINE(int) PDMUsbDBGFStop(PPDMUSBINS pUsbIns, RT_SRC_POS_DECL, const char *pszFormat, ...)
{
#ifdef VBOX_STRICT
    int rc;
    va_list va;
    va_start(va, pszFormat);
    rc = pUsbIns->pUsbHlp->pfnDBGFStopV(pUsbIns, RT_SRC_POS_ARGS, pszFormat, va);
    va_end(va);
    return rc;
#else
    return VINF_SUCCESS;
#endif
}


/* inline wrappers */

#endif



/** Pointer to callbacks provided to the VBoxUsbRegister() call. */
typedef const struct PDMUSBREGCB *PCPDMUSBREGCB;

/**
 * Callbacks for VBoxUSBDeviceRegister().
 */
typedef struct PDMUSBREGCB
{
    /** Interface version.
     * This is set to PDM_USBREG_CB_VERSION. */
    uint32_t                    u32Version;

    /**
     * Registers a device with the current VM instance.
     *
     * @returns VBox status code.
     * @param   pCallbacks      Pointer to the callback table.
     * @param   pDevReg         Pointer to the device registration record.
     *                          This data must be permanent and readonly.
     */
    DECLR3CALLBACKMEMBER(int, pfnRegister,(PCPDMUSBREGCB pCallbacks, PCPDMUSBREG pDevReg));

    /**
     * Allocate memory which is associated with current VM instance
     * and automatically freed on it's destruction.
     *
     * @returns Pointer to allocated memory. The memory is *NOT* zero-ed.
     * @param   pCallbacks      Pointer to the callback table.
     * @param   cb              Number of bytes to allocate.
     */
    DECLR3CALLBACKMEMBER(void *, pfnMMHeapAlloc,(PCPDMUSBREGCB pCallbacks, size_t cb));
} PDMUSBREGCB;

/** Current version of the PDMUSBREGCB structure.  */
#define PDM_USBREG_CB_VERSION 0xee010000


/**
 * The VBoxUsbRegister callback function.
 *
 * PDM will invoke this function after loading a USB device module and letting
 * the module decide which devices to register and how to handle conflicts.
 *
 * @returns VBox status code.
 * @param   pCallbacks      Pointer to the callback table.
 * @param   u32Version      VBox version number.
 */
typedef DECLCALLBACK(int) FNPDMVBOXUSBREGISTER(PCPDMUSBREGCB pCallbacks, uint32_t u32Version);


/**
 * Creates a USB proxy device instance.
 *
 * This will find an appropriate HUB for the USB device, create the necessary CFGM stuff
 * and try instantiate the proxy device.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pUuid           The UUID thats to be associated with the device.
 * @param   fRemote         Whether it's a remove or local device.
 * @param   pszAddress      The address string.
 * @param   pvBackend       Pointer to the backend.
 * @param   iUsbVersion     The preferred USB version.
 * @param   fMaskedIfs      The interfaces to hide from the guest.
 */
PDMR3DECL(int) PDMR3USBCreateProxyDevice(PVM pVM, PCRTUUID pUuid, bool fRemote, const char *pszAddress, void *pvBackend,
                                         uint32_t iUsbVersion, uint32_t fMaskedIfs);

/**
 * Detaches and destroys a USB device.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pUuid           The UUID associated with the device to detach.
 * @thread  EMT
 */
PDMR3DECL(int) PDMR3USBDetachDevice(PVM pVM, PCRTUUID pUuid);

/**
 * Checks if there are any USB hubs attached.
 *
 * @returns true / false accordingly.
 * @param   pVM     Pointer to the shared VM structure.
 */
PDMR3DECL(bool) PDMR3USBHasHub(PVM pVM);


/** @} */

__END_DECLS

#endif
