/** @file
 * PDM - Pluggable Device Manager.
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

#ifndef __VBox_pdm_h__
#define __VBox_pdm_h__

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/iom.h>
#include <VBox/ssm.h>
#include <VBox/cfgm.h>
#include <VBox/dbgf.h>
#include <VBox/err.h>
#include <VBox/pci.h>
#include <VBox/VBoxGuest.h>

#include <iprt/critsect.h>
#include <iprt/stdarg.h>


__BEGIN_DECLS

/** @defgroup grp_pdm       The Pluggable Device Manager API
 * @{
 */

/** Source position.
 * @deprecated Use RT_SRC_POS */
#define PDM_SRC_POS         RT_SRC_POS

/** Source position declaration.
 * @deprecated Use RT_SRC_POS_DECL */
#define PDM_SRC_POS_DECL    RT_SRC_POS_DECL

/** Source position arguments.
 * @deprecated Use RT_SRC_POS_ARGS */
#define PDM_SRC_POS_ARGS    RT_SRC_POS_ARGS


/** @defgroup grp_pdm_queue     The PDM Queue
 * @ingroup grp_pdm
 * @{
 */

/** Pointer to a PDM queue. Also called PDM queue handle. */
typedef struct PDMQUEUE *PPDMQUEUE;

/** Pointer to a PDM queue item core. */
typedef struct PDMQUEUEITEMCORE *PPDMQUEUEITEMCORE;

/**
 * PDM queue item core.
 */
typedef struct PDMQUEUEITEMCORE
{
    /** Pointer to the next item in the pending list - HC Pointer. */
    HCPTRTYPE(PPDMQUEUEITEMCORE)    pNextHC;
    /** Pointer to the next item in the pending list - GC Pointer. */
    GCPTRTYPE(PPDMQUEUEITEMCORE)    pNextGC;
} PDMQUEUEITEMCORE;


/**
 * Queue consumer callback for devices.
 *
 * @returns Success indicator.
 *          If false the item will not be removed and the flushing will stop.
 * @param   pDevIns     The device instance.
 * @param   pItem       The item to consume. Upon return this item will be freed.
 */
typedef DECLCALLBACK(bool) FNPDMQUEUEDEV(PPDMDEVINS pDevIns, PPDMQUEUEITEMCORE pItem);
/** Pointer to a FNPDMQUEUEDEV(). */
typedef FNPDMQUEUEDEV *PFNPDMQUEUEDEV;

/**
 * Queue consumer callback for drivers.
 *
 * @returns Success indicator.
 *          If false the item will not be removed and the flushing will stop.
 * @param   pDrvIns     The driver instance.
 * @param   pItem       The item to consume. Upon return this item will be freed.
 */
typedef DECLCALLBACK(bool) FNPDMQUEUEDRV(PPDMDRVINS pDrvIns, PPDMQUEUEITEMCORE pItem);
/** Pointer to a FNPDMQUEUEDRV(). */
typedef FNPDMQUEUEDRV *PFNPDMQUEUEDRV;

/**
 * Queue consumer callback for internal component.
 *
 * @returns Success indicator.
 *          If false the item will not be removed and the flushing will stop.
 * @param   pVM         The VM handle.
 * @param   pItem       The item to consume. Upon return this item will be freed.
 */
typedef DECLCALLBACK(bool) FNPDMQUEUEINT(PVM pVM, PPDMQUEUEITEMCORE pItem);
/** Pointer to a FNPDMQUEUEINT(). */
typedef FNPDMQUEUEINT *PFNPDMQUEUEINT;

/**
 * Queue consumer callback for external component.
 *
 * @returns Success indicator.
 *          If false the item will not be removed and the flushing will stop.
 * @param   pvUser      User argument.
 * @param   pItem       The item to consume. Upon return this item will be freed.
 */
typedef DECLCALLBACK(bool) FNPDMQUEUEEXT(void *pvUser, PPDMQUEUEITEMCORE pItem);
/** Pointer to a FNPDMQUEUEEXT(). */
typedef FNPDMQUEUEEXT *PFNPDMQUEUEEXT;

/**
 * Create a queue with a device owner.
 *
 * @returns VBox status code.
 * @param   pVM                 VM handle.
 * @param   pDevIns             Device instance.
 * @param   cbItem              Size a queue item.
 * @param   cItems              Number of items in the queue.
 * @param   cMilliesInterval    Number of milliseconds between polling the queue.
 *                              If 0 then the emulation thread will be notified whenever an item arrives.
 * @param   pfnCallback         The consumer function.
 * @param   fGCEnabled          Set if the queue must be usable from GC.
 * @param   ppQueue             Where to store the queue handle on success.
 * @thread  Emulation thread only.
 */
PDMR3DECL(int) PDMR3QueueCreateDevice(PVM pVM, PPDMDEVINS pDevIns, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval,
                                      PFNPDMQUEUEDEV pfnCallback, bool fGCEnabled, PPDMQUEUE *ppQueue);

/**
 * Create a queue with a driver owner.
 *
 * @returns VBox status code.
 * @param   pVM                 VM handle.
 * @param   pDrvIns             Driver instance.
 * @param   cbItem              Size a queue item.
 * @param   cItems              Number of items in the queue.
 * @param   cMilliesInterval    Number of milliseconds between polling the queue.
 *                              If 0 then the emulation thread will be notified whenever an item arrives.
 * @param   pfnCallback         The consumer function.
 * @param   ppQueue             Where to store the queue handle on success.
 * @thread  The emulation thread.
 */
PDMR3DECL(int) PDMR3QueueCreateDriver(PVM pVM, PPDMDRVINS pDrvIns, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval,
                                      PFNPDMQUEUEDRV pfnCallback, PPDMQUEUE *ppQueue);

/**
 * Create a queue with an internal owner.
 *
 * @returns VBox status code.
 * @param   pVM                 VM handle.
 * @param   cbItem              Size a queue item.
 * @param   cItems              Number of items in the queue.
 * @param   cMilliesInterval    Number of milliseconds between polling the queue.
 *                              If 0 then the emulation thread will be notified whenever an item arrives.
 * @param   pfnCallback         The consumer function.
 * @param   fGCEnabled          Set if the queue must be usable from GC.
 * @param   ppQueue             Where to store the queue handle on success.
 * @thread  Emulation thread only.
 */
PDMR3DECL(int) PDMR3QueueCreateInternal(PVM pVM, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval,
                                        PFNPDMQUEUEINT pfnCallback, bool fGCEnabled, PPDMQUEUE *ppQueue);

/**
 * Create a queue with an external owner.
 *
 * @returns VBox status code.
 * @param   pVM                 VM handle.
 * @param   cbItem              Size a queue item.
 * @param   cItems              Number of items in the queue.
 * @param   cMilliesInterval    Number of milliseconds between polling the queue.
 *                              If 0 then the emulation thread will be notified whenever an item arrives.
 * @param   pfnCallback         The consumer function.
 * @param   pvUser              The user argument to the consumer function.
 * @param   ppQueue             Where to store the queue handle on success.
 * @thread  The emulation thread.
 */
PDMR3DECL(int) PDMR3QueueCreateExternal(PVM pVM, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval,
                                        PFNPDMQUEUEEXT pfnCallback, void *pvUser, PPDMQUEUE *ppQueue);

/**
 * Destroy a queue.
 *
 * @returns VBox status code.
 * @param   pQueue      Queue to destroy.
 * @thread  The emulation thread.
 */
PDMR3DECL(int) PDMR3QueueDestroy(PPDMQUEUE pQueue);

/**
 * Destroy a all queues owned by the specified device.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pDevIns     Device instance.
 * @thread  Emulation thread only.
 */
PDMR3DECL(int) PDMR3QueueDestroyDevice(PVM pVM, PPDMDEVINS pDevIns);

/**
 * Destroy a all queues owned by the specified driver.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pDrvIns     Driver instance.
 * @thread  Emulation thread only.
 */
PDMR3DECL(int) PDMR3QueueDestroyDriver(PVM pVM, PPDMDRVINS pDrvIns);

/**
 * Flushes pending queues.
 * This is a forced action callback.
 *
 * @param   pVM     VM handle.
 * @thread  The emulation thread.
 */
PDMR3DECL(void) PDMR3QueueFlushAll(PVM pVM);

/**
 * This is a worker function used by PDMQueueFlush to perform the
 * flush in ring-3.
 *
 * The queue which should be flushed is pointed to by either pQueueFlushGC,
 * pQueueFlushHC, or pQueueue. This function will flush that queue and
 * recalc the queue FF.
 *
 * @param   pVM     The VM handle.
 * @param   pQueue  The queue to flush. Only used in Ring-3.
 */
PDMR3DECL(void) PDMR3QueueFlushWorker(PVM pVM, PPDMQUEUE pQueue);

/**
 * Flushes a PDM queue.
 *
 * @param   pQueue          The queue handle.
 */
PDMDECL(void) PDMQueueFlush(PPDMQUEUE pQueue);

/**
 * Allocate an item from a queue.
 * The allocated item must be handed on to PDMQueueInsert() after the
 * data has been filled in.
 *
 * @returns Pointer to allocated queue item.
 * @returns NULL on failure. The queue is exhausted.
 * @param   pQueue      The queue handle.
 * @thread  Any thread.
 */
PDMDECL(PPDMQUEUEITEMCORE) PDMQueueAlloc(PPDMQUEUE pQueue);

/**
 * Queue an item.
 * The item must have been obtained using PDMQueueAlloc(). Once the item
 * has been passed to this function it must not be touched!
 *
 * @param   pQueue      The queue handle.
 * @param   pItem       The item to insert.
 * @thread  Any thread.
 */
PDMDECL(void) PDMQueueInsert(PPDMQUEUE pQueue, PPDMQUEUEITEMCORE pItem);

/**
 * Queue an item.
 * The item must have been obtained using PDMQueueAlloc(). Once the item
 * have been passed to this function it must not be touched!
 *
 * @param   pQueue          The queue handle.
 * @param   pItem           The item to insert.
 * @param   NanoMaxDelay    The maximum delay before processing the queue, in nanoseconds.
 *                          This applies only to GC.
 * @thread  Any thread.
 */
PDMDECL(void) PDMQueueInsertEx(PPDMQUEUE pQueue, PPDMQUEUEITEMCORE pItem, uint64_t NanoMaxDelay);


/**
 * Gets the GC pointer for the specified queue.
 *
 * @returns The GC address of the queue.
 * @returns NULL if pQueue is invalid.
 * @param   pQueue          The queue handle.
 */
PDMDECL(GCPTRTYPE(PPDMQUEUE)) PDMQueueGCPtr(PPDMQUEUE pQueue);

/** @} */



/** @defgroup grp_pdm_critsect      The PDM Critical Section
 * @ingroup grp_pdm
 * @{
 */

/**
 * A PDM critical section.
 * Initialize using PDMDRVHLP::pfnCritSectInit().
 */
typedef union PDMCRITSECT
{
    /** Padding. */
    uint8_t padding[HC_ARCH_BITS == 64 ? 0xa8 : 0x80];
#ifdef PDMCRITSECTINT_DECLARED
    /** The internal structure (not normally visible). */
    struct PDMCRITSECTINT s;
#endif
} PDMCRITSECT;
/** Pointer to a PDM critical section. */
typedef PDMCRITSECT *PPDMCRITSECT;
/** Pointer to a const PDM critical section. */
typedef const PDMCRITSECT *PCPDMCRITSECT;

/**
 * Initializes a PDM critical section for internal use.
 *
 * The PDM critical sections are derived from the IPRT critical sections, but
 * works in GC as well.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pDevIns         Device instance.
 * @param   pCritSect       Pointer to the critical section.
 * @param   pszName         The name of the critical section (for statistics).
 */
PDMR3DECL(int) PDMR3CritSectInit(PVM pVM, PPDMCRITSECT pCritSect, const char *pszName);

/**
 * Leaves a critical section entered with PDMCritSectEnter().
 *
 * @returns VINF_SUCCESS if entered successfully.
 * @returns rcBusy when encountering a busy critical section in GC/R0.
 * @returns VERR_SEM_DESTROYED if the critical section is dead.
 *
 * @param   pCritSect           The PDM critical section to enter.
 * @param   rcBusy              The status code to return when we're in GC or R0
 *                              and the section is busy.
 */
PDMDECL(int) PDMCritSectEnter(PPDMCRITSECT pCritSect, int rcBusy);

/**
 * Leaves a critical section entered with PDMCritSectEnter().
 *
 * @param   pCritSect           The PDM critical section to leave.
 */
PDMDECL(void) PDMCritSectLeave(PPDMCRITSECT pCritSect);

/**
 * Checks the caller is the owner of the critical section.
 *
 * @returns true if owner.
 * @returns false if not owner.
 * @param   pCritSect   The critical section.
 */
PDMDECL(bool) PDMCritSectIsOwner(PCPDMCRITSECT pCritSect);

/**
 * Deletes the critical section.
 *
 * @returns VBox status code.
 * @param   pCritSect           The PDM critical section to destroy.
 */
PDMR3DECL(int) PDMR3CritSectDelete(PPDMCRITSECT pCritSect);

/**
 * Deletes all remaining critical sections.
 *
 * This is called at the end of the termination process.
 *
 * @returns VBox status.
 *          First error code, rest is lost.
 * @param   pVM         The VM handle.
 * @remark  Don't confuse this with PDMR3CritSectDelete.
 */
PDMDECL(int) PDMR3CritSectTerm(PVM pVM);

/**
 * Process the critical sections queued for ring-3 'leave'.
 *
 * @param   pVM         The VM handle.
 */
PDMR3DECL(void) PDMR3CritSectFF(PVM pVM);

/** @} */



/** @defgroup grp_pdm_interfaces    Interfaces
 * @ingroup grp_pdm
 * @{
 */

/**
 * Driver interface identficators.
 */
typedef enum PDMINTERFACE
{
    /** PDMIBASE                - The interface everyone supports. */
    PDMINTERFACE_BASE = 1,
    /** PDMIMOUSEPORT           - The mouse port interface.             (Down)  Coupled with PDMINTERFACE_MOUSE_CONNECTOR. */
    PDMINTERFACE_MOUSE_PORT,
    /** PDMIMOUSECONNECTOR      - The mouse connector interface.        (Up)    Coupled with PDMINTERFACE_MOUSE_PORT. */
    PDMINTERFACE_MOUSE_CONNECTOR,
    /** PDMIKEYBOARDPORT        - The keyboard port interface.          (Down)  Coupled with PDMINTERFACE_KEYBOARD_CONNECTOR. */
    PDMINTERFACE_KEYBOARD_PORT,
    /** PDMIKEYBOARDCONNECTOR   - The keyboard connector interface.     (Up)    Coupled with PDMINTERFACE_KEYBOARD_PORT. */
    PDMINTERFACE_KEYBOARD_CONNECTOR,
    /** PDMIDISPLAYPORT         - The display port interface.           (Down)  Coupled with PDMINTERFACE_DISPLAY_CONNECTOR. */
    PDMINTERFACE_DISPLAY_PORT,
    /** PDMIDISPLAYCONNECTOR    - The display connector interface.      (Up)    Coupled with PDMINTERFACE_DISPLAY_PORT. */
    PDMINTERFACE_DISPLAY_CONNECTOR,
    /** PDMIBLOCKPORT           - The block notify interface            (Down)  Coupled with PDMINTERFACE_BLOCK. */
    PDMINTERFACE_BLOCK_PORT,
    /** PDMIBLOCK               - The block driver interface            (Up)    Coupled with PDMINTERFACE_BLOCK_PORT. */
    PDMINTERFACE_BLOCK,
    /** PDMIBLOCKBIOS           - The block bios interface.             (External) */
    PDMINTERFACE_BLOCK_BIOS,
    /** PDMIMOUNTNOTIFY         - The mountable notification interface. (Down)  Coupled with PDMINTERFACE_MOUNT. */
    PDMINTERFACE_MOUNT_NOTIFY,
    /** PDMIMOUNT               - The mountable interface.              (Up)    Coupled with PDMINTERFACE_MOUNT_NOTIFY. */
    PDMINTERFACE_MOUNT,
    /** PDMIMEDIA               - The media interface.                  (Up)    No coupling.
     * Used by a block unit driver to implement PDMINTERFACE_BLOCK and PDMINTERFACE_BLOCK_BIOS. */
    PDMINTERFACE_MEDIA,
    /** PDMIISCSITRANSPORT      - The iSCSI transport interface         (Up)    No coupling.
     * used by the iSCSI media driver.  */
    PDMINTERFACE_ISCSITRANSPORT,

    /** PDMINETWORKPORT         - The network port interface.           (Down)  Coupled with PDMINTERFACE_NETWORK_CONNECTOR. */
    PDMINTERFACE_NETWORK_PORT,
    /** PDMINETWORKPORT         - The network connector interface.      (Up)    Coupled with PDMINTERFACE_NETWORK_PORT. */
    PDMINTERFACE_NETWORK_CONNECTOR,
    /** PDMINETWORKCONFIG       - The network configuartion interface.  (Main)  Used by the managment api. */
    PDMINTERFACE_NETWORK_CONFIG,

    /** PDMIAUDIOCONNECTOR      - The audio driver interface.           (Up)    No coupling. */
    PDMINTERFACE_AUDIO_CONNECTOR,

    /** PDMIAUDIOSNIFFERPORT    - The Audio Sniffer Device port interface. */
    PDMINTERFACE_AUDIO_SNIFFER_PORT,
    /** PDMIAUDIOSNIFFERCONNECTOR - The Audio Sniffer Driver connector interface. */
    PDMINTERFACE_AUDIO_SNIFFER_CONNECTOR,

    /** PDMIVMMDEVPORT          - The VMM Device port interface. */
    PDMINTERFACE_VMMDEV_PORT,
    /** PDMIVMMDEVCONNECTOR     - The VMM Device connector interface. */
    PDMINTERFACE_VMMDEV_CONNECTOR,

    /** PDMILEDPORTS            - The generic LED port interface.       (Down)  Coupled with PDMINTERFACE_LED_CONNECTORS. */
    PDMINTERFACE_LED_PORTS,
    /** PDMILEDCONNECTORS       - The generic LED connector interface.  (Up)    Coupled with PDMINTERFACE_LED_PORTS.  */
    PDMINTERFACE_LED_CONNECTORS,

    /** PDMIACPIPORT            - ACPI port interface.                  (Down)   Coupled with PDMINTERFACE_ACPI_CONNECTOR. */
    PDMINTERFACE_ACPI_PORT,
    /** PDMIACPICONNECTOR       - ACPI connector interface.             (Up)     Coupled with PDMINTERFACE_ACPI_PORT. */
    PDMINTERFACE_ACPI_CONNECTOR,

    /** PDMIHGCMPORT            - The Host-Guest communication manager port interface. Normally implemented by VMMDev. */
    PDMINTERFACE_HGCM_PORT,
    /** PDMIHGCMCONNECTOR       - The Host-Guest communication manager connector interface. Normally implemented by Main::VMMDevInterface. */
    PDMINTERFACE_HGCM_CONNECTOR,

    /** VUSBIROOTHUBPORT        - VUSB RootHub port interface.          (Down)   Coupled with PDMINTERFACE_USB_RH_CONNECTOR. */
    PDMINTERFACE_VUSB_RH_PORT,
    /** VUSBIROOTHUBCONNECTOR   - VUSB RootHub connector interface.     (Up)     Coupled with PDMINTERFACE_USB_RH_PORT. */
    PDMINTERFACE_VUSB_RH_CONNECTOR,
    /** VUSBIROOTHUBCONNECTOR   - VUSB RootHub configuration interface. (Main)   Used by the managment api. */
    PDMINTERFACE_VUSB_RH_CONFIG,

    /** VUSBROOTHUBCONNECTOR    - VUSB Device interface.                (Up)     No coupling. */
    PDMINTERFACE_VUSB_DEVICE,

    /** Maximum interface number. */
    PDMINTERFACE_MAX
} PDMINTERFACE;


/**
 * PDM Driver Base Interface.
 */
typedef struct PDMIBASE
{
    /**
     * Queries an interface to the driver.
     *
     * @returns Pointer to interface.
     * @returns NULL if the interface was not supported by the driver.
     * @param   pInterface          Pointer to this interface structure.
     * @param   enmInterface        The requested interface identification.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(void *, pfnQueryInterface,(struct PDMIBASE *pInterface, PDMINTERFACE enmInterface));
} PDMIBASE;
/** Pointer to a PDM Driver Base Interface. */
typedef PDMIBASE *PPDMIBASE;


/**
 * Dummy interface.
 *
 * This is used to typedef other dummy interfaces. The purpose of a dummy
 * interface is to validate the logical function of a driver/device and
 * full a natural interface pair.
 */
typedef struct PDMIDUMMY
{
    RTHCPTR pvDummy;
} PDMIDUMMY;


/** Pointer to a mouse port interface. */
typedef struct PDMIMOUSEPORT *PPDMIMOUSEPORT;
/**
 * Mouse port interface.
 * Pair with PDMIMOUSECONNECTOR.
 */
typedef struct PDMIMOUSEPORT
{
    /**
     * Puts a mouse event.
     * This is called by the source of mouse events. The event will be passed up until the
     * topmost driver, which then calls the registered event handler.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface structure.
     * @param   i32DeltaX       The X delta.
     * @param   i32DeltaY       The Y delta.
     * @param   i32DeltaZ       The Z delta.
     * @param   fButtonStates   The button states, see the PDMIMOUSEPORT_BUTTON_* \#defines.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnPutEvent,(PPDMIMOUSEPORT pInterface, int32_t i32DeltaX, int32_t i32DeltaY, int32_t i32DeltaZ, uint32_t fButtonStates));
} PDMIMOUSEPORT;

/** Mouse button defines for PDMIMOUSEPORT::pfnPutEvent.
 * @{ */
#define PDMIMOUSEPORT_BUTTON_LEFT   BIT(0)
#define PDMIMOUSEPORT_BUTTON_RIGHT  BIT(1)
#define PDMIMOUSEPORT_BUTTON_MIDDLE BIT(2)
/** @} */


/**
 * Mouse connector interface.
 * Pair with PDMIMOUSEPORT.
 */
typedef PDMIDUMMY PDMIMOUSECONNECTOR;
 /** Pointer to a mouse connector interface. */
typedef PDMIMOUSECONNECTOR *PPDMIMOUSECONNECTOR;


/** Pointer to a keyboard port interface. */
typedef struct PDMIKEYBOARDPORT *PPDMIKEYBOARDPORT;
/**
 * Keyboard port interface.
 * Pair with PDMIKEYBOARDCONNECTOR.
 */
typedef struct PDMIKEYBOARDPORT
{
    /**
     * Puts a keyboard event.
     * This is called by the source of keyboard events. The event will be passed up until the
     * topmost driver, which then calls the registered event handler.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface structure.
     * @param   u8KeyCode           The keycode to queue.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnPutEvent,(PPDMIKEYBOARDPORT pInterface, uint8_t u8KeyCode));
} PDMIKEYBOARDPORT;

/**
 * Keyboard LEDs.
 */
typedef enum PDMKEYBLEDS
{
    /** Num Lock */
    PDMKEYBLEDS_NUMLOCK          = 0x0001,
    /** Caps Lock */
    PDMKEYBLEDS_CAPSLOCK         = 0x0002,
    /** Scroll Lock */
    PDMKEYBLEDS_SCROLLLOCK       = 0x0004
} PDMKEYBLEDS;

/** Pointer to keyboard connector interface. */
typedef struct PDMIKEYBOARDCONNECTOR *PPDMIKEYBOARDCONNECTOR;


/**
 * Keyboard connector interface.
 * Pair with PDMIKEYBOARDPORT
 */
typedef struct PDMIKEYBOARDCONNECTOR
{
    /**
     * Notifies the the downstream driver about an LED change initiated by the guest.
     *
     * @param   pInterface      Pointer to the this interface.
     * @param   enmLeds         The new led mask.
     */
    DECLR3CALLBACKMEMBER(void, pfnLedStatusChange,(PPDMIKEYBOARDCONNECTOR pInterface, PDMKEYBLEDS enmLeds));

} PDMIKEYBOARDCONNECTOR;


/** Pointer to a display port interface. */
typedef struct PDMIDISPLAYPORT *PPDMIDISPLAYPORT;
/**
 * Display port interface.
 * Pair with PDMIDISPLAYCONNECTOR.
 */
typedef struct PDMIDISPLAYPORT
{
    /**
     * Update the display with any changed regions.
     *
     * Flushes any display changes to the memory pointed to by the
     * PDMIDISPLAYCONNECTOR interface and calles PDMIDISPLAYCONNECTOR::pfnUpdateRect()
     * while doing so.
     *
     * @param   pInterface          Pointer to this interface.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateDisplay,(PPDMIDISPLAYPORT pInterface));

    /**
     * Update the entire display.
     *
     * Flushes the entire display content to the memory pointed to by the
     * PDMIDISPLAYCONNECTOR interface and calles PDMIDISPLAYCONNECTOR::pfnUpdateRect().
     *
     * @param   pInterface          Pointer to this interface.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateDisplayAll,(PPDMIDISPLAYPORT pInterface));

    /**
     * Return the current guest color depth in bits per pixel (bpp).
     *
     * As the graphics card is able to provide display updates with the bpp
     * requested by the host, this method can be used to query the actual
     * guest color depth.
     *
     * @returns VBox status code.
     * @param   pInterface         Pointer to this interface.
     * @param   pcBits             Where to store the current guest color depth.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryColorDepth,(PPDMIDISPLAYPORT pInterface, uint32_t *pcBits));

    /**
     * Sets the refresh rate and restart the timer.
     * The rate is defined as the minimum interval between the return of
     * one PDMIDISPLAYPORT::pfnRefresh() call to the next one.
     *
     * The interval timer will be restarted by this call. So at VM startup
     * this function must be called to start the refresh cycle. The refresh
     * rate is not saved, but have to be when resuming a loaded VM state.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @param   cMilliesInterval    Number of millies between two refreshes.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetRefreshRate,(PPDMIDISPLAYPORT pInterface, uint32_t cMilliesInterval));

    /**
     * Create a 32-bbp snapshot of the display.
     *
     * This will create a 32-bbp bitmap with dword aligned scanline length. Because
     * of a wish for no locks in the graphics device, this must be called from the
     * emulation thread.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pvData              Pointer the buffer to copy the bits to.
     * @param   cbData              Size of the buffer.
     * @param   pcx                 Where to store the width of the bitmap. (optional)
     * @param   pcy                 Where to store the height of the bitmap. (optional)
     * @param   pcbData             Where to store the actual size of the bitmap. (optional)
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSnapshot,(PPDMIDISPLAYPORT pInterface, void *pvData, size_t cbData, uint32_t *pcx, uint32_t *pcy, size_t *pcbData));

    /**
     * Copy bitmap to the display.
     *
     * This will convert and copy a 32-bbp bitmap (with dword aligned scanline length) to
     * the memory pointed to by the PDMIDISPLAYCONNECTOR interface.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pvData              Pointer to the bitmap bits.
     * @param   x                   The upper left corner x coordinate of the destination rectangle.
     * @param   y                   The upper left corner y coordinate of the destination rectangle.
     * @param   cx                  The width of the source and destination rectangles.
     * @param   cy                  The height of the source and destination rectangles.
     * @thread  The emulation thread.
     * @remark  This is just a convenience for using the bitmap conversions of the
     *          graphics device.
     */
    DECLR3CALLBACKMEMBER(int, pfnDisplayBlt,(PPDMIDISPLAYPORT pInterface, const void *pvData, uint32_t x, uint32_t y, uint32_t cx, uint32_t cy));

    /**
     * Render a rectangle from guest VRAM to Framebuffer.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   x                   The upper left corner x coordinate of the rectangle to be updated.
     * @param   y                   The upper left corner y coordinate of the rectangle to be updated.
     * @param   cx                  The width of the rectangle to be updated.
     * @param   cy                  The height of the rectangle to be updated.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateDisplayRect,(PPDMIDISPLAYPORT pInterface, int32_t x, int32_t y, uint32_t cx, uint32_t cy));

    /**
     * Setup guest physical VRAM to use the provided page aligned
     * memory buffer as the guest VRAM, may be equal to current guest VRAM.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @param   pvBuffer            Page aligned address. NULL for removing previously set buffer.
     * @param   cbBuffer            Size of buffer. Must be equal to a whole number of pages.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetupVRAM,(PPDMIDISPLAYPORT pInterface, void *pvBuffer, uint32_t cbBuffer));
} PDMIDISPLAYPORT;


/** Pointer to a display connector interface. */
typedef struct PDMIDISPLAYCONNECTOR *PPDMIDISPLAYCONNECTOR;
/**
 * Display connector interface.
 * Pair with PDMIDISPLAYPORT.
 */
typedef struct PDMIDISPLAYCONNECTOR
{
    /**
     * Resize the display.
     * This is called when the resolution changes. This usually happens on
     * request from the guest os, but may also happen as the result of a reset.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   cBits               Color depth (bits per pixel) of the new video mode.
     * @param   pvVRAM              Address of guest VRAM.
     * @param   cbLine              Size in bytes of a single scan line.
     * @param   cx                  New display width.
     * @param   cy                  New display height.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnResize,(PPDMIDISPLAYCONNECTOR pInterface, uint32_t cBits, void *pvVRAM, uint32_t cbLine, uint32_t cx, uint32_t cy));

    /**
     * Update a rectangle of the display.
     * PDMIDISPLAYPORT::pfnUpdateDisplay is the caller.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   x                   The upper left corner x coordinate of the rectangle.
     * @param   y                   The upper left corner y coordinate of the rectangle.
     * @param   cx                  The width of the rectangle.
     * @param   cy                  The height of the rectangle.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateRect,(PPDMIDISPLAYCONNECTOR pInterface, uint32_t x, uint32_t y, uint32_t cx, uint32_t cy));

    /**
     * Refresh the display.
     *
     * The interval between these calls is set by
     * PDMIDISPLAYPORT::pfnSetRefreshRate(). The driver should call
     * PDMIDISPLAYPORT::pfnUpdateDisplay() if it wishes to refresh the
     * display. PDMIDISPLAYPORT::pfnUpdateDisplay calls pfnUpdateRect with
     * the changed rectangles.
     *
     * @param   pInterface          Pointer to this interface.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnRefresh,(PPDMIDISPLAYCONNECTOR pInterface));

    /**
     * Reset the display.
     *
     * Notification message when the graphics card has been reset.
     *
     * @param   pInterface          Pointer to this interface.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnReset,(PPDMIDISPLAYCONNECTOR pInterface));

    /**
     * LFB video mode enter/exit.
     *
     * Notification message when LinearFrameBuffer video mode is enabled/disabled.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   fEnabled            false - LFB mode was disabled,
     *                              true -  an LFB mode was disabled
     * @thread  The emulation thread.
     */
    DECLCALLBACKMEMBER(void, pfnLFBModeChange)(PPDMIDISPLAYCONNECTOR pInterface, bool fEnabled);


    /** Read-only attributes.
     * For preformance reasons some readonly attributes are kept in the interface.
     * We trust the interface users to respect the readonlyness of these.
     * @{
     */
    /** Pointer to the display data buffer. */
    uint8_t        *pu8Data;
    /** Size of a scanline in the data buffer. */
    uint32_t        cbScanline;
    /** The color depth (in bits) the graphics card is supposed to provide. */
    uint32_t        cBits;
    /** The display width. */
    uint32_t        cx;
    /** The display height. */
    uint32_t        cy;
    /** @} */
} PDMIDISPLAYCONNECTOR;



/**
 * Block drive type.
 */
typedef enum PDMBLOCKTYPE
{
    /** Error (for the query function). */
    PDMBLOCKTYPE_ERROR = 1,
    /** 360KB 5 1/4" floppy drive. */
    PDMBLOCKTYPE_FLOPPY_360,
    /** 720KB 3 1/2" floppy drive. */
    PDMBLOCKTYPE_FLOPPY_720,
    /** 1.2MB 5 1/4" floppy drive. */
    PDMBLOCKTYPE_FLOPPY_1_20,
    /** 1.44MB 3 1/2" floppy drive. */
    PDMBLOCKTYPE_FLOPPY_1_44,
    /** 2.88MB 3 1/2" floppy drive. */
    PDMBLOCKTYPE_FLOPPY_2_88,
    /** CDROM drive. */
    PDMBLOCKTYPE_CDROM,
    /** DVD drive. */
    PDMBLOCKTYPE_DVD,
    /** Hard disk drive. */
    PDMBLOCKTYPE_HARD_DISK
} PDMBLOCKTYPE;


/**
 * Block raw command data transfer direction.
 */
typedef enum PDMBLOCKTXDIR
{
    PDMBLOCKTXDIR_NONE = 0,
    PDMBLOCKTXDIR_FROM_DEVICE,
    PDMBLOCKTXDIR_TO_DEVICE
} PDMBLOCKTXDIR;

/**
 * Block notify interface.
 * Pair with PDMIBLOCK.
 */
typedef PDMIDUMMY PDMIBLOCKPORT;
/** Pointer to a block notify interface (dummy). */
typedef PDMIBLOCKPORT *PPDMIBLOCKPORT;


/** Pointer to a block interface. */
typedef struct PDMIBLOCK *PPDMIBLOCK;
/**
 * Block interface.
 * Pair with PDMIBLOCK.
 */
typedef struct PDMIBLOCK
{
    /**
     * Read bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start reading from.
     * @param   pvBuf           Where to store the read bits.
     * @param   cbRead          Number of bytes to read.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnRead,(PPDMIBLOCK pInterface, uint64_t off, void *pvBuf, size_t cbRead));

    /**
     * Write bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start writing at.
     * @param   pvBuf           Where to store the write bits.
     * @param   cbWrite         Number of bytes to write.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnWrite,(PPDMIBLOCK pInterface, uint64_t off, const void *pvBuf, size_t cbWrite));

    /**
     * Make sure that the bits written are actually on the storage medium.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnFlush,(PPDMIBLOCK pInterface));

    /**
     * Send a raw command to the underlying device (CDROM).
     * This method is optional (i.e. the function pointer may be NULL).
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pbCmd           Offset to start reading from.
     * @param   enmTxDir        Direction of transfer.
     * @param   pvBuf           Pointer tp the transfer buffer.
     * @param   cbBuf           Size of the transfer buffer.
     * @param   pbSenseKey      Status of the command (when return value is VERR_DEV_IO_ERROR).
     * @param   cTimeoutMillies Command timeout in milliseconds.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSendCmd,(PPDMIBLOCK pInterface, const uint8_t *pbCmd, PDMBLOCKTXDIR enmTxDir, void *pvBuf, size_t *pcbBuf, uint8_t *pbSenseKey, uint32_t cTimeoutMillies));

    /**
     * Check if the media is readonly or not.
     *
     * @returns true if readonly.
     * @returns false if read/write.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsReadOnly,(PPDMIBLOCK pInterface));

    /**
     * Gets the media size in bytes.
     *
     * @returns Media size in bytes.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(uint64_t, pfnGetSize,(PPDMIBLOCK pInterface));

    /**
     * Gets the block drive type.
     *
     * @returns block drive type.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(PDMBLOCKTYPE, pfnGetType,(PPDMIBLOCK pInterface));

    /**
     * Gets the UUID of the block drive.
     * Don't return the media UUID if it's removable.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pUuid           Where to store the UUID on success.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetUuid,(PPDMIBLOCK pInterface, PRTUUID pUuid));
} PDMIBLOCK;


/** Pointer to a mount interface. */
typedef struct PDMIMOUNTNOTIFY *PPDMIMOUNTNOTIFY;
/**
 * Block interface.
 * Pair with PDMIMOUNT.
 */
typedef struct PDMIMOUNTNOTIFY
{
    /**
     * Called when a media is mounted.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnMountNotify,(PPDMIMOUNTNOTIFY pInterface));

    /**
     * Called when a media is unmounted
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUnmountNotify,(PPDMIMOUNTNOTIFY pInterface));
} PDMIMOUNTNOTIFY;


/* Pointer to mount interface. */
typedef struct PDMIMOUNT *PPDMIMOUNT;
/**
 * Mount interface.
 * Pair with PDMIMOUNTNOTIFY.
 */
typedef struct PDMIMOUNT
{
    /**
     * Mount a media.
     *
     * This will not unmount any currently mounted media!
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pszFilename     Pointer to filename. If this is NULL it assumed that the caller have
     *                          constructed a configuration which can be attached to the bottom driver.
     * @param   pszCoreDriver   Core driver name. NULL will cause autodetection. Ignored if pszFilanem is NULL.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnMount,(PPDMIMOUNT pInterface, const char *pszFilename, const char *pszCoreDriver));

    /**
     * Unmount the media.
     *
     * The driver will validate and pass it on. On the rebounce it will decide whether or not to detach it self.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnUnmount,(PPDMIMOUNT pInterface));

    /**
     * Checks if a media is mounted.
     *
     * @returns true if mounted.
     * @returns false if not mounted.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsMounted,(PPDMIMOUNT pInterface));

    /**
     * Locks the media, preventing any unmounting of it.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnLock,(PPDMIMOUNT pInterface));

    /**
     * Unlocks the media, canceling previous calls to pfnLock().
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnUnlock,(PPDMIMOUNT pInterface));

    /**
     * Checks if a media is locked.
     *
     * @returns true if locked.
     * @returns false if not locked.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsLocked,(PPDMIMOUNT pInterface));
} PDMIBLOCKMOUNT;

/**
 * BIOS translation mode.
 */
typedef enum PDMBIOSTRANSLATION
{
    /** No translation. */
    PDMBIOSTRANSLATION_NONE = 1,
    /** LBA translation. */
    PDMBIOSTRANSLATION_LBA,
    /** Automatic select mode. */
    PDMBIOSTRANSLATION_AUTO
} PDMBIOSTRANSLATION;

/** Pointer to BIOS translation mode. */
typedef PDMBIOSTRANSLATION *PPDMBIOSTRANSLATION;

/** Pointer to a media interface. */
typedef struct PDMIMEDIA *PPDMIMEDIA;
/**
 * Media interface.
 * Makes up the fundation for PDMIBLOCK and PDMIBLOCKBIOS.
 */
typedef struct PDMIMEDIA
{
    /**
     * Read bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start reading from.
     * @param   pvBuf           Where to store the read bits.
     * @param   cbRead          Number of bytes to read.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnRead,(PPDMIMEDIA pInterface, uint64_t off, void *pvBuf, size_t cbRead));

    /**
     * Write bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start writing at.
     * @param   pvBuf           Where to store the write bits.
     * @param   cbWrite         Number of bytes to write.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnWrite,(PPDMIMEDIA pInterface, uint64_t off, const void *pvBuf, size_t cbWrite));

    /**
     * Make sure that the bits written are actually on the storage medium.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnFlush,(PPDMIMEDIA pInterface));

    /**
     * Get the media size in bytes.
     *
     * @returns Media size in bytes.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(uint64_t, pfnGetSize,(PPDMIMEDIA pInterface));

    /**
     * Check if the media is readonly or not.
     *
     * @returns true if readonly.
     * @returns false if read/write.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsReadOnly,(PPDMIMEDIA pInterface));

    /**
     * Get stored media geometry - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @returns VERR_PDM_GEOMETRY_NOT_SET if the geometry hasn't been set using pfnBiosSetGeometry() yet.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pcCylinders     Number of cylinders.
     * @param   pcHeads         Number of heads.
     * @param   pcSectors       Number of sectors. This number is 1-based.
     * @remark  This have no influence on the read/write operations.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnBiosGetGeometry,(PPDMIMEDIA pInterface, uint32_t *pcCylinders, uint32_t *pcHeads, uint32_t *pcSectors));

    /**
     * Store the media geometry - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   cCylinders      Number of cylinders.
     * @param   cHeads          Number of heads.
     * @param   cSectors        Number of sectors. This number is 1-based.
     * @remark  This have no influence on the read/write operations.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnBiosSetGeometry,(PPDMIMEDIA pInterface, uint32_t cCylinders, uint32_t cHeads, uint32_t cSectors));

    /**
     * Get stored geometry translation mode - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry translation mode.
     * @returns VERR_PDM_TRANSLATION_NOT_SET if the translation hasn't been set using pfnBiosSetTranslation() yet.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   penmTranslation Where to store the translation type.
     * @remark  This have no influence on the read/write operations.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnBiosGetTranslation,(PPDMIMEDIA pInterface, PPDMBIOSTRANSLATION penmTranslation));

    /**
     * Store media geometry - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   enmTranslation  The translation type.
     * @remark  This have no influence on the read/write operations.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnBiosSetTranslation,(PPDMIMEDIA pInterface, PDMBIOSTRANSLATION enmTranslation));

    /**
     * Gets the UUID of the media drive.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pUuid           Where to store the UUID on success.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetUuid,(PPDMIMEDIA pInterface, PRTUUID pUuid));

} PDMIMEDIA;


/** Pointer to a block BIOS interface. */
typedef struct PDMIBLOCKBIOS *PPDMIBLOCKBIOS;
/**
 * Media BIOS interface.
 * The interface the getting and setting properties which the BIOS/CMOS care about.
 */
typedef struct PDMIBLOCKBIOS
{
    /**
     * Get stored media geometry - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pcCylinders     Number of cylinders.
     * @param   pcHeads         Number of heads.
     * @param   pcSectors       Number of sectors. This number is 1-based.
     * @remark  This have no influence on the read/write operations.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetGeometry,(PPDMIBLOCKBIOS pInterface, uint32_t *pcCylinders, uint32_t *pcHeads, uint32_t *pcSectors));

    /**
     * Store the media geometry - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   cCylinders      Number of cylinders.
     * @param   cHeads          Number of heads.
     * @param   cSectors        Number of sectors. This number is 1-based.
     * @remark  This have no influence on the read/write operations.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetGeometry,(PPDMIBLOCKBIOS pInterface, uint32_t cCylinders, uint32_t cHeads, uint32_t cSectors));

    /**
     * Get stored geometry translation mode - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry translation mode.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   penmTranslation Where to store the translation type.
     * @remark  This have no influence on the read/write operations.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetTranslation,(PPDMIBLOCKBIOS pInterface, PPDMBIOSTRANSLATION penmTranslation));

    /**
     * Store media geometry - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   enmTranslation  The translation type.
     * @remark  This have no influence on the read/write operations.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetTranslation,(PPDMIBLOCKBIOS pInterface, PDMBIOSTRANSLATION enmTranslation));

    /**
     * Checks if the device should be visible to the BIOS or not.
     *
     * @returns true if the device is visible to the BIOS.
     * @returns false if the device is not visible to the BIOS.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsVisible,(PPDMIBLOCKBIOS pInterface));

    /**
     * Gets the block drive type.
     *
     * @returns block drive type.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(PDMBLOCKTYPE, pfnGetType,(PPDMIBLOCKBIOS pInterface));

} PDMIBLOCKBIOS;


/** Pointer to a static block core driver interface. */
typedef struct PDMIMEDIASTATIC *PPDMIMEDIASTATIC;
/**
 * Static block core driver interface.
 */
typedef struct PDMIMEDIASTATIC
{
    /**
     * Check if the specified file is a format which the core driver can handle.
     *
     * @returns true / false accordingly.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pszFilename     Name of the file to probe.
     */
    DECLR3CALLBACKMEMBER(bool, pfnCanHandle,(PPDMIMEDIASTATIC pInterface, const char *pszFilename));
} PDMIMEDIASTATIC;


/** Pointer to an iSCSI Request PDU buffer. */
typedef struct ISCSIREQ *PISCSIREQ;
/**
 * iSCSI Request PDU buffer (gather).
 */
typedef struct ISCSIREQ
{
    /** Length of PDU segment in bytes. */
    size_t cbSeg;
    /** Pointer to PDU segment. */
    const void *pcvSeg;
} ISCSIREQ;

/** Pointer to an iSCSI Response PDU buffer. */
typedef struct ISCSIRES *PISCSIRES;
/**
 * iSCSI Response PDU buffer (scatter).
 */
typedef struct ISCSIRES
{
    /** Length of PDU segment. */
    size_t cbSeg;
    /** Pointer to PDU segment. */
    void *pvSeg;
} ISCSIRES;

/** Pointer to an iSCSI transport driver interface. */
typedef struct PDMIISCSITRANSPORT *PPDMIISCSITRANSPORT;
/**
 * iSCSI transport driver interface.
 */
typedef struct PDMIISCSITRANSPORT
{
    /**
     * Read bytes from an iSCSI transport stream. If the connection fails, it is automatically
     * reopened on the next call after the error is signalled. Error recovery in this case is
     * the duty of the caller.
     *
     * @returns VBox status code.
     * @param   pTransport      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Where to store the read bits.
     * @param   cbBuf           Number of bytes to read.
     * @param   pcbRead         Actual number of bytes read.
     * @thread  Any thread.
     * @todo    Correct the docs.
     */
    DECLR3CALLBACKMEMBER(int, pfnRead,(PPDMIISCSITRANSPORT pTransport, PISCSIRES prgResponse, unsigned int cnResponse));

    /**
     * Write bytes to an iSCSI transport stream. Padding is performed when necessary. If the connection
     * fails, it is automatically reopened on the next call after the error is signalled. Error recovery
     * in this case is the duty of the caller.
     *
     * @returns VBox status code.
     * @param   pTransport      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Where the write bits are stored.
     * @param   cbWrite         Number of bytes to write.
     * @thread  Any thread.
     * @todo    Correct the docs.
     */
    DECLR3CALLBACKMEMBER(int, pfnWrite,(PPDMIISCSITRANSPORT pTransport, PISCSIREQ prgRequest, unsigned int cnRequest));

    /**
     * Open the iSCSI transport stream.
     *
     * @returns VBox status code.
     * @param   pTransport       Pointer to the interface structure containing the called function pointer.
     * @param   pszTargetAddress Pointer to string of the format address:port.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnOpen,(PPDMIISCSITRANSPORT pTransport, const char *pszTargetAddress));

    /**
     * Close the iSCSI transport stream.
     *
     * @returns VBox status code.
     * @param   pTransport      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnClose,(PPDMIISCSITRANSPORT pTransport));
} PDMIISCSITRANSPORT;


/** ACPI power source identifier */
typedef enum PDMACPIPOWERSOURCE
{
    PDM_ACPI_POWER_SOURCE_UNKNOWN  =   0,
    PDM_ACPI_POWER_SOURCE_OUTLET,
    PDM_ACPI_POWER_SOURCE_BATTERY
} PDMACPIPOWERSOURCE;
/** Pointer to ACPI battery state. */
typedef PDMACPIPOWERSOURCE *PPDMACPIPOWERSOURCE;

/** ACPI battey capacity */
typedef enum PDMACPIBATCAPACITY
{
    PDM_ACPI_BAT_CAPACITY_MIN      =   0,
    PDM_ACPI_BAT_CAPACITY_MAX      = 100,
    PDM_ACPI_BAT_CAPACITY_UNKNOWN  = 255
} PDMACPIBATCAPACITY;
/** Pointer to ACPI battery capacity. */
typedef PDMACPIBATCAPACITY *PPDMACPIBATCAPACITY;

/** ACPI battery state. See ACPI 3.0 spec '_BST (Battery Status)' */
typedef enum PDMACPIBATSTATE
{
    PDM_ACPI_BAT_STATE_CHARGED     = 0x00,
    PDM_ACPI_BAT_STATE_CHARGING    = 0x01,
    PDM_ACPI_BAT_STATE_DISCHARGING = 0x02,
    PDM_ACPI_BAT_STATE_CRITICAL    = 0x04
} PDMACPIBATSTATE;
/** Pointer to ACPI battery state. */
typedef PDMACPIBATSTATE *PPDMACPIBATSTATE;

/** Pointer to an ACPI port interface. */
typedef struct PDMIACPIPORT *PPDMIACPIPORT;
/**
 * ACPI port interface.
 */
typedef struct PDMIACPIPORT
{
    /**
     * Send an ACPI power off event.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     */
    DECLR3CALLBACKMEMBER(int, pfnPowerButtonPress,(PPDMIACPIPORT pInterface));
} PDMIACPIPORT;

/** Pointer to an ACPI connector interface. */
typedef struct PDMIACPICONNECTOR *PPDMIACPICONNECTOR;
/**
 * ACPI connector interface.
 */
typedef struct PDMIACPICONNECTOR
{
    /**
     * Get the current power source of the host system.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   penmPowerSource Pointer to the power source result variable.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryPowerSource,(PPDMIACPICONNECTOR, PPDMACPIPOWERSOURCE penmPowerSource));

    /**
     * Query the current battery status of the host system.
     *
     * @returns VBox status code?
     * @param   pInterface              Pointer to the interface structure containing the called function pointer.
     * @param   pfPresent               Is set to true if battery is present, false otherwise.
     * @param   penmRemainingCapacity   Pointer to the battery remaining capacity (0 - 100 or 255 for unknown).
     * @param   penmBatteryState        Pointer to the battery status.
     * @param   pu32PresentRate         Pointer to the present rate (0..1000 of the total capacity).
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryBatteryStatus,(PPDMIACPICONNECTOR, bool *pfPresent, PPDMACPIBATCAPACITY penmRemainingCapacity,
                                                     PPDMACPIBATSTATE penmBatteryState, uint32_t *pu32PresentRate));
} PDMIACPICONNECTOR;

/** Pointer to a VMMDevice port interface. */
typedef struct PDMIVMMDEVPORT *PPDMIVMMDEVPORT;
/**
 * VMMDevice port interface.
 */
typedef struct PDMIVMMDEVPORT
{
    /**
     * Return the current absolute mouse position in pixels
     *
     * @returns VBox status code
     * @param   pAbsX   Pointer of result value, can be NULL
     * @param   pAbsY   Pointer of result value, can be NULL
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryAbsoluteMouse,(PPDMIVMMDEVPORT pInterface, uint32_t *pAbsX, uint32_t *pAbsY));

    /**
     * Set the new absolute mouse position in pixels
     *
     * @returns VBox status code
     * @param   absX   New absolute X position
     * @param   absY   New absolute Y position
     */
    DECLR3CALLBACKMEMBER(int, pfnSetAbsoluteMouse,(PPDMIVMMDEVPORT pInterface, uint32_t absX, uint32_t absY));

    /**
     * Return the current mouse capability flags
     *
     * @returns VBox status code
     * @param   pCapabilities  Pointer of result value
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryMouseCapabilities,(PPDMIVMMDEVPORT pInterface, uint32_t *pCapabilities));

    /**
     * Set the current mouse capability flag (host side)
     *
     * @returns VBox status code
     * @param   capabilities  Capability mask
     */
    DECLR3CALLBACKMEMBER(int, pfnSetMouseCapabilities,(PPDMIVMMDEVPORT pInterface, uint32_t capabilities));

    /**
     * Issue a display resolution change request.
     *
     * Note that there can only one request in the queue and that in case the guest does
     * not process it, issuing another request will overwrite the previous.
     *
     * @returns VBox status code
     * @param   cx          Horizontal pixel resolution (0 = do not change).
     * @param   cy          Vertical pixel resolution (0 = do not change).
     * @param   cBits       Bits per pixel (0 = do not change).
     */
    DECLR3CALLBACKMEMBER(int, pfnRequestDisplayChange,(PPDMIVMMDEVPORT pInterface, uint32_t cx, uint32_t cy, uint32_t cBits));

    /**
     * Pass credentials to guest.
     *
     * Note that there can only be one set of credentials and the guest may or may not
     * query them and may do whatever it wants with them.
     *
     * @returns VBox status code
     * @param   pszUsername            User name, may be empty (UTF-8)
     * @param   pszPassword            Password, may be empty (UTF-8)
     * @param   pszDomain              Domain name, may be empty (UTF-8)
     * @param   fFlags                 Bitflags
     */
    DECLR3CALLBACKMEMBER(int, pfnSetCredentials,(PPDMIVMMDEVPORT pInterface, const char *pszUsername,
                                                 const char *pszPassword, const char *pszDomain,
                                                 uint32_t fFlags));

    /**
     * Notify the driver about a VBVA status change.
     *
     * @returns Nothing. Because it is informational callback.
     * @param   fEnabled    Current VBVA status.
     */
    DECLCALLBACKMEMBER(void, pfnVBVAChange)(PPDMIVMMDEVPORT pInterface, bool fEnabled);

} PDMIVMMDEVPORT;

/** Forward declaration of video accelerator command memory. */
struct _VBVAMEMORY;
/** Pointer to video accelerator command memory. */
typedef struct _VBVAMEMORY *PVBVAMEMORY;

/** Pointer to a VMMDev connector interface. */
typedef struct PDMIVMMDEVCONNECTOR *PPDMIVMMDEVCONNECTOR;
/**
 * VMMDev connector interface.
 * Pair with PDMIVMMDEVPORT.
 */
typedef struct PDMIVMMDEVCONNECTOR
{
    /**
     * Report guest OS version.
     * Called whenever the Additions issue a guest version report request.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pGuestInfo          Pointer to guest information structure
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateGuestVersion,(PPDMIVMMDEVCONNECTOR pInterface, VBoxGuestInfo *pGuestInfo));

    /**
     * Update the mouse capabilities.
     * This is called when the mouse capabilities change. The new capabilities
     * are given and the connector should update its internal state.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   newCapabilities     New capabilities.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateMouseCapabilities,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t newCapabilities));

    /**
     * Update the pointer shape.
     * This is called when the mouse pointer shape changes. The new shape
     * is passed as a caller allocated buffer that will be freed after returning
     *
     * @param   pInterface          Pointer to this interface.
     * @param   fVisible            Visibility indicator (if false, the other parameters are undefined).
     * @param   fAlpha              Flag whether alpha channel is being passed.
     * @param   xHot                Pointer hot spot x coordinate.
     * @param   yHot                Pointer hot spot y coordinate.
     * @param   x                   Pointer new x coordinate on screen.
     * @param   y                   Pointer new y coordinate on screen.
     * @param   cx                  Pointer width in pixels.
     * @param   cy                  Pointer height in pixels.
     * @param   cbScanline          Size of one scanline in bytes.
     * @param   pvShape             New shape buffer.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdatePointerShape,(PPDMIVMMDEVCONNECTOR pInterface, bool fVisible, bool fAlpha,
                                                      uint32_t xHot, uint32_t yHot,
                                                      uint32_t cx, uint32_t cy,
                                                      void *pvShape));

    /**
     * Enable or disable video acceleration on behalf of guest.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   fEnable             Whether to enable acceleration.
     * @param   pVbvaMemory         Video accelerator memory.

     * @return  VBox rc. VINF_SUCCESS if VBVA was enabled.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnVideoAccelEnable,(PPDMIVMMDEVCONNECTOR pInterface, bool fEnable, PVBVAMEMORY pVbvaMemory));

    /**
     * Force video queue processing.
     *
     * @param   pInterface          Pointer to this interface.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnVideoAccelFlush,(PPDMIVMMDEVCONNECTOR pInterface));

    /**
     * Return whether the given video mode is supported/wanted by the host.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to this interface.
     * @param   cy              Video mode horizontal resolution in pixels.
     * @param   cx              Video mode vertical resolution in pixels.
     * @param   cBits           Video mode bits per pixel.
     * @param   pfSupported     Where to put the indicator for whether this mode is supported. (output)
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnVideoModeSupported,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t cx, uint32_t cy, uint32_t cBits, bool *pfSupported));

    /**
     * Queries by how many pixels the height should be reduced when calculating video modes
     *
     * @returns VBox status code
     * @param   pInterface          Pointer to this interface.
     * @param   pcyReduction        Pointer to the result value.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetHeightReduction,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t *pcyReduction));

    /**
     * Informs about a credentials judgement result from the guest.
     *
     * @returns VBox status code
     * @param   pInterface          Pointer to this interface.
     * @param   fFlags              Judgement result flags.
     * @thread  The emulation thread.
     */
     DECLR3CALLBACKMEMBER(int, pfnSetCredentialsJudgementResult,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t fFlags));
} PDMIVMMDEVCONNECTOR;


/**
 * MAC address.
 * (The first 24 bits are the 'company id', where the first bit seems to have a special meaning if set.)
 */
typedef union PDMMAC
{
    /** 8-bit view. */
    uint8_t     au8[6];
    /** 16-bit view. */
    uint16_t    au16[3];
} PDMMAC;
/** Pointer to a MAC address. */
typedef PDMMAC *PPDMMAC;
/** Pointer to a const MAC address. */
typedef const PDMMAC *PCPDMMAC;


/** Pointer to a network port interface */
typedef struct PDMINETWORKPORT *PPDMINETWORKPORT;
/**
 * Network port interface.
 */
typedef struct PDMINETWORKPORT
{
    /**
     * Check how much data the device/driver can receive data now.
     * This must be called before the pfnRecieve() method is called.
     *
     * @returns Number of bytes the device can receive now.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(size_t, pfnCanReceive,(PPDMINETWORKPORT pInterface));

    /**
     * Receive data from the network.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           The available data.
     * @param   cb              Number of bytes available in the buffer.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnReceive,(PPDMINETWORKPORT pInterface, const void *pvBuf, size_t cb));

} PDMINETWORKPORT;


/**
 * Network link state.
 */
typedef enum PDMNETWORKLINKSTATE
{
    /** Invalid state. */
    PDMNETWORKLINKSTATE_INVALID = 0,
    /** The link is up. */
    PDMNETWORKLINKSTATE_UP,
    /** The link is down. */
    PDMNETWORKLINKSTATE_DOWN,
    /** The link is temporarily down while resuming. */
    PDMNETWORKLINKSTATE_DOWN_RESUME
} PDMNETWORKLINKSTATE;

/** Pointer to a network connector interface */
typedef struct PDMINETWORKCONNECTOR *PPDMINETWORKCONNECTOR;
/**
 * Network connector interface.
 */
typedef struct PDMINETWORKCONNECTOR
{
    /**
     * Send data to the network.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Data to send.
     * @param   cb              Number of bytes to send.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnSend,(PPDMINETWORKCONNECTOR pInterface, const void *pvBuf, size_t cb));

    /**
     * Set promiscuous mode.
     *
     * This is called when the promiscuous mode is set. This means that there doesn't have
     * to be a mode change when it's called.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   fPromiscuous    Set if the adaptor is now in promiscuous mode. Clear if it is not.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(void, pfnSetPromiscuousMode,(PPDMINETWORKCONNECTOR pInterface, bool fPromiscuous));

    /**
     * Notification on link status changes.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   enmLinkState    The new link state.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(void, pfnNotifyLinkChanged,(PPDMINETWORKCONNECTOR pInterface, PDMNETWORKLINKSTATE enmLinkState));

    /**
     * More receive buffer has become available.
     *
     * This is called when the NIC frees up receive buffers.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @remark  This function isn't called by pcnet nor yet.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(void, pfnNotifyCanReceive,(PPDMINETWORKCONNECTOR pInterface));

} PDMINETWORKCONNECTOR;


/** Pointer to a network config port interface */
typedef struct PDMINETWORKCONFIG *PPDMINETWORKCONFIG;
/**
 * Network config port interface.
 */
typedef struct PDMINETWORKCONFIG
{
    /**
     * Gets the current Media Access Control (MAC) address.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pMac            Where to store the MAC address.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnGetMac,(PPDMINETWORKCONFIG pInterface, PPDMMAC *pMac));

    /**
     * Gets the new link state.
     *
     * @returns The current link state.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(PDMNETWORKLINKSTATE, pfnGetLinkState,(PPDMINETWORKCONFIG pInterface));

    /**
     * Sets the new link state.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   enmState        The new link state
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnSetLinkState,(PPDMINETWORKCONFIG pInterface, PDMNETWORKLINKSTATE enmState));

} PDMINETWORKCONFIG;


/** Pointer to a network connector interface */
typedef struct PDMIAUDIOCONNECTOR *PPDMIAUDIOCONNECTOR;
/**
 * Audio connector interface.
 */
typedef struct PDMIAUDIOCONNECTOR
{
    DECLR3CALLBACKMEMBER(void, pfnRun,(PPDMIAUDIOCONNECTOR pInterface));

/*    DECLR3CALLBACKMEMBER(int,  pfnSetRecordSource,(PPDMIAUDIOINCONNECTOR pInterface, AUDIORECSOURCE)); */

} PDMIAUDIOCONNECTOR;


/** @todo r=bird: the two following interfaces are hacks to work around the missing audio driver
 * interface. This should be addressed rather than making more temporary hacks. */

/** Pointer to a Audio Sniffer Device port interface. */
typedef struct PDMIAUDIOSNIFFERPORT *PPDMIAUDIOSNIFFERPORT;

/**
 * Audio Sniffer port interface.
 */
typedef struct PDMIAUDIOSNIFFERPORT
{
    /**
     * Enables or disables sniffing. If sniffing is being enabled also sets a flag
     * whether the audio must be also left on the host.
     *
     * @returns VBox status code
     * @param pInterface      Pointer to this interface.
     * @param fEnable         'true' for enable sniffing, 'false' to disable.
     * @param fKeepHostAudio  Indicates whether host audio should also present
     *                        'true' means that sound should not be played
     *                        by the audio device.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetup,(PPDMIAUDIOSNIFFERPORT pInterface, bool fEnable, bool fKeepHostAudio));

} PDMIAUDIOSNIFFERPORT;

/** Pointer to a Audio Sniffer connector interface. */
typedef struct PDMIAUDIOSNIFFERCONNECTOR *PPDMIAUDIOSNIFFERCONNECTOR;

/**
 * Audio Sniffer connector interface.
 * Pair with PDMIAUDIOSNIFFERPORT.
 */
typedef struct PDMIAUDIOSNIFFERCONNECTOR
{
    /**
     * AudioSniffer device calls this method when audio samples
     * are about to be played and sniffing is enabled.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pvSamples           Audio samples buffer.
     * @param   cSamples            How many complete samples are in the buffer.
     * @param   iSampleHz           The sample frequency in Hz.
     * @param   cChannels           Number of channels. 1 for mono, 2 for stereo.
     * @param   cBits               How many bits a sample for a single channel has. Normally 8 or 16.
     * @param   fUnsigned           Whether samples are unsigned values.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnAudioSamplesOut,(PPDMIAUDIOSNIFFERCONNECTOR pInterface, void *pvSamples, uint32_t cSamples,
                                                   int iSampleHz, int cChannels, int cBits, bool fUnsigned));

    /**
     * AudioSniffer device calls this method when output volume is changed.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   u16LeftVolume       0..0xFFFF volume level for left channel.
     * @param   u16RightVolume      0..0xFFFF volume level for right channel.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnAudioVolumeOut,(PPDMIAUDIOSNIFFERCONNECTOR pInterface, uint16_t u16LeftVolume, uint16_t u16RightVolume));

} PDMIAUDIOSNIFFERCONNECTOR;


/**
 * Generic status LED core.
 * Note that a unit doesn't have to support all the indicators.
 */
typedef union PDMLEDCORE
{
    /** 32-bit view. */
    uint32_t volatile u32;
    /** Bit view. */
    struct
    {
        /** Reading/Receiving indicator. */
        uint32_t    fReading : 1;
        /** Writing/Sending indicator. */
        uint32_t    fWriting : 1;
        /** Busy indicator. */
        uint32_t    fBusy : 1;
        /** Error indicator. */
        uint32_t    fError : 1;
    }           s;
} PDMLEDCORE;

/** LED bit masks for the u32 view.
 * @{ */
/** Reading/Receiving indicator. */
#define PDMLED_READING  BIT(0)
/** Writing/Sending indicator. */
#define PDMLED_WRITING  BIT(1)
/** Busy indicator. */
#define PDMLED_BUSY     BIT(2)
/** Error indicator. */
#define PDMLED_ERROR    BIT(3)
/** @} */


/**
 * Generic status LED.
 * Note that a unit doesn't have to support all the indicators.
 */
typedef struct PDMLED
{
    /** Just a magic for sanity checking. */
    uint32_t    u32Magic;
    /** The actual LED status.
     * Only the device is allowed to change this. */
    PDMLEDCORE  Actual;
    /** The asserted LED status which is cleared by the reader.
     * The device will assert the bits but never clear them.
     * The driver clears them as it sees fit. */
    PDMLEDCORE  Asserted;
} PDMLED;

/** Pointer to an LED. */
typedef PDMLED *PPDMLED;
/** Pointer to a const LED. */
typedef const PDMLED *PCPDMLED;

#define PDMLED_MAGIC ( 0x11335577 )

/** Pointer to an LED ports interface. */
typedef struct PDMILEDPORTS      *PPDMILEDPORTS;
/**
 * Interface for exporting LEDs.
 */
typedef struct PDMILEDPORTS
{
    /**
     * Gets the pointer to the status LED of a unit.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   iLUN            The unit which status LED we desire.
     * @param   ppLed           Where to store the LED pointer.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryStatusLed,(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed));

} PDMILEDPORTS;


/** Pointer to an LED connectors interface. */
typedef struct PDMILEDCONNECTORS *PPDMILEDCONNECTORS;
/**
 * Interface for reading LEDs.
 */
typedef struct PDMILEDCONNECTORS
{
    /**
     * Notification about a unit which have been changed.
     *
     * The driver must discard any pointers to data owned by
     * the unit and requery it.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   iLUN            The unit number.
     */
    DECLR3CALLBACKMEMBER(void, pfnUnitChanged,(PPDMILEDCONNECTORS pInterface, unsigned iLUN));
} PDMILEDCONNECTORS;


/** The special status unit number */
#define PDM_STATUS_LUN      999


#ifdef VBOX_HGCM

/** Abstract HGCM command structure. Used only to define a typed pointer. */
struct VBOXHGCMCMD;

/** Pointer to HGCM command structure. This pointer is unique and identifies
 *  the command being processed. The pointer is passed to HGCM connector methods,
 *  and must be passed back to HGCM port when command is completed.
 */
typedef struct VBOXHGCMCMD *PVBOXHGCMCMD;

/** Pointer to a HGCM port interface. */
typedef struct PDMIHGCMPORT *PPDMIHGCMPORT;

/**
 * HGCM port interface. Normally implemented by VMMDev.
 */
typedef struct PDMIHGCMPORT
{
    /**
     * Notify the guest on a command completion.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   rc                  The return code (VBox error code).
     * @param   pCmd                A pointer that identifies the completed command.
     *
     * @returns VBox status code
     */
    DECLR3CALLBACKMEMBER(void, pfnCompleted,(PPDMIHGCMPORT pInterface, int32_t rc, PVBOXHGCMCMD pCmd));

} PDMIHGCMPORT;


/** Pointer to a HGCM connector interface. */
typedef struct PDMIHGCMCONNECTOR *PPDMIHGCMCONNECTOR;

/** Pointer to a HGCM function parameter. */
typedef struct VBOXHGCMSVCPARM *PVBOXHGCMSVCPARM;

/** Pointer to a HGCM service location structure. */
typedef struct HGCMSERVICELOCATION *PHGCMSERVICELOCATION;

/**
 * HGCM connector interface.
 * Pair with PDMIHGCMPORT.
 */
typedef struct PDMIHGCMCONNECTOR
{
    /**
     * Locate a service and inform it about a client connection.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pCmd                A pointer that identifies the command.
     * @param   pServiceLocation    Pointer to the service location structure.
     * @param   pu32ClientID        Where to store the client id for the connection.
     * @return  VBox status code.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnConnect,(PPDMIHGCMCONNECTOR pInterface, PVBOXHGCMCMD pCmd, PHGCMSERVICELOCATION pServiceLocation, uint32_t *pu32ClientID));

    /**
     * Disconnect from service.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pCmd                A pointer that identifies the command.
     * @param   u32ClientID         The client id returned by the pfnConnect call.
     * @return  VBox status code.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnDisconnect,(PPDMIHGCMCONNECTOR pInterface, PVBOXHGCMCMD pCmd, uint32_t u32ClientID));

    /**
     * Process a guest issued command.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pCmd                A pointer that identifies the command.
     * @param   u32ClientID         The client id returned by the pfnConnect call.
     * @param   u32Function         Function to be performed by the service.
     * @param   cParms              Number of parameters in the array pointed to by paParams.
     * @param   paParms             Pointer to an array of parameters.
     * @return  VBox status code.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnCall,(PPDMIHGCMCONNECTOR pInterface, PVBOXHGCMCMD pCmd, uint32_t u32ClientID, uint32_t u32Function,
                                       uint32_t cParms, PVBOXHGCMSVCPARM paParms));

} PDMIHGCMCONNECTOR;

#endif

/** @} */


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
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle it's expected
 *                      to be used primarily in this function.
 */
typedef DECLCALLBACK(int)   FNPDMDRVCONSTRUCT(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle);
/** Pointer to a FNPDMDRVCONSTRUCT() function. */
typedef FNPDMDRVCONSTRUCT *PFNPDMDRVCONSTRUCT;

/**
 * Destruct a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
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
#define PDM_DRVREG_CLASS_MOUSE          BIT(0)
/** Keyboard input driver. */
#define PDM_DRVREG_CLASS_KEYBOARD       BIT(1)
/** Display driver. */
#define PDM_DRVREG_CLASS_DISPLAY        BIT(2)
/** Network transport driver. */
#define PDM_DRVREG_CLASS_NETWORK        BIT(3)
/** Block driver. */
#define PDM_DRVREG_CLASS_BLOCK          BIT(4)
/** Media driver. */
#define PDM_DRVREG_CLASS_MEDIA          BIT(5)
/** Mountable driver. */
#define PDM_DRVREG_CLASS_MOUNTABLE      BIT(6)
/** Audio driver. */
#define PDM_DRVREG_CLASS_AUDIO          BIT(7)
/** VMMDev driver. */
#define PDM_DRVREG_CLASS_VMMDEV         BIT(8)
/** Status driver. */
#define PDM_DRVREG_CLASS_STATUS         BIT(9)
/** ACPI driver. */
#define PDM_DRVREG_CLASS_ACPI           BIT(10)
/** USB related driver. */
#define PDM_DRVREG_CLASS_USB            BIT(11)
/** ISCSI Transport related driver. */
#define PDM_DRVREG_CLASS_ISCSITRANSPORT BIT(12)
/** @} */


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
    DECLR3CALLBACKMEMBER(int, pfnTMTimerCreate,(PPDMDRVINS pDrvIns, TMCLOCK enmClock, PFNTMTIMERDRV pfnCallback, const char *pszDesc, PPTMTIMERHC ppTimer));

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

    /** Just a safety precaution. */
    uint32_t                        u32TheEnd;
} PDMDRVHLP;
/** Pointer PDM Driver API. */
typedef PDMDRVHLP *PPDMDRVHLP;
/** Pointer const PDM Driver API. */
typedef const PDMDRVHLP *PCPDMDRVHLP;

/** Current DRVHLP version number. */
#define PDM_DRVHLP_VERSION  0x90010000



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
    HCPTRTYPE(PCPDMDRVHLP)      pDrvHlp;
    /** Pointer to driver registration structure.  */
    HCPTRTYPE(PCPDMDRVREG)      pDrvReg;
    /** Configuration handle. */
    HCPTRTYPE(PCFGMNODE)        pCfgHandle;
    /** Driver instance number. */
    RTUINT                      iInstance;
    /** Pointer to the base interface of the device/driver instance above. */
    HCPTRTYPE(PPDMIBASE)        pUpBase;
    /** Pointer to the base interface of the driver instance below. */
    HCPTRTYPE(PPDMIBASE)        pDownBase;
    /** The base interface of the driver.
     * The driver constructor initializes this. */
    PDMIBASE                    IBase;
    /* padding to make achInstanceData aligned at 16 byte boundrary. */
    uint32_t                    au32Padding[HC_ARCH_BITS == 32 ? 3 : 1];
    /** Pointer to driver instance data. */
    HCPTRTYPE(void *)           pvInstanceData;
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
 * Don't use any '%' in the error string!
 */
#define PDMDRV_SET_ERROR(pDrvIns, rc, pszError)  \
    PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, pszError)

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




/** @defgroup grp_pdm_device    Devices
 * @ingroup grp_pdm
 * @{
 */


/** @def PDMBOTHCBDECL
 * Macro for declaring a callback which is static in HC and exported in GC.
 */
#if defined(IN_GC) || defined(IN_RING0)
# define PDMBOTHCBDECL(type)    DECLEXPORT(type)
#else
# define PDMBOTHCBDECL(type)    static type
#endif


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
 * during runtime. This is not called during VM construction, the device
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
    /** Name of guest context module (no path).
     * Only evalutated if PDM_DEVREG_FLAGS_GC is set. */
    char                szGCMod[32];
    /** Name of guest context module (no path).
     * Only evalutated if PDM_DEVREG_FLAGS_GC is set. */
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
} PDMDEVREG;
/** Pointer to a PDM Device Structure. */
typedef PDMDEVREG *PPDMDEVREG;
/** Const pointer to a PDM Device Structure. */
typedef PDMDEVREG const *PCPDMDEVREG;

/** Current DEVREG version number. */
#define PDM_DEVREG_VERSION  0xc0010000

/** PDM Device Flags.
 * @{ */
/** This flag is used to indicate that the device has a GC component. */
#define PDM_DEVREG_FLAGS_GC                     0x00000001
/** This flag is used to indicate that the device has a R0 component. */
#define PDM_DEVREG_FLAGS_R0                     0x00010000

/** @def PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT
 * The bit count for the current host. */
#if HC_ARCH_BITS == 32
# define PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT     0x00000002
#elif HC_ARCH_BITS == 64
# define PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT     0x00000004
#else
# error Unsupported HC_ARCH_BITS value.
#endif
/** The host bit count mask. */
#define PDM_DEVREG_FLAGS_HOST_BITS_MASK         0x00000006

/** The device support only 32-bit guests. */
#define PDM_DEVREG_FLAGS_GUEST_BITS_32          0x00000008
/** The device support only 64-bit guests. */
#define PDM_DEVREG_FLAGS_GUEST_BITS_64          0x00000010
/** The device support both 32-bit & 64-bit guests. */
#define PDM_DEVREG_FLAGS_GUEST_BITS_32_64       0x00000018
/** @def PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT
 * The guest bit count for the current compilation. */
#if GC_ARCH_BITS == 32
# define PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT    PDM_DEVREG_FLAGS_GUEST_BITS_32
#elif GC_ARCH_BITS == 64
# define PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT    PDM_DEVREG_FLAGS_GUEST_BITS_64
#else
# error Unsupported GC_ARCH_BITS value.
#endif
/** The guest bit count mask. */
#define PDM_DEVREG_FLAGS_GUEST_BITS_MASK        0x00000018

/** Indicates that the devices support PAE36 on a 32-bit guest. */
#define PDM_DEVREG_FLAGS_PAE36                  0x00000020
/** @} */


/** PDM Device Classes.
 * The order is important, lower bit earlier instantiation.
 * @{ */
/** Architecture device. */
#define PDM_DEVREG_CLASS_ARCH           BIT(0)
/** Architecture BIOS device. */
#define PDM_DEVREG_CLASS_ARCH_BIOS      BIT(1)
/** PCI bus brigde. */
#define PDM_DEVREG_CLASS_BUS_PCI        BIT(2)
/** ISA bus brigde. */
#define PDM_DEVREG_CLASS_BUS_ISA        BIT(3)
/** Input device (mouse, keyboard, joystick,..). */
#define PDM_DEVREG_CLASS_INPUT          BIT(4)
/** Interrupt controller (PIC). */
#define PDM_DEVREG_CLASS_PIC            BIT(5)
/** Interval controoler (PIT). */
#define PDM_DEVREG_CLASS_PIT            BIT(6)
/** RTC/CMOS. */
#define PDM_DEVREG_CLASS_RTC            BIT(7)
/** DMA controller. */
#define PDM_DEVREG_CLASS_DMA            BIT(8)
/** VMM Device. */
#define PDM_DEVREG_CLASS_VMM_DEV        BIT(9)
/** Graphics device, like VGA. */
#define PDM_DEVREG_CLASS_GRAPHICS       BIT(10)
/** Storage controller device. */
#define PDM_DEVREG_CLASS_STORAGE        BIT(11)
/** Network interface controller. */
#define PDM_DEVREG_CLASS_NETWORK        BIT(12)
/** Audio. */
#define PDM_DEVREG_CLASS_AUDIO          BIT(13)
/** USB bus? */
#define PDM_DEVREG_CLASS_BUS_USB        BIT(14) /* ??? */
/** ACPI. */
#define PDM_DEVREG_CLASS_ACPI           BIT(15)
/** Serial Porst */
#define PDM_DEVREG_CLASS_SERIAL_PORT    BIT(16)
/** @} */


/**
 * PCI Bus registaration structure.
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
    DECLR3CALLBACKMEMBER(int, pfnRegisterHC,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, const char *pszName, int iDev));

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
    DECLR3CALLBACKMEMBER(int, pfnIORegionRegisterHC,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iRegion, uint32_t cbRegion, PCIADDRESSSPACE enmType, PFNPCIIOREGIONMAP pfnCallback));

    /**
     * Set the IRQ for a PCI device.
     *
     * @param   pDevIns         Device instance of the PCI Bus.
     * @param   pPciDev         The PCI device structure.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level.
     */
    DECLR3CALLBACKMEMBER(void, pfnSetIrqHC,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel));

    /**
     * Saves a state of the PCI device.
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance of the PCI Bus.
     * @param   pPciDev         Pointer to PCI device.
     * @param   pSSMHandle      The handle to save the state to.
     */
    DECLR3CALLBACKMEMBER(int, pfnSaveExecHC,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PSSMHANDLE pSSMHandle));

    /**
     * Loads a saved PCI device state.
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance of the PCI Bus.
     * @param   pPciDev         Pointer to PCI device.
     * @param   pSSMHandle      The handle to the saved state.
     */
    DECLR3CALLBACKMEMBER(int, pfnLoadExecHC,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PSSMHANDLE pSSMHandle));

    /**
     * Called to perform the job of the bios.
     * This is only called for the first PCI Bus - it is expected to
     * service all the PCI buses.
     *
     * @returns VBox status.
     * @param   pDevIns     Device instance of the first bus.
     */
    DECLR3CALLBACKMEMBER(int, pfnFakePCIBIOSHC,(PPDMDEVINS pDevIns));

    /** The name of the SetIrq GC entry point. */
    const char         *pszSetIrqGC;

    /** The name of the SetIrq R0 entry point. */
    const char         *pszSetIrqR0;

} PDMPCIBUSREG;
/** Pointer to a PCI bus registration structure. */
typedef PDMPCIBUSREG *PPDMPCIBUSREG;

/** Current PDMPCIBUSREG version number. */
#define PDM_PCIBUSREG_VERSION   0xd0010000

/**
 * PCI Bus GC helpers.
 */
typedef struct PDMPCIHLPGC
{
    /** Structure version. PDM_PCIHLPGC_VERSION defines the current version. */
    uint32_t                    u32Version;

    /**
     * Set an ISA IRQ.
     *
     * @param   pDevIns         PCI device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level.
     * @thread  EMT only.
     */
    DECLGCCALLBACKMEMBER(void,  pfnIsaSetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Set an I/O-APIC IRQ.
     *
     * @param   pDevIns         PCI device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level.
     * @thread  EMT only.
     */
    DECLGCCALLBACKMEMBER(void,  pfnIoApicSetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

#ifdef VBOX_WITH_PDM_LOCK
    /**
     * Acquires the PDM lock.
     *
     * @returns VINF_SUCCESS on success.
     * @returns rc if we failed to acquire the lock.
     * @param   pDevIns         The PCI device instance.
     * @param   rc              What to return if we fail to acquire the lock.
     */
    DECLGCCALLBACKMEMBER(int,   pfnLock,(PPDMDEVINS pDevIns, int rc));

    /**
     * Releases the PDM lock.
     *
     * @param   pDevIns         The PCI device instance.
     */
    DECLGCCALLBACKMEMBER(void,  pfnUnlock,(PPDMDEVINS pDevIns));
#endif
    /** Just a safety precaution. */
    uint32_t                    u32TheEnd;
} PDMPCIHLPGC;
/** Pointer to PCI helpers. */
typedef GCPTRTYPE(PDMPCIHLPGC *) PPDMPCIHLPGC;
/** Pointer to const PCI helpers. */
typedef GCPTRTYPE(const PDMPCIHLPGC *) PCPDMPCIHLPGC;

/** Current PDMPCIHLPR3 version number. */
#define PDM_PCIHLPGC_VERSION  0xe1010000


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
     * @param   iLevel          IRQ level.
     * @thread  EMT only.
     */
    DECLR0CALLBACKMEMBER(void,  pfnIsaSetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Set an I/O-APIC IRQ.
     *
     * @param   pDevIns         PCI device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level.
     * @thread  EMT only.
     */
    DECLR0CALLBACKMEMBER(void,  pfnIoApicSetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

#ifdef VBOX_WITH_PDM_LOCK
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
#endif

    /** Just a safety precaution. */
    uint32_t                    u32TheEnd;
} PDMPCIHLPR0;
/** Pointer to PCI helpers. */
typedef HCPTRTYPE(PDMPCIHLPR0 *) PPDMPCIHLPR0;
/** Pointer to const PCI helpers. */
typedef HCPTRTYPE(const PDMPCIHLPR0 *) PCPDMPCIHLPR0;

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
     * @param   iLevel          IRQ level.
     * @thread  EMT only.
     */
    DECLR3CALLBACKMEMBER(void,  pfnIsaSetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Set an I/O-APIC IRQ.
     *
     * @param   pDevIns         The PCI device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level.
     * @thread  EMT only.
     */
    DECLR3CALLBACKMEMBER(void,  pfnIoApicSetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

#ifdef VBOX_WITH_PDM_LOCK
    /**
     * Acquires the PDM lock.
     *
     * @returns VINF_SUCCESS on success.
     * @returns Fatal error on failure.
     * @param   pDevIns         The PCI device instance.
     * @param   rc              Dummy for making the interface identical to the GC and R0 versions.
     */
    DECLR3CALLBACKMEMBER(int,   pfnLock,(PPDMDEVINS pDevIns, int rc));

    /**
     * Releases the PDM lock.
     *
     * @param   pDevIns         The PCI device instance.
     */
    DECLR3CALLBACKMEMBER(void,  pfnUnlock,(PPDMDEVINS pDevIns));
#endif

    /**
     * Gets the address of the GC PCI Bus helpers.
     *
     * This should be called at both construction and relocation time
     * to obtain the correct address of the GC helpers.
     *
     * @returns GC pointer to the PCI Bus helpers.
     * @param   pDevIns         Device instance of the PCI Bus.
     * @thread  EMT only.
     */
    DECLR3CALLBACKMEMBER(PCPDMPCIHLPGC, pfnGetGCHelpers,(PPDMDEVINS pDevIns));

    /**
     * Gets the address of the R0 PCI Bus helpers.
     *
     * This should be called at both construction and relocation time
     * to obtain the correct address of the GC helpers.
     *
     * @returns R0 pointer to the PCI Bus helpers.
     * @param   pDevIns         Device instance of the PCI Bus.
     * @thread  EMT only.
     */
    DECLR3CALLBACKMEMBER(PCPDMPCIHLPR0, pfnGetR0Helpers,(PPDMDEVINS pDevIns));

    /** Just a safety precaution. */
    uint32_t                    u32TheEnd;
} PDMPCIHLPR3;
/** Pointer to PCI helpers. */
typedef HCPTRTYPE(PDMPCIHLPR3 *) PPDMPCIHLPR3;
/** Pointer to const PCI helpers. */
typedef HCPTRTYPE(const PDMPCIHLPR3 *) PCPDMPCIHLPR3;

/** Current PDMPCIHLPR3 version number. */
#define PDM_PCIHLPR3_VERSION  0xf1010000


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
     * @param   iLevel          IRQ level.
     */
    DECLR3CALLBACKMEMBER(void, pfnSetIrqHC,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Get a pending interrupt.
     *
     * @returns Pending interrupt number.
     * @param   pDevIns         Device instance of the PIC.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetInterruptHC,(PPDMDEVINS pDevIns));

    /** The name of the GC SetIrq entry point. */
    const char         *pszSetIrqGC;
    /** The name of the GC GetInterrupt entry point. */
    const char         *pszGetInterruptGC;

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
 * PIC GC helpers.
 */
typedef struct PDMPICHLPGC
{
    /** Structure version. PDM_PICHLPGC_VERSION defines the current version. */
    uint32_t                u32Version;

    /**
     * Set the interrupt force action flag.
     *
     * @param   pDevIns         Device instance of the PIC.
     */
    DECLGCCALLBACKMEMBER(void, pfnSetInterruptFF,(PPDMDEVINS pDevIns));

    /**
     * Clear the interrupt force action flag.
     *
     * @param   pDevIns         Device instance of the PIC.
     */
    DECLGCCALLBACKMEMBER(void, pfnClearInterruptFF,(PPDMDEVINS pDevIns));

#ifdef VBOX_WITH_PDM_LOCK
    /**
     * Acquires the PDM lock.
     *
     * @returns VINF_SUCCESS on success.
     * @returns rc if we failed to acquire the lock.
     * @param   pDevIns         The PIC device instance.
     * @param   rc              What to return if we fail to acquire the lock.
     */
    DECLGCCALLBACKMEMBER(int,   pfnLock,(PPDMDEVINS pDevIns, int rc));

    /**
     * Releases the PDM lock.
     *
     * @param   pDevIns         The PIC device instance.
     */
    DECLGCCALLBACKMEMBER(void,  pfnUnlock,(PPDMDEVINS pDevIns));
#endif
    /** Just a safety precaution. */
    uint32_t                u32TheEnd;
} PDMPICHLPGC;

/** Pointer to PIC GC helpers. */
typedef GCPTRTYPE(PDMPICHLPGC *) PPDMPICHLPGC;
/** Pointer to const PIC GC helpers. */
typedef GCPTRTYPE(const PDMPICHLPGC *) PCPDMPICHLPGC;

/** Current PDMPICHLPGC version number. */
#define PDM_PICHLPGC_VERSION  0xfc010000


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

#ifdef VBOX_WITH_PDM_LOCK
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
#endif

    /** Just a safety precaution. */
    uint32_t                u32TheEnd;
} PDMPICHLPR0;

/** Pointer to PIC R0 helpers. */
typedef HCPTRTYPE(PDMPICHLPR0 *) PPDMPICHLPR0;
/** Pointer to const PIC R0 helpers. */
typedef HCPTRTYPE(const PDMPICHLPR0 *) PCPDMPICHLPR0;

/** Current PDMPICHLPR0 version number. */
#define PDM_PICHLPR0_VERSION  0xfc010000

/**
 * PIC HC helpers.
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

#ifdef VBOX_WITH_PDM_LOCK
    /**
     * Acquires the PDM lock.
     *
     * @returns VINF_SUCCESS on success.
     * @returns Fatal error on failure.
     * @param   pDevIns         The PIC device instance.
     * @param   rc              Dummy for making the interface identical to the GC and R0 versions.
     */
    DECLR3CALLBACKMEMBER(int,   pfnLock,(PPDMDEVINS pDevIns, int rc));

    /**
     * Releases the PDM lock.
     *
     * @param   pDevIns         The PIC device instance.
     */
    DECLR3CALLBACKMEMBER(void,  pfnUnlock,(PPDMDEVINS pDevIns));
#endif

    /**
     * Gets the address of the GC PIC helpers.
     *
     * This should be called at both construction and relocation time
     * to obtain the correct address of the GC helpers.
     *
     * @returns GC pointer to the PIC helpers.
     * @param   pDevIns         Device instance of the PIC.
     */
    DECLR3CALLBACKMEMBER(PCPDMPICHLPGC, pfnGetGCHelpers,(PPDMDEVINS pDevIns));

    /**
     * Gets the address of the R0 PIC helpers.
     *
     * This should be called at both construction and relocation time
     * to obtain the correct address of the GC helpers.
     *
     * @returns R0 pointer to the PIC helpers.
     * @param   pDevIns         Device instance of the PIC.
     */
    DECLR3CALLBACKMEMBER(PCPDMPICHLPR0, pfnGetR0Helpers,(PPDMDEVINS pDevIns));

    /** Just a safety precaution. */
    uint32_t                u32TheEnd;
} PDMPICHLPR3;

/** Pointer to PIC HC helpers. */
typedef HCPTRTYPE(PDMPICHLPR3 *) PPDMPICHLPR3;
/** Pointer to const PIC HC helpers. */
typedef HCPTRTYPE(const PDMPICHLPR3 *) PCPDMPICHLPR3;

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
    DECLR3CALLBACKMEMBER(int, pfnGetInterruptHC,(PPDMDEVINS pDevIns));

    /**
     * Set the APIC base.
     *
     * @param   pDevIns         Device instance of the APIC.
     * @param   u64Base         The new base.
     */
    DECLR3CALLBACKMEMBER(void, pfnSetBaseHC,(PPDMDEVINS pDevIns, uint64_t u64Base));

    /**
     * Get the APIC base.
     *
     * @returns Current base.
     * @param   pDevIns         Device instance of the APIC.
     */
    DECLR3CALLBACKMEMBER(uint64_t, pfnGetBaseHC,(PPDMDEVINS pDevIns));

    /**
     * Set the TPR (task priority register?).
     *
     * @param   pDevIns         Device instance of the APIC.
     * @param   u8TPR           The new TPR.
     */
    DECLR3CALLBACKMEMBER(void, pfnSetTPRHC,(PPDMDEVINS pDevIns, uint8_t u8TPR));

    /**
     * Get the TPR (task priority register?).
     *
     * @returns The current TPR.
     * @param   pDevIns         Device instance of the APIC.
     */
    DECLR3CALLBACKMEMBER(uint8_t, pfnGetTPRHC,(PPDMDEVINS pDevIns));

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
    DECLR3CALLBACKMEMBER(void, pfnBusDeliverHC,(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode));

    /** The name of the GC GetInterrupt entry point. */
    const char         *pszGetInterruptGC;
    /** The name of the GC SetBase entry point. */
    const char         *pszSetBaseGC;
    /** The name of the GC GetBase entry point. */
    const char         *pszGetBaseGC;
    /** The name of the GC SetTPR entry point. */
    const char         *pszSetTPRGC;
    /** The name of the GC GetTPR entry point. */
    const char         *pszGetTPRGC;
    /** The name of the GC BusDeliver entry point. */
    const char         *pszBusDeliverGC;

    /** The name of the R0 GetInterrupt entry point. */
    const char         *pszGetInterruptR0;
    /** The name of the R0 SetBase entry point. */
    const char         *pszSetBaseR0;
    /** The name of the R0 GetBase entry point. */
    const char         *pszGetBaseR0;
    /** The name of the R0 SetTPR entry point. */
    const char         *pszSetTPRR0;
    /** The name of the R0 GetTPR entry point. */
    const char         *pszGetTPRR0;
    /** The name of the R0 BusDeliver entry point. */
    const char         *pszBusDeliverR0;

} PDMAPICREG;
/** Pointer to an APIC registration structure. */
typedef PDMAPICREG *PPDMAPICREG;

/** Current PDMAPICREG version number. */
#define PDM_APICREG_VERSION     0x70010000


/**
 * APIC GC helpers.
 */
typedef struct PDMAPICHLPGC
{
    /** Structure version. PDM_APICHLPGC_VERSION defines the current version. */
    uint32_t                u32Version;

    /**
     * Set the interrupt force action flag.
     *
     * @param   pDevIns         Device instance of the APIC.
     */
    DECLGCCALLBACKMEMBER(void, pfnSetInterruptFF,(PPDMDEVINS pDevIns));

    /**
     * Clear the interrupt force action flag.
     *
     * @param   pDevIns         Device instance of the APIC.
     */
    DECLGCCALLBACKMEMBER(void, pfnClearInterruptFF,(PPDMDEVINS pDevIns));

    /**
     * Sets or clears the APIC bit in the CPUID feature masks.
     *
     * @param   pDevIns         Device instance of the APIC.
     * @param   fEnabled        If true the bit is set, else cleared.
     */
    DECLGCCALLBACKMEMBER(void, pfnChangeFeature,(PPDMDEVINS pDevIns, bool fEnabled));

#ifdef VBOX_WITH_PDM_LOCK
    /**
     * Acquires the PDM lock.
     *
     * @returns VINF_SUCCESS on success.
     * @returns rc if we failed to acquire the lock.
     * @param   pDevIns         The APIC device instance.
     * @param   rc              What to return if we fail to acquire the lock.
     */
    DECLGCCALLBACKMEMBER(int,   pfnLock,(PPDMDEVINS pDevIns, int rc));

    /**
     * Releases the PDM lock.
     *
     * @param   pDevIns         The APIC device instance.
     */
    DECLGCCALLBACKMEMBER(void,  pfnUnlock,(PPDMDEVINS pDevIns));
#endif
    /** Just a safety precaution. */
    uint32_t                u32TheEnd;
} PDMAPICHLPGC;
/** Pointer to APIC GC helpers. */
typedef GCPTRTYPE(PDMAPICHLPGC *) PPDMAPICHLPGC;
/** Pointer to const APIC helpers. */
typedef GCPTRTYPE(const PDMAPICHLPGC *) PCPDMAPICHLPGC;

/** Current PDMAPICHLPGC version number. */
#define PDM_APICHLPGC_VERSION   0x60010000


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
     */
    DECLR0CALLBACKMEMBER(void, pfnSetInterruptFF,(PPDMDEVINS pDevIns));

    /**
     * Clear the interrupt force action flag.
     *
     * @param   pDevIns         Device instance of the APIC.
     */
    DECLR0CALLBACKMEMBER(void, pfnClearInterruptFF,(PPDMDEVINS pDevIns));

    /**
     * Sets or clears the APIC bit in the CPUID feature masks.
     *
     * @param   pDevIns         Device instance of the APIC.
     * @param   fEnabled        If true the bit is set, else cleared.
     */
    DECLR0CALLBACKMEMBER(void, pfnChangeFeature,(PPDMDEVINS pDevIns, bool fEnabled));

#ifdef VBOX_WITH_PDM_LOCK
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
#endif

    /** Just a safety precaution. */
    uint32_t                u32TheEnd;
} PDMAPICHLPR0;
/** Pointer to APIC GC helpers. */
typedef GCPTRTYPE(PDMAPICHLPR0 *) PPDMAPICHLPR0;
/** Pointer to const APIC helpers. */
typedef HCPTRTYPE(const PDMAPICHLPR0 *) PCPDMAPICHLPR0;

/** Current PDMAPICHLPR0 version number. */
#define PDM_APICHLPR0_VERSION   0x60010000

/**
 * APIC HC helpers.
 */
typedef struct PDMAPICHLPR3
{
    /** Structure version. PDM_APICHLPR3_VERSION defines the current version. */
    uint32_t                u32Version;

    /**
     * Set the interrupt force action flag.
     *
     * @param   pDevIns         Device instance of the APIC.
     */
    DECLR3CALLBACKMEMBER(void, pfnSetInterruptFF,(PPDMDEVINS pDevIns));

    /**
     * Clear the interrupt force action flag.
     *
     * @param   pDevIns         Device instance of the APIC.
     */
    DECLR3CALLBACKMEMBER(void, pfnClearInterruptFF,(PPDMDEVINS pDevIns));

    /**
     * Sets or clears the APIC bit in the CPUID feature masks.
     *
     * @param   pDevIns         Device instance of the APIC.
     * @param   fEnabled        If true the bit is set, else cleared.
     */
    DECLR3CALLBACKMEMBER(void, pfnChangeFeature,(PPDMDEVINS pDevIns, bool fEnabled));

#ifdef VBOX_WITH_PDM_LOCK
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
#endif

    /**
     * Gets the address of the GC APIC helpers.
     *
     * This should be called at both construction and relocation time
     * to obtain the correct address of the GC helpers.
     *
     * @returns GC pointer to the APIC helpers.
     * @param   pDevIns         Device instance of the APIC.
     */
    DECLR3CALLBACKMEMBER(PCPDMAPICHLPGC, pfnGetGCHelpers,(PPDMDEVINS pDevIns));

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
typedef HCPTRTYPE(PDMAPICHLPR3 *) PPDMAPICHLPR3;
/** Pointer to const APIC helpers. */
typedef HCPTRTYPE(const PDMAPICHLPR3 *) PCPDMAPICHLPR3;

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
     * @param   iLevel          IRQ level.
     */
    DECLR3CALLBACKMEMBER(void, pfnSetIrqHC,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /** The name of the GC SetIrq entry point. */
    const char         *pszSetIrqGC;

    /** The name of the R0 SetIrq entry point. */
    const char         *pszSetIrqR0;
} PDMIOAPICREG;
/** Pointer to an APIC registration structure. */
typedef PDMIOAPICREG *PPDMIOAPICREG;

/** Current PDMAPICREG version number. */
#define PDM_IOAPICREG_VERSION     0x50010000


/**
 * IOAPIC GC helpers.
 */
typedef struct PDMIOAPICHLPGC
{
    /** Structure version. PDM_IOAPICHLPGC_VERSION defines the current version. */
    uint32_t                u32Version;

    /**
     * Private interface between the IOAPIC and APIC.
     *
     * See comments about this hack on PDMAPICREG::pfnBusDeliverHC.
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
    DECLGCCALLBACKMEMBER(void, pfnApicBusDeliver,(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                  uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode));

#ifdef VBOX_WITH_PDM_LOCK
    /**
     * Acquires the PDM lock.
     *
     * @returns VINF_SUCCESS on success.
     * @returns rc if we failed to acquire the lock.
     * @param   pDevIns         The IOAPIC device instance.
     * @param   rc              What to return if we fail to acquire the lock.
     */
    DECLGCCALLBACKMEMBER(int,   pfnLock,(PPDMDEVINS pDevIns, int rc));

    /**
     * Releases the PDM lock.
     *
     * @param   pDevIns         The IOAPIC device instance.
     */
    DECLGCCALLBACKMEMBER(void,  pfnUnlock,(PPDMDEVINS pDevIns));
#endif

    /** Just a safety precaution. */
    uint32_t                u32TheEnd;
} PDMIOAPICHLPGC;
/** Pointer to IOAPIC GC helpers. */
typedef GCPTRTYPE(PDMAPICHLPGC *)PPDMIOAPICHLPGC;
/** Pointer to const IOAPIC helpers. */
typedef GCPTRTYPE(const PDMIOAPICHLPGC *) PCPDMIOAPICHLPGC;

/** Current PDMIOAPICHLPGC version number. */
#define PDM_IOAPICHLPGC_VERSION   0xfe010000


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
     * See comments about this hack on PDMAPICREG::pfnBusDeliverHC.
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

#ifdef VBOX_WITH_PDM_LOCK
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
#endif

    /** Just a safety precaution. */
    uint32_t                u32TheEnd;
} PDMIOAPICHLPR0;
/** Pointer to IOAPIC R0 helpers. */
typedef HCPTRTYPE(PDMAPICHLPGC *)PPDMIOAPICHLPR0;
/** Pointer to const IOAPIC helpers. */
typedef HCPTRTYPE(const PDMIOAPICHLPR0 *) PCPDMIOAPICHLPR0;

/** Current PDMIOAPICHLPR0 version number. */
#define PDM_IOAPICHLPR0_VERSION   0xfe010000

/**
 * IOAPIC HC helpers.
 */
typedef struct PDMIOAPICHLPR3
{
    /** Structure version. PDM_IOAPICHLPR3_VERSION defines the current version. */
    uint32_t                u32Version;

    /**
     * Private interface between the IOAPIC and APIC.
     *
     * See comments about this hack on PDMAPICREG::pfnBusDeliverHC.
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

#ifdef VBOX_WITH_PDM_LOCK
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
#endif

    /**
     * Gets the address of the GC IOAPIC helpers.
     *
     * This should be called at both construction and relocation time
     * to obtain the correct address of the GC helpers.
     *
     * @returns GC pointer to the IOAPIC helpers.
     * @param   pDevIns         Device instance of the IOAPIC.
     */
    DECLR3CALLBACKMEMBER(PCPDMIOAPICHLPGC, pfnGetGCHelpers,(PPDMDEVINS pDevIns));

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
/** Pointer to IOAPIC HC helpers. */
typedef HCPTRTYPE(PDMIOAPICHLPR3 *) PPDMIOAPICHLPR3;
/** Pointer to const IOAPIC helpers. */
typedef HCPTRTYPE(const PDMIOAPICHLPR3 *) PCPDMIOAPICHLPR3;

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
typedef struct PDMDEVHLP
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
    DECLR3CALLBACKMEMBER(int, pfnIOPortRegisterGC,(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts, RTGCPTR pvUser,
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
    DECLR3CALLBACKMEMBER(int, pfnIOPortRegisterR0,(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts, RTHCPTR pvUser,
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
     * @param   pszDesc             Pointer to description string. This must not be freed.
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
     * @param   pszDesc             Pointer to description string. This must not be freed.
     */
    DECLR3CALLBACKMEMBER(int, pfnMMIORegisterR0,(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTHCPTR pvUser,
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
     *                              It will be copied and doesn't have to stick around.
     * @param   pszDesc             Pointer to description string. This must not be freed.
     * @remark  There is no way to remove the rom, automatically on device cleanup or
     *          manually from the device yet. At present I doubt we need such features...
     */
    DECLR3CALLBACKMEMBER(int, pfnROMRegister,(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, const void *pvBinary, const char *pszDesc));

    /**
     * Register a save state data unit.
     *
     * @returns VBox status.
     * @param   pDevIns         Device instance.
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
    DECLR3CALLBACKMEMBER(int, pfnSSMRegister,(PPDMDEVINS pDevIns, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
                                              PFNSSMDEVSAVEPREP pfnSavePrep, PFNSSMDEVSAVEEXEC pfnSaveExec, PFNSSMDEVSAVEDONE pfnSaveDone,
                                              PFNSSMDEVLOADPREP pfnLoadPrep, PFNSSMDEVLOADEXEC pfnLoadExec, PFNSSMDEVLOADDONE pfnLoadDone));

    /**
     * Creates a timer.
     *
     * @returns VBox status.
     * @param   pDevIns         Device instance.
     * @param   enmClock        The clock to use on this timer.
     * @param   pfnCallback     Callback function.
     * @param   pszDesc         Pointer to description string which must stay around
     *                          until the timer is fully destroyed (i.e. a bit after TMTimerDestroy()).
     * @param   ppTimer         Where to store the timer on success.
     */
    DECLR3CALLBACKMEMBER(int, pfnTMTimerCreate,(PPDMDEVINS pDevIns, TMCLOCK enmClock, PFNTMTIMERDEV pfnCallback, const char *pszDesc, PPTMTIMERHC ppTimer));

    /**
     * Creates an external timer.
     *
     * @returns timer pointer
     * @param   pDevIns         Device instance.
     * @param   enmClock        The clock to use on this timer.
     * @param   pfnCallback     Callback function.
     * @param   pvUser          User pointer
     * @param   pszDesc         Pointer to description string which must stay around
     *                          until the timer is fully destroyed (i.e. a bit after TMTimerDestroy()).
     */
    DECLR3CALLBACKMEMBER(PTMTIMERHC, pfnTMTimerCreateExternal,(PPDMDEVINS pDevIns, TMCLOCK enmClock, PFNTMTIMEREXT pfnCallback, void *pvUser, const char *pszDesc));

    /**
     * Registers the device with the default PCI bus.
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance.
     * @param   pPciDev         The PCI device structure.
     *                          Any PCI enabled device must keep this in it's instance data!
     *                          Fill in the PCI data config before registration, please.
     * @remark  This is the simple interface, a Ex interface will be created if
     *          more features are needed later.
     */
    DECLR3CALLBACKMEMBER(int, pfnPCIRegister,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev));

    /**
     * Registers a I/O region (memory mapped or I/O ports) for a PCI device.
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance.
     * @param   iRegion         The region number.
     * @param   cbRegion        Size of the region.
     * @param   enmType         PCI_ADDRESS_SPACE_MEM, PCI_ADDRESS_SPACE_IO or PCI_ADDRESS_SPACE_MEM_PREFETCH.
     * @param   pfnCallback     Callback for doing the mapping.
     */
    DECLR3CALLBACKMEMBER(int, pfnPCIIORegionRegister,(PPDMDEVINS pDevIns, int iRegion, uint32_t cbRegion, PCIADDRESSSPACE enmType, PFNPCIIOREGIONMAP pfnCallback));

    /**
     * Set the IRQ for a PCI device.
     *
     * @param   pDevIns         Device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level.
     * @thread  Any thread, but will involve the emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnPCISetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Set the IRQ for a PCI device, but don't wait for EMT to process
     * the request when not called from EMT.
     *
     * @param   pDevIns         Device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level.
     * @thread  Any thread, but will involve the emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnPCISetIrqNoWait,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Set ISA IRQ for a device.
     *
     * @param   pDevIns         Device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level.
     * @thread  Any thread, but will involve the emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnISASetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Set the ISA IRQ for a device, but don't wait for EMT to process
     * the request when not called from EMT.
     *
     * @param   pDevIns         Device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level.
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

#if 0
    /* USB... */

#endif

    /**
     * Allocate memory which is associated with current VM instance
     * and automatically freed on it's destruction.
     *
     * @returns Pointer to allocated memory. The memory is *NOT* zero-ed.
     * @param   pDevIns         Device instance.
     * @param   cb              Number of bytes to allocate.
     */
    DECLR3CALLBACKMEMBER(void *, pfnMMHeapAlloc,(PPDMDEVINS pDevIns, size_t cb));

    /**
     * Allocate memory which is associated with current VM instance
     * and automatically freed on it's destruction. The memory is ZEROed.
     *
     * @returns Pointer to allocated memory. The memory is *NOT* zero-ed.
     * @param   pDevIns         Device instance.
     * @param   cb              Number of bytes to allocate.
     */
    DECLR3CALLBACKMEMBER(void *, pfnMMHeapAllocZ,(PPDMDEVINS pDevIns, size_t cb));

    /**
     * Free memory allocated with pfnMMHeapAlloc() and pfnMMHeapAllocZ().
     *
     * @param   pDevIns         Device instance.
     * @param   pv              Pointer to the memory to free.
     */
    DECLR3CALLBACKMEMBER(void, pfnMMHeapFree,(PPDMDEVINS pDevIns, void *pv));

    /**
     * Set the VM error message
     *
     * @returns rc.
     * @param   pDevIns         Device instance.
     * @param   rc              VBox status code.
     * @param   RT_SRC_POS_DECL Use RT_SRC_POS.
     * @param   pszFormat       Error message format string.
     * @param   ...             Error message arguments.
     */
    DECLR3CALLBACKMEMBER(int, pfnVMSetError,(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...));

    /**
     * Set the VM error message
     *
     * @returns rc.
     * @param   pDevIns         Device instance.
     * @param   rc              VBox status code.
     * @param   RT_SRC_POS_DECL Use RT_SRC_POS.
     * @param   pszFormat       Error message format string.
     * @param   va              Error message arguments.
     */
    DECLR3CALLBACKMEMBER(int, pfnVMSetErrorV,(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va));

    /**
     * Assert that the current thread is the emulation thread.
     *
     * @returns True if correct.
     * @returns False if wrong.
     * @param   pDevIns         Device instance.
     * @param   pszFile         Filename of the assertion location.
     * @param   iLine           The linenumber of the assertion location.
     * @param   pszFunction     Function of the assertion location.
     */
    DECLR3CALLBACKMEMBER(bool, pfnAssertEMT,(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction));

    /**
     * Assert that the current thread is NOT the emulation thread.
     *
     * @returns True if correct.
     * @returns False if wrong.
     * @param   pDevIns         Device instance.
     * @param   pszFile         Filename of the assertion location.
     * @param   iLine           The linenumber of the assertion location.
     * @param   pszFunction     Function of the assertion location.
     */
    DECLR3CALLBACKMEMBER(bool, pfnAssertOther,(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction));

    /**
     * Stops the VM and enters the debugger to look at the guest state.
     *
     * Use the PDMDeviceDBGFStop() inline function with the RT_SRC_POS macro instead of
     * invoking this function directly.
     *
     * @returns VBox status code which must be passed up to the VMM.
     * @param   pDevIns         Device instance.
     * @param   pszFile         Filename of the assertion location.
     * @param   iLine           The linenumber of the assertion location.
     * @param   pszFunction     Function of the assertion location.
     * @param   pszFormat       Message. (optional)
     * @param   args            Message parameters.
     */
    DECLR3CALLBACKMEMBER(int, pfnDBGFStopV,(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction, const char *pszFormat, va_list args));

    /**
     * Register a info handler with DBGF,
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance.
     * @param   pszName         The identifier of the info.
     * @param   pszDesc         The description of the info and any arguments the handler may take.
     * @param   pfnHandler      The handler function to be called to display the info.
     */
    DECLR3CALLBACKMEMBER(int, pfnDBGFInfoRegister,(PPDMDEVINS pDevIns, const char *pszName, const char *pszDesc, PFNDBGFHANDLERDEV pfnHandler));

    /**
     * Registers a statistics sample if statistics are enabled.
     *
     * @param   pDevIns         Device instance of the DMA.
     * @param   pvSample        Pointer to the sample.
     * @param   enmType         Sample type. This indicates what pvSample is pointing at.
     * @param   pszName         Sample name. The name is on this form "/<component>/<sample>".
     *                          Further nesting is possible.
     * @param   enmUnit         Sample unit.
     * @param   pszDesc         Sample description.
     */
    DECLR3CALLBACKMEMBER(void, pfnSTAMRegister,(PPDMDEVINS pDevIns, void *pvSample, STAMTYPE enmType, const char *pszName, STAMUNIT enmUnit, const char *pszDesc));

    /**
     * Same as pfnSTAMRegister except that the name is specified in a
     * RTStrPrintf like fashion.
     *
     * @returns VBox status.
     * @param   pDevIns         Device instance of the DMA.
     * @param   pvSample        Pointer to the sample.
     * @param   enmType         Sample type. This indicates what pvSample is pointing at.
     * @param   enmVisibility   Visibility type specifying whether unused statistics should be visible or not.
     * @param   enmUnit         Sample unit.
     * @param   pszDesc         Sample description.
     * @param   pszName         The sample name format string.
     * @param   ...             Arguments to the format string.
     */
    DECLR3CALLBACKMEMBER(void, pfnSTAMRegisterF,(PPDMDEVINS pDevIns, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility,
                                                 STAMUNIT enmUnit, const char *pszDesc, const char *pszName, ...));

    /**
     * Same as pfnSTAMRegister except that the name is specified in a
     * RTStrPrintfV like fashion.
     *
     * @returns VBox status.
     * @param   pDevIns         Device instance of the DMA.
     * @param   pvSample        Pointer to the sample.
     * @param   enmType         Sample type. This indicates what pvSample is pointing at.
     * @param   enmVisibility   Visibility type specifying whether unused statistics should be visible or not.
     * @param   enmUnit         Sample unit.
     * @param   pszDesc         Sample description.
     * @param   pszName         The sample name format string.
     * @param   args            Arguments to the format string.
     */
    DECLR3CALLBACKMEMBER(void, pfnSTAMRegisterV,(PPDMDEVINS pDevIns, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility,
                                                 STAMUNIT enmUnit, const char *pszDesc, const char *pszName, va_list args));

    /**
     * Register the RTC device.
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance.
     * @param   pRtcReg         Pointer to a RTC registration structure.
     * @param   ppRtcHlp        Where to store the pointer to the helper functions.
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
     * @param   pDevIns         Device instance.
     * @param   pCritSect       Pointer to the critical section.
     * @param   pszName         The name of the critical section (for statistics).
     */
    DECLR3CALLBACKMEMBER(int, pfnCritSectInit,(PPDMDEVINS pDevIns, PPDMCRITSECT pCritSect, const char *pszName));


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
     * @param   pDevIns         Device instance.
     */
    DECLR3CALLBACKMEMBER(PVM, pfnGetVM,(PPDMDEVINS pDevIns));

    /**
     * Register the PCI Bus.
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance.
     * @param   pPciBusReg      Pointer to PCI bus registration structure.
     * @param   ppPciHlpR3      Where to store the pointer to the PCI Bus helpers.
     */
    DECLR3CALLBACKMEMBER(int, pfnPCIBusRegister,(PPDMDEVINS pDevIns, PPDMPCIBUSREG pPciBusReg, PCPDMPCIHLPR3 *ppPciHlpR3));

    /**
     * Register the PIC device.
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance.
     * @param   pPicReg         Pointer to a PIC registration structure.
     * @param   ppPicHlpR3      Where to store the pointer to the PIC HC helpers.
     */
    DECLR3CALLBACKMEMBER(int, pfnPICRegister,(PPDMDEVINS pDevIns, PPDMPICREG pPicReg, PCPDMPICHLPR3 *ppPicHlpR3));

    /**
     * Register the APIC device.
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance.
     * @param   pApicReg        Pointer to a APIC registration structure.
     * @param   ppApicHlpR3     Where to store the pointer to the APIC helpers.
     */
    DECLR3CALLBACKMEMBER(int, pfnAPICRegister,(PPDMDEVINS pDevIns, PPDMAPICREG pApicReg, PCPDMAPICHLPR3 *ppApicHlpR3));

    /**
     * Register the I/O APIC device.
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance.
     * @param   pIoApicReg      Pointer to a I/O APIC registration structure.
     * @param   ppIoApicHlpR3   Where to store the pointer to the IOAPIC helpers.
     */
    DECLR3CALLBACKMEMBER(int, pfnIOAPICRegister,(PPDMDEVINS pDevIns, PPDMIOAPICREG pIoApicReg, PCPDMIOAPICHLPR3 *ppIoApicHlpR3));

    /**
     * Register the DMA device.
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance.
     * @param   pDmacReg        Pointer to a DMAC registration structure.
     * @param   ppDmacHlp       Where to store the pointer to the DMA helpers.
     */
    DECLR3CALLBACKMEMBER(int, pfnDMACRegister,(PPDMDEVINS pDevIns, PPDMDMACREG pDmacReg, PCPDMDMACHLP *ppDmacHlp));

    /**
     * Read physical memory.
     *
     * @param   pDevIns         Device instance.
     * @param   GCPhys          Physical address start reading from.
     * @param   pvBuf           Where to put the read bits.
     * @param   cbRead          How many bytes to read.
     * @thread  Any thread, but the call may involve the emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnPhysRead,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead));

    /**
     * Write to physical memory.
     *
     * @param   pDevIns         Device instance.
     * @param   GCPhys          Physical address to write to.
     * @param   pvBuf           What to write.
     * @param   cbWrite         How many bytes to write.
     * @thread  Any thread, but the call may involve the emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnPhysWrite,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite));

    /**
     * Read guest physical memory by virtual address.
     *
     * @param   pDevIns         Device instance.
     * @param   pvDst           Where to put the read bits.
     * @param   GCVirtSrc       Guest virtual address to start reading from.
     * @param   cb              How many bytes to read.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnPhysReadGCVirt,(PPDMDEVINS pDevIns, void *pvDst, RTGCPTR GCVirtSrc, size_t cb));

    /**
     * Write to guest physical memory by virtual address.
     *
     * @param   pDevIns         Device instance.
     * @param   GCVirtDst       Guest virtual address to write to.
     * @param   pvSrc           What to write.
     * @param   cb              How many bytes to write.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnPhysWriteGCVirt,(PPDMDEVINS pDevIns, RTGCPTR GCVirtDst, const void *pvSrc, size_t cb));

    /**
     * Reserve physical address space for ROM and MMIO ranges.
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance.
     * @param   GCPhys          Start physical address.
     * @param   cbRange         The size of the range.
     * @param   pszDesc         Description string.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnPhysReserve,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTUINT cbRange, const char *pszDesc));

    /**
     * Convert a guest physical address to a host virtual address.
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance.
     * @param   GCPhys          Start physical address.
     * @param   cbRange         The size of the range. Use 0 if you don't care about the range.
     * @param   ppvHC           Where to store the HC pointer corresponding to GCPhys.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnPhys2HCVirt,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTUINT cbRange, PRTHCPTR ppvHC));

    /**
     * Convert a guest virtual address to a host virtual address.
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance.
     * @param   GCPtr           Guest virtual address.
     * @param   pHCPtr          Where to store the HC pointer corresponding to GCPtr.
     * @thread  The emulation thread.
     * @remark  Careful with page boundraries.
     */
    DECLR3CALLBACKMEMBER(int, pfnPhysGCPtr2HCPtr,(PPDMDEVINS pDevIns, RTGCPTR GCPtr, PRTHCPTR pHCPtr));

    /**
     * Checks if the Gate A20 is enabled or not.
     *
     * @returns true if A20 is enabled.
     * @returns false if A20 is disabled.
     * @param   pDevIns         Device instance.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(bool, pfnA20IsEnabled,(PPDMDEVINS pDevIns));

    /**
     * Enables or disables the Gate A20.
     *
     * @param   pDevIns         Device instance.
     * @param   fEnable         Set this flag to enable the Gate A20; clear it to disable.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnA20Set,(PPDMDEVINS pDevIns, bool fEnable));

    /**
     * Resets the VM.
     *
     * @returns The appropriate VBox status code to pass around on reset.
     * @param   pDevIns         Device instance.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnVMReset,(PPDMDEVINS pDevIns));

    /**
     * Suspends the VM.
     *
     * @returns The appropriate VBox status code to pass around on suspend.
     * @param   pDevIns         Device instance.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnVMSuspend,(PPDMDEVINS pDevIns));

    /**
     * Power off the VM.
     *
     * @returns The appropriate VBox status code to pass around on power off.
     * @param   pDevIns         Device instance.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnVMPowerOff,(PPDMDEVINS pDevIns));

    /**
     * Acquire global VM lock
     *
     * @returns VBox status code
     * @param   pDevIns         Device instance.
     */
    DECLR3CALLBACKMEMBER(int , pfnLockVM,(PPDMDEVINS pDevIns));

    /**
     * Release global VM lock
     *
     * @returns VBox status code
     * @param   pDevIns         Device instance.
     */
    DECLR3CALLBACKMEMBER(int, pfnUnlockVM,(PPDMDEVINS pDevIns));

    /**
     * Check that the current thread owns the global VM lock.
     *
     * @returns boolean
     * @param   pDevIns         Device instance.
     * @param   pszFile         Filename of the assertion location.
     * @param   iLine           Linenumber of the assertion location.
     * @param   pszFunction     Function of the assertion location.
     */
    DECLR3CALLBACKMEMBER(bool, pfnAssertVMLock,(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction));

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
     * @param   pDevIns         Device instance.
     * @param   uChannel        Channel number.
     * @param   pvBuffer        Pointer to target buffer.
     * @param   off             DMA position.
     * @param   cbBlock         Block size.
     * @param   pcbRead         Where to store the number of bytes which was read. optional.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnDMAReadMemory,(PPDMDEVINS pDevIns, unsigned uChannel, void *pvBuffer, uint32_t off, uint32_t cbBlock, uint32_t *pcbRead));

    /**
     * Write memory.
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance.
     * @param   uChannel        Channel number.
     * @param   pvBuffer        Memory to write.
     * @param   off             DMA position.
     * @param   cbBlock         Block size.
     * @param   pcbWritten      Where to store the number of bytes which was written. optional.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnDMAWriteMemory,(PPDMDEVINS pDevIns, unsigned uChannel, const void *pvBuffer, uint32_t off, uint32_t cbBlock, uint32_t *pcbWritten));

    /**
     * Set the DREQ line.
     *
     * @returns VBox status code.
     * @param pDevIns           Device instance.
     * @param uChannel          Channel number.
     * @param uLevel            Level of the line.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnDMASetDREQ,(PPDMDEVINS pDevIns, unsigned uChannel, unsigned uLevel));

    /**
     * Get channel mode.
     *
     * @returns Channel mode. See specs.
     * @param   pDevIns         Device instance.
     * @param   uChannel        Channel number.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(uint8_t, pfnDMAGetChannelMode,(PPDMDEVINS pDevIns, unsigned uChannel));

    /**
     * Schedule DMA execution.
     *
     * @param   pDevIns         Device instance.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnDMASchedule,(PPDMDEVINS pDevIns));

    /**
     * Write CMOS value and update the checksum(s).
     *
     * @returns VBox status code.
     * @param   pDevIns     Device instance.
     * @param   iReg        The CMOS register index.
     * @param   u8Value     The CMOS register value.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnCMOSWrite,(PPDMDEVINS pDevIns, unsigned iReg, uint8_t u8Value));

    /**
     * Read CMOS value.
     *
     * @returns VBox status code.
     * @param   pDevIns     Device instance.
     * @param   iReg        The CMOS register index.
     * @param   pu8Value    Where to store the CMOS register value.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnCMOSRead,(PPDMDEVINS pDevIns, unsigned iReg, uint8_t *pu8Value));

    /** @} */

    /** Just a safety precaution. (The value is 0.) */
    uint32_t                        u32TheEnd;
} PDMDEVHLP;
#endif /* !IN_RING3 */
/** Pointer PDM Device API. */
typedef HCPTRTYPE(struct PDMDEVHLP *) PPDMDEVHLP;
/** Pointer PDM Device API. */
typedef HCPTRTYPE(const struct PDMDEVHLP *) PCPDMDEVHLP;

/** Current PDMDEVHLP version number. */
#define PDM_DEVHLP_VERSION  0xf2010000


/**
 * PDM Device API - GC Variant.
 */
typedef struct PDMDEVHLPGC
{
    /** Structure version. PDM_DEVHLPGC_VERSION defines the current version. */
    uint32_t                    u32Version;

    /**
     * Set the IRQ for a PCI device.
     *
     * @param   pDevIns         Device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level.
     * @thread  Any thread, but will involve the emulation thread.
     */
    DECLGCCALLBACKMEMBER(void, pfnPCISetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Set ISA IRQ for a device.
     *
     * @param   pDevIns         Device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level.
     * @thread  Any thread, but will involve the emulation thread.
     */
    DECLGCCALLBACKMEMBER(void, pfnISASetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Read physical memory.
     *
     * @param   pDevIns         Device instance.
     * @param   GCPhys          Physical address start reading from.
     * @param   pvBuf           Where to put the read bits.
     * @param   cbRead          How many bytes to read.
     */
    DECLGCCALLBACKMEMBER(void, pfnPhysRead,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead));

    /**
     * Write to physical memory.
     *
     * @param   pDevIns         Device instance.
     * @param   GCPhys          Physical address to write to.
     * @param   pvBuf           What to write.
     * @param   cbWrite         How many bytes to write.
     */
    DECLGCCALLBACKMEMBER(void, pfnPhysWrite,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite));

    /**
     * Checks if the Gate A20 is enabled or not.
     *
     * @returns true if A20 is enabled.
     * @returns false if A20 is disabled.
     * @param   pDevIns         Device instance.
     * @thread  The emulation thread.
     */
    DECLGCCALLBACKMEMBER(bool, pfnA20IsEnabled,(PPDMDEVINS pDevIns));

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
    DECLGCCALLBACKMEMBER(int, pfnVMSetError,(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...));

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
    DECLGCCALLBACKMEMBER(int, pfnVMSetErrorV,(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va));

    /**
     * Set parameters for pending MMIO patch operation
     *
     * @returns VBox status code.
     * @param   pDevIns         Device instance.
     * @param   GCPhys          MMIO physical address
     * @param   pCachedData     GC pointer to cached data
     */
    DECLGCCALLBACKMEMBER(int, pfnPATMSetMMIOPatchInfo,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTGCPTR pCachedData));

    /** Just a safety precaution. */
    uint32_t                        u32TheEnd;
} PDMDEVHLPGC;
/** Pointer PDM Device GC API. */
typedef GCPTRTYPE(struct PDMDEVHLPGC *) PPDMDEVHLPGC;
/** Pointer PDM Device GC API. */
typedef GCPTRTYPE(const struct PDMDEVHLPGC *) PCPDMDEVHLPGC;

/** Current PDMDEVHLP version number. */
#define PDM_DEVHLPGC_VERSION  0xfb010000


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
     * @param   iLevel          IRQ level.
     * @thread  Any thread, but will involve the emulation thread.
     */
    DECLR0CALLBACKMEMBER(void, pfnPCISetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Set ISA IRQ for a device.
     *
     * @param   pDevIns         Device instance.
     * @param   iIrq            IRQ number to set.
     * @param   iLevel          IRQ level.
     * @thread  Any thread, but will involve the emulation thread.
     */
    DECLR0CALLBACKMEMBER(void, pfnISASetIrq,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /**
     * Read physical memory.
     *
     * @param   pDevIns         Device instance.
     * @param   GCPhys          Physical address start reading from.
     * @param   pvBuf           Where to put the read bits.
     * @param   cbRead          How many bytes to read.
     */
    DECLR0CALLBACKMEMBER(void, pfnPhysRead,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead));

    /**
     * Write to physical memory.
     *
     * @param   pDevIns         Device instance.
     * @param   GCPhys          Physical address to write to.
     * @param   pvBuf           What to write.
     * @param   cbWrite         How many bytes to write.
     */
    DECLR0CALLBACKMEMBER(void, pfnPhysWrite,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite));

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
     * Set parameters for pending MMIO patch operation
     *
     * @returns rc.
     * @param   pDevIns         Device instance.
     * @param   GCPhys          MMIO physical address
     * @param   pCachedData     GC pointer to cached data
     */
    DECLR0CALLBACKMEMBER(int, pfnPATMSetMMIOPatchInfo,(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTGCPTR pCachedData));

    /** Just a safety precaution. */
    uint32_t                        u32TheEnd;
} PDMDEVHLPR0;
/** Pointer PDM Device R0 API. */
typedef HCPTRTYPE(struct PDMDEVHLPR0 *) PPDMDEVHLPR0;
/** Pointer PDM Device GC API. */
typedef HCPTRTYPE(const struct PDMDEVHLPR0 *) PCPDMDEVHLPR0;

/** Current PDMDEVHLP version number. */
#define PDM_DEVHLPR0_VERSION  0xfb010000



/**
 * PDM Device Instance.
 */
typedef struct PDMDEVINS
{
    /** Structure version. PDM_DEVINS_VERSION defines the current version. */
    uint32_t                    u32Version;

    /** Internal data. */
    union
    {
#ifdef PDMDEVINSINT_DECLARED
        PDMDEVINSINT            s;
#endif
        uint8_t                 padding[HC_ARCH_BITS == 32 ? 48 : 96];
    } Internal;

    /** Pointer the HC PDM Device API. */
    HCPTRTYPE(PCPDMDEVHLP)      pDevHlp;
    /** Pointer the GC PDM Device API. */
    GCPTRTYPE(PCPDMDEVHLPGC)    pDevHlpGC;
    /** Pointer the R0 PDM Device API. */
    HCPTRTYPE(PCPDMDEVHLPR0)    pDevHlpR0;
    /** Pointer to device registration structure.  */
    HCPTRTYPE(PCPDMDEVREG)      pDevReg;
    /** Configuration handle. */
    HCPTRTYPE(PCFGMNODE)        pCfgHandle;
    /** Device instance number. */
    RTUINT                      iInstance;
    /** Pointer to device instance data. */
    HCPTRTYPE(void *)           pvInstanceDataHC;
    /** Pointer to device instance data. */
    GCPTRTYPE(void *)           pvInstanceDataGC;
    /** The base interface of the device.
     * The device constructor initializes this if it has any
     * device level interfaces to export. To obtain this interface
     * call PDMR3QueryDevice(). */
    PDMIBASE                    IBase;
#if HC_ARCH_BITS == 32
    /* padding to make achInstanceData aligned at 16 byte boundrary. */
    uint32_t                    au32Padding[HC_ARCH_BITS == 32 ? 2 : 0];
#endif 
    /** Device instance data. The size of this area is defined
     * in the PDMDEVREG::cbInstanceData field. */
    char                        achInstanceData[4];
} PDMDEVINS;

/** Current DEVREG version number. */
#define PDM_DEVINS_VERSION  0xf3010000

/** Converts a pointer to the PDMDEVINS::IBase to a pointer to PDMDEVINS. */
#define PDMIBASE_2_PDMDEV(pInterface) ( (PPDMDEVINS)((char *)(pInterface) - RT_OFFSETOF(PDMDEVINS, IBase)) )


/** @def PDMDEV_ASSERT_EMT
 * Assert that the current thread is the emulation thread.
 */
#ifdef VBOX_STRICT
# define PDMDEV_ASSERT_EMT(pDevIns)  pDevIns->pDevHlp->pfnAssertEMT(pDevIns, __FILE__, __LINE__, __FUNCTION__)
#else
# define PDMDEV_ASSERT_EMT(pDevIns)  do { } while (0)
#endif

/** @def PDMDEV_ASSERT_OTHER
 * Assert that the current thread is NOT the emulation thread.
 */
#ifdef VBOX_STRICT
# define PDMDEV_ASSERT_OTHER(pDevIns)  pDevIns->pDevHlp->pfnAssertOther(pDevIns, __FILE__, __LINE__, __FUNCTION__)
#else
# define PDMDEV_ASSERT_OTHER(pDevIns)  do { } while (0)
#endif

/** @def PDMDEV_ASSERT_VMLOCK_OWNER
 * Assert that the current thread is owner of the VM lock.
 */
#ifdef VBOX_STRICT
# define PDMDEV_ASSERT_VMLOCK_OWNER(pDevIns)  pDevIns->pDevHlp->pfnAssertVMLock(pDevIns, __FILE__, __LINE__, __FUNCTION__)
#else
# define PDMDEV_ASSERT_VMLOCK_OWNER(pDevIns)  do { } while (0)
#endif

/** @def PDMDEV_SET_ERROR
 * Set the VM error. See PDMDevHlpVMSetError() for printf like message formatting.
 * Don't use any '%' in the error string!
 */
#define PDMDEV_SET_ERROR(pDevIns, rc, pszError) \
    PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS, pszError)

/** @def PDMINS2DATA
 * Converts a PDM Device or Driver instance pointer to a pointer to the instance data.
 */
#define PDMINS2DATA(pIns, type)     ( (type)(void *)&(pIns)->achInstanceData[0] )

/** @def PDMINS2DATA_GCPTR
 * Converts a PDM Device or Driver instance pointer to a GC pointer to the instance data.
 */
#define PDMINS2DATA_GCPTR(pIns)     ( (pIns)->pvInstanceDataGC )

/** @def PDMINS2DATA_HCPTR
 * Converts a PDM Device or Driver instance pointer to a HC pointer to the instance data.
 */
#define PDMINS2DATA_HCPTR(pIns)     ( (pIns)->pvInstanceDataHC )

/** @def PDMDEVINS_2_GCPTR
 * Converts a PDM Device instance pointer a GC PDM Device instance pointer.
 */
#define PDMDEVINS_2_GCPTR(pDevIns)  ( (GCPTRTYPE(PPDMDEVINS))((RTGCUINTPTR)(pDevIns)->pvInstanceDataGC - RT_OFFSETOF(PDMDEVINS, achInstanceData)) )

/** @def PDMDEVINS_2_HCPTR
 * Converts a PDM Device instance pointer a HC PDM Device instance pointer.
 */
#define PDMDEVINS_2_HCPTR(pDevIns)  ( (HCPTRTYPE(PPDMDEVINS))((RTHCUINTPTR)(pDevIns)->pvInstanceDataHC - RT_OFFSETOF(PDMDEVINS, achInstanceData)) )


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
    rc = pDevIns->pDevHlp->pfnDBGFStopV(pDevIns, RT_SRC_POS_ARGS, pszFormat, args);
    va_end(args);
    return rc;
# else
    return VINF_EM_DBG_STOP;
# endif
#else
    return VINF_SUCCESS;
#endif
}


#ifdef IN_RING3
/**
 * @copydoc PDMDEVHLP::pfnIOPortRegister
 */
DECLINLINE(int) PDMDevHlpIOPortRegister(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts, RTHCPTR pvUser,
                                        PFNIOMIOPORTOUT pfnOut, PFNIOMIOPORTIN pfnIn,
                                        PFNIOMIOPORTOUTSTRING pfnOutStr, PFNIOMIOPORTINSTRING pfnInStr, const char *pszDesc)
{
    return pDevIns->pDevHlp->pfnIOPortRegister(pDevIns, Port, cPorts, pvUser, pfnOut, pfnIn, pfnOutStr, pfnInStr, pszDesc);
}

/**
 * @copydoc PDMDEVHLP::pfnIOPortRegisterGC
 */
DECLINLINE(int) PDMDevHlpIOPortRegisterGC(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts, RTGCPTR pvUser,
                                          const char *pszOut, const char *pszIn, const char *pszOutStr,
                                          const char *pszInStr, const char *pszDesc)
{
    return pDevIns->pDevHlp->pfnIOPortRegisterGC(pDevIns, Port, cPorts, pvUser, pszOut, pszIn, pszOutStr, pszInStr, pszDesc);
}

/**
 * @copydoc PDMDEVHLP::pfnIOPortRegisterR0
 */
DECLINLINE(int) PDMDevHlpIOPortRegisterR0(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts, RTHCPTR pvUser,
                                          const char *pszOut, const char *pszIn, const char *pszOutStr,
                                          const char *pszInStr, const char *pszDesc)
{
    return pDevIns->pDevHlp->pfnIOPortRegisterR0(pDevIns, Port, cPorts, pvUser, pszOut, pszIn, pszOutStr, pszInStr, pszDesc);
}

/**
 * @copydoc PDMDEVHLP::pfnMMIORegister
 */
DECLINLINE(int) PDMDevHlpMMIORegister(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTHCPTR pvUser,
                                      PFNIOMMMIOWRITE pfnWrite, PFNIOMMMIOREAD pfnRead, PFNIOMMMIOFILL pfnFill,
                                      const char *pszDesc)
{
    return pDevIns->pDevHlp->pfnMMIORegister(pDevIns, GCPhysStart, cbRange, pvUser, pfnWrite, pfnRead, pfnFill, pszDesc);
}

/**
 * @copydoc PDMDEVHLP::pfnMMIORegisterGC
 */
DECLINLINE(int) PDMDevHlpMMIORegisterGC(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTGCPTR pvUser,
                                        const char *pszWrite, const char *pszRead, const char *pszFill, const char *pszDesc)
{
    return pDevIns->pDevHlp->pfnMMIORegisterGC(pDevIns, GCPhysStart, cbRange, pvUser, pszWrite, pszRead, pszFill, pszDesc);
}

/**
 * @copydoc PDMDEVHLP::pfnMMIORegisterR0
 */
DECLINLINE(int) PDMDevHlpMMIORegisterR0(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTHCPTR pvUser,
                                        const char *pszWrite, const char *pszRead, const char *pszFill, const char *pszDesc)
{
    return pDevIns->pDevHlp->pfnMMIORegisterR0(pDevIns, GCPhysStart, cbRange, pvUser, pszWrite, pszRead, pszFill, pszDesc);
}

/**
 * @copydoc PDMDEVHLP::pfnROMRegister
 */
DECLINLINE(int) PDMDevHlpROMRegister(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, const void *pvBinary, const char *pszDesc)
{
    return pDevIns->pDevHlp->pfnROMRegister(pDevIns, GCPhysStart, cbRange, pvBinary, pszDesc);
}

/**
 * @copydoc PDMDEVHLP::pfnSSMRegister
 */
DECLINLINE(int) PDMDevHlpSSMRegister(PPDMDEVINS pDevIns, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
                                     PFNSSMDEVSAVEPREP pfnSavePrep, PFNSSMDEVSAVEEXEC pfnSaveExec, PFNSSMDEVSAVEDONE pfnSaveDone,
                                     PFNSSMDEVLOADPREP pfnLoadPrep, PFNSSMDEVLOADEXEC pfnLoadExec, PFNSSMDEVLOADDONE pfnLoadDone)
{
    return pDevIns->pDevHlp->pfnSSMRegister(pDevIns, pszName, u32Instance, u32Version, cbGuess,
                                            pfnSavePrep, pfnSaveExec, pfnSaveDone,
                                            pfnLoadPrep, pfnLoadExec, pfnLoadDone);
}

/**
 * @copydoc PDMDEVHLP::pfnTMTimerCreate
 */
DECLINLINE(int) PDMDevHlpTMTimerCreate(PPDMDEVINS pDevIns, TMCLOCK enmClock, PFNTMTIMERDEV pfnCallback, const char *pszDesc, PPTMTIMERHC ppTimer)
{
    return pDevIns->pDevHlp->pfnTMTimerCreate(pDevIns, enmClock, pfnCallback, pszDesc, ppTimer);
}

/**
 * @copydoc PDMDEVHLP::pfnPCIRegister
 */
DECLINLINE(int) PDMDevHlpPCIRegister(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev)
{
    return pDevIns->pDevHlp->pfnPCIRegister(pDevIns, pPciDev);
}

/**
 * @copydoc PDMDEVHLP::pfnPCIIORegionRegister
 */
DECLINLINE(int) PDMDevHlpPCIIORegionRegister(PPDMDEVINS pDevIns, int iRegion, uint32_t cbRegion, PCIADDRESSSPACE enmType, PFNPCIIOREGIONMAP pfnCallback)
{
    return pDevIns->pDevHlp->pfnPCIIORegionRegister(pDevIns, iRegion, cbRegion, enmType, pfnCallback);
}

/**
 * @copydoc PDMDEVHLP::pfnDriverAttach
 */
DECLINLINE(int) PDMDevHlpDriverAttach(PPDMDEVINS pDevIns, RTUINT iLun, PPDMIBASE pBaseInterface, PPDMIBASE *ppBaseInterface, const char *pszDesc)
{
    return pDevIns->pDevHlp->pfnDriverAttach(pDevIns, iLun, pBaseInterface, ppBaseInterface, pszDesc);
}

/**
 * @copydoc PDMDEVHLP::pfnMMHeapAlloc
 */
DECLINLINE(void *) PDMDevHlpMMHeapAlloc(PPDMDEVINS pDevIns, size_t cb)
{
    return pDevIns->pDevHlp->pfnMMHeapAlloc(pDevIns, cb);
}

/**
 * @copydoc PDMDEVHLP::pfnMMHeapAllocZ
 */
DECLINLINE(void *) PDMDevHlpMMHeapAllocZ(PPDMDEVINS pDevIns, size_t cb)
{
    return pDevIns->pDevHlp->pfnMMHeapAllocZ(pDevIns, cb);
}

/**
 * @copydoc PDMDEVHLP::pfnMMHeapFree
 */
DECLINLINE(void) PDMDevHlpMMHeapFree(PPDMDEVINS pDevIns, void *pv)
{
    pDevIns->pDevHlp->pfnMMHeapFree(pDevIns, pv);
}

/**
 * @copydoc PDMDEVHLP::pfnDBGFInfoRegister
 */
DECLINLINE(int) PDMDevHlpDBGFInfoRegister(PPDMDEVINS pDevIns, const char *pszName, const char *pszDesc, PFNDBGFHANDLERDEV pfnHandler)
{
    return pDevIns->pDevHlp->pfnDBGFInfoRegister(pDevIns, pszName, pszDesc, pfnHandler);
}

/**
 * @copydoc PDMDEVHLP::pfnSTAMRegister
 */
DECLINLINE(void) PDMDevHlpSTAMRegister(PPDMDEVINS pDevIns, void *pvSample, STAMTYPE enmType, const char *pszName, STAMUNIT enmUnit, const char *pszDesc)
{
    pDevIns->pDevHlp->pfnSTAMRegister(pDevIns, pvSample, enmType, pszName, enmUnit, pszDesc);
}

/**
 * @copydoc PDMDEVHLP::pfnSTAMRegisterF
 */
DECLINLINE(void) PDMDevHlpSTAMRegisterF(PPDMDEVINS pDevIns, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility, STAMUNIT enmUnit,
                                        const char *pszDesc, const char *pszName, ...)
{
    va_list va;
    va_start(va, pszName);
    pDevIns->pDevHlp->pfnSTAMRegisterV(pDevIns, pvSample, enmType, enmVisibility, enmUnit, pszDesc, pszName, va);
    va_end(va);
}

/**
 * @copydoc PDMDEVHLP::pfnPDMQueueCreate
 */
DECLINLINE(int) PDMDevHlpPDMQueueCreate(PPDMDEVINS pDevIns, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval,
                                        PFNPDMQUEUEDEV pfnCallback, bool fGCEnabled, PPDMQUEUE *ppQueue)
{
    return pDevIns->pDevHlp->pfnPDMQueueCreate(pDevIns, cbItem, cItems, cMilliesInterval, pfnCallback, fGCEnabled, ppQueue);
}

/**
 * @copydoc PDMDEVHLP::pfnCritSectInit
 */
DECLINLINE(int) PDMDevHlpCritSectInit(PPDMDEVINS pDevIns, PPDMCRITSECT pCritSect, const char *pszName)
{
    return pDevIns->pDevHlp->pfnCritSectInit(pDevIns, pCritSect, pszName);
}

/**
 * @copydoc PDMDEVHLP::pfnGetVM
 */
DECLINLINE(PVM) PDMDevHlpGetVM(PPDMDEVINS pDevIns)
{
    return pDevIns->pDevHlp->pfnGetVM(pDevIns);
}

/**
 * @copydoc PDMDEVHLP::pfnPhysReadGCVirt
 */
DECLINLINE(int) PDMDevHlpPhysReadGCVirt(PPDMDEVINS pDevIns, void *pvDst, RTGCPTR GCVirtSrc, size_t cb)
{
    return pDevIns->pDevHlp->pfnPhysReadGCVirt(pDevIns, pvDst, GCVirtSrc, cb);
}

/**
 * @copydoc PDMDEVHLP::pfnPhysWriteGCVirt
 */
DECLINLINE(int) PDMDevHlpPhysWriteGCVirt(PPDMDEVINS pDevIns, RTGCPTR GCVirtDst, const void *pvSrc, size_t cb)
{
    return pDevIns->pDevHlp->pfnPhysWriteGCVirt(pDevIns, GCVirtDst, pvSrc, cb);
}

/**
 * @copydoc PDMDEVHLP::pfnPhysReserve
 */
DECLINLINE(int) PDMDevHlpPhysReserve(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTUINT cbRange, const char *pszDesc)
{
    return pDevIns->pDevHlp->pfnPhysReserve(pDevIns, GCPhys, cbRange, pszDesc);
}

/**
 * @copydoc PDMDEVHLP::pfnPhys2HCVirt
 */
DECLINLINE(int) PDMDevHlpPhys2HCVirt(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTUINT cbRange, PRTHCPTR ppvHC)
{
    return pDevIns->pDevHlp->pfnPhys2HCVirt(pDevIns, GCPhys, cbRange, ppvHC);
}

/**
 * @copydoc PDMDEVHLP::pfnPhysGCPtr2HCPtr
 */
DECLINLINE(int) PDMDevHlpPhysGCPtr2HCPtr(PPDMDEVINS pDevIns, RTGCPTR GCPtr, PRTHCPTR pHCPtr)
{
    return pDevIns->pDevHlp->pfnPhysGCPtr2HCPtr(pDevIns, GCPtr, pHCPtr);
}

/**
 * @copydoc PDMDEVHLP::pfnA20Set
 */
DECLINLINE(void) PDMDevHlpA20Set(PPDMDEVINS pDevIns, bool fEnable)
{
    pDevIns->pDevHlp->pfnA20Set(pDevIns, fEnable);
}

/**
 * @copydoc PDMDEVHLP::pfnVMReset
 */
DECLINLINE(int) PDMDevHlpVMReset(PPDMDEVINS pDevIns)
{
    return pDevIns->pDevHlp->pfnVMReset(pDevIns);
}

/**
 * @copydoc PDMDEVHLP::pfnVMSuspend
 */
DECLINLINE(int) PDMDevHlpVMSuspend(PPDMDEVINS pDevIns)
{
    return pDevIns->pDevHlp->pfnVMSuspend(pDevIns);
}

/**
 * @copydoc PDMDEVHLP::pfnVMPowerOff
 */
DECLINLINE(int) PDMDevHlpVMPowerOff(PPDMDEVINS pDevIns)
{
    return pDevIns->pDevHlp->pfnVMPowerOff(pDevIns);
}

/**
 * @copydoc PDMDEVHLP::pfnDMARegister
 */
DECLINLINE(int) PDMDevHlpDMARegister(PPDMDEVINS pDevIns, unsigned uChannel, PFNDMATRANSFERHANDLER pfnTransferHandler, void *pvUser)
{
    return pDevIns->pDevHlp->pfnDMARegister(pDevIns, uChannel, pfnTransferHandler, pvUser);
}

/**
 * @copydoc PDMDEVHLP::pfnDMAReadMemory
 */
DECLINLINE(int) PDMDevHlpDMAReadMemory(PPDMDEVINS pDevIns, unsigned uChannel, void *pvBuffer, uint32_t off, uint32_t cbBlock, uint32_t *pcbRead)
{
    return pDevIns->pDevHlp->pfnDMAReadMemory(pDevIns, uChannel, pvBuffer, off, cbBlock, pcbRead);
}

/**
 * @copydoc PDMDEVHLP::pfnDMAWriteMemory
 */
DECLINLINE(int) PDMDevHlpDMAWriteMemory(PPDMDEVINS pDevIns, unsigned uChannel, const void *pvBuffer, uint32_t off, uint32_t cbBlock, uint32_t *pcbWritten)
{
    return pDevIns->pDevHlp->pfnDMAWriteMemory(pDevIns, uChannel, pvBuffer, off, cbBlock, pcbWritten);
}

/**
 * @copydoc PDMDEVHLP::pfnDMASetDREQ
 */
DECLINLINE(int) PDMDevHlpDMASetDREQ(PPDMDEVINS pDevIns, unsigned uChannel, unsigned uLevel)
{
    return pDevIns->pDevHlp->pfnDMASetDREQ(pDevIns, uChannel, uLevel);
}

/**
 * @copydoc PDMDEVHLP::pfnDMAGetChannelMode
 */
DECLINLINE(uint8_t) PDMDevHlpDMAGetChannelMode(PPDMDEVINS pDevIns, unsigned uChannel)
{
    return pDevIns->pDevHlp->pfnDMAGetChannelMode(pDevIns, uChannel);
}

/**
 * @copydoc PDMDEVHLP::pfnDMASchedule
 */
DECLINLINE(void) PDMDevHlpDMASchedule(PPDMDEVINS pDevIns)
{
    pDevIns->pDevHlp->pfnDMASchedule(pDevIns);
}

/**
 * @copydoc PDMDEVHLP::pfnCMOSWrite
 */
DECLINLINE(int) PDMDevHlpCMOSWrite(PPDMDEVINS pDevIns, unsigned iReg, uint8_t u8Value)
{
    return pDevIns->pDevHlp->pfnCMOSWrite(pDevIns, iReg, u8Value);
}

/**
 * @copydoc PDMDEVHLP::pfnCMOSRead
 */
DECLINLINE(int) PDMDevHlpCMOSRead(PPDMDEVINS pDevIns, unsigned iReg, uint8_t *pu8Value)
{
    return pDevIns->pDevHlp->pfnCMOSRead(pDevIns, iReg, pu8Value);
}
#endif /* IN_RING3 */


/**
 * @copydoc PDMDEVHLP::pfnPCISetIrq
 */
DECLINLINE(void) PDMDevHlpPCISetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
#ifdef IN_GC
    pDevIns->pDevHlpGC->pfnPCISetIrq(pDevIns, iIrq, iLevel);
#elif defined(IN_RING0)
    pDevIns->pDevHlpR0->pfnPCISetIrq(pDevIns, iIrq, iLevel);
#else
    pDevIns->pDevHlp->pfnPCISetIrq(pDevIns, iIrq, iLevel);
#endif
}

/**
 * @copydoc PDMDEVHLP::pfnPCISetIrqNoWait
 */
DECLINLINE(void) PDMDevHlpPCISetIrqNoWait(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
#ifdef IN_GC
    pDevIns->pDevHlpGC->pfnPCISetIrq(pDevIns, iIrq, iLevel);
#elif defined(IN_RING0)
    pDevIns->pDevHlpR0->pfnPCISetIrq(pDevIns, iIrq, iLevel);
#else
    pDevIns->pDevHlp->pfnPCISetIrqNoWait(pDevIns, iIrq, iLevel);
#endif
}

/**
 * @copydoc PDMDEVHLP::pfnISASetIrq
 */
DECLINLINE(void) PDMDevHlpISASetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
#ifdef IN_GC
    pDevIns->pDevHlpGC->pfnISASetIrq(pDevIns, iIrq, iLevel);
#elif defined(IN_RING0)
    pDevIns->pDevHlpR0->pfnISASetIrq(pDevIns, iIrq, iLevel);
#else
    pDevIns->pDevHlp->pfnISASetIrq(pDevIns, iIrq, iLevel);
#endif
}

/**
 * @copydoc PDMDEVHLP::pfnISASetIrqNoWait
 */
DECLINLINE(void) PDMDevHlpISASetIrqNoWait(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
#ifdef IN_GC
    pDevIns->pDevHlpGC->pfnISASetIrq(pDevIns, iIrq, iLevel);
#elif defined(IN_RING0)
    pDevIns->pDevHlpR0->pfnISASetIrq(pDevIns, iIrq, iLevel);
#else
    pDevIns->pDevHlp->pfnISASetIrqNoWait(pDevIns, iIrq, iLevel);
#endif
}

/**
 * @copydoc PDMDEVHLP::pfnPhysRead
 */
DECLINLINE(void) PDMDevHlpPhysRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead)
{
#ifdef IN_GC
    pDevIns->pDevHlpGC->pfnPhysRead(pDevIns, GCPhys, pvBuf, cbRead);
#elif defined(IN_RING0)
    pDevIns->pDevHlpR0->pfnPhysRead(pDevIns, GCPhys, pvBuf, cbRead);
#else
    pDevIns->pDevHlp->pfnPhysRead(pDevIns, GCPhys, pvBuf, cbRead);
#endif
}

/**
 * @copydoc PDMDEVHLP::pfnPhysWrite
 */
DECLINLINE(void) PDMDevHlpPhysWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite)
{
#ifdef IN_GC
    pDevIns->pDevHlpGC->pfnPhysWrite(pDevIns, GCPhys, pvBuf, cbWrite);
#elif defined(IN_RING0)
    pDevIns->pDevHlpR0->pfnPhysWrite(pDevIns, GCPhys, pvBuf, cbWrite);
#else
    pDevIns->pDevHlp->pfnPhysWrite(pDevIns, GCPhys, pvBuf, cbWrite);
#endif
}

/**
 * @copydoc PDMDEVHLP::pfnA20IsEnabled
 */
DECLINLINE(bool) PDMDevHlpA20IsEnabled(PPDMDEVINS pDevIns)
{
#ifdef IN_GC
    return pDevIns->pDevHlpGC->pfnA20IsEnabled(pDevIns);
#elif defined(IN_RING0)
    return pDevIns->pDevHlpR0->pfnA20IsEnabled(pDevIns);
#else
    return pDevIns->pDevHlp->pfnA20IsEnabled(pDevIns);
#endif
}

/**
 * @copydoc PDMDEVHLP::pfnVMSetError
 */
DECLINLINE(int) PDMDevHlpVMSetError(PPDMDEVINS pDevIns, const int rc, RT_SRC_POS_DECL, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
#ifdef IN_GC
    pDevIns->pDevHlpGC->pfnVMSetErrorV(pDevIns, rc, RT_SRC_POS_ARGS, pszFormat, va);
#elif defined(IN_RING0)
    pDevIns->pDevHlpR0->pfnVMSetErrorV(pDevIns, rc, RT_SRC_POS_ARGS, pszFormat, va);
#else
    pDevIns->pDevHlp->pfnVMSetErrorV(pDevIns, rc, RT_SRC_POS_ARGS, pszFormat, va);
#endif
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




/** @defgroup grp_pdm_services  Services
 * @ingroup grp_pdm
 * @{ */


/**
 * Construct a service instance for a VM.
 *
 * @returns VBox status.
 * @param   pSrvIns     The service instance data.
 *                      If the registration structure is needed, pSrvIns->pReg points to it.
 * @param   pCfg        Configuration node handle for the service. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pSrvIns->pCfg, but since it's primary
 *                      usage is expected in this function it is passed as a parameter.
 */
typedef DECLCALLBACK(int)   FNPDMSRVCONSTRUCT(PPDMSRVINS pSrvIns, PCFGMNODE pCfg);
/** Pointer to a FNPDMSRVCONSTRUCT() function. */
typedef FNPDMSRVCONSTRUCT *PFNPDMSRVCONSTRUCT;

/**
 * Destruct a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pSrvIns     The service instance data.
 */
typedef DECLCALLBACK(void)   FNPDMSRVDESTRUCT(PPDMSRVINS pSrvIns);
/** Pointer to a FNPDMSRVDESTRUCT() function. */
typedef FNPDMSRVDESTRUCT *PFNPDMSRVDESTRUCT;

/**
 * Power On notification.
 *
 * @param   pSrvIns     The service instance data.
 */
typedef DECLCALLBACK(void)   FNPDMSRVPOWERON(PPDMSRVINS pSrvIns);
/** Pointer to a FNPDMSRVPOWERON() function. */
typedef FNPDMSRVPOWERON *PFNPDMSRVPOWERON;

/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pSrvIns     The service instance data.
 */
typedef DECLCALLBACK(void)  FNPDMSRVRESET(PPDMSRVINS pSrvIns);
/** Pointer to a FNPDMSRVRESET() function. */
typedef FNPDMSRVRESET *PFNPDMSRVRESET;

/**
 * Suspend notification.
 *
 * @returns VBox status.
 * @param   pSrvIns     The service instance data.
 */
typedef DECLCALLBACK(void)  FNPDMSRVSUSPEND(PPDMSRVINS pSrvIns);
/** Pointer to a FNPDMSRVSUSPEND() function. */
typedef FNPDMSRVSUSPEND *PFNPDMSRVSUSPEND;

/**
 * Resume notification.
 *
 * @returns VBox status.
 * @param   pSrvIns     The service instance data.
 */
typedef DECLCALLBACK(void)  FNPDMSRVRESUME(PPDMSRVINS pSrvIns);
/** Pointer to a FNPDMSRVRESUME() function. */
typedef FNPDMSRVRESUME *PFNPDMSRVRESUME;

/**
 * Power Off notification.
 *
 * @param   pSrvIns     The service instance data.
 */
typedef DECLCALLBACK(void)   FNPDMSRVPOWEROFF(PPDMSRVINS pSrvIns);
/** Pointer to a FNPDMSRVPOWEROFF() function. */
typedef FNPDMSRVPOWEROFF *PFNPDMSRVPOWEROFF;

/**
 * Detach notification.
 *
 * This is called when a driver or device is detached from the service
 *
 * @param   pSrvIns     The service instance data.
 */
typedef DECLCALLBACK(void)  FNPDMSRVDETACH(PPDMSRVINS pSrvIns, PPDMDEVINS pDevIns, PPDMDRVINS pDrvIns);
/** Pointer to a FNPDMSRVDETACH() function. */
typedef FNPDMSRVDETACH *PFNPDMSRVDETACH;



/** PDM Service Registration Structure,
 * This structure is used when registering a driver from
 * VBoxServicesRegister() (HC Ring-3). PDM will continue use till
 * the VM is terminated.
 */
typedef struct PDMSRVREG
{
    /** Structure version. PDM_SRVREG_VERSION defines the current version. */
    uint32_t            u32Version;
    /** Driver name. */
    char                szServiceName[32];
    /** The description of the driver. The UTF-8 string pointed to shall, like this structure,
     * remain unchanged from registration till VM destruction. */
    const char         *pszDescription;

    /** Flags, combination of the PDM_SRVREG_FLAGS_* \#defines. */
    RTUINT              fFlags;
    /** Size of the instance data. */
    RTUINT              cbInstance;

    /** Construct instance - required. */
    PFNPDMSRVCONSTRUCT  pfnConstruct;
    /** Destruct instance - optional. */
    PFNPDMSRVDESTRUCT   pfnDestruct;
    /** Power on notification - optional. */
    PFNPDMSRVPOWERON    pfnPowerOn;
    /** Reset notification - optional. */
    PFNPDMSRVRESET      pfnReset;
    /** Suspend notification  - optional. */
    PFNPDMSRVSUSPEND    pfnSuspend;
    /** Resume notification - optional. */
    PFNPDMSRVRESUME     pfnResume;
    /** Detach notification - optional. */
    PFNPDMSRVDETACH     pfnDetach;
    /** Power off notification - optional. */
    PFNPDMSRVPOWEROFF   pfnPowerOff;

} PDMSRVREG;
/** Pointer to a PDM Driver Structure. */
typedef PDMSRVREG *PPDMSRVREG;
/** Const pointer to a PDM Driver Structure. */
typedef PDMSRVREG const *PCPDMSRVREG;



/**
 * PDM Service API.
 */
typedef struct PDMSRVHLP
{
    /** Structure version. PDM_SRVHLP_VERSION defines the current version. */
    uint32_t                    u32Version;

    /**
     * Assert that the current thread is the emulation thread.
     *
     * @returns True if correct.
     * @returns False if wrong.
     * @param   pSrvIns         Service instance.
     * @param   pszFile         Filename of the assertion location.
     * @param   iLine           Linenumber of the assertion location.
     * @param   pszFunction     Function of the assertion location.
     */
    DECLR3CALLBACKMEMBER(bool, pfnAssertEMT,(PPDMSRVINS pSrvIns, const char *pszFile, unsigned iLine, const char *pszFunction));

    /**
     * Assert that the current thread is NOT the emulation thread.
     *
     * @returns True if correct.
     * @returns False if wrong.
     * @param   pSrvIns         Service instance.
     * @param   pszFile         Filename of the assertion location.
     * @param   iLine           Linenumber of the assertion location.
     * @param   pszFunction     Function of the assertion location.
     */
    DECLR3CALLBACKMEMBER(bool, pfnAssertOther,(PPDMSRVINS pSrvIns, const char *pszFile, unsigned iLine, const char *pszFunction));

    /**
     * Creates a timer.
     *
     * @returns VBox status.
     * @param   pVM             The VM to create the timer in.
     * @param   pSrvIns         Service instance.
     * @param   enmClock        The clock to use on this timer.
     * @param   pfnCallback     Callback function.
     * @param   pszDesc         Pointer to description string which must stay around
     *                          until the timer is fully destroyed (i.e. a bit after TMTimerDestroy()).
     * @param   ppTimer         Where to store the timer on success.
     */
    DECLR3CALLBACKMEMBER(int, pfnTMTimerCreate,(PPDMSRVINS pSrvIns, TMCLOCK enmClock, PFNTMTIMERDEV pfnCallback, const char *pszDesc, PPTMTIMERHC ppTimer));

    /**
     * Query the virtual timer frequency.
     *
     * @returns Frequency in Hz.
     * @param   pSrvIns         Service instance.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(uint64_t, pfnTMGetVirtualFreq,(PPDMSRVINS pSrvIns));

    /**
     * Query the virtual time.
     *
     * @returns The current virtual time.
     * @param   pSrvIns         Service instance.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(uint64_t, pfnTMGetVirtualTime,(PPDMSRVINS pSrvIns));

} PDMSRVHLP;
/** Pointer PDM Service API. */
typedef PDMSRVHLP *PPDMSRVHLP;
/** Pointer const PDM Service API. */
typedef const PDMSRVHLP *PCPDMSRVHLP;

/** Current SRVHLP version number. */
#define PDM_SRVHLP_VERSION  0xf9010000


/**
 * PDM Service Instance.
 */
typedef struct PDMSRVINS
{
    /** Structure version. PDM_SRVINS_VERSION defines the current version. */
    uint32_t                    u32Version;

    /** Internal data. */
    union
    {
#ifdef PDMSRVINSINT_DECLARED
        PDMSRVINSINT            s;
#endif
        uint8_t                 padding[HC_ARCH_BITS == 32 ? 32 : 32];
    } Internal;

    /** Pointer the PDM Service API. */
    HCPTRTYPE(PCPDMSRVHLP)      pHlp;
    /** Pointer to driver registration structure.  */
    HCPTRTYPE(PCPDMSRVREG)      pReg;
    /** Configuration handle. */
    HCPTRTYPE(PCFGMNODE)        pCfg;
    /** The base interface of the service.
     * The service constructor initializes this. */
    PDMIBASE                    IBase;
    /* padding to make achInstanceData aligned at 16 byte boundrary. */
    uint32_t                    au32Padding[2];
    /** Pointer to driver instance data. */
    HCPTRTYPE(void *)           pvInstanceData;
    /** Driver instance data. The size of this area is defined
     * in the PDMSRVREG::cbInstanceData field. */
    char                        achInstanceData[4];
} PDMSRVINS;

/** Current PDMSRVREG version number. */
#define PDM_SRVINS_VERSION  0xf7010000

/** Converts a pointer to the PDMSRVINS::IBase to a pointer to PDMSRVINS. */
#define PDMIBASE_2_PDMSRV(pInterface) ( (PPDMSRVINS)((char *)(pInterface) - RT_OFFSETOF(PDMSRVINS, IBase)) )



/** Pointer to callbacks provided to the VBoxServiceRegister() call. */
typedef struct PDMSRVREGCB *PPDMSRVREGCB;

/**
 * Callbacks for VBoxServiceRegister().
 */
typedef struct PDMSRVREGCB
{
    /** Interface version.
     * This is set to PDM_SRVREG_CB_VERSION. */
    uint32_t                    u32Version;

    /**
     * Registers a service with the current VM instance.
     *
     * @returns VBox status code.
     * @param   pCallbacks      Pointer to the callback table.
     * @param   pSrvReg         Pointer to the device registration record.
     *                          This data must be permanent and readonly.
     */
    DECLR3CALLBACKMEMBER(int, pfnRegister,(PPDMSRVREGCB pCallbacks, PCPDMSRVREG pSrvReg));
} PDMSRVREGCB;

/** Current version of the PDMSRVREGCB structure. */
#define PDM_SRVREG_CB_VERSION 0xf8010000


/**
 * The VBoxServicesRegister callback function.
 *
 * PDM will invoke this function after loading a device module and letting
 * the module decide which devices to register and how to handle conflicts.
 *
 * @returns VBox status code.
 * @param   pCallbacks      Pointer to the callback table.
 * @param   u32Version      VBox version number.
 */
typedef DECLCALLBACK(int) FNPDMVBOXSERVICESREGISTER(PPDMSRVREGCB pCallbacks, uint32_t u32Version);


/** @} */

/**
 * Gets the pending interrupt.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pu8Interrupt    Where to store the interrupt on success.
 */
PDMDECL(int) PDMGetInterrupt(PVM pVM, uint8_t *pu8Interrupt);

/**
 * Sets the pending ISA interrupt.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   u8Irq           The IRQ line.
 * @param   u8Level         The new level.
 */
PDMDECL(int) PDMIsaSetIrq(PVM pVM, uint8_t u8Irq, uint8_t u8Level);

/**
 * Sets the pending I/O APIC interrupt.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   u8Irq           The IRQ line.
 * @param   u8Level         The new level.
 */
PDMDECL(int) PDMIoApicSetIrq(PVM pVM, uint8_t u8Irq, uint8_t u8Level);

/**
 * Set the APIC base.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   u64Base         The new base.
 */
PDMDECL(int) PDMApicSetBase(PVM pVM, uint64_t u64Base);

/**
 * Get the APIC base.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pu64Base        Where to store the APIC base.
 */
PDMDECL(int) PDMApicGetBase(PVM pVM, uint64_t *pu64Base);

/**
 * Set the TPR (task priority register?).
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   u8TPR           The new TPR.
 */
PDMDECL(int) PDMApicSetTPR(PVM pVM, uint8_t u8TPR);

/**
 * Get the TPR (task priority register?).
 *
 * @returns The current TPR.
 * @param   pVM             VM handle.
 * @param   pu8TPR          Where to store the TRP.
 */
PDMDECL(int) PDMApicGetTPR(PVM pVM, uint8_t *pu8TPR);


#ifdef IN_RING3
/** @defgroup grp_pdm_r3    The PDM Host Context Ring-3 API
 * @ingroup grp_pdm
 * @{
 */

/**
 * Initializes the PDM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
PDMR3DECL(int) PDMR3Init(PVM pVM);

/**
 * This function will notify all the devices and their
 * attached drivers about the VM now being powered on.
 *
 * @param   pVM     VM Handle.
 */
PDMR3DECL(void) PDMR3PowerOn(PVM pVM);

/**
 * This function will notify all the devices and their
 * attached drivers about the VM now being reset.
 *
 * @param   pVM     VM Handle.
 */
PDMR3DECL(void) PDMR3Reset(PVM pVM);

/**
 * This function will notify all the devices and their
 * attached drivers about the VM now being reset.
 *
 * @param   pVM     VM Handle.
 */
PDMR3DECL(void) PDMR3Suspend(PVM pVM);

/**
 * This function will notify all the devices and their
 * attached drivers about the VM now being resumed.
 *
 * @param   pVM     VM Handle.
 */
PDMR3DECL(void) PDMR3Resume(PVM pVM);

/**
 * This function will notify all the devices and their
 * attached drivers about the VM being powered off.
 *
 * @param   pVM     VM Handle.
 */
PDMR3DECL(void) PDMR3PowerOff(PVM pVM);


/**
 * Applies relocations to GC modules.
 *
 * This must be done very early in the relocation
 * process so that components can resolve GC symbols during relocation.
 *
 * @param   pVM         VM handle.
 * @param   offDelta    Relocation delta relative to old location.
 */
PDMR3DECL(void) PDMR3LdrRelocate(PVM pVM, RTGCINTPTR offDelta);

/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * @param   pVM         VM handle.
 * @param   offDelta    Relocation delta relative to old location.
 */
PDMR3DECL(void) PDMR3Relocate(PVM pVM, RTGCINTPTR offDelta);

/**
 * Terminates the PDM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
PDMR3DECL(int) PDMR3Term(PVM pVM);


/**
 * Get the address of a symbol in a given HC ring-3 module.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pszModule       Module name.
 * @param   pszSymbol       Symbol name. If it's value is less than 64k it's treated like a
 *                          ordinal value rather than a string pointer.
 * @param   ppvValue        Where to store the symbol value.
 */
PDMR3DECL(int) PDMR3GetSymbolR3(PVM pVM, const char *pszModule, const char *pszSymbol, void **ppvValue);

/**
 * Get the address of a symbol in a given HC ring-0 module.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pszModule       Module name. If NULL the main R0 module (VMMR0.r0) is assumed.
 * @param   pszSymbol       Symbol name. If it's value is less than 64k it's treated like a
 *                          ordinal value rather than a string pointer.
 * @param   ppvValue        Where to store the symbol value.
 */
PDMR3DECL(int) PDMR3GetSymbolR0(PVM pVM, const char *pszModule, const char *pszSymbol, void **ppvValue);

/**
 * Same as PDMR3GetSymbolR0 except that the module will be attempted loaded if not found.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pszModule       Module name. If NULL the main R0 module (VMMR0.r0) is assumed.
 * @param   pszSymbol       Symbol name. If it's value is less than 64k it's treated like a
 *                          ordinal value rather than a string pointer.
 * @param   ppvValue        Where to store the symbol value.
 */
PDMR3DECL(int) PDMR3GetSymbolR0Lazy(PVM pVM, const char *pszModule, const char *pszSymbol, void **ppvValue);

/**
 * Loads a module into the guest context (i.e. into the Hypervisor memory region).
 *
 * The external (to PDM) use of this interface is to load VMMGC.gc.
 *
 * @returns VBox status code.
 * @param   pVM             The VM to load it into.
 * @param   pszFilename     Filename of the module binary.
 * @param   pszName         Module name. Case sensitive and the length is limited!
 */
PDMR3DECL(int) PDMR3LoadGC(PVM pVM, const char *pszFilename, const char *pszName);

/**
 * Get the address of a symbol in a given GC module.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pszModule       Module name. If NULL the main GC module (VMMGC.gc) is assumed.
 * @param   pszSymbol       Symbol name. If it's value is less than 64k it's treated like a
 *                          ordinal value rather than a string pointer.
 * @param   pGCPtrValue     Where to store the symbol value.
 */
PDMR3DECL(int) PDMR3GetSymbolGC(PVM pVM, const char *pszModule, const char *pszSymbol, PRTGCPTR pGCPtrValue);

/**
 * Same as PDMR3GetSymbolGC except that the module will be attempted loaded if not found.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pszModule       Module name. If NULL the main GC module (VMMGC.gc) is assumed.
 * @param   pszSymbol       Symbol name. If it's value is less than 64k it's treated like a
 *                          ordinal value rather than a string pointer.
 * @param   pGCPtrValue     Where to store the symbol value.
 */
PDMR3DECL(int) PDMR3GetSymbolGCLazy(PVM pVM, const char *pszModule, const char *pszSymbol, PRTGCPTR pGCPtrValue);

/**
 * Queries module information from an EIP.
 *
 * This is typically used to locate a crash address.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle
 * @param   uEIP        EIP to locate.
 * @param   pszModName  Where to store the module name.
 * @param   cchModName  Size of the module name buffer.
 * @param   pMod        Base address of the module.
 * @param   pszNearSym1 Name of the closes symbol from below.
 * @param   cchNearSym1 Size of the buffer pointed to by pszNearSym1.
 * @param   pNearSym1   The address of pszNearSym1.
 * @param   pszNearSym2 Name of the closes symbol from below.
 * @param   cchNearSym2 Size of the buffer pointed to by pszNearSym2.
 * @param   pNearSym2   The address of pszNearSym2.
 */
PDMR3DECL(int) PDMR3QueryModFromEIP(PVM pVM, uint32_t uEIP,
                                    char *pszModName,  unsigned cchModName,  RTGCPTR *pMod,
                                    char *pszNearSym1, unsigned cchNearSym1, RTGCPTR *pNearSym1,
                                    char *pszNearSym2, unsigned cchNearSym2, RTGCPTR *pNearSym2);


/**
 * Module enumeration callback function.
 *
 * @returns VBox status.
 *          Failure will stop the search and return the return code.
 *          Warnings will be ignored and not returned.
 * @param   pVM             VM Handle.
 * @param   pszFilename     Module filename.
 * @param   pszName         Module name. (short and unique)
 * @param   ImageBase       Address where to executable image is loaded.
 * @param   cbImage         Size of the executable image.
 * @param   fGC             Set if guest context, clear if host context.
 * @param   pvArg           User argument.
 */
typedef DECLCALLBACK(int) FNPDMR3ENUM(PVM pVM, const char *pszFilename, const char *pszName, RTUINTPTR ImageBase, size_t cbImage, bool fGC);
/** Pointer to a FNPDMR3ENUM() function. */
typedef FNPDMR3ENUM *PFNPDMR3ENUM;


/**
 * Enumerate all PDM modules.
 *
 * @returns VBox status.
 * @param   pVM             VM Handle.
 * @param   pfnCallback     Function to call back for each of the modules.
 * @param   pvArg           User argument.
 */
PDMR3DECL(int)  PDMR3EnumModules(PVM pVM, PFNPDMR3ENUM pfnCallback, void *pvArg);


/**
 * Queries the base interace of a device instance.
 *
 * The caller can use this to query other interfaces the device implements
 * and use them to talk to the device.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pszDevice       Device name.
 * @param   iInstance       Device instance.
 * @param   ppBase          Where to store the pointer to the base device interface on success.
 * @remark  We're doing any locking ATM, so don't try call this at times when the
 *          device chain is known to be updated.
 */
PDMR3DECL(int) PDMR3QueryDevice(PVM pVM, const char *pszDevice, unsigned iInstance, PPDMIBASE *ppBase);

/**
 * Queries the base interface of a device LUN.
 *
 * This differs from PDMR3QueryLun by that it returns the interface on the
 * device and not the top level driver.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pszDevice       Device name.
 * @param   iInstance       Device instance.
 * @param   iLun            The Logical Unit to obtain the interface of.
 * @param   ppBase          Where to store the base interface pointer.
 * @remark  We're doing any locking ATM, so don't try call this at times when the
 *          device chain is known to be updated.
 */
PDMR3DECL(int) PDMR3QueryDeviceLun(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun, PPDMIBASE *ppBase);

/**
 * Query the interface of the top level driver on a LUN.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pszDevice       Device name.
 * @param   iInstance       Device instance.
 * @param   iLun            The Logical Unit to obtain the interface of.
 * @param   ppBase          Where to store the base interface pointer.
 * @remark  We're doing any locking ATM, so don't try call this at times when the
 *          device chain is known to be updated.
 */
PDMR3DECL(int) PDMR3QueryLun(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun, PPDMIBASE *ppBase);

/**
 * Attaches a preconfigured driver to an existing device instance.
 *
 * This is used to change drivers and suchlike at runtime.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pszDevice       Device name.
 * @param   iInstance       Device instance.
 * @param   iLun            The Logical Unit to obtain the interface of.
 * @param   ppBase          Where to store the base interface pointer. Optional.
 * @thread  EMT
 */
PDMR3DECL(int) PDMR3DeviceAttach(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun, PPDMIBASE *ppBase);

/**
 * Detaches a driver from an existing device instance.
 *
 * This is used to change drivers and suchlike at runtime.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pszDevice       Device name.
 * @param   iInstance       Device instance.
 * @param   iLun            The Logical Unit to obtain the interface of.
 * @thread  EMT
 */
PDMR3DECL(int) PDMR3DeviceDetach(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun);

/**
 * Executes pending DMA transfers.
 * Forced Action handler.
 *
 * @param   pVM             VM handle.
 */
PDMR3DECL(void) PDMR3DmaRun(PVM pVM);

/**
 * Call polling function.
 *
 * @param   pVM             VM handle.
 */
PDMR3DECL(void) PDMR3Poll(PVM pVM);

/**
 * Serivce a VMMCALLHOST_PDM_LOCK call.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
PDMR3DECL(int) PDMR3LockCall(PVM pVM);

/** @} */
#endif


#ifdef IN_GC
/** @defgroup grp_pdm_gc    The PDM Guest Context API
 * @ingroup grp_pdm
 * @{
 */
/** @} */
#endif

__END_DECLS

/** @} */

#endif

