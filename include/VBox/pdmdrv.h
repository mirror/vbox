/** @file
 * PDM - Pluggable Device Manager, Drivers.
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

#ifndef ___VBox_pdmdrv_h
#define ___VBox_pdmdrv_h

#include <VBox/pdmqueue.h>
#include <VBox/pdmcritsect.h>
#include <VBox/pdmthread.h>
#include <VBox/pdmifs.h>
#include <VBox/pdmins.h>
#include <VBox/tm.h>
#include <VBox/ssm.h>
#include <VBox/cfgm.h>
#include <VBox/dbgf.h>
#include <VBox/mm.h>
#include <VBox/err.h>
#include <iprt/stdarg.h>

__BEGIN_DECLS

/** @defgroup grp_pdm_driver    Drivers
 * @ingroup grp_pdm
 * @{
 */

/**
 * Construct a driver instance for a VM.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle as it's expected
 *                      to be used frequently in this function.
 */
typedef DECLCALLBACK(int)   FNPDMDRVCONSTRUCT(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle);
/** Pointer to a FNPDMDRVCONSTRUCT() function. */
typedef FNPDMDRVCONSTRUCT *PFNPDMDRVCONSTRUCT;

/**
 * Destruct a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that
 * any non-VM resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
typedef DECLCALLBACK(void)   FNPDMDRVDESTRUCT(PPDMDRVINS pDrvIns);
/** Pointer to a FNPDMDRVDESTRUCT() function. */
typedef FNPDMDRVDESTRUCT *PFNPDMDRVDESTRUCT;

/**
 * Driver I/O Control interface.
 *
 * This is used by external components, such as the COM interface, to
 * communicate with a driver using a driver specific interface. Generally,
 * the driver interfaces are used for this task.
 *
 * @returns VBox status code.
 * @param   pDrvIns     Pointer to the driver instance.
 * @param   uFunction   Function to perform.
 * @param   pvIn        Pointer to input data.
 * @param   cbIn        Size of input data.
 * @param   pvOut       Pointer to output data.
 * @param   cbOut       Size of output data.
 * @param   pcbOut      Where to store the actual size of the output data.
 */
typedef DECLCALLBACK(int) FNPDMDRVIOCTL(PPDMDRVINS pDrvIns, RTUINT uFunction,
                                        void *pvIn, RTUINT cbIn,
                                        void *pvOut, RTUINT cbOut, PRTUINT pcbOut);
/** Pointer to a FNPDMDRVIOCTL() function. */
typedef FNPDMDRVIOCTL *PFNPDMDRVIOCTL;

/**
 * Power On notification.
 *
 * @param   pDrvIns     The driver instance data.
 */
typedef DECLCALLBACK(void)   FNPDMDRVPOWERON(PPDMDRVINS pDrvIns);
/** Pointer to a FNPDMDRVPOWERON() function. */
typedef FNPDMDRVPOWERON *PFNPDMDRVPOWERON;

/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 */
typedef DECLCALLBACK(void)  FNPDMDRVRESET(PPDMDRVINS pDrvIns);
/** Pointer to a FNPDMDRVRESET() function. */
typedef FNPDMDRVRESET *PFNPDMDRVRESET;

/**
 * Suspend notification.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 */
typedef DECLCALLBACK(void)  FNPDMDRVSUSPEND(PPDMDRVINS pDrvIns);
/** Pointer to a FNPDMDRVSUSPEND() function. */
typedef FNPDMDRVSUSPEND *PFNPDMDRVSUSPEND;

/**
 * Resume notification.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 */
typedef DECLCALLBACK(void)  FNPDMDRVRESUME(PPDMDRVINS pDrvIns);
/** Pointer to a FNPDMDRVRESUME() function. */
typedef FNPDMDRVRESUME *PFNPDMDRVRESUME;

/**
 * Power Off notification.
 *
 * @param   pDrvIns     The driver instance data.
 */
typedef DECLCALLBACK(void)   FNPDMDRVPOWEROFF(PPDMDRVINS pDrvIns);
/** Pointer to a FNPDMDRVPOWEROFF() function. */
typedef FNPDMDRVPOWEROFF *PFNPDMDRVPOWEROFF;

/**
 * Detach notification.
 *
 * This is called when a driver below it in the chain is detaching itself
 * from it. The driver should adjust it's state to reflect this.
 *
 * This is like ejecting a cdrom or floppy.
 *
 * @param   pDrvIns     The driver instance.
 */
typedef DECLCALLBACK(void)  FNPDMDRVDETACH(PPDMDRVINS pDrvIns);
/** Pointer to a FNPDMDRVDETACH() function. */
typedef FNPDMDRVDETACH *PFNPDMDRVDETACH;



/** PDM Driver Registration Structure,
 * This structure is used when registering a driver from
 * VBoxInitDrivers() (HC Ring-3). PDM will continue use till
 * the VM is terminated.
 */
typedef struct PDMDRVREG
{
    /** Structure version. PDM_DRVREG_VERSION defines the current version. */
    uint32_t            u32Version;
    /** Driver name. */
    char                szDriverName[32];
    /** The description of the driver. The UTF-8 string pointed to shall, like this structure,
     * remain unchanged from registration till VM destruction. */
    const char         *pszDescription;

    /** Flags, combination of the PDM_DRVREG_FLAGS_* \#defines. */
    RTUINT              fFlags;
    /** Driver class(es), combination of the PDM_DRVREG_CLASS_* \#defines. */
    RTUINT              fClass;
    /** Maximum number of instances (per VM). */
    RTUINT              cMaxInstances;
    /** Size of the instance data. */
    RTUINT              cbInstance;

    /** Construct instance - required. */
    PFNPDMDRVCONSTRUCT  pfnConstruct;
    /** Destruct instance - optional. */
    PFNPDMDRVDESTRUCT   pfnDestruct;
    /** I/O control - optional. */
    PFNPDMDRVIOCTL      pfnIOCtl;
    /** Power on notification - optional. */
    PFNPDMDRVPOWERON    pfnPowerOn;
    /** Reset notification - optional. */
    PFNPDMDRVRESET      pfnReset;
    /** Suspend notification  - optional. */
    PFNPDMDRVSUSPEND    pfnSuspend;
    /** Resume notification - optional. */
    PFNPDMDRVRESUME     pfnResume;
    /** Detach notification - optional. */
    PFNPDMDRVDETACH     pfnDetach;
    /** Power off notification - optional. */
    PFNPDMDRVPOWEROFF   pfnPowerOff;

} PDMDRVREG;
/** Pointer to a PDM Driver Structure. */
typedef PDMDRVREG *PPDMDRVREG;
/** Const pointer to a PDM Driver Structure. */
typedef PDMDRVREG const *PCPDMDRVREG;

/** Current DRVREG version number. */
#define PDM_DRVREG_VERSION  0x80010000

/** PDM Device Flags.
 * @{ */
/** @def PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT
 * The bit count for the current host. */
#if HC_ARCH_BITS == 32
# define PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT     0x000000001
#elif HC_ARCH_BITS == 64
# define PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT     0x000000002
#else
# error Unsupported HC_ARCH_BITS value.
#endif
/** The host bit count mask. */
#define PDM_DRVREG_FLAGS_HOST_BITS_MASK         0x000000003

/** @} */


/** PDM Driver Classes.
 * @{ */
/** Mouse input driver. */
#define PDM_DRVREG_CLASS_MOUSE          RT_BIT(0)
/** Keyboard input driver. */
#define PDM_DRVREG_CLASS_KEYBOARD       RT_BIT(1)
/** Display driver. */
#define PDM_DRVREG_CLASS_DISPLAY        RT_BIT(2)
/** Network transport driver. */
#define PDM_DRVREG_CLASS_NETWORK        RT_BIT(3)
/** Block driver. */
#define PDM_DRVREG_CLASS_BLOCK          RT_BIT(4)
/** Media driver. */
#define PDM_DRVREG_CLASS_MEDIA          RT_BIT(5)
/** Mountable driver. */
#define PDM_DRVREG_CLASS_MOUNTABLE      RT_BIT(6)
/** Audio driver. */
#define PDM_DRVREG_CLASS_AUDIO          RT_BIT(7)
/** VMMDev driver. */
#define PDM_DRVREG_CLASS_VMMDEV         RT_BIT(8)
/** Status driver. */
#define PDM_DRVREG_CLASS_STATUS         RT_BIT(9)
/** ACPI driver. */
#define PDM_DRVREG_CLASS_ACPI           RT_BIT(10)
/** USB related driver. */
#define PDM_DRVREG_CLASS_USB            RT_BIT(11)
/** ISCSI Transport related driver. */
#define PDM_DRVREG_CLASS_ISCSITRANSPORT RT_BIT(12)
/** Char driver. */
#define PDM_DRVREG_CLASS_CHAR           RT_BIT(13)
/** Stream driver. */
#define PDM_DRVREG_CLASS_STREAM         RT_BIT(14)
/** @} */


/**
 * USB hub registration structure.
 */
typedef struct PDMUSBHUBREG
{
    /** Structure version number. PDM_USBHUBREG_VERSION defines the current version. */
    uint32_t            u32Version;

    /**
     * Request the hub to attach of the specified device.
     *
     * @returns VBox status code.
     * @param   pDrvIns     The hub instance.
     * @param   pUsbIns     The device to attach.
     * @param   piPort      Where to store the port number the device was attached to.
     * @thread EMT.
     */
    DECLR3CALLBACKMEMBER(int, pfnAttachDevice,(PPDMDRVINS pDrvIns, PPDMUSBINS pUsbIns, uint32_t *piPort));

    /**
     * Request the hub to detach of the specified device.
     *
     * The device has previously been attached to the hub with the
     * pfnAttachDevice call. This call is not currently expected to
     * fail.
     *
     * @returns VBox status code.
     * @param   pDrvIns     The hub instance.
     * @param   pUsbIns     The device to detach.
     * @param   iPort       The port number returned by the attach call.
     * @thread EMT.
     */
    DECLR3CALLBACKMEMBER(int, pfnDetachDevice,(PPDMDRVINS pDrvIns, PPDMUSBINS pUsbIns, uint32_t iPort));

    /** Counterpart to u32Version, same value. */
    uint32_t            u32TheEnd;
} PDMUSBHUBREG;
/** Pointer to a const USB hub registration structure. */
typedef const PDMUSBHUBREG *PCPDMUSBHUBREG;

/** Current PDMUSBHUBREG version number. */
#define PDM_USBHUBREG_VERSION       0xeb010000


/**
 * USB hub helpers.
 * This is currently just a place holder.
 */
typedef struct PDMUSBHUBHLP
{
    /** Structure version. PDM_USBHUBHLP_VERSION defines the current version. */
    uint32_t                    u32Version;

    /** Just a safety precaution. */
    uint32_t                    u32TheEnd;
} PDMUSBHUBHLP;
/** Pointer to PCI helpers. */
typedef PDMUSBHUBHLP *PPDMUSBHUBHLP;
/** Pointer to const PCI helpers. */
typedef const PDMUSBHUBHLP *PCPDMUSBHUBHLP;
/** Pointer to const PCI helpers pointer. */
typedef PCPDMUSBHUBHLP *PPCPDMUSBHUBHLP;

/** Current PDMUSBHUBHLP version number. */
#define PDM_USBHUBHLP_VERSION       0xea010000



/**
 * Poller callback.
 *
 * @param   pDrvIns     The driver instance.
 */
typedef DECLCALLBACK(void) FNPDMDRVPOLLER(PPDMDRVINS pDrvIns);
/** Pointer to a FNPDMDRVPOLLER function. */
typedef FNPDMDRVPOLLER *PFNPDMDRVPOLLER;

#ifdef IN_RING3
/**
 * PDM Driver API.
 */
typedef struct PDMDRVHLP
{
    /** Structure version. PDM_DRVHLP_VERSION defines the current version. */
    uint32_t                    u32Version;

    /**
     * Attaches a driver (chain) to the driver.
     *
     * @returns VBox status code.
     * @param   pDrvIns             Driver instance.
     * @param   ppBaseInterface     Where to store the pointer to the base interface.
     */
    DECLR3CALLBACKMEMBER(int, pfnAttach,(PPDMDRVINS pDrvIns, PPDMIBASE *ppBaseInterface));

    /**
     * Detach the driver the drivers below us.
     *
     * @returns VBox status code.
     * @param   pDrvIns             Driver instance.
     */
    DECLR3CALLBACKMEMBER(int, pfnDetach,(PPDMDRVINS pDrvIns));

    /**
     * Detach the driver from the driver above it and destroy this
     * driver and all drivers below it.
     *
     * @returns VBox status code.
     * @param   pDrvIns             Driver instance.
     */
    DECLR3CALLBACKMEMBER(int, pfnDetachSelf,(PPDMDRVINS pDrvIns));

    /**
     * Prepare a media mount.
     *
     * The driver must not have anything attached to itself
     * when calling this function as the purpose is to set up the configuration
     * of an future attachment.
     *
     * @returns VBox status code
     * @param   pDrvIns             Driver instance.
     * @param   pszFilename     Pointer to filename. If this is NULL it assumed that the caller have
     *                          constructed a configuration which can be attached to the bottom driver.
     * @param   pszCoreDriver   Core driver name. NULL will cause autodetection. Ignored if pszFilanem is NULL.
     */
    DECLR3CALLBACKMEMBER(int, pfnMountPrepare,(PPDMDRVINS pDrvIns, const char *pszFilename, const char *pszCoreDriver));

    /**
     * Assert that the current thread is the emulation thread.
     *
     * @returns True if correct.
     * @returns False if wrong.
     * @param   pDrvIns         Driver instance.
     * @param   pszFile         Filename of the assertion location.
     * @param   iLine           Linenumber of the assertion location.
     * @param   pszFunction     Function of the assertion location.
     */
    DECLR3CALLBACKMEMBER(bool, pfnAssertEMT,(PPDMDRVINS pDrvIns, const char *pszFile, unsigned iLine, const char *pszFunction));

    /**
     * Assert that the current thread is NOT the emulation thread.
     *
     * @returns True if correct.
     * @returns False if wrong.
     * @param   pDrvIns         Driver instance.
     * @param   pszFile         Filename of the assertion location.
     * @param   iLine           Linenumber of the assertion location.
     * @param   pszFunction     Function of the assertion location.
     */
    DECLR3CALLBACKMEMBER(bool, pfnAssertOther,(PPDMDRVINS pDrvIns, const char *pszFile, unsigned iLine, const char *pszFunction));

    /**
     * Set the VM error message
     *
     * @returns rc.
     * @param   pDrvIns         Driver instance.
     * @param   rc              VBox status code.
     * @param   RT_SRC_POS_DECL Use RT_SRC_POS.
     * @param   pszFormat       Error message format string.
     * @param   ...             Error message arguments.
     */
    DECLR3CALLBACKMEMBER(int, pfnVMSetError,(PPDMDRVINS pDrvIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...));

    /**
     * Set the VM error message
     *
     * @returns rc.
     * @param   pDrvIns         Driver instance.
     * @param   rc              VBox status code.
     * @param   RT_SRC_POS_DECL Use RT_SRC_POS.
     * @param   pszFormat       Error message format string.
     * @param   va              Error message arguments.
     */
    DECLR3CALLBACKMEMBER(int, pfnVMSetErrorV,(PPDMDRVINS pDrvIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va));

    /**
     * Set the VM runtime error message
     *
     * @returns VBox status code.
     * @param   pDrvIns         Driver instance.
     * @param   fFatal          Whether it is a fatal error or not.
     * @param   pszErrorID      Error ID string.
     * @param   pszFormat       Error message format string.
     * @param   ...             Error message arguments.
     */
    DECLR3CALLBACKMEMBER(int, pfnVMSetRuntimeError,(PPDMDRVINS pDrvIns, bool fFatal, const char *pszErrorID, const char *pszFormat, ...));

    /**
     * Set the VM runtime error message
     *
     * @returns VBox status code.
     * @param   pDrvIns         Driver instance.
     * @param   fFatal          Whether it is a fatal error or not.
     * @param   pszErrorID      Error ID string.
     * @param   pszFormat       Error message format string.
     * @param   va              Error message arguments.
     */
    DECLR3CALLBACKMEMBER(int, pfnVMSetRuntimeErrorV,(PPDMDRVINS pDrvIns, bool fFatal, const char *pszErrorID, const char *pszFormat, va_list va));

    /**
     * Create a queue.
     *
     * @returns VBox status code.
     * @param   pDrvIns             Driver instance.
     * @param   cbItem              Size a queue item.
     * @param   cItems              Number of items in the queue.
     * @param   cMilliesInterval    Number of milliseconds between polling the queue.
     *                              If 0 then the emulation thread will be notified whenever an item arrives.
     * @param   pfnCallback         The consumer function.
     * @param   ppQueue             Where to store the queue handle on success.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnPDMQueueCreate,(PPDMDRVINS pDrvIns, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval, PFNPDMQUEUEDRV pfnCallback, PPDMQUEUE *ppQueue));

    /**
     * Register a poller function.
     * TEMPORARY HACK FOR NETWORKING! DON'T USE!
     *
     * @returns VBox status code.
     * @param   pDrvIns             Driver instance.
     * @param   pfnPoller           The callback function.
     */
    DECLR3CALLBACKMEMBER(int, pfnPDMPollerRegister,(PPDMDRVINS pDrvIns, PFNPDMDRVPOLLER pfnPoller));

    /**
     * Query the virtual timer frequency.
     *
     * @returns Frequency in Hz.
     * @param   pDrvIns             Driver instance.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(uint64_t, pfnTMGetVirtualFreq,(PPDMDRVINS pDrvIns));

    /**
     * Query the virtual time.
     *
     * @returns The current virtual time.
     * @param   pDrvIns             Driver instance.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(uint64_t, pfnTMGetVirtualTime,(PPDMDRVINS pDrvIns));

    /**
     * Creates a timer.
     *
     * @returns VBox status.
     * @param   pDrvIns         Driver instance.
     * @param   enmClock        The clock to use on this timer.
     * @param   pfnCallback     Callback function.
     * @param   pszDesc         Pointer to description string which must stay around
     *                          until the timer is fully destroyed (i.e. a bit after TMTimerDestroy()).
     * @param   ppTimer         Where to store the timer on success.
     */
    DECLR3CALLBACKMEMBER(int, pfnTMTimerCreate,(PPDMDRVINS pDrvIns, TMCLOCK enmClock, PFNTMTIMERDRV pfnCallback, const char *pszDesc, PPTMTIMERR3 ppTimer));

    /**
     * Register a save state data unit.
     *
     * @returns VBox status.
     * @param   pDrvIns         Driver instance.
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
    DECLR3CALLBACKMEMBER(int, pfnSSMRegister,(PPDMDRVINS pDrvIns, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
                                              PFNSSMDRVSAVEPREP pfnSavePrep, PFNSSMDRVSAVEEXEC pfnSaveExec, PFNSSMDRVSAVEDONE pfnSaveDone,
                                              PFNSSMDRVLOADPREP pfnLoadPrep, PFNSSMDRVLOADEXEC pfnLoadExec, PFNSSMDRVLOADDONE pfnLoadDone));

    /**
     * Deregister a save state data unit.
     *
     * @returns VBox status.
     * @param   pDrvIns         Driver instance.
     * @param   pszName         Data unit name.
     * @param   u32Instance     The instance identifier of the data unit.
     *                          This must together with the name be unique.
     */
    DECLR3CALLBACKMEMBER(int, pfnSSMDeregister,(PPDMDRVINS pDrvIns, const char *pszName, uint32_t u32Instance));

    /**
     * Registers a statistics sample if statistics are enabled.
     *
     * @param   pDrvIns     Driver instance.
     * @param   pvSample    Pointer to the sample.
     * @param   enmType     Sample type. This indicates what pvSample is pointing at.
     * @param   pszName     Sample name. The name is on this form "/<component>/<sample>".
     *                      Further nesting is possible.
     * @param   enmUnit     Sample unit.
     * @param   pszDesc     Sample description.
     */
    DECLR3CALLBACKMEMBER(void, pfnSTAMRegister,(PPDMDRVINS pDrvIns, void *pvSample, STAMTYPE enmType, const char *pszName,
                                                STAMUNIT enmUnit, const char *pszDesc));

    /**
     * Same as pfnSTAMRegister except that the name is specified in a
     * RTStrPrintf like fashion.
     *
     * @returns VBox status.
     * @param   pDrvIns     Driver instance.
     * @param   pvSample    Pointer to the sample.
     * @param   enmType     Sample type. This indicates what pvSample is pointing at.
     * @param   enmVisibility  Visibility type specifying whether unused statistics should be visible or not.
     * @param   enmUnit     Sample unit.
     * @param   pszDesc     Sample description.
     * @param   pszName     The sample name format string.
     * @param   ...         Arguments to the format string.
     */
    DECLR3CALLBACKMEMBER(void, pfnSTAMRegisterF,(PPDMDRVINS pDrvIns, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility,
                                                 STAMUNIT enmUnit, const char *pszDesc, const char *pszName, ...));

    /**
     * Same as pfnSTAMRegister except that the name is specified in a
     * RTStrPrintfV like fashion.
     *
     * @returns VBox status.
     * @param   pDrvIns         Driver instance.
     * @param   pvSample        Pointer to the sample.
     * @param   enmType         Sample type. This indicates what pvSample is pointing at.
     * @param   enmVisibility   Visibility type specifying whether unused statistics should be visible or not.
     * @param   enmUnit         Sample unit.
     * @param   pszDesc         Sample description.
     * @param   pszName         The sample name format string.
     * @param   args            Arguments to the format string.
     */
    DECLR3CALLBACKMEMBER(void, pfnSTAMRegisterV,(PPDMDRVINS pDrvIns, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility,
                                                 STAMUNIT enmUnit, const char *pszDesc, const char *pszName, va_list args));

    /**
     * Calls the HC R0 VMM entry point, in a safer but slower manner than SUPCallVMMR0.
     *
     * When entering using this call the R0 components can call into the host kernel
     * (i.e. use the SUPR0 and RT APIs).
     *
     * See VMMR0Entry() for more details.
     *
     * @returns error code specific to uFunction.
     * @param   pDrvIns     The driver instance.
     * @param   uOperation  Operation to execute.
     *                      This is limited to services.
     * @param   pvArg       Pointer to argument structure or if cbArg is 0 just an value.
     * @param   cbArg       The size of the argument. This is used to copy whatever the argument
     *                      points at into a kernel buffer to avoid problems like the user page
     *                      being invalidated while we're executing the call.
     */
    DECLR3CALLBACKMEMBER(int, pfnSUPCallVMMR0Ex,(PPDMDRVINS pDrvIns, unsigned uOperation, void *pvArg, unsigned cbArg));

    /**
     * Registers a USB HUB.
     *
     * @returns VBox status code.
     * @param   pDrvIns         The driver instance.
     * @param   fVersions       Indicates the kinds of USB devices that can be attached to this HUB.
     * @param   cPorts          The number of ports.
     * @param   pUsbHubReg      The hub callback structure that PDMUsb uses to interact with it.
     * @param   ppUsbHubHlp     The helper callback structure that the hub uses to talk to PDMUsb.
     *
     * @thread  EMT.
     */
    DECLR3CALLBACKMEMBER(int, pfnUSBRegisterHub,(PPDMDRVINS pDrvIns, uint32_t fVersions, uint32_t cPorts, PCPDMUSBHUBREG pUsbHubReg, PPCPDMUSBHUBHLP ppUsbHubHlp));

    /**
     * Creates a PDM thread.
     *
     * This differs from the RTThreadCreate() API in that PDM takes care of suspending,
     * resuming, and destroying the thread as the VM state changes.
     *
     * @returns VBox status code.
     * @param   pDrvIns     The driver instance.
     * @param   ppThread    Where to store the thread 'handle'.
     * @param   pvUser      The user argument to the thread function.
     * @param   pfnThread   The thread function.
     * @param   pfnWakeup   The wakup callback. This is called on the EMT thread when
     *                      a state change is pending.
     * @param   cbStack     See RTThreadCreate.
     * @param   enmType     See RTThreadCreate.
     * @param   pszName     See RTThreadCreate.
     */
    DECLR3CALLBACKMEMBER(int, pfnPDMThreadCreate,(PPDMDRVINS pDrvIns, PPPDMTHREAD ppThread, void *pvUser, PFNPDMTHREADDRV pfnThread,
                                                  PFNPDMTHREADWAKEUPDRV pfnWakeup, size_t cbStack, RTTHREADTYPE enmType, const char *pszName));

    /** Just a safety precaution. */
    uint32_t                        u32TheEnd;
} PDMDRVHLP;
/** Pointer PDM Driver API. */
typedef PDMDRVHLP *PPDMDRVHLP;
/** Pointer const PDM Driver API. */
typedef const PDMDRVHLP *PCPDMDRVHLP;

/** Current DRVHLP version number. */
#define PDM_DRVHLP_VERSION  0x90020000



/**
 * PDM Driver Instance.
 */
typedef struct PDMDRVINS
{
    /** Structure version. PDM_DRVINS_VERSION defines the current version. */
    uint32_t                    u32Version;

    /** Internal data. */
    union
    {
#ifdef PDMDRVINSINT_DECLARED
        PDMDRVINSINT            s;
#endif
        uint8_t                 padding[HC_ARCH_BITS == 32 ? 32 : 64];
    } Internal;

    /** Pointer the PDM Driver API. */
    R3PTRTYPE(PCPDMDRVHLP)      pDrvHlp;
    /** Pointer to driver registration structure.  */
    R3PTRTYPE(PCPDMDRVREG)      pDrvReg;
    /** Configuration handle. */
    R3PTRTYPE(PCFGMNODE)        pCfgHandle;
    /** Driver instance number. */
    RTUINT                      iInstance;
    /** Pointer to the base interface of the device/driver instance above. */
    R3PTRTYPE(PPDMIBASE)        pUpBase;
    /** Pointer to the base interface of the driver instance below. */
    R3PTRTYPE(PPDMIBASE)        pDownBase;
    /** The base interface of the driver.
     * The driver constructor initializes this. */
    PDMIBASE                    IBase;
    /* padding to make achInstanceData aligned at 16 byte boundrary. */
    uint32_t                    au32Padding[HC_ARCH_BITS == 32 ? 3 : 1];
    /** Pointer to driver instance data. */
    R3PTRTYPE(void *)           pvInstanceData;
    /** Driver instance data. The size of this area is defined
     * in the PDMDRVREG::cbInstanceData field. */
    char                        achInstanceData[4];
} PDMDRVINS;

/** Current DRVREG version number. */
#define PDM_DRVINS_VERSION  0xa0010000

/** Converts a pointer to the PDMDRVINS::IBase to a pointer to PDMDRVINS. */
#define PDMIBASE_2_PDMDRV(pInterface) ( (PPDMDRVINS)((char *)(pInterface) - RT_OFFSETOF(PDMDRVINS, IBase)) )

/**
 * @copydoc PDMDRVHLP::pfnVMSetError
 */
DECLINLINE(int) PDMDrvHlpVMSetError(PPDMDRVINS pDrvIns, const int rc, RT_SRC_POS_DECL, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    pDrvIns->pDrvHlp->pfnVMSetErrorV(pDrvIns, rc, RT_SRC_POS_ARGS, pszFormat, va);
    va_end(va);
    return rc;
}

/** @def PDMDRV_SET_ERROR
 * Set the VM error. See PDMDrvHlpVMSetError() for printf like message formatting.
 */
#define PDMDRV_SET_ERROR(pDrvIns, rc, pszError)  \
    PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, "%s", pszError)

/**
 * @copydoc PDMDRVHLP::pfnVMSetRuntimeError
 */
DECLINLINE(int) PDMDrvHlpVMSetRuntimeError(PPDMDRVINS pDrvIns, bool fFatal, const char *pszErrorID, const char *pszFormat, ...)
{
    va_list va;
    int rc;
    va_start(va, pszFormat);
    rc = pDrvIns->pDrvHlp->pfnVMSetRuntimeErrorV(pDrvIns, fFatal, pszErrorID, pszFormat, va);
    va_end(va);
    return rc;
}

/** @def PDMDRV_SET_RUNTIME_ERROR
 * Set the VM runtime error. See PDMDrvHlpVMSetRuntimeError() for printf like message formatting.
 */
#define PDMDRV_SET_RUNTIME_ERROR(pDrvIns, fFatal, pszErrorID, pszError)  \
    PDMDrvHlpVMSetRuntimeError(pDrvIns, fFatal, pszErrorID, "%s", pszError)

#endif /* IN_RING3 */


/** @def PDMDRV_ASSERT_EMT
 * Assert that the current thread is the emulation thread.
 */
#ifdef VBOX_STRICT
# define PDMDRV_ASSERT_EMT(pDrvIns)  pDrvIns->pDrvHlp->pfnAssertEMT(pDrvIns, __FILE__, __LINE__, __FUNCTION__)
#else
# define PDMDRV_ASSERT_EMT(pDrvIns)  do { } while (0)
#endif

/** @def PDMDRV_ASSERT_OTHER
 * Assert that the current thread is NOT the emulation thread.
 */
#ifdef VBOX_STRICT
# define PDMDRV_ASSERT_OTHER(pDrvIns)  pDrvIns->pDrvHlp->pfnAssertOther(pDrvIns, __FILE__, __LINE__, __FUNCTION__)
#else
# define PDMDRV_ASSERT_OTHER(pDrvIns)  do { } while (0)
#endif


#ifdef IN_RING3
/**
 * @copydoc PDMDRVHLP::pfnSTAMRegister
 */
DECLINLINE(void) PDMDrvHlpSTAMRegister(PPDMDRVINS pDrvIns, void *pvSample, STAMTYPE enmType, const char *pszName, STAMUNIT enmUnit, const char *pszDesc)
{
    pDrvIns->pDrvHlp->pfnSTAMRegister(pDrvIns, pvSample, enmType, pszName, enmUnit, pszDesc);
}

/**
 * @copydoc PDMDRVHLP::pfnSTAMRegisterF
 */
DECLINLINE(void) PDMDrvHlpSTAMRegisterF(PPDMDRVINS pDrvIns, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility, STAMUNIT enmUnit,
                                        const char *pszDesc, const char *pszName, ...)
{
    va_list va;
    va_start(va, pszName);
    pDrvIns->pDrvHlp->pfnSTAMRegisterV(pDrvIns, pvSample, enmType, enmVisibility, enmUnit, pszDesc, pszName, va);
    va_end(va);
}

/**
 * @copydoc PDMDRVHLP::pfnUSBRegisterHub
 */
DECLINLINE(int) PDMDrvHlpUSBRegisterHub(PPDMDRVINS pDrvIns, uint32_t fVersions, uint32_t cPorts, PCPDMUSBHUBREG pUsbHubReg, PPCPDMUSBHUBHLP ppUsbHubHlp)
{
    return pDrvIns->pDrvHlp->pfnUSBRegisterHub(pDrvIns, fVersions, cPorts, pUsbHubReg, ppUsbHubHlp);
}

/**
 * @copydoc PDMDRVHLP::pfnPDMThreadCreate
 */
DECLINLINE(int) PDMDrvHlpPDMThreadCreate(PPDMDRVINS pDrvIns, PPPDMTHREAD ppThread, void *pvUser, PFNPDMTHREADDRV pfnThread,
                                         PFNPDMTHREADWAKEUPDRV pfnWakeup, size_t cbStack, RTTHREADTYPE enmType, const char *pszName)
{
    return pDrvIns->pDrvHlp->pfnPDMThreadCreate(pDrvIns, ppThread, pvUser, pfnThread, pfnWakeup, cbStack, enmType, pszName);
}

#endif /* IN_RING3 */



/** Pointer to callbacks provided to the VBoxDriverRegister() call. */
typedef struct PDMDRVREGCB *PPDMDRVREGCB;
/** Pointer to const callbacks provided to the VBoxDriverRegister() call. */
typedef const struct PDMDRVREGCB *PCPDMDRVREGCB;

/**
 * Callbacks for VBoxDriverRegister().
 */
typedef struct PDMDRVREGCB
{
    /** Interface version.
     * This is set to PDM_DRVREG_CB_VERSION. */
    uint32_t                    u32Version;

    /**
     * Registers a driver with the current VM instance.
     *
     * @returns VBox status code.
     * @param   pCallbacks      Pointer to the callback table.
     * @param   pDrvReg         Pointer to the driver registration record.
     *                          This data must be permanent and readonly.
     */
    DECLR3CALLBACKMEMBER(int, pfnRegister,(PCPDMDRVREGCB pCallbacks, PCPDMDRVREG pDrvReg));
} PDMDRVREGCB;

/** Current version of the PDMDRVREGCB structure.  */
#define PDM_DRVREG_CB_VERSION 0xb0010000


/**
 * The VBoxDriverRegister callback function.
 *
 * PDM will invoke this function after loading a driver module and letting
 * the module decide which drivers to register and how to handle conflicts.
 *
 * @returns VBox status code.
 * @param   pCallbacks      Pointer to the callback table.
 * @param   u32Version      VBox version number.
 */
typedef DECLCALLBACK(int) FNPDMVBOXDRIVERSREGISTER(PCPDMDRVREGCB pCallbacks, uint32_t u32Version);

/**
 * Register external drivers
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pfnCallback Driver registration callback
 */
PDMR3DECL(int) PDMR3RegisterDrivers(PVM pVM, FNPDMVBOXDRIVERSREGISTER pfnCallback);

/** @} */

__END_DECLS

#endif
