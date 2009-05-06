/** @file
 * PDM - Pluggable Device Manager, Devices.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_pdmdev_h
#define ___VBox_pdmdev_h

#include <VBox/pdmqueue.h>
#include <VBox/pdmcritsect.h>
#include <VBox/pdmthread.h>
#include <VBox/pdmifs.h>
#include <VBox/pdmins.h>
#include <VBox/iom.h>
#include <VBox/tm.h>
#include <VBox/ssm.h>
#include <VBox/cfgm.h>
#include <VBox/dbgf.h>
#include <VBox/err.h>
#include <VBox/pci.h>
#include <iprt/stdarg.h>

__BEGIN_DECLS

/** @defgroup grp_pdm_device    The PDM Devices API
 * @ingroup grp_pdm
 * @{
 */

/**
 * Construct a device instance for a VM.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 *                      If the registration structure is needed, pDevIns->pDevReg points to it.
 * @param   iInstance   Instance number. Use this to figure out which registers and such to use.
 *                      The instance number is also found in pDevIns->iInstance, but since it's
 *                      likely to be freqently used PDM passes it as parameter.
 * @param   pCfgHandle  Configuration node handle for the device. Use this to obtain the configuration
 *                      of the device instance. It's also found in pDevIns->pCfgHandle, but since it's
 *                      primary usage will in this function it's passed as a parameter.
 */
typedef DECLCALLBACK(int)   FNPDMDEVCONSTRUCT(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle);
/** Pointer to a FNPDMDEVCONSTRUCT() function. */
typedef FNPDMDEVCONSTRUCT *PFNPDMDEVCONSTRUCT;

/**
 * Destruct a device instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
typedef DECLCALLBACK(int)   FNPDMDEVDESTRUCT(PPDMDEVINS pDevIns);
/** Pointer to a FNPDMDEVDESTRUCT() function. */
typedef FNPDMDEVDESTRUCT *PFNPDMDEVDESTRUCT;

/**
 * Device relocation callback.
 *
 * When this callback is called the device instance data, and if the
 * device have a GC component, is being relocated, or/and the selectors
 * have been changed. The device must use the chance to perform the
 * necessary pointer relocations and data updates.
 *
 * Before the GC code is executed the first time, this function will be
 * called with a 0 delta so GC pointer calculations can be one in one place.
 *
 * @param   pDevIns     Pointer to the device instance.
 * @param   offDelta    The relocation delta relative to the old location.
 *
 * @remark  A relocation CANNOT fail.
 */
typedef DECLCALLBACK(void) FNPDMDEVRELOCATE(PPDMDEVINS pDevIns, RTGCINTPTR offDelta);
/** Pointer to a FNPDMDEVRELOCATE() function. */
typedef FNPDMDEVRELOCATE *PFNPDMDEVRELOCATE;


/**
 * Device I/O Control interface.
 *
 * This is used by external components, such as the COM interface, to
 * communicate with devices using a class wide interface or a device
 * specific interface.
 *
 * @returns VBox status code.
 * @param   pDevIns     Pointer to the device instance.
 * @param   uFunction   Function to perform.
 * @param   pvIn        Pointer to input data.
 * @param   cbIn        Size of input data.
 * @param   pvOut       Pointer to output data.
 * @param   cbOut       Size of output data.
 * @param   pcbOut      Where to store the actual size of the output data.
 */
typedef DECLCALLBACK(int) FNPDMDEVIOCTL(PPDMDEVINS pDevIns, RTUINT uFunction,
                                        void *pvIn, RTUINT cbIn,
                                        void *pvOut, RTUINT cbOut, PRTUINT pcbOut);
/** Pointer to a FNPDMDEVIOCTL() function. */
typedef FNPDMDEVIOCTL *PFNPDMDEVIOCTL;

/**
 * Power On notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
typedef DECLCALLBACK(void)   FNPDMDEVPOWERON(PPDMDEVINS pDevIns);
/** Pointer to a FNPDMDEVPOWERON() function. */
typedef FNPDMDEVPOWERON *PFNPDMDEVPOWERON;

/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
typedef DECLCALLBACK(void)  FNPDMDEVRESET(PPDMDEVINS pDevIns);
/** Pointer to a FNPDMDEVRESET() function. */
typedef FNPDMDEVRESET *PFNPDMDEVRESET;

/**
 * Suspend notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
typedef DECLCALLBACK(void)  FNPDMDEVSUSPEND(PPDMDEVINS pDevIns);
/** Pointer to a FNPDMDEVSUSPEND() function. */
typedef FNPDMDEVSUSPEND *PFNPDMDEVSUSPEND;

/**
 * Resume notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
typedef DECLCALLBACK(void)  FNPDMDEVRESUME(PPDMDEVINS pDevIns);
/** Pointer to a FNPDMDEVRESUME() function. */
typedef FNPDMDEVRESUME *PFNPDMDEVRESUME;

/**
 * Power Off notification.
 *
 * @param   pDevIns     The device instance data.
 */
typedef DECLCALLBACK(void)   FNPDMDEVPOWEROFF(PPDMDEVINS pDevIns);
/** Pointer to a FNPDMDEVPOWEROFF() function. */
typedef FNPDMDEVPOWEROFF *PFNPDMDEVPOWEROFF;

/**
 * Attach command.
 *
 * This is called to let the device attach to a driver for a specified LUN
 * at runtime. This is not called during VM construction, the device
 * constructor have to attach to all the available drivers.
 *
 * This is like plugging in the keyboard or mouse after turning on the PC.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 */
typedef DECLCALLBACK(int)  FNPDMDEVATTACH(PPDMDEVINS pDevIns, unsigned iLUN);
/** Pointer to a FNPDMDEVATTACH() function. */
typedef FNPDMDEVATTACH *PFNPDMDEVATTACH;

/**
 * Detach notification.
 *
 * This is called when a driver is detaching itself from a LUN of the device.
 * The device should adjust it's state to reflect this.
 *
 * This is like unplugging the network cable to use it for the laptop or
 * something while the PC is still running.
 *
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 */
typedef DECLCALLBACK(void)  FNPDMDEVDETACH(PPDMDEVINS pDevIns, unsigned iLUN);
/** Pointer to a FNPDMDEVDETACH() function. */
typedef FNPDMDEVDETACH *PFNPDMDEVDETACH;

/**
 * Query the base interface of a logical unit.
 *
 * @returns VBOX status code.
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logicial unit to query.
 * @param   ppBase      Where to store the pointer to the base interface of the LUN.
 */
typedef DECLCALLBACK(int) FNPDMDEVQUERYINTERFACE(PPDMDEVINS pDevIns, unsigned iLUN, PPDMIBASE *ppBase);
/** Pointer to a FNPDMDEVQUERYINTERFACE() function. */
typedef FNPDMDEVQUERYINTERFACE *PFNPDMDEVQUERYINTERFACE;

/**
 * Init complete notification.
 * This can be done to do communication with other devices and other
 * initialization which requires everything to be in place.
 *
 * @returns VBOX status code.
 * @param   pDevIns     The device instance.
 */
typedef DECLCALLBACK(int) FNPDMDEVINITCOMPLETE(PPDMDEVINS pDevIns);
/** Pointer to a FNPDMDEVINITCOMPLETE() function. */
typedef FNPDMDEVINITCOMPLETE *PFNPDMDEVINITCOMPLETE;



/** PDM Device Registration Structure,
 * This structure is used when registering a device from
 * VBoxInitDevices() in HC Ring-3. PDM will continue use till
 * the VM is terminated.
 */
typedef struct PDMDEVREG
{
    /** Structure version. PDM_DEVREG_VERSION defines the current version. */
    uint32_t            u32Version;
    /** Device name. */
    char                szDeviceName[32];
    /** Name of the raw-mode context module (no path).
     * Only evalutated if PDM_DEVREG_FLAGS_RC is set. */
    char                szRCMod[32];
    /** Name of the ring-0 module (no path).
     * Only evalutated if PDM_DEVREG_FLAGS_R0 is set. */
    char                szR0Mod[32];
    /** The description of the device. The UTF-8 string pointed to shall, like this structure,
     * remain unchanged from registration till VM destruction. */
    const char         *pszDescription;

    /** Flags, combination of the PDM_DEVREG_FLAGS_* \#defines. */
    RTUINT              fFlags;
    /** Device class(es), combination of the PDM_DEVREG_CLASS_* \#defines. */
    RTUINT              fClass;
    /** Maximum number of instances (per VM). */
    RTUINT              cMaxInstances;
    /** Size of the instance data. */
    RTUINT              cbInstance;

    /** Construct instance - required. */
    PFNPDMDEVCONSTRUCT  pfnConstruct;
    /** Destruct instance - optional. */
    PFNPDMDEVDESTRUCT   pfnDestruct;
    /** Relocation command - optional. */
    PFNPDMDEVRELOCATE   pfnRelocate;
    /** I/O Control interface - optional. */
    PFNPDMDEVIOCTL      pfnIOCtl;
    /** Power on notification - optional. */
    PFNPDMDEVPOWERON    pfnPowerOn;
    /** Reset notification - optional. */
    PFNPDMDEVRESET      pfnReset;
    /** Suspend notification  - optional. */
    PFNPDMDEVSUSPEND    pfnSuspend;
    /** Resume notification - optional. */
    PFNPDMDEVRESUME     pfnResume;
    /** Attach command - optional. */
    PFNPDMDEVATTACH     pfnAttach;
    /** Detach notification - optional. */
    PFNPDMDEVDETACH     pfnDetach;
    /** Query a LUN base interface - optional. */
    PFNPDMDEVQUERYINTERFACE pfnQueryInterface;
    /** Init complete notification - optional. */
    PFNPDMDEVINITCOMPLETE   pfnInitComplete;
    /** Power off notification - optional. */
    PFNPDMDEVPOWEROFF   pfnPowerOff;
    /** @todo */
    PFNRT               pfnSoftReset;
    /** Initialization safty marker. */
    uint32_t            u32VersionEnd;
} PDMDEVREG;
/** Pointer to a PDM Device Structure. */
typedef PDMDEVREG *PPDMDEVREG;
/** Const pointer to a PDM Device Structure. */
typedef PDMDEVREG const *PCPDMDEVREG;

/** Current DEVREG version number. */
#define PDM_DEVREG_VERSION                      0xc0020000

/** PDM Device Flags.
 * @{ */
/** This flag is used to indicate that the device has a RC component. */
#define PDM_DEVREG_FLAGS_RC                     0x00000001
/** This flag is used to indicate that the device has a R0 component. */
#define PDM_DEVREG_FLAGS_R0                     0x00000002

/** @def PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT
 * The bit count for the current host. */
#if HC_ARCH_BITS == 32
# define PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT     0x00000010
#elif HC_ARCH_BITS == 64
# define PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT     0x00000020
#else
# error Unsupported HC_ARCH_BITS value.
#endif
/** The host bit count mask. */
#define PDM_DEVREG_FLAGS_HOST_BITS_MASK         0x00000030

/** The device support only 32-bit guests. */
#define PDM_DEVREG_FLAGS_GUEST_BITS_32          0x00000100
/** The device support only 64-bit guests. */
#define PDM_DEVREG_FLAGS_GUEST_BITS_64          0x00000200
/** The device support both 32-bit & 64-bit guests. */
#define PDM_DEVREG_FLAGS_GUEST_BITS_32_64       0x00000300
/** @def PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT
 * The guest bit count for the current compilation. */
#if GC_ARCH_BITS == 32
# define PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT    PDM_DEVREG_FLAGS_GUEST_BITS_32
#elif GC_ARCH_BITS == 64
# define PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT    PDM_DEVREG_FLAGS_GUEST_BITS_32_64
#else
# error Unsupported GC_ARCH_BITS value.
#endif
/** The guest bit count mask. */
#define PDM_DEVREG_FLAGS_GUEST_BITS_MASK        0x00000300

/** A convenience. */
#define PDM_DEVREG_FLAGS_DEFAULT_BITS           (PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT | PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT)

/** Indicates that the devices support PAE36 on a 32-bit guest. */
#define PDM_DEVREG_FLAGS_PAE36                  0x00001000

/** Indicates that the device needs to be notified before the drivers when suspending. */
#define PDM_DEVREG_FLAGS_FIRST_SUSPEND_NOTIFICATION 0x00002000

/** Indicates that the device needs to be notified before the drivers when powering off. */
#define PDM_DEVREG_FLAGS_FIRST_POWEROFF_NOTIFICATION 0x00004000
/** @} */


/** PDM Device Classes.
 * The order is important, lower bit earlier instantiation.
 * @{ */
/** Architecture device. */
#define PDM_DEVREG_CLASS_ARCH                   RT_BIT(0)
/** Architecture BIOS device. */
#define PDM_DEVREG_CLASS_ARCH_BIOS              RT_BIT(1)
/** PCI bus brigde. */
#define PDM_DEVREG_CLASS_BUS_PCI                RT_BIT(2)
/** ISA bus brigde. */
#define PDM_DEVREG_CLASS_BUS_ISA                RT_BIT(3)
/** Input device (mouse, keyboard, joystick, HID, ...). */
#define PDM_DEVREG_CLASS_INPUT                  RT_BIT(4)
/** Interrupt controller (PIC). */
#define PDM_DEVREG_CLASS_PIC                    RT_BIT(5)
/** Interval controoler (PIT). */
#define PDM_DEVREG_CLASS_PIT                    RT_BIT(6)
/** RTC/CMOS. */
#define PDM_DEVREG_CLASS_RTC                    RT_BIT(7)
/** DMA controller. */
#define PDM_DEVREG_CLASS_DMA                    RT_BIT(8)
/** VMM Device. */
#define PDM_DEVREG_CLASS_VMM_DEV                RT_BIT(9)
/** Graphics device, like VGA. */
#define PDM_DEVREG_CLASS_GRAPHICS               RT_BIT(10)
/** Storage controller device. */
#define PDM_DEVREG_CLASS_STORAGE                RT_BIT(11)
/** Network interface controller. */
#define PDM_DEVREG_CLASS_NETWORK                RT_BIT(12)
/** Audio. */
#define PDM_DEVREG_CLASS_AUDIO                  RT_BIT(13)
/** USB HIC. */
#define PDM_DEVREG_CLASS_BUS_USB                RT_BIT(14)
/** ACPI. */
#define PDM_DEVREG_CLASS_ACPI                   RT_BIT(15)
/** Serial controller device. */
#define PDM_DEVREG_CLASS_SERIAL                 RT_BIT(16)
/** Parallel controller device */
#define PDM_DEVREG_CLASS_PARALLEL               RT_BIT(17)
/** Misc devices (always last). */
#define PDM_DEVREG_CLASS_MISC                   RT_BIT(31)
/** @} */


/** @name IRQ Level for use with the *SetIrq APIs.
 * @{
 */
/** Assert the IRQ (can assume value 1). */
#define PDM_IRQ_LEVEL_HIGH                      RT_BIT(0)
/** Deassert the IRQ (can assume value 0). */
#define PDM_IRQ_LEVEL_LOW           0
/** flip-flop - assert and then deassert it again immediately. */
#define PDM_IRQ_LEVEL_FLIP_FLOP                 (RT_BIT(1) | PDM_IRQ_LEVEL_HIGH)
/** @} */


/**
 * PCI Bus registration structure.
 * All the callbacks, except the PCIBIOS hack, are working on PCI devices.
 */
typedef struct PDMPCIBUSREG
{
    /** Structure version number. PDM_PCIBUSREG_VERSION defines the current version. */
    uint32_t            u32Version;

    /**
     * Registers the device with the default PCI bus.
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance of the PCI Bus.
     * @param   pPciDev         The PCI device structure.
     *                          Any PCI enabled device must keep this in it's instance data!
     *                          Fill in the PCI data config before registration, please.
     * @param   pszName         Pointer to device name (permanent, readonly). For debugging, not unique.
     * @param   iDev            The device number ((dev << 3) | function) the device should have on the bus.
     *                          If negative, the pci bus device will assign one.
     */
    DECLR3CALLBACKMEMBER(int, pfnRegisterR3,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, const char *pszName, int iDev));

    /**
     * Registers a I/O region (memory mapped or I/O ports) for a PCI device.
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance of the PCI Bus.
     * @param   pPciDev         The PCI device structure.
     * @param   iRegion         The region number.
     * @param   cbRegion        Size of the region.
     * @param   iType           PCI_ADDRESS_SPACE_MEM, PCI_ADDRESS_SPACE_IO or PCI_ADDRESS_SPACE_MEM_PREFETCH.
     * @param   pfnCallback     Callback for doing the mapping.
     */
    DECLR3CALLBACKMEMBER(int, pfnIORegionRegisterR3,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iRegion, uint32_t cbRegion, PCIADDRESSSPACE enmType, PFNPCIIOREGIONMAP pfnCallback));

    /**
     * Register PCI configuration space read/write callbacks.
     *
     * @param   pDevIns         Device instance of the PCI Bus.
     * @param   pPciDev         The PCI device structure.
     * @param   pfnRead         Pointer to the user defined PCI config read function.
     * @param   ppfnReadOld     Pointer to function pointer which will receive the old (default)
     *                          PCI config read function. This way, user can decide when (and if)
     *                          to call default PCI config read function. Can be NULL.
     * @param   pfnWrite        Pointer to the user defined PCI config write function.
     * @param   pfnWriteOld     Pointer to function pointer which will receive the old (default)
     *                          PCI config write function. This way, user can decide when (and if)
     *                          to call default PCI config write function. Can be NULL.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(void, pfnSetConfigCallbacksR3,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PFNPCICONFIGREAD pfnRead, PPFNPCICONFIGREAD ppfnReadOld,
                                                        PFNPCICONFIGWRITE pfnWrite, PPFNPCICONFIGWRITE ppfnWriteOld));

    /**
     * Set the IRQ for a PCI device.
     *
     * @param   pDevIns         Device instance of the PCI Bus.
     * @param   pPciDev         The PCI device structure.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level. See the PDM_IRQ_LEVEL_* \#defines.
     */
    DECLR3CALLBACKMEMBER(void, pfnSetIrqR3,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel));

    /**
     * Saves a state of the PCI device.
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance of the PCI Bus.
     * @param   pPciDev         Pointer to PCI device.
     * @param   pSSMHandle      The handle to save the state to.
     */
    DECLR3CALLBACKMEMBER(int, pfnSaveExecR3,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PSSMHANDLE pSSMHandle));

    /**
     * Loads a saved PCI device state.
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance of the PCI Bus.
     * @param   pPciDev         Pointer to PCI device.
     * @param   pSSMHandle      The handle to the saved state.
     */
    DECLR3CALLBACKMEMBER(int, pfnLoadExecR3,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PSSMHANDLE pSSMHandle));

    /**
     * Called to perform the job of the bios.
     * This is only called for the first PCI Bus - it is expected to
     * service all the PCI buses.
     *
     * @returns VBox status.
     * @param   pDevIns     Device instance of the first bus.
     */
    DECLR3CALLBACKMEMBER(int, pfnFakePCIBIOSR3,(PPDMDEVINS pDevIns));

    /** The name of the SetIrq RC entry point. */
    const char         *pszSetIrqRC;

    /** The name of the SetIrq R0 entry point. */
    const char         *pszSetIrqR0;

} PDMPCIBUSREG;
/** Pointer to a PCI bus registration structure. */
typedef PDMPCIBUSREG *PPDMPCIBUSREG;

/** Current PDMPCIBUSREG version number. */
#define PDM_PCIBUSREG_VERSION   0xd0020000

/**
 * PCI Bus RC helpers.
 */
typedef struct PDMPCIHLPRC
{
    /** Structure version. PDM_PCIHLPRC_VERSION defines the current version. */
    uint32_t                    u32Version;

    /**
     * Set an ISA IRQ.
     *
     * @param   pDevIns         PCI device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level. See the PDM_IRQ_LEVEL_* \#defines.
     * @thread  EMT only.
     */
    DECLRCCALLBACKMEMBER(void,  pfnIsaSetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Set an I/O-APIC IRQ.
     *
     * @param   pDevIns         PCI device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level. See the PDM_IRQ_LEVEL_* \#defines.
     * @thread  EMT only.
     */
    DECLRCCALLBACKMEMBER(void,  pfnIoApicSetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Acquires the PDM lock.
     *
     * @returns VINF_SUCCESS on success.
     * @returns rc if we failed to acquire the lock.
     * @param   pDevIns         The PCI device instance.
     * @param   rc              What to return if we fail to acquire the lock.
     */
    DECLRCCALLBACKMEMBER(int,   pfnLock,(PPDMDEVINS pDevIns, int rc));

    /**
     * Releases the PDM lock.
     *
     * @param   pDevIns         The PCI device instance.
     */
    DECLRCCALLBACKMEMBER(void,  pfnUnlock,(PPDMDEVINS pDevIns));

    /** Just a safety precaution. */
    uint32_t                    u32TheEnd;
} PDMPCIHLPRC;
/** Pointer to PCI helpers. */
typedef RCPTRTYPE(PDMPCIHLPRC *) PPDMPCIHLPRC;
/** Pointer to const PCI helpers. */
typedef RCPTRTYPE(const PDMPCIHLPRC *) PCPDMPCIHLPRC;

/** Current PDMPCIHLPR3 version number. */
#define PDM_PCIHLPRC_VERSION  0xe1010000


/**
 * PCI Bus R0 helpers.
 */
typedef struct PDMPCIHLPR0
{
    /** Structure version. PDM_PCIHLPR0_VERSION defines the current version. */
    uint32_t                    u32Version;

    /**
     * Set an ISA IRQ.
     *
     * @param   pDevIns         PCI device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level. See the PDM_IRQ_LEVEL_* \#defines.
     * @thread  EMT only.
     */
    DECLR0CALLBACKMEMBER(void,  pfnIsaSetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Set an I/O-APIC IRQ.
     *
     * @param   pDevIns         PCI device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level. See the PDM_IRQ_LEVEL_* \#defines.
     * @thread  EMT only.
     */
    DECLR0CALLBACKMEMBER(void,  pfnIoApicSetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Acquires the PDM lock.
     *
     * @returns VINF_SUCCESS on success.
     * @returns rc if we failed to acquire the lock.
     * @param   pDevIns         The PCI device instance.
     * @param   rc              What to return if we fail to acquire the lock.
     */
    DECLR0CALLBACKMEMBER(int,   pfnLock,(PPDMDEVINS pDevIns, int rc));

    /**
     * Releases the PDM lock.
     *
     * @param   pDevIns         The PCI device instance.
     */
    DECLR0CALLBACKMEMBER(void,  pfnUnlock,(PPDMDEVINS pDevIns));

    /** Just a safety precaution. */
    uint32_t                    u32TheEnd;
} PDMPCIHLPR0;
/** Pointer to PCI helpers. */
typedef R0PTRTYPE(PDMPCIHLPR0 *) PPDMPCIHLPR0;
/** Pointer to const PCI helpers. */
typedef R0PTRTYPE(const PDMPCIHLPR0 *) PCPDMPCIHLPR0;

/** Current PDMPCIHLPR0 version number. */
#define PDM_PCIHLPR0_VERSION  0xe1010000

/**
 * PCI device helpers.
 */
typedef struct PDMPCIHLPR3
{
    /** Structure version. PDM_PCIHLPR3_VERSION defines the current version. */
    uint32_t                    u32Version;

    /**
     * Set an ISA IRQ.
     *
     * @param   pDevIns         The PCI device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level. See the PDM_IRQ_LEVEL_* \#defines.
     * @thread  EMT only.
     */
    DECLR3CALLBACKMEMBER(void,  pfnIsaSetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Set an I/O-APIC IRQ.
     *
     * @param   pDevIns         The PCI device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level. See the PDM_IRQ_LEVEL_* \#defines.
     * @thread  EMT only.
     */
    DECLR3CALLBACKMEMBER(void,  pfnIoApicSetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Checks if the given address is an MMIO2 base address or not.
     *
     * @returns true/false accordingly.
     * @param   pDevIns         The PCI device instance.
     * @param   pOwner          The owner of the memory, optional.
     * @param   GCPhys          The address to check.
     */
    DECLR3CALLBACKMEMBER(bool,  pfnIsMMIO2Base,(PPDMDEVINS pDevIns, PPDMDEVINS pOwner, RTGCPHYS GCPhys));

    /**
     * Gets the address of the RC PCI Bus helpers.
     *
     * This should be called at both construction and relocation time
     * to obtain the correct address of the RC helpers.
     *
     * @returns RC pointer to the PCI Bus helpers.
     * @param   pDevIns         Device instance of the PCI Bus.
     * @thread  EMT only.
     */
    DECLR3CALLBACKMEMBER(PCPDMPCIHLPRC, pfnGetRCHelpers,(PPDMDEVINS pDevIns));

    /**
     * Gets the address of the R0 PCI Bus helpers.
     *
     * This should be called at both construction and relocation time
     * to obtain the correct address of the R0 helpers.
     *
     * @returns R0 pointer to the PCI Bus helpers.
     * @param   pDevIns         Device instance of the PCI Bus.
     * @thread  EMT only.
     */
    DECLR3CALLBACKMEMBER(PCPDMPCIHLPR0, pfnGetR0Helpers,(PPDMDEVINS pDevIns));

    /**
     * Acquires the PDM lock.
     *
     * @returns VINF_SUCCESS on success.
     * @returns Fatal error on failure.
     * @param   pDevIns         The PCI device instance.
     * @param   rc              Dummy for making the interface identical to the RC and R0 versions.
     */
    DECLR3CALLBACKMEMBER(int,   pfnLock,(PPDMDEVINS pDevIns, int rc));

    /**
     * Releases the PDM lock.
     *
     * @param   pDevIns         The PCI device instance.
     */
    DECLR3CALLBACKMEMBER(void,  pfnUnlock,(PPDMDEVINS pDevIns));

    /** Just a safety precaution. */
    uint32_t                    u32TheEnd;
} PDMPCIHLPR3;
/** Pointer to PCI helpers. */
typedef R3PTRTYPE(PDMPCIHLPR3 *) PPDMPCIHLPR3;
/** Pointer to const PCI helpers. */
typedef R3PTRTYPE(const PDMPCIHLPR3 *) PCPDMPCIHLPR3;

/** Current PDMPCIHLPR3 version number. */
#define PDM_PCIHLPR3_VERSION  0xf1020000


/**
 * Programmable Interrupt Controller registration structure.
 */
typedef struct PDMPICREG
{
    /** Structure version number. PDM_PICREG_VERSION defines the current version. */
    uint32_t            u32Version;

    /**
     * Set the an IRQ.
     *
     * @param   pDevIns         Device instance of the PIC.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level. See the PDM_IRQ_LEVEL_* \#defines.
     */
    DECLR3CALLBACKMEMBER(void, pfnSetIrqR3,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Get a pending interrupt.
     *
     * @returns Pending interrupt number.
     * @param   pDevIns         Device instance of the PIC.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetInterruptR3,(PPDMDEVINS pDevIns));

    /** The name of the RC SetIrq entry point. */
    const char         *pszSetIrqRC;
    /** The name of the RC GetInterrupt entry point. */
    const char         *pszGetInterruptRC;

    /** The name of the R0 SetIrq entry point. */
    const char         *pszSetIrqR0;
    /** The name of the R0 GetInterrupt entry point. */
    const char         *pszGetInterruptR0;
} PDMPICREG;
/** Pointer to a PIC registration structure. */
typedef PDMPICREG *PPDMPICREG;

/** Current PDMPICREG version number. */
#define PDM_PICREG_VERSION      0xe0020000

/**
 * PIC RC helpers.
 */
typedef struct PDMPICHLPRC
{
    /** Structure version. PDM_PICHLPRC_VERSION defines the current version. */
    uint32_t                u32Version;

    /**
     * Set the interrupt force action flag.
     *
     * @param   pDevIns         Device instance of the PIC.
     */
    DECLRCCALLBACKMEMBER(void, pfnSetInterruptFF,(PPDMDEVINS pDevIns));

    /**
     * Clear the interrupt force action flag.
     *
     * @param   pDevIns         Device instance of the PIC.
     */
    DECLRCCALLBACKMEMBER(void, pfnClearInterruptFF,(PPDMDEVINS pDevIns));

    /**
     * Acquires the PDM lock.
     *
     * @returns VINF_SUCCESS on success.
     * @returns rc if we failed to acquire the lock.
     * @param   pDevIns         The PIC device instance.
     * @param   rc              What to return if we fail to acquire the lock.
     */
    DECLRCCALLBACKMEMBER(int,   pfnLock,(PPDMDEVINS pDevIns, int rc));

    /**
     * Releases the PDM lock.
     *
     * @param   pDevIns         The PIC device instance.
     */
    DECLRCCALLBACKMEMBER(void,  pfnUnlock,(PPDMDEVINS pDevIns));

    /** Just a safety precaution. */
    uint32_t                u32TheEnd;
} PDMPICHLPRC;

/** Pointer to PIC RC helpers. */
typedef RCPTRTYPE(PDMPICHLPRC *) PPDMPICHLPRC;
/** Pointer to const PIC RC helpers. */
typedef RCPTRTYPE(const PDMPICHLPRC *) PCPDMPICHLPRC;

/** Current PDMPICHLPRC version number. */
#define PDM_PICHLPRC_VERSION  0xfc010000


/**
 * PIC R0 helpers.
 */
typedef struct PDMPICHLPR0
{
    /** Structure version. PDM_PICHLPR0_VERSION defines the current version. */
    uint32_t                u32Version;

    /**
     * Set the interrupt force action flag.
     *
     * @param   pDevIns         Device instance of the PIC.
     */
    DECLR0CALLBACKMEMBER(void, pfnSetInterruptFF,(PPDMDEVINS pDevIns));

    /**
     * Clear the interrupt force action flag.
     *
     * @param   pDevIns         Device instance of the PIC.
     */
    DECLR0CALLBACKMEMBER(void, pfnClearInterruptFF,(PPDMDEVINS pDevIns));

    /**
     * Acquires the PDM lock.
     *
     * @returns VINF_SUCCESS on success.
     * @returns rc if we failed to acquire the lock.
     * @param   pDevIns         The PIC device instance.
     * @param   rc              What to return if we fail to acquire the lock.
     */
    DECLR0CALLBACKMEMBER(int,   pfnLock,(PPDMDEVINS pDevIns, int rc));

    /**
     * Releases the PDM lock.
     *
     * @param   pDevIns         The PCI device instance.
     */
    DECLR0CALLBACKMEMBER(void,  pfnUnlock,(PPDMDEVINS pDevIns));

    /** Just a safety precaution. */
    uint32_t                u32TheEnd;
} PDMPICHLPR0;

/** Pointer to PIC R0 helpers. */
typedef R0PTRTYPE(PDMPICHLPR0 *) PPDMPICHLPR0;
/** Pointer to const PIC R0 helpers. */
typedef R0PTRTYPE(const PDMPICHLPR0 *) PCPDMPICHLPR0;

/** Current PDMPICHLPR0 version number. */
#define PDM_PICHLPR0_VERSION  0xfc010000

/**
 * PIC R3 helpers.
 */
typedef struct PDMPICHLPR3
{
    /** Structure version. PDM_PICHLP_VERSION defines the current version. */
    uint32_t                u32Version;

    /**
     * Set the interrupt force action flag.
     *
     * @param   pDevIns         Device instance of the PIC.
     */
    DECLR3CALLBACKMEMBER(void, pfnSetInterruptFF,(PPDMDEVINS pDevIns));

    /**
     * Clear the interrupt force action flag.
     *
     * @param   pDevIns         Device instance of the PIC.
     */
    DECLR3CALLBACKMEMBER(void, pfnClearInterruptFF,(PPDMDEVINS pDevIns));

    /**
     * Acquires the PDM lock.
     *
     * @returns VINF_SUCCESS on success.
     * @returns Fatal error on failure.
     * @param   pDevIns         The PIC device instance.
     * @param   rc              Dummy for making the interface identical to the RC and R0 versions.
     */
    DECLR3CALLBACKMEMBER(int,   pfnLock,(PPDMDEVINS pDevIns, int rc));

    /**
     * Releases the PDM lock.
     *
     * @param   pDevIns         The PIC device instance.
     */
    DECLR3CALLBACKMEMBER(void,  pfnUnlock,(PPDMDEVINS pDevIns));

    /**
     * Gets the address of the RC PIC helpers.
     *
     * This should be called at both construction and relocation time
     * to obtain the correct address of the RC helpers.
     *
     * @returns RC pointer to the PIC helpers.
     * @param   pDevIns         Device instance of the PIC.
     */
    DECLR3CALLBACKMEMBER(PCPDMPICHLPRC, pfnGetRCHelpers,(PPDMDEVINS pDevIns));

    /**
     * Gets the address of the R0 PIC helpers.
     *
     * This should be called at both construction and relocation time
     * to obtain the correct address of the R0 helpers.
     *
     * @returns R0 pointer to the PIC helpers.
     * @param   pDevIns         Device instance of the PIC.
     */
    DECLR3CALLBACKMEMBER(PCPDMPICHLPR0, pfnGetR0Helpers,(PPDMDEVINS pDevIns));

    /** Just a safety precaution. */
    uint32_t                u32TheEnd;
} PDMPICHLPR3;

/** Pointer to PIC R3 helpers. */
typedef R3PTRTYPE(PDMPICHLPR3 *) PPDMPICHLPR3;
/** Pointer to const PIC R3 helpers. */
typedef R3PTRTYPE(const PDMPICHLPR3 *) PCPDMPICHLPR3;

/** Current PDMPICHLPR3 version number. */
#define PDM_PICHLPR3_VERSION  0xf0010000



/**
 * Advanced Programmable Interrupt Controller registration structure.
 */
typedef struct PDMAPICREG
{
    /** Structure version number. PDM_APICREG_VERSION defines the current version. */
    uint32_t            u32Version;

    /**
     * Get a pending interrupt.
     *
     * @returns Pending interrupt number.
     * @param   pDevIns         Device instance of the APIC.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetInterruptR3,(PPDMDEVINS pDevIns));

    /**
     * Check if the APIC has a pending interrupt/if a TPR change would active one
     *
     * @returns Pending interrupt yes/no
     * @param   pDevIns         Device instance of the APIC.
     */
    DECLR3CALLBACKMEMBER(bool, pfnHasPendingIrqR3,(PPDMDEVINS pDevIns));

    /**
     * Set the APIC base.
     *
     * @param   pDevIns         Device instance of the APIC.
     * @param   u64Base         The new base.
     */
    DECLR3CALLBACKMEMBER(void, pfnSetBaseR3,(PPDMDEVINS pDevIns, uint64_t u64Base));

    /**
     * Get the APIC base.
     *
     * @returns Current base.
     * @param   pDevIns         Device instance of the APIC.
     */
    DECLR3CALLBACKMEMBER(uint64_t, pfnGetBaseR3,(PPDMDEVINS pDevIns));

    /**
     * Set the TPR (task priority register).
     *
     * @param   pDevIns         Device instance of the APIC.
     * @param   u8TPR           The new TPR.
     */
    DECLR3CALLBACKMEMBER(void, pfnSetTPRR3,(PPDMDEVINS pDevIns, uint8_t u8TPR));

    /**
     * Get the TPR (task priority register).
     *
     * @returns The current TPR.
     * @param   pDevIns         Device instance of the APIC.
     * @param   pfPending       Pending interrupt state (out).
     */
    DECLR3CALLBACKMEMBER(uint8_t, pfnGetTPRR3,(PPDMDEVINS pDevIns));

    /**
     * Write MSR in APIC range.
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance of the APIC.
     * @param   idCpu           Target CPU.
     * @param   u32Reg          MSR to write.
     * @param   u64Value        Value to write.
     */
    DECLR3CALLBACKMEMBER(int, pfnWriteMSRR3, (PPDMDEVINS pDevIns, VMCPUID idCpu, uint32_t u32Reg, uint64_t u64Value));

    /**
     * Read MSR in APIC range.
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance of the APIC.
     * @param   idCpu           Target CPU.
     * @param   u32Reg          MSR to read.
     * @param   pu64Value       Value read.
     */
    DECLR3CALLBACKMEMBER(int, pfnReadMSRR3, (PPDMDEVINS pDevIns, VMCPUID idCpu, uint32_t u32Reg, uint64_t *pu64Value));

    /**
     * Private interface between the IOAPIC and APIC.
     *
     * This is a low-level, APIC/IOAPIC implementation specific interface
     * which is registered with PDM only because it makes life so much
     * simpler right now (GC bits). This is a bad bad hack! The correct
     * way of doing this would involve some way of querying GC interfaces
     * and relocating them. Perhaps doing some kind of device init in GC...
     *
     * @returns The current TPR.
     * @param   pDevIns         Device instance of the APIC.
     * @param   u8Dest          See APIC implementation.
     * @param   u8DestMode      See APIC implementation.
     * @param   u8DeliveryMode  See APIC implementation.
     * @param   iVector         See APIC implementation.
     * @param   u8Polarity      See APIC implementation.
     * @param   u8TriggerMode   See APIC implementation.
     */
    DECLR3CALLBACKMEMBER(void, pfnBusDeliverR3,(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode));

    /** The name of the RC GetInterrupt entry point. */
    const char         *pszGetInterruptRC;
    /** The name of the RC HasPendingIrq entry point. */
    const char         *pszHasPendingIrqRC;
    /** The name of the RC SetBase entry point. */
    const char         *pszSetBaseRC;
    /** The name of the RC GetBase entry point. */
    const char         *pszGetBaseRC;
    /** The name of the RC SetTPR entry point. */
    const char         *pszSetTPRRC;
    /** The name of the RC GetTPR entry point. */
    const char         *pszGetTPRRC;
    /** The name of the RC WriteMSR entry point. */
    const char         *pszWriteMSRRC;
    /** The name of the RC ReadMSR entry point. */
    const char         *pszReadMSRRC;
    /** The name of the RC BusDeliver entry point. */
    const char         *pszBusDeliverRC;

    /** The name of the R0 GetInterrupt entry point. */
    const char         *pszGetInterruptR0;
    /** The name of the R0 HasPendingIrq entry point. */
    const char         *pszHasPendingIrqR0;
    /** The name of the R0 SetBase entry point. */
    const char         *pszSetBaseR0;
    /** The name of the R0 GetBase entry point. */
    const char         *pszGetBaseR0;
    /** The name of the R0 SetTPR entry point. */
    const char         *pszSetTPRR0;
    /** The name of the R0 GetTPR entry point. */
    const char         *pszGetTPRR0;
    /** The name of the R0 WriteMSR entry point. */
    const char         *pszWriteMSRR0;
    /** The name of the R0 ReadMSR entry point. */
    const char         *pszReadMSRR0;
    /** The name of the R0 BusDeliver entry point. */
    const char         *pszBusDeliverR0;

} PDMAPICREG;
/** Pointer to an APIC registration structure. */
typedef PDMAPICREG *PPDMAPICREG;

/** Current PDMAPICREG version number. */
#define PDM_APICREG_VERSION     0x70010000


/**
 * APIC version argument for pfnChangeFeature.
 */
typedef enum PDMAPICVERSION
{
    /** Invalid 0 entry. */
    PDMAPICVERSION_INVALID = 0,
    /** No APIC. */
    PDMAPICVERSION_NONE,
    /** Standard APIC (X86_CPUID_FEATURE_EDX_APIC). */
    PDMAPICVERSION_APIC,
    /** Intel X2APIC (X86_CPUID_FEATURE_ECX_X2APIC). */
    PDMAPICVERSION_X2APIC,
    /** The usual 32-bit paranoia. */
    PDMAPICVERSION_32BIT_HACK = 0x7fffffff
} PDMAPICVERSION;


/**
 * APIC RC helpers.
 */
typedef struct PDMAPICHLPRC
{
    /** Structure version. PDM_APICHLPRC_VERSION defines the current version. */
    uint32_t                u32Version;

    /**
     * Set the interrupt force action flag.
     *
     * @param   pDevIns         Device instance of the APIC.
     * @param   idCpu           Virtual CPU to set flag upon.
     */
    DECLRCCALLBACKMEMBER(void, pfnSetInterruptFF,(PPDMDEVINS pDevIns, VMCPUID idCpu));

    /**
     * Clear the interrupt force action flag.
     *
     * @param   pDevIns         Device instance of the APIC.
     * @param   idCpu           Virtual CPU to clear flag upon.
     */
    DECLRCCALLBACKMEMBER(void, pfnClearInterruptFF,(PPDMDEVINS pDevIns, VMCPUID idCpu));

    /**
     * Modifies APIC-related bits in the CPUID feature mask.
     *
     * @param   pDevIns         Device instance of the APIC.
     * @param   enmVersion      Supported APIC version.
     */
    DECLRCCALLBACKMEMBER(void, pfnChangeFeature,(PPDMDEVINS pDevIns, PDMAPICVERSION enmVersion));

    /**
     * Acquires the PDM lock.
     *
     * @returns VINF_SUCCESS on success.
     * @returns rc if we failed to acquire the lock.
     * @param   pDevIns         The APIC device instance.
     * @param   rc              What to return if we fail to acquire the lock.
     */
    DECLRCCALLBACKMEMBER(int,   pfnLock,(PPDMDEVINS pDevIns, int rc));

    /**
     * Releases the PDM lock.
     *
     * @param   pDevIns         The APIC device instance.
     */
    DECLRCCALLBACKMEMBER(void,  pfnUnlock,(PPDMDEVINS pDevIns));

    /**
     * Get the virtual CPU id corresponding to the current EMT.
     *
     * @param   pDevIns         The APIC device instance.
     */
    DECLRCCALLBACKMEMBER(VMCPUID, pfnGetCpuId,(PPDMDEVINS pDevIns));

    /**
     * Sends SIPI to given virtual CPU.
     *
     * @param   pDevIns         The APIC device instance.
     * @param   idCpu           Virtual CPU to perform SIPI on
     * @param   iVector         SIPI vector
     */
    DECLRCCALLBACKMEMBER(void,  pfnSendSipi,(PPDMDEVINS pDevIns, VMCPUID idCpu, int iVector));

    /** Just a safety precaution. */
    uint32_t                u32TheEnd;
} PDMAPICHLPRC;
/** Pointer to APIC GC helpers. */
typedef RCPTRTYPE(PDMAPICHLPRC *) PPDMAPICHLPRC;
/** Pointer to const APIC helpers. */
typedef RCPTRTYPE(const PDMAPICHLPRC *) PCPDMAPICHLPRC;

/** Current PDMAPICHLPRC version number. */
#define PDM_APICHLPRC_VERSION   0x60010000


/**
 * APIC R0 helpers.
 */
typedef struct PDMAPICHLPR0
{
    /** Structure version. PDM_APICHLPR0_VERSION defines the current version. */
    uint32_t                u32Version;

    /**
     * Set the interrupt force action flag.
     *
     * @param   pDevIns         Device instance of the APIC.
     * @param   idCpu           Virtual CPU to set flag upon.
     */
    DECLR0CALLBACKMEMBER(void, pfnSetInterruptFF,(PPDMDEVINS pDevIns, VMCPUID idCpu));

    /**
     * Clear the interrupt force action flag.
     *
     * @param   pDevIns         Device instance of the APIC.
     * @param   idCpu           Virtual CPU to clear flag upon.
     */
    DECLR0CALLBACKMEMBER(void, pfnClearInterruptFF,(PPDMDEVINS pDevIns, VMCPUID idCpu));

    /**
     * Modifies APIC-related bits in the CPUID feature mask.
     *
     * @param   pDevIns         Device instance of the APIC.
     * @param   enmVersion      Supported APIC version.
     */
    DECLR0CALLBACKMEMBER(void, pfnChangeFeature,(PPDMDEVINS pDevIns, PDMAPICVERSION enmVersion));

    /**
     * Acquires the PDM lock.
     *
     * @returns VINF_SUCCESS on success.
     * @returns rc if we failed to acquire the lock.
     * @param   pDevIns         The APIC device instance.
     * @param   rc              What to return if we fail to acquire the lock.
     */
    DECLR0CALLBACKMEMBER(int,   pfnLock,(PPDMDEVINS pDevIns, int rc));

    /**
     * Releases the PDM lock.
     *
     * @param   pDevIns         The APIC device instance.
     */
    DECLR0CALLBACKMEMBER(void,  pfnUnlock,(PPDMDEVINS pDevIns));

    /**
     * Get the virtual CPU id corresponding to the current EMT.
     *
     * @param   pDevIns         The APIC device instance.
     */
    DECLR0CALLBACKMEMBER(VMCPUID, pfnGetCpuId,(PPDMDEVINS pDevIns));

    /**
     * Sends SIPI to given virtual CPU.
     *
     * @param   pDevIns         The APIC device instance.
     * @param   idCpu           Virtual CPU to perform SIPI on
     * @param   iVector         SIPI vector
     */
    DECLR0CALLBACKMEMBER(void,    pfnSendSipi,(PPDMDEVINS pDevIns, VMCPUID idCpu, int iVector));

    /** Just a safety precaution. */
    uint32_t                u32TheEnd;
} PDMAPICHLPR0;
/** Pointer to APIC GC helpers. */
typedef RCPTRTYPE(PDMAPICHLPR0 *) PPDMAPICHLPR0;
/** Pointer to const APIC helpers. */
typedef R0PTRTYPE(const PDMAPICHLPR0 *) PCPDMAPICHLPR0;

/** Current PDMAPICHLPR0 version number. */
#define PDM_APICHLPR0_VERSION   0x60010000

/**
 * APIC R3 helpers.
 */
typedef struct PDMAPICHLPR3
{
    /** Structure version. PDM_APICHLPR3_VERSION defines the current version. */
    uint32_t                u32Version;

    /**
     * Set the interrupt force action flag.
     *
     * @param   pDevIns         Device instance of the APIC.
     * @param   idCpu           Virtual CPU to set flag upon.
     */
    DECLR3CALLBACKMEMBER(void, pfnSetInterruptFF,(PPDMDEVINS pDevIns, VMCPUID idCpu));

    /**
     * Clear the interrupt force action flag.
     *
     * @param   pDevIns         Device instance of the APIC.
     * @param   idCpu           Virtual CPU to clear flag upon.
     */
    DECLR3CALLBACKMEMBER(void, pfnClearInterruptFF,(PPDMDEVINS pDevIns, VMCPUID idCpu));

    /**
     * Modifies APIC-related bits in the CPUID feature mask.
     *
     * @param   pDevIns         Device instance of the APIC.
     * @param   enmVersion      Supported APIC version.
     */
    DECLR3CALLBACKMEMBER(void, pfnChangeFeature,(PPDMDEVINS pDevIns, PDMAPICVERSION enmVersion));

    /**
     * Acquires the PDM lock.
     *
     * @returns VINF_SUCCESS on success.
     * @returns Fatal error on failure.
     * @param   pDevIns         The APIC device instance.
     * @param   rc              Dummy for making the interface identical to the GC and R0 versions.
     */
    DECLR3CALLBACKMEMBER(int,   pfnLock,(PPDMDEVINS pDevIns, int rc));

    /**
     * Releases the PDM lock.
     *
     * @param   pDevIns         The APIC device instance.
     */
    DECLR3CALLBACKMEMBER(void,  pfnUnlock,(PPDMDEVINS pDevIns));

    /**
     * Get the virtual CPU id corresponding to the current EMT.
     *
     * @param   pDevIns         The APIC device instance.
     */
    DECLR3CALLBACKMEMBER(VMCPUID, pfnGetCpuId,(PPDMDEVINS pDevIns));

    /**
     * Sends SIPI to given virtual CPU.
     *
     * @param   pDevIns         The APIC device instance.
     * @param   idCpu           Virtual CPU to perform SIPI on
     * @param   iVector         SIPI vector
     */
    DECLR3CALLBACKMEMBER(void,    pfnSendSipi,(PPDMDEVINS pDevIns, VMCPUID idCpu, int iVector));

    /**
     * Gets the address of the RC APIC helpers.
     *
     * This should be called at both construction and relocation time
     * to obtain the correct address of the RC helpers.
     *
     * @returns GC pointer to the APIC helpers.
     * @param   pDevIns         Device instance of the APIC.
     */
    DECLR3CALLBACKMEMBER(PCPDMAPICHLPRC, pfnGetRCHelpers,(PPDMDEVINS pDevIns));

    /**
     * Gets the address of the R0 APIC helpers.
     *
     * This should be called at both construction and relocation time
     * to obtain the correct address of the R0 helpers.
     *
     * @returns R0 pointer to the APIC helpers.
     * @param   pDevIns         Device instance of the APIC.
     */
    DECLR3CALLBACKMEMBER(PCPDMAPICHLPR0, pfnGetR0Helpers,(PPDMDEVINS pDevIns));

    /** Just a safety precaution. */
    uint32_t                u32TheEnd;
} PDMAPICHLPR3;
/** Pointer to APIC helpers. */
typedef R3PTRTYPE(PDMAPICHLPR3 *) PPDMAPICHLPR3;
/** Pointer to const APIC helpers. */
typedef R3PTRTYPE(const PDMAPICHLPR3 *) PCPDMAPICHLPR3;

/** Current PDMAPICHLP version number. */
#define PDM_APICHLPR3_VERSION  0xfd010000


/**
 * I/O APIC registration structure.
 */
typedef struct PDMIOAPICREG
{
    /** Struct version+magic number (PDM_IOAPICREG_VERSION). */
    uint32_t            u32Version;

    /**
     * Set the an IRQ.
     *
     * @param   pDevIns         Device instance of the I/O APIC.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level. See the PDM_IRQ_LEVEL_* \#defines.
     */
    DECLR3CALLBACKMEMBER(void, pfnSetIrqR3,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /** The name of the GC SetIrq entry point. */
    const char         *pszSetIrqRC;

    /** The name of the R0 SetIrq entry point. */
    const char         *pszSetIrqR0;
} PDMIOAPICREG;
/** Pointer to an APIC registration structure. */
typedef PDMIOAPICREG *PPDMIOAPICREG;

/** Current PDMAPICREG version number. */
#define PDM_IOAPICREG_VERSION     0x50010000


/**
 * IOAPIC RC helpers.
 */
typedef struct PDMIOAPICHLPRC
{
    /** Structure version. PDM_IOAPICHLPRC_VERSION defines the current version. */
    uint32_t                u32Version;

    /**
     * Private interface between the IOAPIC and APIC.
     *
     * See comments about this hack on PDMAPICREG::pfnBusDeliverR3.
     *
     * @returns The current TPR.
     * @param   pDevIns         Device instance of the IOAPIC.
     * @param   u8Dest          See APIC implementation.
     * @param   u8DestMode      See APIC implementation.
     * @param   u8DeliveryMode  See APIC implementation.
     * @param   iVector         See APIC implementation.
     * @param   u8Polarity      See APIC implementation.
     * @param   u8TriggerMode   See APIC implementation.
     */
    DECLRCCALLBACKMEMBER(void, pfnApicBusDeliver,(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                  uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode));

    /**
     * Acquires the PDM lock.
     *
     * @returns VINF_SUCCESS on success.
     * @returns rc if we failed to acquire the lock.
     * @param   pDevIns         The IOAPIC device instance.
     * @param   rc              What to return if we fail to acquire the lock.
     */
    DECLRCCALLBACKMEMBER(int,   pfnLock,(PPDMDEVINS pDevIns, int rc));

    /**
     * Releases the PDM lock.
     *
     * @param   pDevIns         The IOAPIC device instance.
     */
    DECLRCCALLBACKMEMBER(void,  pfnUnlock,(PPDMDEVINS pDevIns));

    /** Just a safety precaution. */
    uint32_t                u32TheEnd;
} PDMIOAPICHLPRC;
/** Pointer to IOAPIC RC helpers. */
typedef RCPTRTYPE(PDMIOAPICHLPRC *) PPDMIOAPICHLPRC;
/** Pointer to const IOAPIC helpers. */
typedef RCPTRTYPE(const PDMIOAPICHLPRC *) PCPDMIOAPICHLPRC;

/** Current PDMIOAPICHLPRC version number. */
#define PDM_IOAPICHLPRC_VERSION   0xfe010000


/**
 * IOAPIC R0 helpers.
 */
typedef struct PDMIOAPICHLPR0
{
    /** Structure version. PDM_IOAPICHLPR0_VERSION defines the current version. */
    uint32_t                u32Version;

    /**
     * Private interface between the IOAPIC and APIC.
     *
     * See comments about this hack on PDMAPICREG::pfnBusDeliverR3.
     *
     * @returns The current TPR.
     * @param   pDevIns         Device instance of the IOAPIC.
     * @param   u8Dest          See APIC implementation.
     * @param   u8DestMode      See APIC implementation.
     * @param   u8DeliveryMode  See APIC implementation.
     * @param   iVector         See APIC implementation.
     * @param   u8Polarity      See APIC implementation.
     * @param   u8TriggerMode   See APIC implementation.
     */
    DECLR0CALLBACKMEMBER(void, pfnApicBusDeliver,(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                  uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode));

    /**
     * Acquires the PDM lock.
     *
     * @returns VINF_SUCCESS on success.
     * @returns rc if we failed to acquire the lock.
     * @param   pDevIns         The IOAPIC device instance.
     * @param   rc              What to return if we fail to acquire the lock.
     */
    DECLR0CALLBACKMEMBER(int,   pfnLock,(PPDMDEVINS pDevIns, int rc));

    /**
     * Releases the PDM lock.
     *
     * @param   pDevIns         The IOAPIC device instance.
     */
    DECLR0CALLBACKMEMBER(void,  pfnUnlock,(PPDMDEVINS pDevIns));

    /** Just a safety precaution. */
    uint32_t                u32TheEnd;
} PDMIOAPICHLPR0;
/** Pointer to IOAPIC R0 helpers. */
typedef R0PTRTYPE(PDMIOAPICHLPR0 *) PPDMIOAPICHLPR0;
/** Pointer to const IOAPIC helpers. */
typedef R0PTRTYPE(const PDMIOAPICHLPR0 *) PCPDMIOAPICHLPR0;

/** Current PDMIOAPICHLPR0 version number. */
#define PDM_IOAPICHLPR0_VERSION   0xfe010000

/**
 * IOAPIC R3 helpers.
 */
typedef struct PDMIOAPICHLPR3
{
    /** Structure version. PDM_IOAPICHLPR3_VERSION defines the current version. */
    uint32_t                u32Version;

    /**
     * Private interface between the IOAPIC and APIC.
     *
     * See comments about this hack on PDMAPICREG::pfnBusDeliverR3.
     *
     * @returns The current TPR.
     * @param   pDevIns         Device instance of the IOAPIC.
     * @param   u8Dest          See APIC implementation.
     * @param   u8DestMode      See APIC implementation.
     * @param   u8DeliveryMode  See APIC implementation.
     * @param   iVector         See APIC implementation.
     * @param   u8Polarity      See APIC implementation.
     * @param   u8TriggerMode   See APIC implementation.
     */
    DECLR3CALLBACKMEMBER(void, pfnApicBusDeliver,(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                  uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode));

    /**
     * Acquires the PDM lock.
     *
     * @returns VINF_SUCCESS on success.
     * @returns Fatal error on failure.
     * @param   pDevIns         The IOAPIC device instance.
     * @param   rc              Dummy for making the interface identical to the GC and R0 versions.
     */
    DECLR3CALLBACKMEMBER(int,   pfnLock,(PPDMDEVINS pDevIns, int rc));

    /**
     * Releases the PDM lock.
     *
     * @param   pDevIns         The IOAPIC device instance.
     */
    DECLR3CALLBACKMEMBER(void,  pfnUnlock,(PPDMDEVINS pDevIns));

    /**
     * Gets the address of the RC IOAPIC helpers.
     *
     * This should be called at both construction and relocation time
     * to obtain the correct address of the RC helpers.
     *
     * @returns RC pointer to the IOAPIC helpers.
     * @param   pDevIns         Device instance of the IOAPIC.
     */
    DECLR3CALLBACKMEMBER(PCPDMIOAPICHLPRC, pfnGetRCHelpers,(PPDMDEVINS pDevIns));

    /**
     * Gets the address of the R0 IOAPIC helpers.
     *
     * This should be called at both construction and relocation time
     * to obtain the correct address of the R0 helpers.
     *
     * @returns R0 pointer to the IOAPIC helpers.
     * @param   pDevIns         Device instance of the IOAPIC.
     */
    DECLR3CALLBACKMEMBER(PCPDMIOAPICHLPR0, pfnGetR0Helpers,(PPDMDEVINS pDevIns));

    /** Just a safety precaution. */
    uint32_t                u32TheEnd;
} PDMIOAPICHLPR3;
/** Pointer to IOAPIC R3 helpers. */
typedef R3PTRTYPE(PDMIOAPICHLPR3 *) PPDMIOAPICHLPR3;
/** Pointer to const IOAPIC helpers. */
typedef R3PTRTYPE(const PDMIOAPICHLPR3 *) PCPDMIOAPICHLPR3;

/** Current PDMIOAPICHLPR3 version number. */
#define PDM_IOAPICHLPR3_VERSION   0xff010000



#ifdef IN_RING3

/**
 * DMA Transfer Handler.
 *
 * @returns             Number of bytes transferred.
 * @param pDevIns       Device instance of the DMA.
 * @param pvUser        User pointer.
 * @param uChannel      Channel number.
 * @param off           DMA position.
 * @param cb            Block size.
 */
typedef DECLCALLBACK(uint32_t) FNDMATRANSFERHANDLER(PPDMDEVINS pDevIns, void *pvUser, unsigned uChannel, uint32_t off, uint32_t cb);
/** Pointer to a FNDMATRANSFERHANDLER(). */
typedef FNDMATRANSFERHANDLER *PFNDMATRANSFERHANDLER;

/**
 * DMA Controller registration structure.
 */
typedef struct PDMDMAREG
{
    /** Structure version number. PDM_DMACREG_VERSION defines the current version. */
    uint32_t            u32Version;

    /**
     * Execute pending transfers.
     *
     * @returns A more work indiciator. I.e. 'true' if there is more to be done, and 'false' if all is done.
     * @param pDevIns           Device instance of the DMAC.
     */
    DECLR3CALLBACKMEMBER(bool, pfnRun,(PPDMDEVINS pDevIns));

    /**
     * Register transfer function for DMA channel.
     *
     * @param pDevIns               Device instance of the DMAC.
     * @param uChannel              Channel number.
     * @param pfnTransferHandler    Device specific transfer function.
     * @param pvUSer                User pointer to be passed to the callback.
     */
    DECLR3CALLBACKMEMBER(void, pfnRegister,(PPDMDEVINS pDevIns, unsigned uChannel, PFNDMATRANSFERHANDLER pfnTransferHandler, void *pvUser));

    /**
     * Read memory
     *
     * @returns Number of bytes read.
     * @param pDevIns           Device instance of the DMAC.
     * @param pvBuffer          Pointer to target buffer.
     * @param off               DMA position.
     * @param cbBlock           Block size.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnReadMemory,(PPDMDEVINS pDevIns, unsigned uChannel, void *pvBuffer, uint32_t off, uint32_t cbBlock));

    /**
     * Write memory
     *
     * @returns Number of bytes written.
     * @param pDevIns           Device instance of the DMAC.
     * @param pvBuffer          Memory to write.
     * @param off               DMA position.
     * @param cbBlock           Block size.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnWriteMemory,(PPDMDEVINS pDevIns, unsigned uChannel, const void *pvBuffer, uint32_t off, uint32_t cbBlock));

    /**
     * Set the DREQ line.
     *
     * @param pDevIns           Device instance of the DMAC.
     * @param uChannel          Channel number.
     * @param uLevel            Level of the line.
     */
    DECLR3CALLBACKMEMBER(void, pfnSetDREQ,(PPDMDEVINS pDevIns, unsigned uChannel, unsigned uLevel));

    /**
     * Get channel mode
     *
     * @returns                 Channel mode.
     * @param pDevIns           Device instance of the DMAC.
     * @param uChannel          Channel number.
     */
    DECLR3CALLBACKMEMBER(uint8_t, pfnGetChannelMode,(PPDMDEVINS pDevIns, unsigned uChannel));

} PDMDMACREG;
/** Pointer to a DMAC registration structure. */
typedef PDMDMACREG *PPDMDMACREG;

/** Current PDMDMACREG version number. */
#define PDM_DMACREG_VERSION     0xf5010000


/**
 * DMA Controller device helpers.
 */
typedef struct PDMDMACHLP
{
    /** Structure version. PDM_DMACHLP_VERSION defines the current version. */
    uint32_t                u32Version;

    /* to-be-defined */

} PDMDMACHLP;
/** Pointer to DMAC helpers. */
typedef PDMDMACHLP *PPDMDMACHLP;
/** Pointer to const DMAC helpers. */
typedef const PDMDMACHLP *PCPDMDMACHLP;

/** Current PDMDMACHLP version number. */
#define PDM_DMACHLP_VERSION  0xf6010000

#endif /* IN_RING3 */



/**
 * RTC registration structure.
 */
typedef struct PDMRTCREG
{
    /** Structure version number. PDM_RTCREG_VERSION defines the current version. */
    uint32_t            u32Version;
    uint32_t            u32Alignment;   /**< structure size alignment. */

    /**
     * Write to a CMOS register and update the checksum if necessary.
     *
     * @returns VBox status code.
     * @param   pDevIns     Device instance of the RTC.
     * @param   iReg        The CMOS register index.
     * @param   u8Value     The CMOS register value.
     */
    DECLR3CALLBACKMEMBER(int, pfnWrite,(PPDMDEVINS pDevIns, unsigned iReg, uint8_t u8Value));

    /**
     * Read a CMOS register.
     *
     * @returns VBox status code.
     * @param   pDevIns     Device instance of the RTC.
     * @param   iReg        The CMOS register index.
     * @param   pu8Value    Where to store the CMOS register value.
     */
    DECLR3CALLBACKMEMBER(int, pfnRead,(PPDMDEVINS pDevIns, unsigned iReg, uint8_t *pu8Value));

} PDMRTCREG;
/** Pointer to a RTC registration structure. */
typedef PDMRTCREG *PPDMRTCREG;
/** Pointer to a const RTC registration structure. */
typedef const PDMRTCREG *PCPDMRTCREG;

/** Current PDMRTCREG version number. */
#define PDM_RTCREG_VERSION     0xfa010000


/**
 * RTC device helpers.
 */
typedef struct PDMRTCHLP
{
    /** Structure version. PDM_RTCHLP_VERSION defines the current version. */
    uint32_t                u32Version;

    /* to-be-defined */

} PDMRTCHLP;
/** Pointer to RTC helpers. */
typedef PDMRTCHLP *PPDMRTCHLP;
/** Pointer to const RTC helpers. */
typedef const PDMRTCHLP *PCPDMRTCHLP;

/** Current PDMRTCHLP version number. */
#define PDM_RTCHLP_VERSION  0xf6010000



#ifdef IN_RING3

/**
 * PDM Device API.
 */
typedef struct PDMDEVHLPR3
{
    /** Structure version. PDM_DEVHLP_VERSION defines the current version. */
    uint32_t                        u32Version;

    /**
     * Register a number of I/O ports with a device.
     *
     * These callbacks are of course for the host context (HC).
     * Register HC handlers before guest context (GC) handlers! There must be a
     * HC handler for every GC handler!
     *
     * @returns VBox status.
     * @param   pDevIns             The device instance to register the ports with.
     * @param   Port                First port number in the range.
     * @param   cPorts              Number of ports to register.
     * @param   pvUser              User argument.
     * @param   pfnOut              Pointer to function which is gonna handle OUT operations.
     * @param   pfnIn               Pointer to function which is gonna handle IN operations.
     * @param   pfnOutStr           Pointer to function which is gonna handle string OUT operations.
     * @param   pfnInStr            Pointer to function which is gonna handle string IN operations.
     * @param   pszDesc             Pointer to description string. This must not be freed.
     */
    DECLR3CALLBACKMEMBER(int, pfnIOPortRegister,(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts, RTHCPTR pvUser,
                                                 PFNIOMIOPORTOUT pfnOut, PFNIOMIOPORTIN pfnIn,
                                                 PFNIOMIOPORTOUTSTRING pfnOutStr, PFNIOMIOPORTINSTRING pfnInStr, const char *pszDesc));

    /**
     * Register a number of I/O ports with a device for GC.
     *
     * These callbacks are for the host context (GC).
     * Register host context (HC) handlers before guest context handlers! There must be a
     * HC handler for every GC handler!
     *
     * @returns VBox status.
     * @param   pDevIns             The device instance to register the ports with and which GC module
     *                              to resolve the names against.
     * @param   Port                First port number in the range.
     * @param   cPorts              Number of ports to register.
     * @param   pvUser              User argument.
     * @param   pszOut              Name of the GC function which is gonna handle OUT operations.
     * @param   pszIn               Name of the GC function which is gonna handle IN operations.
     * @param   pszOutStr           Name of the GC function which is gonna handle string OUT operations.
     * @param   pszInStr            Name of the GC function which is gonna handle string IN operations.
     * @param   pszDesc             Pointer to description string. This must not be freed.
     */
    DECLR3CALLBACKMEMBER(int, pfnIOPortRegisterGC,(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts, RTRCPTR pvUser,
                                                   const char *pszOut, const char *pszIn,
                                                   const char *pszOutStr, const char *pszInStr, const char *pszDesc));

    /**
     * Register a number of I/O ports with a device.
     *
     * These callbacks are of course for the ring-0 host context (R0).
     * Register R3 (HC) handlers before R0 (R0) handlers! There must be a R3 (HC) handler for every R0 handler!
     *
     * @returns VBox status.
     * @param   pDevIns             The device instance to register the ports with.
     * @param   Port                First port number in the range.
     * @param   cPorts              Number of ports to register.
     * @param   pvUser              User argument. (if pointer, then it must be in locked memory!)
     * @param   pszOut              Name of the R0 function which is gonna handle OUT operations.
     * @param   pszIn               Name of the R0 function which is gonna handle IN operations.
     * @param   pszOutStr           Name of the R0 function which is gonna handle string OUT operations.
     * @param   pszInStr            Name of the R0 function which is gonna handle string IN operations.
     * @param   pszDesc             Pointer to description string. This must not be freed.
     */
    DECLR3CALLBACKMEMBER(int, pfnIOPortRegisterR0,(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts, RTR0PTR pvUser,
                                                   const char *pszOut, const char *pszIn,
                                                   const char *pszOutStr, const char *pszInStr, const char *pszDesc));

    /**
     * Deregister I/O ports.
     *
     * This naturally affects both guest context (GC), ring-0 (R0) and ring-3 (R3/HC) handlers.
     *
     * @returns VBox status.
     * @param   pDevIns             The device instance owning the ports.
     * @param   Port                First port number in the range.
     * @param   cPorts              Number of ports to deregister.
     */
    DECLR3CALLBACKMEMBER(int, pfnIOPortDeregister,(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts));

    /**
     * Register a Memory Mapped I/O (MMIO) region.
     *
     * These callbacks are of course for the host context (HC).
     * Register HC handlers before guest context (GC) handlers! There must be a
     * HC handler for every GC handler!
     *
     * @returns VBox status.
     * @param   pDevIns             The device instance to register the MMIO with.
     * @param   GCPhysStart         First physical address in the range.
     * @param   cbRange             The size of the range (in bytes).
     * @param   pvUser              User argument.
     * @param   pfnWrite            Pointer to function which is gonna handle Write operations.
     * @param   pfnRead             Pointer to function which is gonna handle Read operations.
     * @param   pfnFill             Pointer to function which is gonna handle Fill/memset operations. (optional)
     * @param   pszDesc             Pointer to description string. This must not be freed.
     */
    DECLR3CALLBACKMEMBER(int, pfnMMIORegister,(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTHCPTR pvUser,
                                               PFNIOMMMIOWRITE pfnWrite, PFNIOMMMIOREAD pfnRead, PFNIOMMMIOFILL pfnFill,
                                               const char *pszDesc));

    /**
     * Register a Memory Mapped I/O (MMIO) region for GC.
     *
     * These callbacks are for the guest context (GC).
     * Register host context (HC) handlers before guest context handlers! There must be a
     * HC handler for every GC handler!
     *
     * @returns VBox status.
     * @param   pDevIns             The device instance to register the MMIO with.
     * @param   GCPhysStart         First physical address in the range.
     * @param   cbRange             The size of the range (in bytes).
     * @param   pvUser              User argument.
     * @param   pszWrite            Name of the GC function which is gonna handle Write operations.
     * @param   pszRead             Name of the GC function which is gonna handle Read operations.
     * @param   pszFill             Name of the GC function which is gonna handle Fill/memset operations. (optional)
     * @param   pszDesc             Obsolete. NULL is fine.
     * @todo    Remove pszDesc in the next major revision of PDMDEVHLPR3.
     */
    DECLR3CALLBACKMEMBER(int, pfnMMIORegisterGC,(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTGCPTR pvUser,
                                                 const char *pszWrite, const char *pszRead, const char *pszFill,
                                                 const char *pszDesc));

    /**
     * Register a Memory Mapped I/O (MMIO) region for R0.
     *
     * These callbacks are for the ring-0 host context (R0).
     * Register R3 (HC) handlers before R0 handlers! There must be a R3 handler for every R0 handler!
     *
     * @returns VBox status.
     * @param   pDevIns             The device instance to register the MMIO with.
     * @param   GCPhysStart         First physical address in the range.
     * @param   cbRange             The size of the range (in bytes).
     * @param   pvUser              User argument. (if pointer, then it must be in locked memory!)
     * @param   pszWrite            Name of the GC function which is gonna handle Write operations.
     * @param   pszRead             Name of the GC function which is gonna handle Read operations.
     * @param   pszFill             Name of the GC function which is gonna handle Fill/memset operations. (optional)
     * @param   pszDesc             Obsolete. NULL is fine.
     * @todo    Remove pszDesc in the next major revision of PDMDEVHLPR3.
     */
    DECLR3CALLBACKMEMBER(int, pfnMMIORegisterR0,(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTR0PTR pvUser,
                                                 const char *pszWrite, const char *pszRead, const char *pszFill,
                                                 const char *pszDesc));

    /**
     * Deregister a Memory Mapped I/O (MMIO) region.
     *
     * This naturally affects both guest context (GC), ring-0 (R0) and ring-3 (R3/HC) handlers.
     *
     * @returns VBox status.
     * @param   pDevIns             The device instance owning the MMIO region(s).
     * @param   GCPhysStart         First physical address in the range.
     * @param   cbRange             The size of the range (in bytes).
     */
    DECLR3CALLBACKMEMBER(int, pfnMMIODeregister,(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange));

    /**
     * Register a ROM (BIOS) region.
     *
     * It goes without saying that this is read-only memory. The memory region must be
     * in unassigned memory. I.e. from the top of the address space or on the PC in
     * the 0xa0000-0xfffff range.
     *
     * @returns VBox status.
     * @param   pDevIns             The device instance owning the ROM region.
     * @param   GCPhysStart         First physical address in the range.
     *                              Must be page aligned!
     * @param   cbRange             The size of the range (in bytes).
     *                              Must be page aligned!
     * @param   pvBinary            Pointer to the binary data backing the ROM image.
     *                              This must be cbRange bytes big.
     *                              It will be copied and doesn't have to stick around if fShadow is clear.
     * @param   fFlags              Shadow ROM flags, PGMPHYS_ROM_FLAGS_* in pgm.h.
     * @param   pszDesc             Pointer to description string. This must not be freed.
     *
     * @remark  There is no way to remove the rom, automatically on device cleanup or
     *          manually from the device yet. At present I doubt we need such features...
     */
    DECLR3CALLBACKMEMBER(int, pfnROMRegister,(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, const void *pvBinary, uint32_t fFlags, const char *pszDesc));

    /**
     * Register a save state data unit.
     *
     * @returns VBox status.
     * @param   pDevIns             Device instance.
     * @param   pszName             Data unit name.
     * @param   u32Instance         The instance identifier of the data unit.
     *                              This must together with the name be unique.
     * @param   u32Version          Data layout version number.
     * @param   cbGuess             The approximate amount of data in the unit.
     *                              Only for progress indicators.
     * @param   pfnSavePrep         Prepare save callback, optional.
     * @param   pfnSaveExec         Execute save callback, optional.
     * @param   pfnSaveDone         Done save callback, optional.
     * @param   pfnLoadPrep         Prepare load callback, optional.
     * @param   pfnLoadExec         Execute load callback, optional.
     * @param   pfnLoadDone         Done load callback, optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnSSMRegister,(PPDMDEVINS pDevIns, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
                                              PFNSSMDEVSAVEPREP pfnSavePrep, PFNSSMDEVSAVEEXEC pfnSaveExec, PFNSSMDEVSAVEDONE pfnSaveDone,
                                              PFNSSMDEVLOADPREP pfnLoadPrep, PFNSSMDEVLOADEXEC pfnLoadExec, PFNSSMDEVLOADDONE pfnLoadDone));

    /**
     * Creates a timer.
     *
     * @returns VBox status.
     * @param   pDevIns             Device instance.
     * @param   enmClock            The clock to use on this timer.
     * @param   pfnCallback         Callback function.
     * @param   pszDesc             Pointer to description string which must stay around
     *                              until the timer is fully destroyed (i.e. a bit after TMTimerDestroy()).
     * @param   ppTimer             Where to store the timer on success.
     */
    DECLR3CALLBACKMEMBER(int, pfnTMTimerCreate,(PPDMDEVINS pDevIns, TMCLOCK enmClock, PFNTMTIMERDEV pfnCallback, const char *pszDesc, PPTMTIMERR3 ppTimer));

    /**
     * Creates an external timer.
     *
     * @returns timer pointer
     * @param   pDevIns             Device instance.
     * @param   enmClock            The clock to use on this timer.
     * @param   pfnCallback         Callback function.
     * @param   pvUser              User pointer
     * @param   pszDesc             Pointer to description string which must stay around
     *                              until the timer is fully destroyed (i.e. a bit after TMTimerDestroy()).
     */
    DECLR3CALLBACKMEMBER(PTMTIMERR3, pfnTMTimerCreateExternal,(PPDMDEVINS pDevIns, TMCLOCK enmClock, PFNTMTIMEREXT pfnCallback, void *pvUser, const char *pszDesc));

    /**
     * Registers the device with the default PCI bus.
     *
     * @returns VBox status code.
     * @param   pDevIns             Device instance.
     * @param   pPciDev             The PCI device structure.
     *                              Any PCI enabled device must keep this in it's instance data!
     *                              Fill in the PCI data config before registration, please.
     * @remark  This is the simple interface, a Ex interface will be created if
     *          more features are needed later.
     */
    DECLR3CALLBACKMEMBER(int, pfnPCIRegister,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev));

    /**
     * Registers a I/O region (memory mapped or I/O ports) for a PCI device.
     *
     * @returns VBox status code.
     * @param   pDevIns             Device instance.
     * @param   iRegion             The region number.
     * @param   cbRegion            Size of the region.
     * @param   enmType             PCI_ADDRESS_SPACE_MEM, PCI_ADDRESS_SPACE_IO or PCI_ADDRESS_SPACE_MEM_PREFETCH.
     * @param   pfnCallback         Callback for doing the mapping.
     */
    DECLR3CALLBACKMEMBER(int, pfnPCIIORegionRegister,(PPDMDEVINS pDevIns, int iRegion, uint32_t cbRegion, PCIADDRESSSPACE enmType, PFNPCIIOREGIONMAP pfnCallback));

    /**
     * Register PCI configuration space read/write callbacks.
     *
     * @param   pDevIns             Device instance.
     * @param   pPciDev             The PCI device structure.
     *                              If NULL the default PCI device for this device instance is used.
     * @param   pfnRead             Pointer to the user defined PCI config read function.
     * @param   ppfnReadOld         Pointer to function pointer which will receive the old (default)
     *                              PCI config read function. This way, user can decide when (and if)
     *                              to call default PCI config read function. Can be NULL.
     * @param   pfnWrite            Pointer to the user defined PCI config write function.
     * @param   pfnWriteOld         Pointer to function pointer which will receive the old (default)
     *                              PCI config write function. This way, user can decide when (and if)
     *                              to call default PCI config write function. Can be NULL.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(void, pfnPCISetConfigCallbacks,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PFNPCICONFIGREAD pfnRead, PPFNPCICONFIGREAD ppfnReadOld,
                                                         PFNPCICONFIGWRITE pfnWrite, PPFNPCICONFIGWRITE ppfnWriteOld));

    /**
     * Set the IRQ for a PCI device.
     *
     * @param   pDevIns             Device instance.
     * @param   iIrq                IRQ number to set.
     * @param   iLevel              IRQ level. See the PDM_IRQ_LEVEL_* \#defines.
     * @thread  Any thread, but will involve the emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnPCISetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Set the IRQ for a PCI device, but don't wait for EMT to process
     * the request when not called from EMT.
     *
     * @param   pDevIns             Device instance.
     * @param   iIrq                IRQ number to set.
     * @param   iLevel              IRQ level.
     * @thread  Any thread, but will involve the emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnPCISetIrqNoWait,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Set ISA IRQ for a device.
     *
     * @param   pDevIns             Device instance.
     * @param   iIrq                IRQ number to set.
     * @param   iLevel              IRQ level. See the PDM_IRQ_LEVEL_* \#defines.
     * @thread  Any thread, but will involve the emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnISASetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Set the ISA IRQ for a device, but don't wait for EMT to process
     * the request when not called from EMT.
     *
     * @param   pDevIns             Device instance.
     * @param   iIrq                IRQ number to set.
     * @param   iLevel              IRQ level. See the PDM_IRQ_LEVEL_* \#defines.
     * @thread  Any thread, but will involve the emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnISASetIrqNoWait,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Attaches a driver (chain) to the device.
     *
     * The first call for a LUN this will serve as a registartion of the LUN. The pBaseInterface and
     * the pszDesc string will be registered with that LUN and kept around for PDMR3QueryDeviceLun().
     *
     * @returns VBox status code.
     * @param   pDevIns             Device instance.
     * @param   iLun                The logical unit to attach.
     * @param   pBaseInterface      Pointer to the base interface for that LUN. (device side / down)
     * @param   ppBaseInterface     Where to store the pointer to the base interface. (driver side / up)
     * @param   pszDesc             Pointer to a string describing the LUN. This string must remain valid
     *                              for the live of the device instance.
     */
    DECLR3CALLBACKMEMBER(int, pfnDriverAttach,(PPDMDEVINS pDevIns, RTUINT iLun, PPDMIBASE pBaseInterface, PPDMIBASE *ppBaseInterface, const char *pszDesc));

    /**
     * Allocate memory which is associated with current VM instance
     * and automatically freed on it's destruction.
     *
     * @returns Pointer to allocated memory. The memory is *NOT* zero-ed.
     * @param   pDevIns             Device instance.
     * @param   cb                  Number of bytes to allocate.
     */
    DECLR3CALLBACKMEMBER(void *, pfnMMHeapAlloc,(PPDMDEVINS pDevIns, size_t cb));

    /**
     * Allocate memory which is associated with current VM instance
     * and automatically freed on it's destruction. The memory is ZEROed.
     *
     * @returns Pointer to allocated memory. The memory is *NOT* zero-ed.
     * @param   pDevIns             Device instance.
     * @param   cb                  Number of bytes to allocate.
     */
    DECLR3CALLBACKMEMBER(void *, pfnMMHeapAllocZ,(PPDMDEVINS pDevIns, size_t cb));

    /**
     * Free memory allocated with pfnMMHeapAlloc() and pfnMMHeapAllocZ().
     *
     * @param   pDevIns             Device instance.
     * @param   pv                  Pointer to the memory to free.
     */
    DECLR3CALLBACKMEMBER(void, pfnMMHeapFree,(PPDMDEVINS pDevIns, void *pv));

    /**
     * Set the VM error message
     *
     * @returns rc.
     * @param   pDevIns             Device instance.
     * @param   rc                  VBox status code.
     * @param   RT_SRC_POS_DECL     Use RT_SRC_POS.
     * @param   pszFormat           Error message format string.
     * @param   ...                 Error message arguments.
     */
    DECLR3CALLBACKMEMBER(int, pfnVMSetError,(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...));

    /**
     * Set the VM error message
     *
     * @returns rc.
     * @param   pDevIns             Device instance.
     * @param   rc                  VBox status code.
     * @param   RT_SRC_POS_DECL     Use RT_SRC_POS.
     * @param   pszFormat           Error message format string.
     * @param   va                  Error message arguments.
     */
    DECLR3CALLBACKMEMBER(int, pfnVMSetErrorV,(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va));

    /**
     * Set the VM runtime error message
     *
     * @returns VBox status code.
     * @param   pDevIns             Device instance.
     * @param   fFlags              The action flags. See VMSETRTERR_FLAGS_*.
     * @param   pszErrorId          Error ID string.
     * @param   pszFormat           Error message format string.
     * @param   ...                 Error message arguments.
     */
    DECLR3CALLBACKMEMBER(int, pfnVMSetRuntimeError,(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, ...));

    /**
     * Set the VM runtime error message
     *
     * @returns VBox status code.
     * @param   pDevIns             Device instance.
     * @param   fFlags              The action flags. See VMSETRTERR_FLAGS_*.
     * @param   pszErrorId          Error ID string.
     * @param   pszFormat           Error message format string.
     * @param   va                  Error message arguments.
     */
    DECLR3CALLBACKMEMBER(int, pfnVMSetRuntimeErrorV,(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, va_list va));

    /**
     * Assert that the current thread is the emulation thread.
     *
     * @returns True if correct.
     * @returns False if wrong.
     * @param   pDevIns             Device instance.
     * @param   pszFile             Filename of the assertion location.
     * @param   iLine               The linenumber of the assertion location.
     * @param   pszFunction         Function of the assertion location.
     */
    DECLR3CALLBACKMEMBER(bool, pfnAssertEMT,(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction));

    /**
     * Assert that the current thread is NOT the emulation thread.
     *
     * @returns True if correct.
     * @returns False if wrong.
     * @param   pDevIns             Device instance.
     * @param   pszFile             Filename of the assertion location.
     * @param   iLine               The linenumber of the assertion location.
     * @param   pszFunction         Function of the assertion location.
     */
    DECLR3CALLBACKMEMBER(bool, pfnAssertOther,(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction));

    /**
     * Stops the VM and enters the debugger to look at the guest state.
     *
     * Use the PDMDeviceDBGFStop() inline function with the RT_SRC_POS macro instead of
     * invoking this function directly.
     *
     * @returns VBox status code which must be passed up to the VMM.
     * @param   pDevIns             Device instance.
     * @param   pszFile             Filename of the assertion location.
     * @param   iLine               The linenumber of the assertion location.
     * @param   pszFunction         Function of the assertion location.
     * @param   pszFormat           Message. (optional)
     * @param   args                Message parameters.
     */
    DECLR3CALLBACKMEMBER(int, pfnDBGFStopV,(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction, const char *pszFormat, va_list args));

    /**
     * Register a info handler with DBGF,
     *
     * @returns VBox status code.
     * @param   pDevIns             Device instance.
     * @param   pszName             The identifier of the info.
     * @param   pszDesc             The description of the info and any arguments
     *                              the handler may take.
     * @param   pfnHandler          The handler function to be called to display the
     *                              info.
     */
    DECLR3CALLBACKMEMBER(int, pfnDBGFInfoRegister,(PPDMDEVINS pDevIns, const char *pszName, const char *pszDesc, PFNDBGFHANDLERDEV pfnHandler));

    /**
     * Registers a statistics sample if statistics are enabled.
     *
     * @param   pDevIns             Device instance of the DMA.
     * @param   pvSample            Pointer to the sample.
     * @param   enmType             Sample type. This indicates what pvSample is
     *                              pointing at.
     * @param   pszName             Sample name. The name is on this form
     *                              "/<component>/<sample>". Further nesting is
     *                              possible.
     * @param   enmUnit             Sample unit.
     * @param   pszDesc             Sample description.
     */
    DECLR3CALLBACKMEMBER(void, pfnSTAMRegister,(PPDMDEVINS pDevIns, void *pvSample, STAMTYPE enmType, const char *pszName, STAMUNIT enmUnit, const char *pszDesc));

    /**
     * Same as pfnSTAMRegister except that the name is specified in a
     * RTStrPrintf like fashion.
     *
     * @returns VBox status.
     * @param   pDevIns             Device instance of the DMA.
     * @param   pvSample            Pointer to the sample.
     * @param   enmType             Sample type. This indicates what pvSample is
     *                              pointing at.
     * @param   enmVisibility       Visibility type specifying whether unused
     *                              statistics should be visible or not.
     * @param   enmUnit             Sample unit.
     * @param   pszDesc             Sample description.
     * @param   pszName             The sample name format string.
     * @param   ...                 Arguments to the format string.
     */
    DECLR3CALLBACKMEMBER(void, pfnSTAMRegisterF,(PPDMDEVINS pDevIns, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility,
                                                 STAMUNIT enmUnit, const char *pszDesc, const char *pszName, ...));

    /**
     * Same as pfnSTAMRegister except that the name is specified in a
     * RTStrPrintfV like fashion.
     *
     * @returns VBox status.
     * @param   pDevIns             Device instance of the DMA.
     * @param   pvSample            Pointer to the sample.
     * @param   enmType             Sample type. This indicates what pvSample is
     *                              pointing at.
     * @param   enmVisibility       Visibility type specifying whether unused
     *                              statistics should be visible or not.
     * @param   enmUnit             Sample unit.
     * @param   pszDesc             Sample description.
     * @param   pszName             The sample name format string.
     * @param   args                Arguments to the format string.
     */
    DECLR3CALLBACKMEMBER(void, pfnSTAMRegisterV,(PPDMDEVINS pDevIns, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility,
                                                 STAMUNIT enmUnit, const char *pszDesc, const char *pszName, va_list args));

    /**
     * Register the RTC device.
     *
     * @returns VBox status code.
     * @param   pDevIns             Device instance.
     * @param   pRtcReg             Pointer to a RTC registration structure.
     * @param   ppRtcHlp            Where to store the pointer to the helper
     *                              functions.
     */
    DECLR3CALLBACKMEMBER(int, pfnRTCRegister,(PPDMDEVINS pDevIns, PCPDMRTCREG pRtcReg, PCPDMRTCHLP *ppRtcHlp));

    /**
     * Create a queue.
     *
     * @returns VBox status code.
     * @param   pDevIns             The device instance.
     * @param   cbItem              The size of a queue item.
     * @param   cItems              The number of items in the queue.
     * @param   cMilliesInterval    The number of milliseconds between polling the queue.
     *                              If 0 then the emulation thread will be notified whenever an item arrives.
     * @param   pfnCallback         The consumer function.
     * @param   fGCEnabled          Set if the queue should work in GC too.
     * @param   ppQueue             Where to store the queue handle on success.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnPDMQueueCreate,(PPDMDEVINS pDevIns, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval,
                                                 PFNPDMQUEUEDEV pfnCallback, bool fGCEnabled, PPDMQUEUE *ppQueue));

    /**
     * Initializes a PDM critical section.
     *
     * The PDM critical sections are derived from the IPRT critical sections, but
     * works in GC as well.
     *
     * @returns VBox status code.
     * @param   pDevIns             Device instance.
     * @param   pCritSect           Pointer to the critical section.
     * @param   pszName             The name of the critical section (for
     *                              statistics).
     */
    DECLR3CALLBACKMEMBER(int, pfnCritSectInit,(PPDMDEVINS pDevIns, PPDMCRITSECT pCritSect, const char *pszName));

    /**
     * Get the real world UTC time adjusted for VM lag, user offset and warpdrive.
     *
     * @returns pTime.
     * @param   pDevIns             Device instance.
     * @param   pTime               Where to store the time.
     */
    DECLR3CALLBACKMEMBER(PRTTIMESPEC, pfnUTCNow,(PPDMDEVINS pDevIns, PRTTIMESPEC pTime));

    /**
     * Creates a PDM thread.
     *
     * This differs from the RTThreadCreate() API in that PDM takes care of suspending,
     * resuming, and destroying the thread as the VM state changes.
     *
     * @returns VBox status code.
     * @param   pDevIns             The device instance.
     * @param   ppThread            Where to store the thread 'handle'.
     * @param   pvUser              The user argument to the thread function.
     * @param   pfnThread           The thread function.
     * @param   pfnWakeup           The wakup callback. This is called on the EMT
     *                              thread when a state change is pending.
     * @param   cbStack             See RTThreadCreate.
     * @param   enmType             See RTThreadCreate.
     * @param   pszName             See RTThreadCreate.
     */
    DECLR3CALLBACKMEMBER(int, pfnPDMThreadCreate,(PPDMDEVINS pDevIns, PPPDMTHREAD ppThread, void *pvUser, PFNPDMTHREADDEV pfnThread,
                                                  PFNPDMTHREADWAKEUPDEV pfnWakeup, size_t cbStack, RTTHREADTYPE enmType, const char *pszName));

    /**
     * Convert a guest virtual address to a guest physical address.
     *
     * @returns VBox status code.
     * @param   pDevIns             Device instance.
     * @param   GCPtr               Guest virtual address.
     * @param   pGCPhys             Where to store the GC physical address
     *                              corresponding to GCPtr.
     * @thread  The emulation thread.
     * @remark  Careful with page boundraries.
     */
    DECLR3CALLBACKMEMBER(int, pfnPhysGCPtr2GCPhys, (PPDMDEVINS pDevIns, RTGCPTR GCPtr, PRTGCPHYS pGCPhys));

    /**
     * Gets the VM state.
     *
     * @returns VM state.
     * @param   pDevIns             The device instance.
     * @thread  Any thread (just keep in mind that it's volatile info).
     */
    DECLR3CALLBACKMEMBER(VMSTATE, pfnVMState, (PPDMDEVINS pDevIns));

    /** Space reserved for future members.
     * @{ */
    DECLR3CALLBACKMEMBER(void, pfnReserved4,(void));
    DECLR3CALLBACKMEMBER(void, pfnReserved5,(void));
    DECLR3CALLBACKMEMBER(void, pfnReserved6,(void));
    DECLR3CALLBACKMEMBER(void, pfnReserved7,(void));
    DECLR3CALLBACKMEMBER(void, pfnReserved8,(void));
    DECLR3CALLBACKMEMBER(void, pfnReserved9,(void));
    DECLR3CALLBACKMEMBER(void, pfnReserved10,(void));
    /** @} */


    /** API available to trusted devices only.
     *
     * These APIs are providing unrestricted access to the guest and the VM,
     * or they are interacting intimately with PDM.
     *
     * @{
     */
    /**
     * Gets the VM handle. Restricted API.
     *
     * @returns VM Handle.
     * @param   pDevIns             Device instance.
     */
    DECLR3CALLBACKMEMBER(PVM, pfnGetVM,(PPDMDEVINS pDevIns));

    /**
     * Register the PCI Bus.
     *
     * @returns VBox status code.
     * @param   pDevIns             Device instance.
     * @param   pPciBusReg          Pointer to PCI bus registration structure.
     * @param   ppPciHlpR3          Where to store the pointer to the PCI Bus
     *                              helpers.
     */
    DECLR3CALLBACKMEMBER(int, pfnPCIBusRegister,(PPDMDEVINS pDevIns, PPDMPCIBUSREG pPciBusReg, PCPDMPCIHLPR3 *ppPciHlpR3));

    /**
     * Register the PIC device.
     *
     * @returns VBox status code.
     * @param   pDevIns             Device instance.
     * @param   pPicReg             Pointer to a PIC registration structure.
     * @param   ppPicHlpR3          Where to store the pointer to the PIC HC
     *                              helpers.
     */
    DECLR3CALLBACKMEMBER(int, pfnPICRegister,(PPDMDEVINS pDevIns, PPDMPICREG pPicReg, PCPDMPICHLPR3 *ppPicHlpR3));

    /**
     * Register the APIC device.
     *
     * @returns VBox status code.
     * @param   pDevIns             Device instance.
     * @param   pApicReg            Pointer to a APIC registration structure.
     * @param   ppApicHlpR3         Where to store the pointer to the APIC helpers.
     */
    DECLR3CALLBACKMEMBER(int, pfnAPICRegister,(PPDMDEVINS pDevIns, PPDMAPICREG pApicReg, PCPDMAPICHLPR3 *ppApicHlpR3));

    /**
     * Register the I/O APIC device.
     *
     * @returns VBox status code.
     * @param   pDevIns             Device instance.
     * @param   pIoApicReg          Pointer to a I/O APIC registration structure.
     * @param   ppIoApicHlpR3       Where to store the pointer to the IOAPIC
     *                              helpers.
     */
    DECLR3CALLBACKMEMBER(int, pfnIOAPICRegister,(PPDMDEVINS pDevIns, PPDMIOAPICREG pIoApicReg, PCPDMIOAPICHLPR3 *ppIoApicHlpR3));

    /**
     * Register the DMA device.
     *
     * @returns VBox status code.
     * @param   pDevIns             Device instance.
     * @param   pDmacReg            Pointer to a DMAC registration structure.
     * @param   ppDmacHlp           Where to store the pointer to the DMA helpers.
     */
    DECLR3CALLBACKMEMBER(int, pfnDMACRegister,(PPDMDEVINS pDevIns, PPDMDMACREG pDmacReg, PCPDMDMACHLP *ppDmacHlp));

    /**
     * Read physical memory.
     *
     * @returns VINF_SUCCESS (for now).
     * @param   pDevIns             Device instance.
     * @param   GCPhys              Physical address start reading from.
     * @param   pvBuf               Where to put the read bits.
     * @param   cbRead              How many bytes to read.
     * @thread  Any thread, but the call may involve the emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnPhysRead,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead));

    /**
     * Write to physical memory.
     *
     * @returns VINF_SUCCESS for now, and later maybe VERR_EM_MEMORY.
     * @param   pDevIns             Device instance.
     * @param   GCPhys              Physical address to write to.
     * @param   pvBuf               What to write.
     * @param   cbWrite             How many bytes to write.
     * @thread  Any thread, but the call may involve the emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnPhysWrite,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite));

    /**
     * Requests the mapping of a guest page into ring-3.
     *
     * When you're done with the page, call pfnPhysReleasePageMappingLock() ASAP to
     * release it.
     *
     * This API will assume your intention is to write to the page, and will
     * therefore replace shared and zero pages. If you do not intend to modify the
     * page, use the pfnPhysGCPhys2CCPtrReadOnly() API.
     *
     * @returns VBox status code.
     * @retval  VINF_SUCCESS on success.
     * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical
     *          backing or if the page has any active access handlers. The caller
     *          must fall back on using PGMR3PhysWriteExternal.
     * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
     *
     * @param   pVM                 The VM handle.
     * @param   GCPhys              The guest physical address of the page that
     *                              should be mapped.
     * @param   fFlags              Flags reserved for future use, MBZ.
     * @param   ppv                 Where to store the address corresponding to
     *                              GCPhys.
     * @param   pLock               Where to store the lock information that
     *                              pfnPhysReleasePageMappingLock needs.
     *
     * @remark  Avoid calling this API from within critical sections (other than the
     *          PGM one) because of the deadlock risk when we have to delegating the
     *          task to an EMT.
     * @thread  Any.
     */
    DECLR3CALLBACKMEMBER(int, pfnPhysGCPhys2CCPtr,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, uint32_t fFlags, void **ppv, PPGMPAGEMAPLOCK pLock));

    /**
     * Requests the mapping of a guest page into ring-3, external threads.
     *
     * When you're done with the page, call pfnPhysReleasePageMappingLock() ASAP to
     * release it.
     *
     * @returns VBox status code.
     * @retval  VINF_SUCCESS on success.
     * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical
     *          backing or if the page as an active ALL access handler. The caller
     *          must fall back on using PGMPhysRead.
     * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
     *
     * @param   pDevIns             Device instance.
     * @param   GCPhys              The guest physical address of the page that
     *                              should be mapped.
     * @param   fFlags              Flags reserved for future use, MBZ.
     * @param   ppv                 Where to store the address corresponding to
     *                              GCPhys.
     * @param   pLock               Where to store the lock information that
     *                              pfnPhysReleasePageMappingLock needs.
     *
     * @remark  Avoid calling this API from within critical sections.
     * @thread  Any.
     */
    DECLR3CALLBACKMEMBER(int, pfnPhysGCPhys2CCPtrReadOnly,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, uint32_t fFlags, void const **ppv, PPGMPAGEMAPLOCK pLock));

    /**
     * Release the mapping of a guest page.
     *
     * This is the counter part of pfnPhysGCPhys2CCPtr and
     * pfnPhysGCPhys2CCPtrReadOnly.
     *
     * @param   pDevIns             Device instance.
     * @param   pLock               The lock structure initialized by the mapping
     *                              function.
     */
    DECLR3CALLBACKMEMBER(void, pfnPhysReleasePageMappingLock,(PPDMDEVINS pDevIns, PPGMPAGEMAPLOCK pLock));

    /**
     * Read guest physical memory by virtual address.
     *
     * @param   pDevIns             Device instance.
     * @param   pvDst               Where to put the read bits.
     * @param   GCVirtSrc           Guest virtual address to start reading from.
     * @param   cb                  How many bytes to read.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnPhysReadGCVirt,(PPDMDEVINS pDevIns, void *pvDst, RTGCPTR GCVirtSrc, size_t cb));

    /**
     * Write to guest physical memory by virtual address.
     *
     * @param   pDevIns             Device instance.
     * @param   GCVirtDst           Guest virtual address to write to.
     * @param   pvSrc               What to write.
     * @param   cb                  How many bytes to write.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnPhysWriteGCVirt,(PPDMDEVINS pDevIns, RTGCPTR GCVirtDst, const void *pvSrc, size_t cb));

    /**
     * Checks if the Gate A20 is enabled or not.
     *
     * @returns true if A20 is enabled.
     * @returns false if A20 is disabled.
     * @param   pDevIns             Device instance.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(bool, pfnA20IsEnabled,(PPDMDEVINS pDevIns));

    /**
     * Enables or disables the Gate A20.
     *
     * @param   pDevIns             Device instance.
     * @param   fEnable             Set this flag to enable the Gate A20; clear it
     *                              to disable.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnA20Set,(PPDMDEVINS pDevIns, bool fEnable));

    /**
     * Resets the VM.
     *
     * @returns The appropriate VBox status code to pass around on reset.
     * @param   pDevIns             Device instance.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnVMReset,(PPDMDEVINS pDevIns));

    /**
     * Suspends the VM.
     *
     * @returns The appropriate VBox status code to pass around on suspend.
     * @param   pDevIns             Device instance.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnVMSuspend,(PPDMDEVINS pDevIns));

    /**
     * Power off the VM.
     *
     * @returns The appropriate VBox status code to pass around on power off.
     * @param   pDevIns             Device instance.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnVMPowerOff,(PPDMDEVINS pDevIns));

    /**
     * Register transfer function for DMA channel.
     *
     * @returns VBox status code.
     * @param   pDevIns             Device instance.
     * @param   uChannel            Channel number.
     * @param   pfnTransferHandler  Device specific transfer callback function.
     * @param   pvUser              User pointer to pass to the callback.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnDMARegister,(PPDMDEVINS pDevIns, unsigned uChannel, PFNDMATRANSFERHANDLER pfnTransferHandler, void *pvUser));

    /**
     * Read memory.
     *
     * @returns VBox status code.
     * @param   pDevIns             Device instance.
     * @param   uChannel            Channel number.
     * @param   pvBuffer            Pointer to target buffer.
     * @param   off                 DMA position.
     * @param   cbBlock             Block size.
     * @param   pcbRead             Where to store the number of bytes which was
     *                              read. optional.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnDMAReadMemory,(PPDMDEVINS pDevIns, unsigned uChannel, void *pvBuffer, uint32_t off, uint32_t cbBlock, uint32_t *pcbRead));

    /**
     * Write memory.
     *
     * @returns VBox status code.
     * @param   pDevIns             Device instance.
     * @param   uChannel            Channel number.
     * @param   pvBuffer            Memory to write.
     * @param   off                 DMA position.
     * @param   cbBlock             Block size.
     * @param   pcbWritten          Where to store the number of bytes which was
     *                              written. optional.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnDMAWriteMemory,(PPDMDEVINS pDevIns, unsigned uChannel, const void *pvBuffer, uint32_t off, uint32_t cbBlock, uint32_t *pcbWritten));

    /**
     * Set the DREQ line.
     *
     * @returns VBox status code.
     * @param pDevIns               Device instance.
     * @param uChannel              Channel number.
     * @param uLevel                Level of the line.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnDMASetDREQ,(PPDMDEVINS pDevIns, unsigned uChannel, unsigned uLevel));

    /**
     * Get channel mode.
     *
     * @returns Channel mode. See specs.
     * @param   pDevIns             Device instance.
     * @param   uChannel            Channel number.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(uint8_t, pfnDMAGetChannelMode,(PPDMDEVINS pDevIns, unsigned uChannel));

    /**
     * Schedule DMA execution.
     *
     * @param   pDevIns             Device instance.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnDMASchedule,(PPDMDEVINS pDevIns));

    /**
     * Write CMOS value and update the checksum(s).
     *
     * @returns VBox status code.
     * @param   pDevIns             Device instance.
     * @param   iReg                The CMOS register index.
     * @param   u8Value             The CMOS register value.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnCMOSWrite,(PPDMDEVINS pDevIns, unsigned iReg, uint8_t u8Value));

    /**
     * Read CMOS value.
     *
     * @returns VBox status code.
     * @param   pDevIns             Device instance.
     * @param   iReg                The CMOS register index.
     * @param   pu8Value            Where to store the CMOS register value.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnCMOSRead,(PPDMDEVINS pDevIns, unsigned iReg, uint8_t *pu8Value));

    /**
     * Get the specified CPUID leaf for the virtual CPU associated with the calling
     * thread.
     *
     * @param   pDevIns             Device instance.
     * @param   iLeaf               The CPUID leaf to get.
     * @param   pEax                Where to store the EAX value.
     * @param   pEbx                Where to store the EBX value.
     * @param   pEcx                Where to store the ECX value.
     * @param   pEdx                Where to store the EDX value.
     * @thread  EMT.
     */
    DECLR3CALLBACKMEMBER(void, pfnGetCpuId,(PPDMDEVINS pDevIns, uint32_t iLeaf, uint32_t *pEax, uint32_t *pEbx, uint32_t *pEcx, uint32_t *pEdx));

    /**
     * Changes the protection of shadowed ROM mapping.
     *
     * This is intented for use by the system BIOS, chipset or device in question to
     * change the protection of shadowed ROM code after init and on reset.
     *
     * @param   pDevIns             Device instance.
     * @param   GCPhysStart         Where the mapping starts.
     * @param   cbRange             The size of the mapping.
     * @param   enmProt             The new protection type.
     */
    DECLR3CALLBACKMEMBER(int, pfnROMProtectShadow,(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, PGMROMPROT enmProt));

    /**
     * Allocate and register a MMIO2 region.
     *
     * As mentioned elsewhere, MMIO2 is just RAM spelled differently. It's
     * RAM associated with a device. It is also non-shared memory with a
     * permanent ring-3 mapping and page backing (presently).
     *
     * @returns VBox status.
     * @param   pDevIns             The device instance.
     * @param   iRegion             The region number. Use the PCI region number as
     *                              this must be known to the PCI bus device too. If
     *                              it's not associated with the PCI device, then
     *                              any number up to UINT8_MAX is fine.
     * @param   cb                  The size (in bytes) of the region.
     * @param   fFlags              Reserved for future use, must be zero.
     * @param   ppv                 Where to store the address of the ring-3 mapping
     *                              of the memory.
     * @param   pszDesc             Pointer to description string. This must not be
     *                              freed.
     * @thread  EMT.
     */
    DECLR3CALLBACKMEMBER(int, pfnMMIO2Register,(PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS cb, uint32_t fFlags, void **ppv, const char *pszDesc));

    /**
     * Deregisters and frees a MMIO2 region.
     *
     * Any physical (and virtual) access handlers registered for the region must
     * be deregistered before calling this function.
     *
     * @returns VBox status code.
     * @param   pDevIns             The device instance.
     * @param   iRegion             The region number used during registration.
     * @thread  EMT.
     */
    DECLR3CALLBACKMEMBER(int, pfnMMIO2Deregister,(PPDMDEVINS pDevIns, uint32_t iRegion));

    /**
     * Maps a MMIO2 region into the physical memory space.
     *
     * A MMIO2 range may overlap with base memory if a lot of RAM
     * is configured for the VM, in which case we'll drop the base
     * memory pages. Presently we will make no attempt to preserve
     * anything that happens to be present in the base memory that
     * is replaced, this is of course incorrectly but it's too much
     * effort.
     *
     * @returns VBox status code.
     * @param   pDevIns             The device instance.
     * @param   iRegion             The region number used during registration.
     * @param   GCPhys              The physical address to map it at.
     * @thread  EMT.
     */
    DECLR3CALLBACKMEMBER(int, pfnMMIO2Map,(PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS GCPhys));

    /**
     * Unmaps a MMIO2 region previously mapped using pfnMMIO2Map.
     *
     * @returns VBox status code.
     * @param   pDevIns             The device instance.
     * @param   iRegion             The region number used during registration.
     * @param   GCPhys              The physical address it's currently mapped at.
     * @thread  EMT.
     */
    DECLR3CALLBACKMEMBER(int, pfnMMIO2Unmap,(PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS GCPhys));

    /**
     * Maps a portion of an MMIO2 region into the hypervisor region.
     *
     * Callers of this API must never deregister the MMIO2 region before the
     * VM is powered off.
     *
     * @return VBox status code.
     * @param   pDevIns             The device owning the MMIO2 memory.
     * @param   iRegion             The region.
     * @param   off                 The offset into the region. Will be rounded down
     *                              to closest page boundrary.
     * @param   cb                  The number of bytes to map. Will be rounded up
     *                              to the closest page boundrary.
     * @param   pszDesc             Mapping description.
     * @param   pRCPtr              Where to store the RC address.
     */
    DECLR3CALLBACKMEMBER(int, pfnMMHyperMapMMIO2,(PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS off, RTGCPHYS cb,
                                                  const char *pszDesc, PRTRCPTR pRCPtr));

    /**
     * Maps a portion of an MMIO2 region into kernel space (host).
     *
     * The kernel mapping will become invalid when the MMIO2 memory is deregistered
     * or the VM is terminated.
     *
     * @return VBox status code.
     * @param   pDevIns             The device owning the MMIO2 memory.
     * @param   iRegion             The region.
     * @param   off                 The offset into the region. Must be page
     *                              aligned.
     * @param   cb                  The number of bytes to map. Must be page
     *                              aligned.
     * @param   pszDesc             Mapping description.
     * @param   pR0Ptr              Where to store the R0 address.
     */
    DECLR3CALLBACKMEMBER(int, pfnMMIO2MapKernel,(PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS off, RTGCPHYS cb,
                                                  const char *pszDesc, PRTR0PTR pR0Ptr));

    /**
     * Registers the VMM device heap
     *
     * @returns VBox status code.
     * @param   pDevIns             The device instance.
     * @param   GCPhys              The physical address.
     * @param   pvHeap              Ring 3 heap pointer.
     * @param   cbSize              Size of the heap.
     * @thread  EMT.
     */
    DECLR3CALLBACKMEMBER(int, pfnRegisterVMMDevHeap,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTR3PTR pvHeap, unsigned cbSize));

    /**
     * Unregisters the VMM device heap
     *
     * @returns VBox status code.
     * @param   pDevIns             The device instance.
     * @param   GCPhys              The physical address.
     * @thread  EMT.
     */
    DECLR3CALLBACKMEMBER(int, pfnUnregisterVMMDevHeap,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys));

    /**
     * Gets the VMCPU handle. Restricted API.
     *
     * @returns VMCPU Handle.
     * @param   pDevIns             Device instance.
     */
    DECLR3CALLBACKMEMBER(PVMCPU, pfnGetVMCPU,(PPDMDEVINS pDevIns));

    /** @} */

    /** Just a safety precaution. (PDM_DEVHLP_VERSION) */
    uint32_t                        u32TheEnd;
} PDMDEVHLPR3;
#endif /* !IN_RING3 */
/** Pointer to the R3 PDM Device API. */
typedef R3PTRTYPE(struct PDMDEVHLPR3 *) PPDMDEVHLPR3;
/** Pointer to the R3 PDM Device API, const variant. */
typedef R3PTRTYPE(const struct PDMDEVHLPR3 *) PCPDMDEVHLPR3;

/** Current PDMDEVHLP version number. */
#define PDM_DEVHLP_VERSION  0xf2090000


/**
 * PDM Device API - RC Variant.
 */
typedef struct PDMDEVHLPRC
{
    /** Structure version. PDM_DEVHLPRC_VERSION defines the current version. */
    uint32_t                    u32Version;

    /**
     * Set the IRQ for a PCI device.
     *
     * @param   pDevIns         Device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level. See the PDM_IRQ_LEVEL_* \#defines.
     * @thread  Any thread, but will involve the emulation thread.
     */
    DECLRCCALLBACKMEMBER(void, pfnPCISetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Set ISA IRQ for a device.
     *
     * @param   pDevIns         Device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level. See the PDM_IRQ_LEVEL_* \#defines.
     * @thread  Any thread, but will involve the emulation thread.
     */
    DECLRCCALLBACKMEMBER(void, pfnISASetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Read physical memory.
     *
     * @returns VINF_SUCCESS (for now).
     * @param   pDevIns         Device instance.
     * @param   GCPhys          Physical address start reading from.
     * @param   pvBuf           Where to put the read bits.
     * @param   cbRead          How many bytes to read.
     */
    DECLRCCALLBACKMEMBER(int, pfnPhysRead,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead));

    /**
     * Write to physical memory.
     *
     * @returns VINF_SUCCESS for now, and later maybe VERR_EM_MEMORY.
     * @param   pDevIns         Device instance.
     * @param   GCPhys          Physical address to write to.
     * @param   pvBuf           What to write.
     * @param   cbWrite         How many bytes to write.
     */
    DECLRCCALLBACKMEMBER(int, pfnPhysWrite,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite));

    /**
     * Checks if the Gate A20 is enabled or not.
     *
     * @returns true if A20 is enabled.
     * @returns false if A20 is disabled.
     * @param   pDevIns         Device instance.
     * @thread  The emulation thread.
     */
    DECLRCCALLBACKMEMBER(bool, pfnA20IsEnabled,(PPDMDEVINS pDevIns));

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
    DECLRCCALLBACKMEMBER(int, pfnVMSetError,(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...));

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
    DECLRCCALLBACKMEMBER(int, pfnVMSetErrorV,(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va));

    /**
     * Set the VM runtime error message
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance.
     * @param   fFlags          The action flags. See VMSETRTERR_FLAGS_*.
     * @param   pszErrorId      Error ID string.
     * @param   pszFormat       Error message format string.
     * @param   ...             Error message arguments.
     */
    DECLRCCALLBACKMEMBER(int, pfnVMSetRuntimeError,(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, ...));

    /**
     * Set the VM runtime error message
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance.
     * @param   fFlags          The action flags. See VMSETRTERR_FLAGS_*.
     * @param   pszErrorId      Error ID string.
     * @param   pszFormat       Error message format string.
     * @param   va              Error message arguments.
     */
    DECLRCCALLBACKMEMBER(int, pfnVMSetRuntimeErrorV,(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, va_list va));

    /**
     * Set parameters for pending MMIO patch operation
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance.
     * @param   GCPhys          MMIO physical address
     * @param   pCachedData     GC pointer to cached data
     */
    DECLRCCALLBACKMEMBER(int, pfnPATMSetMMIOPatchInfo,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTGCPTR pCachedData));

    /**
     * Gets the VM handle. Restricted API.
     *
     * @returns VM Handle.
     * @param   pDevIns         Device instance.
     */
    DECLRCCALLBACKMEMBER(PVM, pfnGetVM,(PPDMDEVINS pDevIns));

    /**
     * Gets the VMCPU handle. Restricted API.
     *
     * @returns VMCPU Handle.
     * @param   pDevIns             Device instance.
     */
    DECLRCCALLBACKMEMBER(PVMCPU, pfnGetVMCPU,(PPDMDEVINS pDevIns));

    /** Just a safety precaution. */
    uint32_t                        u32TheEnd;
} PDMDEVHLPRC;
/** Pointer PDM Device RC API. */
typedef RCPTRTYPE(struct PDMDEVHLPRC *) PPDMDEVHLPRC;
/** Pointer PDM Device RC API. */
typedef RCPTRTYPE(const struct PDMDEVHLPRC *) PCPDMDEVHLPRC;

/** Current PDMDEVHLP version number. */
#define PDM_DEVHLPRC_VERSION  0xfb020000


/**
 * PDM Device API - R0 Variant.
 */
typedef struct PDMDEVHLPR0
{
    /** Structure version. PDM_DEVHLPR0_VERSION defines the current version. */
    uint32_t                    u32Version;

    /**
     * Set the IRQ for a PCI device.
     *
     * @param   pDevIns         Device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level. See the PDM_IRQ_LEVEL_* \#defines.
     * @thread  Any thread, but will involve the emulation thread.
     */
    DECLR0CALLBACKMEMBER(void, pfnPCISetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Set ISA IRQ for a device.
     *
     * @param   pDevIns         Device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level. See the PDM_IRQ_LEVEL_* \#defines.
     * @thread  Any thread, but will involve the emulation thread.
     */
    DECLR0CALLBACKMEMBER(void, pfnISASetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Read physical memory.
     *
     * @returns VINF_SUCCESS (for now).
     * @param   pDevIns         Device instance.
     * @param   GCPhys          Physical address start reading from.
     * @param   pvBuf           Where to put the read bits.
     * @param   cbRead          How many bytes to read.
     */
    DECLR0CALLBACKMEMBER(int, pfnPhysRead,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead));

    /**
     * Write to physical memory.
     *
     * @returns VINF_SUCCESS for now, and later maybe VERR_EM_MEMORY.
     * @param   pDevIns         Device instance.
     * @param   GCPhys          Physical address to write to.
     * @param   pvBuf           What to write.
     * @param   cbWrite         How many bytes to write.
     */
    DECLR0CALLBACKMEMBER(int, pfnPhysWrite,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite));

    /**
     * Checks if the Gate A20 is enabled or not.
     *
     * @returns true if A20 is enabled.
     * @returns false if A20 is disabled.
     * @param   pDevIns         Device instance.
     * @thread  The emulation thread.
     */
    DECLR0CALLBACKMEMBER(bool, pfnA20IsEnabled,(PPDMDEVINS pDevIns));

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
    DECLR0CALLBACKMEMBER(int, pfnVMSetError,(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...));

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
    DECLR0CALLBACKMEMBER(int, pfnVMSetErrorV,(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va));

    /**
     * Set the VM runtime error message
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance.
     * @param   fFlags          The action flags. See VMSETRTERR_FLAGS_*.
     * @param   pszErrorId      Error ID string.
     * @param   pszFormat       Error message format string.
     * @param   ...             Error message arguments.
     */
    DECLR0CALLBACKMEMBER(int, pfnVMSetRuntimeError,(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, ...));

    /**
     * Set the VM runtime error message
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance.
     * @param   fFlags          The action flags. See VMSETRTERR_FLAGS_*.
     * @param   pszErrorId      Error ID string.
     * @param   pszFormat       Error message format string.
     * @param   va              Error message arguments.
     */
    DECLR0CALLBACKMEMBER(int, pfnVMSetRuntimeErrorV,(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, va_list va));

    /**
     * Set parameters for pending MMIO patch operation
     *
     * @returns rc.
     * @param   pDevIns         Device instance.
     * @param   GCPhys          MMIO physical address
     * @param   pCachedData     GC pointer to cached data
     */
    DECLR0CALLBACKMEMBER(int, pfnPATMSetMMIOPatchInfo,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTGCPTR pCachedData));

    /**
     * Gets the VM handle. Restricted API.
     *
     * @returns VM Handle.
     * @param   pDevIns         Device instance.
     */
    DECLR0CALLBACKMEMBER(PVM, pfnGetVM,(PPDMDEVINS pDevIns));

    /**
     * Checks if our current CPU state allows for IO block emulation fallback to the recompiler
     *
     * @returns true = yes, false = no
     * @param   pDevIns         Device instance.
     */
    DECLR0CALLBACKMEMBER(bool, pfnCanEmulateIoBlock,(PPDMDEVINS pDevIns));

    /**
     * Gets the VMCPU handle. Restricted API.
     *
     * @returns VMCPU Handle.
     * @param   pDevIns             Device instance.
     */
    DECLR0CALLBACKMEMBER(PVMCPU, pfnGetVMCPU,(PPDMDEVINS pDevIns));

    /** Just a safety precaution. */
    uint32_t                        u32TheEnd;
} PDMDEVHLPR0;
/** Pointer PDM Device R0 API. */
typedef R0PTRTYPE(struct PDMDEVHLPR0 *) PPDMDEVHLPR0;
/** Pointer PDM Device GC API. */
typedef R0PTRTYPE(const struct PDMDEVHLPR0 *) PCPDMDEVHLPR0;

/** Current PDMDEVHLP version number. */
#define PDM_DEVHLPR0_VERSION  0xfb030000



/**
 * PDM Device Instance.
 */
typedef struct PDMDEVINS
{
    /** Structure version. PDM_DEVINS_VERSION defines the current version. */
    uint32_t                    u32Version;
    /** Device instance number. */
    RTUINT                      iInstance;

    /** Pointer the GC PDM Device API. */
    PCPDMDEVHLPRC               pDevHlpRC;
    /** Pointer to device instance data. */
    RTRCPTR                     pvInstanceDataRC;

    /** Pointer the R0 PDM Device API. */
    PCPDMDEVHLPR0               pDevHlpR0;
    /** Pointer to device instance data (R0). */
    RTR0PTR                     pvInstanceDataR0;

    /** Pointer the HC PDM Device API. */
    PCPDMDEVHLPR3               pDevHlpR3;
    /** Pointer to device instance data. */
    RTR3PTR                     pvInstanceDataR3;

    /** Pointer to device registration structure.  */
    R3PTRTYPE(PCPDMDEVREG)      pDevReg;
    /** Configuration handle. */
    R3PTRTYPE(PCFGMNODE)        pCfgHandle;

    /** The base interface of the device.
     * The device constructor initializes this if it has any
     * device level interfaces to export. To obtain this interface
     * call PDMR3QueryDevice(). */
    PDMIBASE                    IBase;
    /** Align the internal data more naturally. */
    RTR3PTR                     R3PtrPadding;

    /** Internal data. */
    union
    {
#ifdef PDMDEVINSINT_DECLARED
        PDMDEVINSINT            s;
#endif
        uint8_t                 padding[HC_ARCH_BITS == 32 ? 64 + 16 : 112];
    } Internal;

    /** Device instance data. The size of this area is defined
     * in the PDMDEVREG::cbInstanceData field. */
    char                        achInstanceData[8];
} PDMDEVINS;

/** Current PDMDEVINS version number. */
#define PDM_DEVINS_VERSION  0xf3020000

/** Converts a pointer to the PDMDEVINS::IBase to a pointer to PDMDEVINS. */
#define PDMIBASE_2_PDMDEV(pInterface) ( (PPDMDEVINS)((char *)(pInterface) - RT_OFFSETOF(PDMDEVINS, IBase)) )


/** @def PDMDEV_ASSERT_EMT
 * Assert that the current thread is the emulation thread.
 */
#ifdef VBOX_STRICT
# define PDMDEV_ASSERT_EMT(pDevIns)  pDevIns->pDevHlpR3->pfnAssertEMT(pDevIns, __FILE__, __LINE__, __FUNCTION__)
#else
# define PDMDEV_ASSERT_EMT(pDevIns)  do { } while (0)
#endif

/** @def PDMDEV_ASSERT_OTHER
 * Assert that the current thread is NOT the emulation thread.
 */
#ifdef VBOX_STRICT
# define PDMDEV_ASSERT_OTHER(pDevIns)  pDevIns->pDevHlpR3->pfnAssertOther(pDevIns, __FILE__, __LINE__, __FUNCTION__)
#else
# define PDMDEV_ASSERT_OTHER(pDevIns)  do { } while (0)
#endif

/** @def PDMDEV_ASSERT_VMLOCK_OWNER
 * Assert that the current thread is owner of the VM lock.
 */
#ifdef VBOX_STRICT
# define PDMDEV_ASSERT_VMLOCK_OWNER(pDevIns)  pDevIns->pDevHlpR3->pfnAssertVMLock(pDevIns, __FILE__, __LINE__, __FUNCTION__)
#else
# define PDMDEV_ASSERT_VMLOCK_OWNER(pDevIns)  do { } while (0)
#endif

/** @def PDMDEV_SET_ERROR
 * Set the VM error. See PDMDevHlpVMSetError() for printf like message formatting.
 */
#define PDMDEV_SET_ERROR(pDevIns, rc, pszError) \
    PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS, "%s", pszError)

/** @def PDMDEV_SET_RUNTIME_ERROR
 * Set the VM runtime error. See PDMDevHlpVMSetRuntimeError() for printf like message formatting.
 */
#define PDMDEV_SET_RUNTIME_ERROR(pDevIns, fFlags, pszErrorId, pszError) \
    PDMDevHlpVMSetRuntimeError(pDevIns, fFlags, pszErrorId, "%s", pszError)

/** @def PDMDEVINS_2_RCPTR
 * Converts a PDM Device instance pointer a RC PDM Device instance pointer.
 */
#define PDMDEVINS_2_RCPTR(pDevIns)  ( (RCPTRTYPE(PPDMDEVINS))((RTGCUINTPTR)(pDevIns)->pvInstanceDataRC - RT_OFFSETOF(PDMDEVINS, achInstanceData)) )

/** @def PDMDEVINS_2_R3PTR
 * Converts a PDM Device instance pointer a R3 PDM Device instance pointer.
 */
#define PDMDEVINS_2_R3PTR(pDevIns)  ( (R3PTRTYPE(PPDMDEVINS))((RTHCUINTPTR)(pDevIns)->pvInstanceDataR3 - RT_OFFSETOF(PDMDEVINS, achInstanceData)) )

/** @def PDMDEVINS_2_R0PTR
 * Converts a PDM Device instance pointer a R0 PDM Device instance pointer.
 */
#define PDMDEVINS_2_R0PTR(pDevIns)  ( (R0PTRTYPE(PPDMDEVINS))((RTR0UINTPTR)(pDevIns)->pvInstanceDataR0 - RT_OFFSETOF(PDMDEVINS, achInstanceData)) )


/**
 * VBOX_STRICT wrapper for pDevHlp->pfnDBGFStopV.
 *
 * @returns VBox status code which must be passed up to the VMM.
 * @param   pDevIns             Device instance.
 * @param   RT_SRC_POS_DECL     Use RT_SRC_POS.
 * @param   pszFormat           Message. (optional)
 * @param   ...                 Message parameters.
 */
DECLINLINE(int) PDMDeviceDBGFStop(PPDMDEVINS pDevIns, RT_SRC_POS_DECL, const char *pszFormat, ...)
{
#ifdef VBOX_STRICT
# ifdef IN_RING3
    int rc;
    va_list args;
    va_start(args, pszFormat);
    rc = pDevIns->pDevHlpR3->pfnDBGFStopV(pDevIns, RT_SRC_POS_ARGS, pszFormat, args);
    va_end(args);
    return rc;
# else
    return VINF_EM_DBG_STOP;
# endif
#else
    NOREF(pDevIns);
    NOREF(pszFile);
    NOREF(iLine);
    NOREF(pszFunction);
    NOREF(pszFormat);
    return VINF_SUCCESS;
#endif
}


#ifdef IN_RING3
/**
 * @copydoc PDMDEVHLPR3::pfnIOPortRegister
 */
DECLINLINE(int) PDMDevHlpIOPortRegister(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts, RTHCPTR pvUser,
                                        PFNIOMIOPORTOUT pfnOut, PFNIOMIOPORTIN pfnIn,
                                        PFNIOMIOPORTOUTSTRING pfnOutStr, PFNIOMIOPORTINSTRING pfnInStr, const char *pszDesc)
{
    return pDevIns->pDevHlpR3->pfnIOPortRegister(pDevIns, Port, cPorts, pvUser, pfnOut, pfnIn, pfnOutStr, pfnInStr, pszDesc);
}

/**
 * @copydoc PDMDEVHLPR3::pfnIOPortRegisterGC
 */
DECLINLINE(int) PDMDevHlpIOPortRegisterGC(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts, RTRCPTR pvUser,
                                          const char *pszOut, const char *pszIn, const char *pszOutStr,
                                          const char *pszInStr, const char *pszDesc)
{
    return pDevIns->pDevHlpR3->pfnIOPortRegisterGC(pDevIns, Port, cPorts, pvUser, pszOut, pszIn, pszOutStr, pszInStr, pszDesc);
}

/**
 * @copydoc PDMDEVHLPR3::pfnIOPortRegisterR0
 */
DECLINLINE(int) PDMDevHlpIOPortRegisterR0(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts, RTR0PTR pvUser,
                                          const char *pszOut, const char *pszIn, const char *pszOutStr,
                                          const char *pszInStr, const char *pszDesc)
{
    return pDevIns->pDevHlpR3->pfnIOPortRegisterR0(pDevIns, Port, cPorts, pvUser, pszOut, pszIn, pszOutStr, pszInStr, pszDesc);
}

/**
 * @copydoc PDMDEVHLPR3::pfnMMIORegister
 */
DECLINLINE(int) PDMDevHlpMMIORegister(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTHCPTR pvUser,
                                      PFNIOMMMIOWRITE pfnWrite, PFNIOMMMIOREAD pfnRead, PFNIOMMMIOFILL pfnFill,
                                      const char *pszDesc)
{
    return pDevIns->pDevHlpR3->pfnMMIORegister(pDevIns, GCPhysStart, cbRange, pvUser, pfnWrite, pfnRead, pfnFill, pszDesc);
}

/**
 * @copydoc PDMDEVHLPR3::pfnMMIORegisterGC
 */
DECLINLINE(int) PDMDevHlpMMIORegisterGC(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTGCPTR pvUser,
                                        const char *pszWrite, const char *pszRead, const char *pszFill)
{
    return pDevIns->pDevHlpR3->pfnMMIORegisterGC(pDevIns, GCPhysStart, cbRange, pvUser, pszWrite, pszRead, pszFill, NULL);
}

/**
 * @copydoc PDMDEVHLPR3::pfnMMIORegisterR0
 */
DECLINLINE(int) PDMDevHlpMMIORegisterR0(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTR0PTR pvUser,
                                        const char *pszWrite, const char *pszRead, const char *pszFill)
{
    return pDevIns->pDevHlpR3->pfnMMIORegisterR0(pDevIns, GCPhysStart, cbRange, pvUser, pszWrite, pszRead, pszFill, NULL);
}

/**
 * @copydoc PDMDEVHLPR3::pfnROMRegister
 */
DECLINLINE(int) PDMDevHlpROMRegister(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, const void *pvBinary, uint32_t fFlags, const char *pszDesc)
{
    return pDevIns->pDevHlpR3->pfnROMRegister(pDevIns, GCPhysStart, cbRange, pvBinary, fFlags, pszDesc);
}
/**
 * @copydoc PDMDEVHLPR3::pfnROMProtectShadow
 */
DECLINLINE(int) PDMDevHlpROMProtectShadow(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, PGMROMPROT enmProt)
{
    return pDevIns->pDevHlpR3->pfnROMProtectShadow(pDevIns, GCPhysStart, cbRange, enmProt);
}

/**
 * @copydoc PDMDEVHLPR3::pfnMMIO2Register
 */
DECLINLINE(int) PDMDevHlpMMIO2Register(PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS cb, uint32_t fFlags, void **ppv, const char *pszDesc)
{
    return pDevIns->pDevHlpR3->pfnMMIO2Register(pDevIns, iRegion, cb, fFlags, ppv, pszDesc);
}

/**
 * @copydoc PDMDEVHLPR3::pfnMMIO2Deregister
 */
DECLINLINE(int) PDMDevHlpMMIO2Deregister(PPDMDEVINS pDevIns, uint32_t iRegion)
{
    return pDevIns->pDevHlpR3->pfnMMIO2Deregister(pDevIns, iRegion);
}

/**
 * @copydoc PDMDEVHLPR3::pfnMMIO2Map
 */
DECLINLINE(int) PDMDevHlpMMIO2Map(PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS GCPhys)
{
    return pDevIns->pDevHlpR3->pfnMMIO2Map(pDevIns, iRegion, GCPhys);
}

/**
 * @copydoc PDMDEVHLPR3::pfnMMIO2Unmap
 */
DECLINLINE(int) PDMDevHlpMMIO2Unmap(PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS GCPhys)
{
    return pDevIns->pDevHlpR3->pfnMMIO2Unmap(pDevIns, iRegion, GCPhys);
}

/**
 * @copydoc PDMDEVHLPR3::pfnMMHyperMapMMIO2
 */
DECLINLINE(int) PDMDevHlpMMHyperMapMMIO2(PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS off, RTGCPHYS cb,
                                         const char *pszDesc, PRTRCPTR pRCPtr)
{
    return pDevIns->pDevHlpR3->pfnMMHyperMapMMIO2(pDevIns, iRegion, off, cb, pszDesc, pRCPtr);
}

/**
 * @copydoc PDMDEVHLPR3::pfnMMIO2MapKernel
 */
DECLINLINE(int) PDMDevHlpMMIO2MapKernel(PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS off, RTGCPHYS cb,
                                         const char *pszDesc, PRTR0PTR pR0Ptr)
{
    return pDevIns->pDevHlpR3->pfnMMIO2MapKernel(pDevIns, iRegion, off, cb, pszDesc, pR0Ptr);
}

/**
 * @copydoc PDMDEVHLPR3::pfnRegisterVMMDevHeap
 */
DECLINLINE(int) PDMDevHlpRegisterVMMDevHeap(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTR3PTR pvHeap, unsigned cbSize)
{
    return pDevIns->pDevHlpR3->pfnRegisterVMMDevHeap(pDevIns, GCPhys, pvHeap, cbSize);
}

/**
 * @copydoc PDMDEVHLPR3::pfnUnregisterVMMDevHeap
 */
DECLINLINE(int) PDMDevHlpUnregisterVMMDevHeap(PPDMDEVINS pDevIns, RTGCPHYS GCPhys)
{
    return pDevIns->pDevHlpR3->pfnUnregisterVMMDevHeap(pDevIns, GCPhys);
}

/**
 * @copydoc PDMDEVHLPR3::pfnSSMRegister
 */
DECLINLINE(int) PDMDevHlpSSMRegister(PPDMDEVINS pDevIns, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
                                     PFNSSMDEVSAVEPREP pfnSavePrep, PFNSSMDEVSAVEEXEC pfnSaveExec, PFNSSMDEVSAVEDONE pfnSaveDone,
                                     PFNSSMDEVLOADPREP pfnLoadPrep, PFNSSMDEVLOADEXEC pfnLoadExec, PFNSSMDEVLOADDONE pfnLoadDone)
{
    return pDevIns->pDevHlpR3->pfnSSMRegister(pDevIns, pszName, u32Instance, u32Version, cbGuess,
                                            pfnSavePrep, pfnSaveExec, pfnSaveDone,
                                            pfnLoadPrep, pfnLoadExec, pfnLoadDone);
}

/**
 * @copydoc PDMDEVHLPR3::pfnTMTimerCreate
 */
DECLINLINE(int) PDMDevHlpTMTimerCreate(PPDMDEVINS pDevIns, TMCLOCK enmClock, PFNTMTIMERDEV pfnCallback, const char *pszDesc, PPTMTIMERR3 ppTimer)
{
    return pDevIns->pDevHlpR3->pfnTMTimerCreate(pDevIns, enmClock, pfnCallback, pszDesc, ppTimer);
}

/**
 * @copydoc PDMDEVHLPR3::pfnPCIRegister
 */
DECLINLINE(int) PDMDevHlpPCIRegister(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev)
{
    return pDevIns->pDevHlpR3->pfnPCIRegister(pDevIns, pPciDev);
}

/**
 * @copydoc PDMDEVHLPR3::pfnPCIIORegionRegister
 */
DECLINLINE(int) PDMDevHlpPCIIORegionRegister(PPDMDEVINS pDevIns, int iRegion, uint32_t cbRegion, PCIADDRESSSPACE enmType, PFNPCIIOREGIONMAP pfnCallback)
{
    return pDevIns->pDevHlpR3->pfnPCIIORegionRegister(pDevIns, iRegion, cbRegion, enmType, pfnCallback);
}

/**
 * @copydoc PDMDEVHLPR3::pfnPCISetConfigCallbacks
 */
DECLINLINE(void) PDMDevHlpPCISetConfigCallbacks(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PFNPCICONFIGREAD pfnRead, PPFNPCICONFIGREAD ppfnReadOld,
                                                PFNPCICONFIGWRITE pfnWrite, PPFNPCICONFIGWRITE ppfnWriteOld)
{
    pDevIns->pDevHlpR3->pfnPCISetConfigCallbacks(pDevIns, pPciDev, pfnRead, ppfnReadOld, pfnWrite, ppfnWriteOld);
}

/**
 * @copydoc PDMDEVHLPR3::pfnDriverAttach
 */
DECLINLINE(int) PDMDevHlpDriverAttach(PPDMDEVINS pDevIns, RTUINT iLun, PPDMIBASE pBaseInterface, PPDMIBASE *ppBaseInterface, const char *pszDesc)
{
    return pDevIns->pDevHlpR3->pfnDriverAttach(pDevIns, iLun, pBaseInterface, ppBaseInterface, pszDesc);
}

/**
 * @copydoc PDMDEVHLPR3::pfnMMHeapAlloc
 */
DECLINLINE(void *) PDMDevHlpMMHeapAlloc(PPDMDEVINS pDevIns, size_t cb)
{
    return pDevIns->pDevHlpR3->pfnMMHeapAlloc(pDevIns, cb);
}

/**
 * @copydoc PDMDEVHLPR3::pfnMMHeapAllocZ
 */
DECLINLINE(void *) PDMDevHlpMMHeapAllocZ(PPDMDEVINS pDevIns, size_t cb)
{
    return pDevIns->pDevHlpR3->pfnMMHeapAllocZ(pDevIns, cb);
}

/**
 * @copydoc PDMDEVHLPR3::pfnMMHeapFree
 */
DECLINLINE(void) PDMDevHlpMMHeapFree(PPDMDEVINS pDevIns, void *pv)
{
    pDevIns->pDevHlpR3->pfnMMHeapFree(pDevIns, pv);
}

/**
 * @copydoc PDMDEVHLPR3::pfnDBGFInfoRegister
 */
DECLINLINE(int) PDMDevHlpDBGFInfoRegister(PPDMDEVINS pDevIns, const char *pszName, const char *pszDesc, PFNDBGFHANDLERDEV pfnHandler)
{
    return pDevIns->pDevHlpR3->pfnDBGFInfoRegister(pDevIns, pszName, pszDesc, pfnHandler);
}

/**
 * @copydoc PDMDEVHLPR3::pfnSTAMRegister
 */
DECLINLINE(void) PDMDevHlpSTAMRegister(PPDMDEVINS pDevIns, void *pvSample, STAMTYPE enmType, const char *pszName, STAMUNIT enmUnit, const char *pszDesc)
{
    pDevIns->pDevHlpR3->pfnSTAMRegister(pDevIns, pvSample, enmType, pszName, enmUnit, pszDesc);
}

/**
 * @copydoc PDMDEVHLPR3::pfnSTAMRegisterF
 */
DECLINLINE(void) PDMDevHlpSTAMRegisterF(PPDMDEVINS pDevIns, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility, STAMUNIT enmUnit,
                                        const char *pszDesc, const char *pszName, ...)
{
    va_list va;
    va_start(va, pszName);
    pDevIns->pDevHlpR3->pfnSTAMRegisterV(pDevIns, pvSample, enmType, enmVisibility, enmUnit, pszDesc, pszName, va);
    va_end(va);
}

/**
 * @copydoc PDMDEVHLPR3::pfnPDMQueueCreate
 */
DECLINLINE(int) PDMDevHlpPDMQueueCreate(PPDMDEVINS pDevIns, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval,
                                        PFNPDMQUEUEDEV pfnCallback, bool fGCEnabled, PPDMQUEUE *ppQueue)
{
    return pDevIns->pDevHlpR3->pfnPDMQueueCreate(pDevIns, cbItem, cItems, cMilliesInterval, pfnCallback, fGCEnabled, ppQueue);
}

/**
 * @copydoc PDMDEVHLPR3::pfnCritSectInit
 */
DECLINLINE(int) PDMDevHlpCritSectInit(PPDMDEVINS pDevIns, PPDMCRITSECT pCritSect, const char *pszName)
{
    return pDevIns->pDevHlpR3->pfnCritSectInit(pDevIns, pCritSect, pszName);
}

/**
 * @copydoc PDMDEVHLPR3::pfnUTCNow
 */
DECLINLINE(PRTTIMESPEC) PDMDevHlpUTCNow(PPDMDEVINS pDevIns, PRTTIMESPEC pTime)
{
    return pDevIns->pDevHlpR3->pfnUTCNow(pDevIns, pTime);
}

/**
 * @copydoc PDMDEVHLPR3::pfnPhysReadGCVirt
 */
DECLINLINE(int) PDMDevHlpPhysReadGCVirt(PPDMDEVINS pDevIns, void *pvDst, RTGCPTR GCVirtSrc, size_t cb)
{
    return pDevIns->pDevHlpR3->pfnPhysReadGCVirt(pDevIns, pvDst, GCVirtSrc, cb);
}

/**
 * @copydoc PDMDEVHLPR3::pfnPhysWriteGCVirt
 */
DECLINLINE(int) PDMDevHlpPhysWriteGCVirt(PPDMDEVINS pDevIns, RTGCPTR GCVirtDst, const void *pvSrc, size_t cb)
{
    return pDevIns->pDevHlpR3->pfnPhysWriteGCVirt(pDevIns, GCVirtDst, pvSrc, cb);
}

/**
 * @copydoc PDMDEVHLPR3::pfnPhysGCPtr2GCPhys
 */
DECLINLINE(int) PDMDevHlpPhysGCPtr2GCPhys(PPDMDEVINS pDevIns, RTGCPTR GCPtr, PRTGCPHYS pGCPhys)
{
    return pDevIns->pDevHlpR3->pfnPhysGCPtr2GCPhys(pDevIns, GCPtr, pGCPhys);
}

/**
 * @copydoc PDMDEVHLPR3::pfnVMState
 */
DECLINLINE(VMSTATE) PDMDevHlpVMState(PPDMDEVINS pDevIns)
{
    return pDevIns->pDevHlpR3->pfnVMState(pDevIns);
}

/**
 * @copydoc PDMDEVHLPR3::pfnA20Set
 */
DECLINLINE(void) PDMDevHlpA20Set(PPDMDEVINS pDevIns, bool fEnable)
{
    pDevIns->pDevHlpR3->pfnA20Set(pDevIns, fEnable);
}

/**
 * @copydoc PDMDEVHLPR3::pfnVMReset
 */
DECLINLINE(int) PDMDevHlpVMReset(PPDMDEVINS pDevIns)
{
    return pDevIns->pDevHlpR3->pfnVMReset(pDevIns);
}

/**
 * @copydoc PDMDEVHLPR3::pfnVMSuspend
 */
DECLINLINE(int) PDMDevHlpVMSuspend(PPDMDEVINS pDevIns)
{
    return pDevIns->pDevHlpR3->pfnVMSuspend(pDevIns);
}

/**
 * @copydoc PDMDEVHLPR3::pfnVMPowerOff
 */
DECLINLINE(int) PDMDevHlpVMPowerOff(PPDMDEVINS pDevIns)
{
    return pDevIns->pDevHlpR3->pfnVMPowerOff(pDevIns);
}

/**
 * @copydoc PDMDEVHLPR3::pfnDMARegister
 */
DECLINLINE(int) PDMDevHlpDMARegister(PPDMDEVINS pDevIns, unsigned uChannel, PFNDMATRANSFERHANDLER pfnTransferHandler, void *pvUser)
{
    return pDevIns->pDevHlpR3->pfnDMARegister(pDevIns, uChannel, pfnTransferHandler, pvUser);
}

/**
 * @copydoc PDMDEVHLPR3::pfnDMAReadMemory
 */
DECLINLINE(int) PDMDevHlpDMAReadMemory(PPDMDEVINS pDevIns, unsigned uChannel, void *pvBuffer, uint32_t off, uint32_t cbBlock, uint32_t *pcbRead)
{
    return pDevIns->pDevHlpR3->pfnDMAReadMemory(pDevIns, uChannel, pvBuffer, off, cbBlock, pcbRead);
}

/**
 * @copydoc PDMDEVHLPR3::pfnDMAWriteMemory
 */
DECLINLINE(int) PDMDevHlpDMAWriteMemory(PPDMDEVINS pDevIns, unsigned uChannel, const void *pvBuffer, uint32_t off, uint32_t cbBlock, uint32_t *pcbWritten)
{
    return pDevIns->pDevHlpR3->pfnDMAWriteMemory(pDevIns, uChannel, pvBuffer, off, cbBlock, pcbWritten);
}

/**
 * @copydoc PDMDEVHLPR3::pfnDMASetDREQ
 */
DECLINLINE(int) PDMDevHlpDMASetDREQ(PPDMDEVINS pDevIns, unsigned uChannel, unsigned uLevel)
{
    return pDevIns->pDevHlpR3->pfnDMASetDREQ(pDevIns, uChannel, uLevel);
}

/**
 * @copydoc PDMDEVHLPR3::pfnDMAGetChannelMode
 */
DECLINLINE(uint8_t) PDMDevHlpDMAGetChannelMode(PPDMDEVINS pDevIns, unsigned uChannel)
{
    return pDevIns->pDevHlpR3->pfnDMAGetChannelMode(pDevIns, uChannel);
}

/**
 * @copydoc PDMDEVHLPR3::pfnDMASchedule
 */
DECLINLINE(void) PDMDevHlpDMASchedule(PPDMDEVINS pDevIns)
{
    pDevIns->pDevHlpR3->pfnDMASchedule(pDevIns);
}

/**
 * @copydoc PDMDEVHLPR3::pfnCMOSWrite
 */
DECLINLINE(int) PDMDevHlpCMOSWrite(PPDMDEVINS pDevIns, unsigned iReg, uint8_t u8Value)
{
    return pDevIns->pDevHlpR3->pfnCMOSWrite(pDevIns, iReg, u8Value);
}

/**
 * @copydoc PDMDEVHLPR3::pfnCMOSRead
 */
DECLINLINE(int) PDMDevHlpCMOSRead(PPDMDEVINS pDevIns, unsigned iReg, uint8_t *pu8Value)
{
    return pDevIns->pDevHlpR3->pfnCMOSRead(pDevIns, iReg, pu8Value);
}

/**
 * @copydoc PDMDEVHLPR3::pfnGetCpuId
 */
DECLINLINE(void) PDMDevHlpGetCpuId(PPDMDEVINS pDevIns, uint32_t iLeaf, uint32_t *pEax, uint32_t *pEbx, uint32_t *pEcx, uint32_t *pEdx)
{
    pDevIns->pDevHlpR3->pfnGetCpuId(pDevIns, iLeaf, pEax, pEbx, pEcx, pEdx);
}

/**
 * @copydoc PDMDEVHLPR3::pfnPDMThreadCreate
 */
DECLINLINE(int) PDMDevHlpPDMThreadCreate(PPDMDEVINS pDevIns, PPPDMTHREAD ppThread, void *pvUser, PFNPDMTHREADDEV pfnThread,
                                         PFNPDMTHREADWAKEUPDEV pfnWakeup, size_t cbStack, RTTHREADTYPE enmType, const char *pszName)
{
    return pDevIns->pDevHlpR3->pfnPDMThreadCreate(pDevIns, ppThread, pvUser, pfnThread, pfnWakeup, cbStack, enmType, pszName);
}
#endif /* IN_RING3 */


/**
 * @copydoc PDMDEVHLPR3::pfnGetVM
 */
DECLINLINE(PVM) PDMDevHlpGetVM(PPDMDEVINS pDevIns)
{
    return pDevIns->CTX_SUFF(pDevHlp)->pfnGetVM(pDevIns);
}

/**
 * @copydoc PDMDEVHLPR3::pfnGetVMCPU
 */
DECLINLINE(PVMCPU) PDMDevHlpGetVMCPU(PPDMDEVINS pDevIns)
{
    return pDevIns->CTX_SUFF(pDevHlp)->pfnGetVMCPU(pDevIns);
}

#ifdef IN_RING0
/**
 * @copydoc PDMDEVHLPR0::pfnCanEmulateIoBlock
 */
DECLINLINE(bool) PDMDevHlpCanEmulateIoBlock(PPDMDEVINS pDevIns)
{
    return pDevIns->CTX_SUFF(pDevHlp)->pfnCanEmulateIoBlock(pDevIns);
}
#endif

/**
 * @copydoc PDMDEVHLPR3::pfnPCISetIrq
 */
DECLINLINE(void) PDMDevHlpPCISetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    pDevIns->CTX_SUFF(pDevHlp)->pfnPCISetIrq(pDevIns, iIrq, iLevel);
}

/**
 * @copydoc PDMDEVHLPR3::pfnPCISetIrqNoWait
 */
DECLINLINE(void) PDMDevHlpPCISetIrqNoWait(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    pDevIns->CTX_SUFF(pDevHlp)->pfnPCISetIrq(pDevIns, iIrq, iLevel);
}

/**
 * @copydoc PDMDEVHLPR3::pfnISASetIrq
 */
DECLINLINE(void) PDMDevHlpISASetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    pDevIns->CTX_SUFF(pDevHlp)->pfnISASetIrq(pDevIns, iIrq, iLevel);
}

/**
 * @copydoc PDMDEVHLPR3::pfnISASetIrqNoWait
 */
DECLINLINE(void) PDMDevHlpISASetIrqNoWait(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    pDevIns->CTX_SUFF(pDevHlp)->pfnISASetIrq(pDevIns, iIrq, iLevel);
}

/**
 * @copydoc PDMDEVHLPR3::pfnPhysRead
 */
DECLINLINE(int) PDMDevHlpPhysRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead)
{
    return pDevIns->CTX_SUFF(pDevHlp)->pfnPhysRead(pDevIns, GCPhys, pvBuf, cbRead);
}

/**
 * @copydoc PDMDEVHLPR3::pfnPhysWrite
 */
DECLINLINE(int) PDMDevHlpPhysWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite)
{
    return pDevIns->CTX_SUFF(pDevHlp)->pfnPhysWrite(pDevIns, GCPhys, pvBuf, cbWrite);
}

#ifdef IN_RING3

/**
 * @copydoc PDMDEVHLPR3::pfnPhysGCPhys2CCPtr
 */
DECLINLINE(int) PDMDevHlpPhysGCPhys2CCPtr(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, uint32_t fFlags, void **ppv, PPGMPAGEMAPLOCK pLock)
{
    return pDevIns->CTX_SUFF(pDevHlp)->pfnPhysGCPhys2CCPtr(pDevIns, GCPhys, fFlags, ppv, pLock);
}

/**
 * @copydoc PDMDEVHLPR3::pfnPhysGCPhys2CCPtrReadOnly
 */
DECLINLINE(int) PDMDevHlpPhysGCPhys2CCPtrReadOnly(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, uint32_t fFlags, void const **ppv, PPGMPAGEMAPLOCK pLock)
{
    return pDevIns->CTX_SUFF(pDevHlp)->pfnPhysGCPhys2CCPtrReadOnly(pDevIns, GCPhys, fFlags, ppv, pLock);
}

/**
 * @copydoc PDMDEVHLPR3::pfnPhysReleasePageMappingLock
 */
DECLINLINE(void) PDMDevHlpPhysReleasePageMappingLock(PPDMDEVINS pDevIns, PPGMPAGEMAPLOCK pLock)
{
    pDevIns->CTX_SUFF(pDevHlp)->pfnPhysReleasePageMappingLock(pDevIns, pLock);
}

#endif /* IN_RING3 */

/**
 * @copydoc PDMDEVHLPR3::pfnA20IsEnabled
 */
DECLINLINE(bool) PDMDevHlpA20IsEnabled(PPDMDEVINS pDevIns)
{
    return pDevIns->CTX_SUFF(pDevHlp)->pfnA20IsEnabled(pDevIns);
}

/**
 * @copydoc PDMDEVHLPR3::pfnVMSetError
 */
DECLINLINE(int) PDMDevHlpVMSetError(PPDMDEVINS pDevIns, const int rc, RT_SRC_POS_DECL, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    pDevIns->CTX_SUFF(pDevHlp)->pfnVMSetErrorV(pDevIns, rc, RT_SRC_POS_ARGS, pszFormat, va);
    va_end(va);
    return rc;
}

/**
 * @copydoc PDMDEVHLPR3::pfnVMSetRuntimeError
 */
DECLINLINE(int) PDMDevHlpVMSetRuntimeError(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, ...)
{
    va_list va;
    int rc;
    va_start(va, pszFormat);
    rc = pDevIns->CTX_SUFF(pDevHlp)->pfnVMSetRuntimeErrorV(pDevIns, fFlags, pszErrorId, pszFormat, va);
    va_end(va);
    return rc;
}



/** Pointer to callbacks provided to the VBoxDeviceRegister() call. */
typedef struct PDMDEVREGCB *PPDMDEVREGCB;

/**
 * Callbacks for VBoxDeviceRegister().
 */
typedef struct PDMDEVREGCB
{
    /** Interface version.
     * This is set to PDM_DEVREG_CB_VERSION. */
    uint32_t                    u32Version;

    /**
     * Registers a device with the current VM instance.
     *
     * @returns VBox status code.
     * @param   pCallbacks      Pointer to the callback table.
     * @param   pDevReg         Pointer to the device registration record.
     *                          This data must be permanent and readonly.
     */
    DECLR3CALLBACKMEMBER(int, pfnRegister,(PPDMDEVREGCB pCallbacks, PCPDMDEVREG pDevReg));

    /**
     * Allocate memory which is associated with current VM instance
     * and automatically freed on it's destruction.
     *
     * @returns Pointer to allocated memory. The memory is *NOT* zero-ed.
     * @param   pCallbacks      Pointer to the callback table.
     * @param   cb              Number of bytes to allocate.
     */
    DECLR3CALLBACKMEMBER(void *, pfnMMHeapAlloc,(PPDMDEVREGCB pCallbacks, size_t cb));
} PDMDEVREGCB;

/** Current version of the PDMDEVREGCB structure.  */
#define PDM_DEVREG_CB_VERSION 0xf4010000


/**
 * The VBoxDevicesRegister callback function.
 *
 * PDM will invoke this function after loading a device module and letting
 * the module decide which devices to register and how to handle conflicts.
 *
 * @returns VBox status code.
 * @param   pCallbacks      Pointer to the callback table.
 * @param   u32Version      VBox version number.
 */
typedef DECLCALLBACK(int) FNPDMVBOXDEVICESREGISTER(PPDMDEVREGCB pCallbacks, uint32_t u32Version);

/** @} */

__END_DECLS

#endif
