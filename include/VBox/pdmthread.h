/** @file
 * PDM - Pluggable Device Manager, Threads.
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

#ifndef ___VBox_pdmthread_h
#define ___VBox_pdmthread_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#ifdef IN_RING3
# include <iprt/thread.h>
#endif 

__BEGIN_DECLS

/** @group grp_pdm_thread       Threads
 * @ingroup grp_pdm
 * @{
 */

/**
 * The thread state
 */
typedef enum PDMTHREADSTATE
{
    /** The usual invalid 0 entry. */
    PDMTHREADSTATE_INVALID = 0,
    /** The thread is initializing.
     * Prev state: none
     * Next state: suspended, terminating (error) */
    PDMTHREADSTATE_INITIALIZING,
    /** The thread has been asked to suspend.
     * Prev state: running
     * Next state: suspended */
    PDMTHREADSTATE_SUSPENDING,
    /** The thread is supended.
     * Prev state: suspending, initializing
     * Next state: resuming, terminated. */
    PDMTHREADSTATE_SUSPENDED,
    /** The thread is active.
     * Prev state: suspended
     * Next state: running, terminating. */
    PDMTHREADSTATE_RESUMING,
    /** The thread is active.
     * Prev state: resuming
     * Next state: suspending, terminating. */
    PDMTHREADSTATE_RUNNING,
    /** The thread has been asked to terminate.
     * Prev state: initializing, suspended, resuming, running
     * Next state: terminated. */
    PDMTHREADSTATE_TERMINATING,
    /** The thread is terminating / has terminated.
     * Prev state: terminating
     * Next state: none */
    PDMTHREADSTATE_TERMINATED,
    /** The usual 32-bit hack. */
    PDMTHREADSTATE_32BIT_HACK = 0x7fffffff
} PDMTHREADSTATE;

/** A pointer to a PDM thread. */
typedef R3PTRTYPE(struct PDMTHREAD *) PPDMTHREAD;
/** A pointer to a pointer to a PDM thread. */
typedef PPDMTHREAD *PPPDMTHREAD;

/**
 * PDM thread, device variation.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pThread     The PDM thread data.
 */
typedef int FNPDMTHREADDEV(PPDMDEVINS pDevIns, PPDMTHREAD pThread);
/** Pointer to a FNPDMTHREADDEV(). */
typedef FNPDMTHREADDEV *PFNPDMTHREADDEV;

/**
 * PDM thread, USB device variation.
 *
 * @returns VBox status code.
 * @param   pUsbIns     The USB device instance.
 * @param   pThread     The PDM thread data.
 */
typedef int FNPDMTHREADUSB(PPDMUSBINS pUsbIns, PPDMTHREAD pThread);
/** Pointer to a FNPDMTHREADUSB(). */
typedef FNPDMTHREADUSB *PFNPDMTHREADUSB;

/**
 * PDM thread, driver variation.
 *
 * @returns VBox status code.
 * @param   pDrvIns     The driver instance.
 * @param   pThread     The PDM thread data.
 */
typedef int FNPDMTHREADDRV(PPDMDRVINS pDrvIns, PPDMTHREAD pThread);
/** Pointer to a FNPDMTHREADDRV(). */
typedef FNPDMTHREADDRV *PFNPDMTHREADDRV;

/**
 * PDM thread, driver variation.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pThread     The PDM thread data.
 */
typedef int FNPDMTHREADINT(PVM pVM, PPDMTHREAD pThread);
/** Pointer to a FNPDMTHREADINT(). */
typedef FNPDMTHREADINT *PFNPDMTHREADINT;

/**
 * PDM thread, driver variation.
 *
 * @returns VBox status code.
 * @param   pThread     The PDM thread data.
 */
typedef int FNPDMTHREADEXT(PPDMTHREAD pThread);
/** Pointer to a FNPDMTHREADEXT(). */
typedef FNPDMTHREADEXT *PFNPDMTHREADEXT;



/**
 * PDM thread wakeup call, device variation.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pThread     The PDM thread data.
 */
typedef int FNPDMTHREADWAKEUPDEV(PPDMDEVINS pDevIns, PPDMTHREAD pThread);
/** Pointer to a FNPDMTHREADDEV(). */
typedef FNPDMTHREADWAKEUPDEV *PFNPDMTHREADWAKEUPDEV;

/**
 * PDM thread wakeup call, device variation.
 *
 * @returns VBox status code.
 * @param   pUsbIns     The USB device instance.
 * @param   pThread     The PDM thread data.
 */
typedef int FNPDMTHREADWAKEUPUSB(PPDMUSBINS pUsbIns, PPDMTHREAD pThread);
/** Pointer to a FNPDMTHREADUSB(). */
typedef FNPDMTHREADWAKEUPUSB *PFNPDMTHREADWAKEUPUSB;

/**
 * PDM thread wakeup call, driver variation.
 *
 * @returns VBox status code.
 * @param   pDrvIns     The driver instance.
 * @param   pThread     The PDM thread data.
 */
typedef int FNPDMTHREADWAKEUPDRV(PPDMDRVINS pDrvIns, PPDMTHREAD pThread);
/** Pointer to a FNPDMTHREADDRV(). */
typedef FNPDMTHREADWAKEUPDRV *PFNPDMTHREADWAKEUPDRV;

/**
 * PDM thread wakeup call, internal variation.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pThread     The PDM thread data.
 */
typedef int FNPDMTHREADWAKEUPINT(PVM pVM, PPDMTHREAD pThread);
/** Pointer to a FNPDMTHREADWAKEUPINT(). */
typedef FNPDMTHREADWAKEUPINT *PFNPDMTHREADWAKEUPINT;

/**
 * PDM thread wakeup call, external variation.
 *
 * @returns VBox status code.
 * @param   pThread     The PDM thread data.
 */
typedef int FNPDMTHREADWAKEUPEXT(PPDMTHREAD pThread);
/** Pointer to a FNPDMTHREADEXT(). */
typedef FNPDMTHREADWAKEUPEXT *PFNPDMTHREADWAKEUPEXT;


/**
 * PDM Thread instance data.
 */
typedef struct PDMTHREAD
{
    /** PDMTHREAD_VERSION. */
    uint32_t                    u32Version;
    /** The thread state. */
    PDMTHREADSTATE volatile     enmState;
    /** The thread handle. */
    RTTHREAD                    Thread;
    /** The user parameter. */
    R3PTRTYPE(void *)           pvUser;
    /** Data specific to the kind of thread.
     * This should really be in PDMTHREADINT, but is placed here because of the
     * function pointer typedefs. So, don't touch these, please.
     */
    union
    {
        /** PDMTHREADTYPE_DEVICE data. */
        struct
        {
            /** The device instance. */
            PPDMDEVINSR3                        pDevIns;
            /** The thread function. */
            R3PTRTYPE(PFNPDMTHREADDEV)          pfnThread;
            /** Thread. */
            R3PTRTYPE(PFNPDMTHREADWAKEUPDEV)    pfnWakeUp;
        } Dev;

        /** PDMTHREADTYPE_USB data. */
        struct
        {
            /** The device instance. */
            PPDMUSBINS                          pUsbIns;
            /** The thread function. */
            R3PTRTYPE(PFNPDMTHREADUSB)          pfnThread;
            /** Thread. */
            R3PTRTYPE(PFNPDMTHREADWAKEUPUSB)    pfnWakeUp;
        } Usb;

        /** PDMTHREADTYPE_DRIVER data. */
        struct
        {
            /** The driver instance. */
            R3PTRTYPE(PPDMDRVINS)               pDrvIns;
            /** The thread function. */
            R3PTRTYPE(PFNPDMTHREADDRV)          pfnThread;
            /** Thread. */
            R3PTRTYPE(PFNPDMTHREADWAKEUPDRV)    pfnWakeUp;
        } Drv;

        /** PDMTHREADTYPE_INTERNAL data. */
        struct
        {
            /** The thread function. */
            R3PTRTYPE(PFNPDMTHREADINT)          pfnThread;
            /** Thread. */
            R3PTRTYPE(PFNPDMTHREADWAKEUPINT)    pfnWakeUp;
        } Int;

        /** PDMTHREADTYPE_EXTERNAL data. */
        struct
        {
            /** The thread function. */
            R3PTRTYPE(PFNPDMTHREADEXT)          pfnThread;
            /** Thread. */
            R3PTRTYPE(PFNPDMTHREADWAKEUPEXT)    pfnWakeUp;
        } Ext;
    } u;

    /** Internal data. */
    union
    {
#ifdef PDMTHREADINT_DECLARED
        PDMTHREADINT            s;
#endif
        uint8_t                 padding[64];
    } Internal;
} PDMTHREAD;

/** PDMTHREAD::u32Version value. */
#define PDMTHREAD_VERSION   0xef010000

#ifdef IN_RING3
/**
 * Creates a PDM thread for internal use in the VM.
 * 
 * @returns VBox status code.
 * @param   pVM         The VM handle. 
 * @param   ppThread    Where to store the thread 'handle'.
 * @param   pvUser      The user argument to the thread function.
 * @param   pfnThread   The thread function.
 * @param   pfnWakeUp   The wakup callback. This is called on the EMT thread when
 *                      a state change is pending.
 * @param   cbStack     See RTThreadCreate.
 * @param   enmType     See RTThreadCreate.
 * @param   pszName     See RTThreadCreate.
 */
PDMR3DECL(int) PDMR3ThreadCreate(PVM pVM, PPPDMTHREAD ppThread, void *pvUser, PFNPDMTHREADINT pfnThread,
                                 PFNPDMTHREADWAKEUPINT pfnWakeUp, size_t cbStack, RTTHREADTYPE enmType, const char *pszName);

/**
 * Creates a PDM thread for VM use by some external party.
 * 
 * @returns VBox status code.
 * @param   pVM         The VM handle. 
 * @param   ppThread    Where to store the thread 'handle'.
 * @param   pvUser      The user argument to the thread function.
 * @param   pfnThread   The thread function.
 * @param   pfnWakeUp   The wakup callback. This is called on the EMT thread when
 *                      a state change is pending.
 * @param   cbStack     See RTThreadCreate.
 * @param   enmType     See RTThreadCreate.
 * @param   pszName     See RTThreadCreate.
 */
PDMR3DECL(int) PDMR3ThreadCreateExternal(PVM pVM, PPPDMTHREAD ppThread, void *pvUser, PFNPDMTHREADEXT pfnThread,
                                         PFNPDMTHREADWAKEUPEXT pfnWakeUp, size_t cbStack, RTTHREADTYPE enmType, const char *pszName);

/**
 * Destroys a PDM thread.
 *
 * This will wakeup the thread, tell it to terminate, and wait for it terminate.
 *
 * @returns VBox status code.
 *          This reflects the success off destroying the thread and not the exit code
 *          of the thread as this is stored in *pRcThread.
 * @param   pThread         The thread to destroy.
 * @param   pRcThread       Where to store the thread exit code. Optional.
 * @thread  The emulation thread (EMT).
 */
PDMR3DECL(int) PDMR3ThreadDestroy(PPDMTHREAD pThread, int *pRcThread);

/**
 * Called by the PDM thread in response to a wakeup call with
 * suspending as the new state.
 *
 * The thread will block in side this call until the state is changed in
 * response to a VM state change or to the device/driver/whatever calling the
 * PDMR3ThreadResume API.
 *
 * @returns VBox status code.
 *          On failure, terminate the thread.
 * @param   pThread     The PDM thread.
 */
PDMR3DECL(int) PDMR3ThreadIAmSuspending(PPDMTHREAD pThread);

/**
 * Called by the PDM thread in response to a resuming state.
 *
 * The purpose of this API is to tell the PDMR3ThreadResume caller that
 * the the PDM thread has successfully resumed. It will also do the
 * state transition from the resuming to the running state.
 *
 * @returns VBox status code.
 *          On failure, terminate the thread.
 * @param   pThread     The PDM thread.
 */
PDMR3DECL(int) PDMR3ThreadIAmRunning(PPDMTHREAD pThread);

/**
 * Suspends the thread.
 *
 * This can be called at the power off / suspend notifications to suspend the
 * PDM thread a bit early. The thread will be automatically suspend upon
 * completion of the device/driver notification cycle.
 *
 * The caller is responsible for serializing the control operations on the
 * thread. That basically means, always do these calls from the EMT.
 *
 * @returns VBox status code.
 * @param   pThread     The PDM thread.
 */
PDMR3DECL(int) PDMR3ThreadSuspend(PPDMTHREAD pThread);

/**
 * Resumes the thread.
 *
 * This can be called the power on / resume notifications to resume the
 * PDM thread a bit early. The thread will be automatically resumed upon
 * return from these two notification callbacks (devices/drivers).
 *
 * The caller is responsible for serializing the control operations on the
 * thread. That basically means, always do these calls from the EMT.
 *
 * @returns VBox status code.
 * @param   pThread     The PDM thread.
 */
PDMR3DECL(int) PDMR3ThreadResume(PPDMTHREAD pThread);
#endif /* IN_RING3 */

/** @} */

__END_DECLS

#endif
