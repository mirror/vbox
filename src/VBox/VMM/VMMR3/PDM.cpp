/* $Id$ */
/** @file
 * PDM - Pluggable Device Manager.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/** @page   pg_pdm      PDM - The Pluggable Device & Driver Manager
 *
 * VirtualBox is designed to be very configurable, i.e. the ability to select
 * virtual devices and configure them uniquely for a VM.  For this reason
 * virtual devices are not statically linked with the VMM but loaded, linked and
 * instantiated at runtime by PDM using the information found in the
 * Configuration Manager (CFGM).
 *
 * While the chief purpose of PDM is to manager of devices their drivers, it
 * also serves as somewhere to put usful things like cross context queues, cross
 * context synchronization (like critsect), VM centric thread management,
 * asynchronous I/O framework, and so on.
 *
 * @see grp_pdm
 *
 *
 * @section sec_pdm_dev     The Pluggable Devices
 *
 * Devices register themselves when the module containing them is loaded.  PDM
 * will call the entry point 'VBoxDevicesRegister' when loading a device module.
 * The device module will then use the supplied callback table to check the VMM
 * version and to register its devices.  Each device have an unique (for the
 * configured VM) name.  The name is not only used in PDM but also in CFGM (to
 * organize device and device instance settings) and by anyone who wants to talk
 * to a specific device instance.
 *
 * When all device modules have been successfully loaded PDM will instantiate
 * those devices which are configured for the VM.  Note that a device may have
 * more than one instance, see network adaptors for instance.  When
 * instantiating a device PDM provides device instance memory and a callback
 * table (aka Device Helpers / DevHlp) with the VM APIs which the device
 * instance is trusted with.
 *
 * Some devices are trusted devices, most are not.  The trusted devices are an
 * integrated part of the VM and can obtain the VM handle from their device
 * instance handles, thus enabling them to call any VM api.  Untrusted devices
 * can only use the callbacks provided during device instantiation.
 *
 * The main purpose in having DevHlps rather than just giving all the devices
 * the VM handle and let them call the internal VM APIs directly, is both to
 * create a binary interface that can be supported across releases and to
 * create a barrier between devices and the VM.  (The trusted / untrusted bit
 * hasn't turned out to be of much use btw., but it's easy to maintain so there
 * isn't any point in removing it.)
 *
 * A device can provide a ring-0 and/or a raw-mode context extension to improve
 * the VM performance by handling exits and traps (respectively) without
 * requiring context switches (to ring-3).  Callbacks for MMIO and I/O ports can
 * needs to be registered specifically for the additional contexts for this to
 * make sense.  Also, the device has to be trusted to be loaded into R0/RC
 * because of the extra privilege it entails.  Note that raw-mode code and data
 * will be subject to relocation.
 *
 *
 * @section sec_pdm_special_devs    Special Devices
 *
 * Several kinds of devices interacts with the VMM and/or other device and PDM
 * will work like a mediator for these. The typical pattern is that the device
 * calls a special registration device helper with a set of callbacks, PDM
 * responds by copying this and providing a pointer to a set helper callbacks
 * for that particular kind of device. Unlike interfaces where the callback
 * table pointer is used a 'this' pointer, these arrangements will use the
 * device instance pointer (PPDMDEVINS) as a kind of 'this' pointer.
 *
 * For an example of this kind of setup, see the PIC. The PIC registers itself
 * by calling PDMDEVHLPR3::pfnPICRegister.  PDM saves the device instance,
 * copies the callback tables (PDMPICREG), resolving the ring-0 and raw-mode
 * addresses in the process, and hands back the pointer to a set of helper
 * methods (PDMPICHLPR3).  The PCI device then queries the ring-0 and raw-mode
 * helpers using PDMPICHLPR3::pfnGetR0Helpers and PDMPICHLPR3::pfnGetRCHelpers.
 * The PCI device repeats ths pfnGetRCHelpers call in it's relocation method
 * since the address changes when RC is relocated.
 *
 * @see grp_pdm_device
 *
 *
 * @section sec_pdm_usbdev  The Pluggable USB Devices
 *
 * USB devices are handled a little bit differently than other devices.  The
 * general concepts wrt. pluggability are mostly the same, but the details
 * varies.  The registration entry point is 'VBoxUsbRegister', the device
 * instance is PDMUSBINS and the callbacks helpers are different.  Also, USB
 * device are restricted to ring-3 and cannot have any ring-0 or raw-mode
 * extensions (at least not yet).
 *
 * The way USB devices work differs greatly from other devices though since they
 * aren't attaches directly to the PCI/ISA/whatever system buses but via a
 * USB host control (OHCI, UHCI or EHCI).  USB devices handles USB requests
 * (URBs) and does not register I/O ports, MMIO ranges or PCI bus
 * devices/functions.
 *
 * @see grp_pdm_usbdev
 *
 *
 * @section sec_pdm_drv     The Pluggable Drivers
 *
 * The VM devices are often accessing host hardware or OS facilities.  For most
 * devices these facilities can be abstracted in one or more levels.  These
 * abstractions are called drivers.
 *
 * For instance take a DVD/CD drive.  This can be connected to a SCSI
 * controller, an ATA controller or a SATA controller.  The basics of the DVD/CD
 * drive implementation remains the same - eject, insert, read, seek, and such.
 * (For the scsi case, you might wanna speak SCSI directly to, but that can of
 * course be fixed - see SCSI passthru.)  So, it
 * makes much sense to have a generic CD/DVD driver which implements this.
 *
 * Then the media 'inserted' into the DVD/CD drive can be a ISO image, or it can
 * be read from a real CD or DVD drive (there are probably other custom formats
 * someone could desire to read or construct too).  So, it would make sense to
 * have abstracted interfaces for dealing with this in a generic way so the
 * cdrom unit doesn't have to implement it all.  Thus we have created the
 * CDROM/DVD media driver family.
 *
 * So, for this example the IDE controller #1 (i.e. secondary) will have
 * the DVD/CD Driver attached to it's LUN #0 (master).  When a media is mounted
 * the DVD/CD Driver will have a ISO, HostDVD or RAW (media) Driver attached.
 *
 * It is possible to configure many levels of drivers inserting filters, loggers,
 * or whatever you desire into the chain.  We're using this for network sniffing
 * for instance.
 *
 * The drivers are loaded in a similar manner to that of the device, namely by
 * iterating a keyspace in CFGM, load the modules listed there and call
 * 'VBoxDriversRegister' with a callback table.
 *
 * @see grp_pdm_driver
 *
 *
 * @section sec_pdm_ifs     Interfaces
 *
 * The pluggable drivers and devices exposes one standard interface (callback
 * table) which is used to construct, destruct, attach, detach,( ++,) and query
 * other interfaces. A device will query the interfaces required for it's
 * operation during init and hot-plug.  PDM may query some interfaces during
 * runtime mounting too.
 *
 * An interface here means a function table contained within the device or
 * driver instance data. Its method are invoked with the function table pointer
 * as the first argument and they will calculate the address of the device or
 * driver instance data from it. (This is one of the aspects which *might* have
 * been better done in C++.)
 *
 * @see grp_pdm_interfaces
 *
 *
 * @section sec_pdm_utils   Utilities
 *
 * As mentioned earlier, PDM is the location of any usful constructs that doesn't
 * quite fit into IPRT. The next subsections will discuss these.
 *
 * One thing these APIs all have in common is that resources will be associated
 * with a device / driver and automatically freed after it has been destroyed if
 * the destructor didn't do this.
 *
 *
 * @subsection sec_pdm_async_completion     Async I/O
 *
 * The PDM Async I/O API provides a somewhat platform agnostic interface for
 * asynchronous I/O.  For reasons of performance and complexity this does not
 * build upon any IPRT API.
 *
 * @todo more details.
 *
 * @see grp_pdm_async_completion
 *
 *
 * @subsection sec_pdm_async_task   Async Task - not implemented
 *
 * @todo implement and describe
 *
 * @see grp_pdm_async_task
 *
 *
 * @subsection sec_pdm_critsect     Critical Section
 *
 * The PDM Critical Section API is currently building on the IPRT API with the
 * same name.  It adds the possibility to use critical sections in ring-0 and
 * raw-mode as well as in ring-3.  There are certain restrictions on the RC and
 * R0 usage though since we're not able to wait on it, nor wake up anyone that
 * is waiting on it.  These restrictions origins with the use of a ring-3 event
 * semaphore.  In a later incarnation we plan to replace the ring-3 event
 * semaphore with a ring-0 one, thus enabling us to wake up waiters while
 * exectuing in ring-0 and making the hardware assisted execution mode more
 * efficient. (Raw-mode won't benefit much from this, naturally.)
 *
 * @see grp_pdm_critsect
 *
 *
 * @subsection sec_pdm_queue        Queue
 *
 * The PDM Queue API is for queuing one or more tasks for later consumption in
 * ring-3 by EMT, and optionally forcing a delayed or ASAP return to ring-3.  The
 * queues can also be run on a timer basis as an alternative to the ASAP thing.
 * The queue will be flushed at forced action time.
 *
 * A queue can also be used by another thread (a I/O worker for instance) to
 * send work / events over to the EMT.
 *
 * @see grp_pdm_queue
 *
 *
 * @subsection sec_pdm_task        Task - not implemented yet
 *
 * The PDM Task API is for flagging a task for execution at a later point when
 * we're back in ring-3, optionally forcing the ring-3 return to happen ASAP.
 * As you can see the concept is similar to queues only simpler.
 *
 * A task can also be scheduled by another thread (a I/O worker for instance) as
 * a mean of getting something done in EMT.
 *
 * @see grp_pdm_task
 *
 *
 * @subsection sec_pdm_thread       Thread
 *
 * The PDM Thread API is there to help devices and drivers manage their threads
 * correctly wrt. power on, suspend, resume, power off and destruction.
 *
 * The general usage pattern for threads in the employ of devices and drivers is
 * that they shuffle data or requests while the VM is running and stop doing
 * this when the VM is paused or powered down. Rogue threads running while the
 * VM is paused can cause the state to change during saving or have other
 * unwanted side effects. The PDM Threads API ensures that this won't happen.
 *
 * @see grp_pdm_thread
 *
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PDM
#include "PDMInternal.h"
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/ssm.h>
#include <VBox/vmm/vm.h>
#include <VBox/vmm/uvm.h>
#include <VBox/vmm/vmm.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/sup.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/ldr.h>
#include <iprt/path.h>
#include <iprt/string.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The PDM saved state version. */
#define PDM_SAVED_STATE_VERSION             4
#define PDM_SAVED_STATE_VERSION_PRE_NMI_FF  3

/** The number of nanoseconds a suspend callback needs to take before
 * PDMR3Suspend warns about it taking too long. */
#define PDMSUSPEND_WARN_AT_NS               UINT64_C(1200000000)

/** The number of nanoseconds a suspend callback needs to take before
 * PDMR3PowerOff warns about it taking too long. */
#define PDMPOWEROFF_WARN_AT_NS              UINT64_C( 900000000)


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Statistics of asynchronous notification tasks - used by reset, suspend and
 * power off.
 */
typedef struct PDMNOTIFYASYNCSTATS
{
    /** The the start timestamp. */
    uint64_t        uStartNsTs;
    /** When to log the next time. */
    uint64_t        cNsElapsedNextLog;
    /** The loop counter. */
    uint32_t        cLoops;
    /** The number of pending asynchronous notification tasks. */
    uint32_t        cAsync;
    /** The name of the operation (log prefix). */
    const char     *pszOp;
    /** The current list buffer position. */
    size_t          offList;
    /** String containing a list of the pending tasks. */
    char            szList[1024];
} PDMNOTIFYASYNCSTATS;
/** Pointer to the stats of pending asynchronous notification tasks. */
typedef PDMNOTIFYASYNCSTATS *PPDMNOTIFYASYNCSTATS;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) pdmR3LiveExec(PVM pVM, PSSMHANDLE pSSM, uint32_t uPass);
static DECLCALLBACK(int) pdmR3SaveExec(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int) pdmR3LoadExec(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);
static DECLCALLBACK(int) pdmR3LoadPrep(PVM pVM, PSSMHANDLE pSSM);



/**
 * Initializes the PDM part of the UVM.
 *
 * This doesn't really do much right now but has to be here for the sake
 * of completeness.
 *
 * @returns VBox status code.
 * @param   pUVM        Pointer to the user mode VM structure.
 */
VMMR3DECL(int) PDMR3InitUVM(PUVM pUVM)
{
    AssertCompile(sizeof(pUVM->pdm.s) <= sizeof(pUVM->pdm.padding));
    AssertRelease(sizeof(pUVM->pdm.s) <= sizeof(pUVM->pdm.padding));
    pUVM->pdm.s.pModules   = NULL;
    pUVM->pdm.s.pCritSects = NULL;
    return RTCritSectInit(&pUVM->pdm.s.ListCritSect);
}


/**
 * Initializes the PDM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) PDMR3Init(PVM pVM)
{
    LogFlow(("PDMR3Init\n"));

    /*
     * Assert alignment and sizes.
     */
    AssertRelease(!(RT_OFFSETOF(VM, pdm.s) & 31));
    AssertRelease(sizeof(pVM->pdm.s) <= sizeof(pVM->pdm.padding));
    AssertCompileMemberAlignment(PDM, CritSect, sizeof(uintptr_t));

    /*
     * Init the structure.
     */
    pVM->pdm.s.GCPhysVMMDevHeap = NIL_RTGCPHYS;

    /*
     * Initialize critical sections first.
     */
    int rc = pdmR3CritSectInitStats(pVM);
    if (RT_SUCCESS(rc))
        rc = PDMR3CritSectInit(pVM, &pVM->pdm.s.CritSect, RT_SRC_POS, "PDM");
    if (RT_SUCCESS(rc))
    {
        rc = PDMR3CritSectInit(pVM, &pVM->pdm.s.NopCritSect, RT_SRC_POS, "NOP");
        if (RT_SUCCESS(rc))
            pVM->pdm.s.NopCritSect.s.Core.fFlags |= RTCRITSECT_FLAGS_NOP;
    }

    /*
     * Initialize sub components.
     */
    if (RT_SUCCESS(rc))
        rc = pdmR3LdrInitU(pVM->pUVM);
#ifdef VBOX_WITH_PDM_ASYNC_COMPLETION
    if (RT_SUCCESS(rc))
        rc = pdmR3AsyncCompletionInit(pVM);
#endif
    if (RT_SUCCESS(rc))
        rc = pdmR3BlkCacheInit(pVM);
    if (RT_SUCCESS(rc))
        rc = pdmR3DrvInit(pVM);
    if (RT_SUCCESS(rc))
        rc = pdmR3DevInit(pVM);
    if (RT_SUCCESS(rc))
    {
        /*
         * Register the saved state data unit.
         */
        rc = SSMR3RegisterInternal(pVM, "pdm", 1, PDM_SAVED_STATE_VERSION, 128,
                                   NULL, pdmR3LiveExec, NULL,
                                   NULL, pdmR3SaveExec, NULL,
                                   pdmR3LoadPrep, pdmR3LoadExec, NULL);
        if (RT_SUCCESS(rc))
        {
            LogFlow(("PDM: Successfully initialized\n"));
            return rc;
        }
    }

    /*
     * Cleanup and return failure.
     */
    PDMR3Term(pVM);
    LogFlow(("PDMR3Init: returns %Rrc\n", rc));
    return rc;
}


/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * @param   pVM         VM handle.
 * @param   offDelta    Relocation delta relative to old location.
 * @remark  The loader subcomponent is relocated by PDMR3LdrRelocate() very
 *          early in the relocation phase.
 */
VMMR3DECL(void) PDMR3Relocate(PVM pVM, RTGCINTPTR offDelta)
{
    LogFlow(("PDMR3Relocate\n"));

    /*
     * Queues.
     */
    pdmR3QueueRelocate(pVM, offDelta);
    pVM->pdm.s.pDevHlpQueueRC = PDMQueueRCPtr(pVM->pdm.s.pDevHlpQueueR3);

    /*
     * Critical sections.
     */
    pdmR3CritSectRelocate(pVM);

    /*
     * The registered PIC.
     */
    if (pVM->pdm.s.Pic.pDevInsRC)
    {
        pVM->pdm.s.Pic.pDevInsRC            += offDelta;
        pVM->pdm.s.Pic.pfnSetIrqRC          += offDelta;
        pVM->pdm.s.Pic.pfnGetInterruptRC    += offDelta;
    }

    /*
     * The registered APIC.
     */
    if (pVM->pdm.s.Apic.pDevInsRC)
    {
        pVM->pdm.s.Apic.pDevInsRC           += offDelta;
        pVM->pdm.s.Apic.pfnGetInterruptRC   += offDelta;
        pVM->pdm.s.Apic.pfnSetBaseRC        += offDelta;
        pVM->pdm.s.Apic.pfnGetBaseRC        += offDelta;
        pVM->pdm.s.Apic.pfnSetTPRRC         += offDelta;
        pVM->pdm.s.Apic.pfnGetTPRRC         += offDelta;
        pVM->pdm.s.Apic.pfnBusDeliverRC     += offDelta;
        if (pVM->pdm.s.Apic.pfnLocalInterruptRC)
            pVM->pdm.s.Apic.pfnLocalInterruptRC += offDelta;
        pVM->pdm.s.Apic.pfnWriteMSRRC       += offDelta;
        pVM->pdm.s.Apic.pfnReadMSRRC        += offDelta;
    }

    /*
     * The registered I/O APIC.
     */
    if (pVM->pdm.s.IoApic.pDevInsRC)
    {
        pVM->pdm.s.IoApic.pDevInsRC         += offDelta;
        pVM->pdm.s.IoApic.pfnSetIrqRC       += offDelta;
        if (pVM->pdm.s.IoApic.pfnSendMsiRC)
            pVM->pdm.s.IoApic.pfnSendMsiRC      += offDelta;
    }

    /*
     * The register PCI Buses.
     */
    for (unsigned i = 0; i < RT_ELEMENTS(pVM->pdm.s.aPciBuses); i++)
    {
        if (pVM->pdm.s.aPciBuses[i].pDevInsRC)
        {
            pVM->pdm.s.aPciBuses[i].pDevInsRC   += offDelta;
            pVM->pdm.s.aPciBuses[i].pfnSetIrqRC += offDelta;
        }
    }

    /*
     * Devices & Drivers.
     */
    PCPDMDEVHLPRC pDevHlpRC;
    int rc = PDMR3LdrGetSymbolRC(pVM, NULL, "g_pdmRCDevHlp", &pDevHlpRC);
    AssertReleaseMsgRC(rc, ("rc=%Rrc when resolving g_pdmRCDevHlp\n", rc));

    PCPDMDRVHLPRC pDrvHlpRC;
    rc = PDMR3LdrGetSymbolRC(pVM, NULL, "g_pdmRCDevHlp", &pDrvHlpRC);
    AssertReleaseMsgRC(rc, ("rc=%Rrc when resolving g_pdmRCDevHlp\n", rc));

    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextR3)
    {
        if (pDevIns->pReg->fFlags & PDM_DEVREG_FLAGS_RC)
        {
            pDevIns->pHlpRC             = pDevHlpRC;
            pDevIns->pvInstanceDataRC   = MMHyperR3ToRC(pVM, pDevIns->pvInstanceDataR3);
            if (pDevIns->pCritSectRoR3)
                pDevIns->pCritSectRoRC  = MMHyperR3ToRC(pVM, pDevIns->pCritSectRoR3);
            pDevIns->Internal.s.pVMRC   = pVM->pVMRC;
            if (pDevIns->Internal.s.pPciBusR3)
                pDevIns->Internal.s.pPciBusRC    = MMHyperR3ToRC(pVM, pDevIns->Internal.s.pPciBusR3);
            if (pDevIns->Internal.s.pPciDeviceR3)
                pDevIns->Internal.s.pPciDeviceRC = MMHyperR3ToRC(pVM, pDevIns->Internal.s.pPciDeviceR3);
            if (pDevIns->pReg->pfnRelocate)
            {
                LogFlow(("PDMR3Relocate: Relocating device '%s'/%d\n",
                         pDevIns->pReg->szName, pDevIns->iInstance));
                pDevIns->pReg->pfnRelocate(pDevIns, offDelta);
            }
        }

        for (PPDMLUN pLun = pDevIns->Internal.s.pLunsR3; pLun; pLun = pLun->pNext)
        {
            for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
            {
                if (pDrvIns->pReg->fFlags & PDM_DRVREG_FLAGS_RC)
                {
                    pDrvIns->pHlpRC = pDrvHlpRC;
                    pDrvIns->pvInstanceDataRC = MMHyperR3ToRC(pVM, pDrvIns->pvInstanceDataR3);
                    pDrvIns->Internal.s.pVMRC = pVM->pVMRC;
                    if (pDrvIns->pReg->pfnRelocate)
                    {
                        LogFlow(("PDMR3Relocate: Relocating driver '%s'/%u attached to '%s'/%d/%u\n",
                                 pDrvIns->pReg->szName, pDrvIns->iInstance,
                                 pDevIns->pReg->szName, pDevIns->iInstance, pLun->iLun));
                        pDrvIns->pReg->pfnRelocate(pDrvIns, offDelta);
                    }
                }
            }
        }

    }
}


/**
 * Worker for pdmR3Term that terminates a LUN chain.
 *
 * @param   pVM         Pointer to the shared VM structure.
 * @param   pLun        The head of the chain.
 * @param   pszDevice   The name of the device (for logging).
 * @param   iInstance   The device instance number (for logging).
 */
static void pdmR3TermLuns(PVM pVM, PPDMLUN pLun, const char *pszDevice, unsigned iInstance)
{
    for (; pLun; pLun = pLun->pNext)
    {
        /*
         * Destroy them one at a time from the bottom up.
         * (The serial device/drivers depends on this - bad.)
         */
        PPDMDRVINS pDrvIns = pLun->pBottom;
        pLun->pBottom = pLun->pTop = NULL;
        while (pDrvIns)
        {
            PPDMDRVINS pDrvNext = pDrvIns->Internal.s.pUp;

            if (pDrvIns->pReg->pfnDestruct)
            {
                LogFlow(("pdmR3DevTerm: Destroying - driver '%s'/%d on LUN#%d of device '%s'/%d\n",
                         pDrvIns->pReg->szName, pDrvIns->iInstance, pLun->iLun, pszDevice, iInstance));
                pDrvIns->pReg->pfnDestruct(pDrvIns);
            }
            pDrvIns->Internal.s.pDrv->cInstances--;

            TMR3TimerDestroyDriver(pVM, pDrvIns);
            //PDMR3QueueDestroyDriver(pVM, pDrvIns);
            //pdmR3ThreadDestroyDriver(pVM, pDrvIns);
            SSMR3DeregisterDriver(pVM, pDrvIns, NULL, 0);

            pDrvIns = pDrvNext;
        }
    }
}


/**
 * Terminates the PDM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) PDMR3Term(PVM pVM)
{
    LogFlow(("PDMR3Term:\n"));
    AssertMsg(PDMCritSectIsInitialized(&pVM->pdm.s.CritSect), ("bad init order!\n"));

    /*
     * Iterate the device instances and attach drivers, doing
     * relevant destruction processing.
     *
     * N.B. There is no need to mess around freeing memory allocated
     *      from any MM heap since MM will do that in its Term function.
     */
    /* usb ones first. */
    for (PPDMUSBINS pUsbIns = pVM->pdm.s.pUsbInstances; pUsbIns; pUsbIns = pUsbIns->Internal.s.pNext)
    {
        pdmR3TermLuns(pVM, pUsbIns->Internal.s.pLuns, pUsbIns->pReg->szName, pUsbIns->iInstance);

        if (pUsbIns->pReg->pfnDestruct)
        {
            LogFlow(("pdmR3DevTerm: Destroying - device '%s'/%d\n",
                     pUsbIns->pReg->szName, pUsbIns->iInstance));
            pUsbIns->pReg->pfnDestruct(pUsbIns);
        }

        //TMR3TimerDestroyUsb(pVM, pUsbIns);
        //SSMR3DeregisterUsb(pVM, pUsbIns, NULL, 0);
        pdmR3ThreadDestroyUsb(pVM, pUsbIns);
    }

    /* then the 'normal' ones. */
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextR3)
    {
        pdmR3TermLuns(pVM, pDevIns->Internal.s.pLunsR3, pDevIns->pReg->szName, pDevIns->iInstance);

        if (pDevIns->pReg->pfnDestruct)
        {
            LogFlow(("pdmR3DevTerm: Destroying - device '%s'/%d\n",
                     pDevIns->pReg->szName, pDevIns->iInstance));
            pDevIns->pReg->pfnDestruct(pDevIns);
        }

        TMR3TimerDestroyDevice(pVM, pDevIns);
        //SSMR3DeregisterDriver(pVM, pDevIns, NULL, 0);
        pdmR3CritSectDeleteDevice(pVM, pDevIns);
        //pdmR3ThreadDestroyDevice(pVM, pDevIns);
        //PDMR3QueueDestroyDevice(pVM, pDevIns);
        PGMR3PhysMMIO2Deregister(pVM, pDevIns, UINT32_MAX);
    }

    /*
     * Destroy all threads.
     */
    pdmR3ThreadDestroyAll(pVM);

    /*
     * Destroy the block cache.
     */
    pdmR3BlkCacheTerm(pVM);

#ifdef VBOX_WITH_PDM_ASYNC_COMPLETION
    /*
     * Free async completion managers.
     */
    pdmR3AsyncCompletionTerm(pVM);
#endif

    /*
     * Free modules.
     */
    pdmR3LdrTermU(pVM->pUVM);

    /*
     * Destroy the PDM lock.
     */
    PDMR3CritSectDelete(&pVM->pdm.s.CritSect);
    /* The MiscCritSect is deleted by PDMR3CritSectTerm. */

    LogFlow(("PDMR3Term: returns %Rrc\n", VINF_SUCCESS));
    return VINF_SUCCESS;
}


/**
 * Terminates the PDM part of the UVM.
 *
 * This will unload any modules left behind.
 *
 * @param   pUVM        Pointer to the user mode VM structure.
 */
VMMR3DECL(void) PDMR3TermUVM(PUVM pUVM)
{
    /*
     * In the normal cause of events we will now call pdmR3LdrTermU for
     * the second time. In the case of init failure however, this might
     * the first time, which is why we do it.
     */
    pdmR3LdrTermU(pUVM);

    Assert(pUVM->pdm.s.pCritSects == NULL);
    RTCritSectDelete(&pUVM->pdm.s.ListCritSect);
}


/**
 * Bits that are saved in pass 0 and in the final pass.
 *
 * @param   pVM             The VM handle.
 * @param   pSSM            The saved state handle.
 */
static void pdmR3SaveBoth(PVM pVM, PSSMHANDLE pSSM)
{
    /*
     * Save the list of device instances so we can check that they're all still
     * there when we load the state and that nothing new has been added.
     */
    uint32_t i = 0;
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextR3, i++)
    {
        SSMR3PutU32(pSSM, i);
        SSMR3PutStrZ(pSSM, pDevIns->pReg->szName);
        SSMR3PutU32(pSSM, pDevIns->iInstance);
    }
    SSMR3PutU32(pSSM, UINT32_MAX); /* terminator */
}


/**
 * Live save.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pSSM            The saved state handle.
 * @param   uPass           The pass.
 */
static DECLCALLBACK(int) pdmR3LiveExec(PVM pVM, PSSMHANDLE pSSM, uint32_t uPass)
{
    LogFlow(("pdmR3LiveExec:\n"));
    AssertReturn(uPass == 0, VERR_INTERNAL_ERROR_4);
    pdmR3SaveBoth(pVM, pSSM);
    return VINF_SSM_DONT_CALL_AGAIN;
}


/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pSSM            The saved state handle.
 */
static DECLCALLBACK(int) pdmR3SaveExec(PVM pVM, PSSMHANDLE pSSM)
{
    LogFlow(("pdmR3SaveExec:\n"));

    /*
     * Save interrupt and DMA states.
     */
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU pVCpu = &pVM->aCpus[idCpu];
        SSMR3PutU32(pSSM, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_APIC));
        SSMR3PutU32(pSSM, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_PIC));
        SSMR3PutU32(pSSM, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_NMI));
        SSMR3PutU32(pSSM, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_SMI));
    }
    SSMR3PutU32(pSSM, VM_FF_ISSET(pVM, VM_FF_PDM_DMA));

    pdmR3SaveBoth(pVM, pSSM);
    return VINF_SUCCESS;
}


/**
 * Prepare state load operation.
 *
 * This will dispatch pending operations and clear the FFs governed by PDM and its devices.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pSSM        The SSM handle.
 */
static DECLCALLBACK(int) pdmR3LoadPrep(PVM pVM, PSSMHANDLE pSSM)
{
    LogFlow(("pdmR3LoadPrep: %s%s\n",
             VM_FF_ISSET(pVM, VM_FF_PDM_QUEUES)     ? " VM_FF_PDM_QUEUES" : "",
             VM_FF_ISSET(pVM, VM_FF_PDM_DMA)        ? " VM_FF_PDM_DMA" : ""));
#ifdef LOG_ENABLED
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU pVCpu = &pVM->aCpus[idCpu];
        LogFlow(("pdmR3LoadPrep: VCPU %u %s%s\n", idCpu,
                VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_APIC) ? " VMCPU_FF_INTERRUPT_APIC" : "",
                VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_PIC)  ? " VMCPU_FF_INTERRUPT_PIC" : ""));
    }
#endif

    /*
     * In case there is work pending that will raise an interrupt,
     * start a DMA transfer, or release a lock. (unlikely)
     */
    if (VM_FF_ISSET(pVM, VM_FF_PDM_QUEUES))
        PDMR3QueueFlushAll(pVM);

    /* Clear the FFs. */
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU pVCpu = &pVM->aCpus[idCpu];
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_APIC);
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_PIC);
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_NMI);
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_SMI);
    }
    VM_FF_CLEAR(pVM, VM_FF_PDM_DMA);

    return VINF_SUCCESS;
}


/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 * @param   uVersion        Data layout version.
 * @param   uPass           The data pass.
 */
static DECLCALLBACK(int) pdmR3LoadExec(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    int rc;

    LogFlow(("pdmR3LoadExec: uPass=%#x\n", uPass));

    /*
     * Validate version.
     */
    if (    uVersion != PDM_SAVED_STATE_VERSION
        &&  uVersion != PDM_SAVED_STATE_VERSION_PRE_NMI_FF)
    {
        AssertMsgFailed(("Invalid version uVersion=%d!\n", uVersion));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    if (uPass == SSM_PASS_FINAL)
    {
        /*
         * Load the interrupt and DMA states.
         */
        for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
        {
            PVMCPU pVCpu = &pVM->aCpus[idCpu];

            /* APIC interrupt */
            uint32_t fInterruptPending = 0;
            rc = SSMR3GetU32(pSSM, &fInterruptPending);
            if (RT_FAILURE(rc))
                return rc;
            if (fInterruptPending & ~1)
            {
                AssertMsgFailed(("fInterruptPending=%#x (APIC)\n", fInterruptPending));
                return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
            }
            AssertRelease(!VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_APIC));
            if (fInterruptPending)
                VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_APIC);

            /* PIC interrupt */
            fInterruptPending = 0;
            rc = SSMR3GetU32(pSSM, &fInterruptPending);
            if (RT_FAILURE(rc))
                return rc;
            if (fInterruptPending & ~1)
            {
                AssertMsgFailed(("fInterruptPending=%#x (PIC)\n", fInterruptPending));
                return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
            }
            AssertRelease(!VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_PIC));
            if (fInterruptPending)
                VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_PIC);

            if (uVersion > PDM_SAVED_STATE_VERSION_PRE_NMI_FF)
            {
                /* NMI interrupt */
                fInterruptPending = 0;
                rc = SSMR3GetU32(pSSM, &fInterruptPending);
                if (RT_FAILURE(rc))
                    return rc;
                if (fInterruptPending & ~1)
                {
                    AssertMsgFailed(("fInterruptPending=%#x (NMI)\n", fInterruptPending));
                    return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
                }
                AssertRelease(!VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_NMI));
                if (fInterruptPending)
                    VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_NMI);

                /* SMI interrupt */
                fInterruptPending = 0;
                rc = SSMR3GetU32(pSSM, &fInterruptPending);
                if (RT_FAILURE(rc))
                    return rc;
                if (fInterruptPending & ~1)
                {
                    AssertMsgFailed(("fInterruptPending=%#x (SMI)\n", fInterruptPending));
                    return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
                }
                AssertRelease(!VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_SMI));
                if (fInterruptPending)
                    VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_SMI);
            }
        }

        /* DMA pending */
        uint32_t fDMAPending = 0;
        rc = SSMR3GetU32(pSSM, &fDMAPending);
        if (RT_FAILURE(rc))
            return rc;
        if (fDMAPending & ~1)
        {
            AssertMsgFailed(("fDMAPending=%#x\n", fDMAPending));
            return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
        }
        if (fDMAPending)
            VM_FF_SET(pVM, VM_FF_PDM_DMA);
        Log(("pdmR3LoadExec: VM_FF_PDM_DMA=%RTbool\n", VM_FF_ISSET(pVM, VM_FF_PDM_DMA)));
    }

    /*
     * Load the list of devices and verify that they are all there.
     */
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextR3)
        pDevIns->Internal.s.fIntFlags &= ~PDMDEVINSINT_FLAGS_FOUND;

    for (uint32_t i = 0; ; i++)
    {
        /* Get the sequence number / terminator. */
        uint32_t    u32Sep;
        rc = SSMR3GetU32(pSSM, &u32Sep);
        if (RT_FAILURE(rc))
            return rc;
        if (u32Sep == UINT32_MAX)
            break;
        if (u32Sep != i)
            AssertMsgFailedReturn(("Out of sequence. u32Sep=%#x i=%#x\n", u32Sep, i), VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

        /* Get the name and instance number. */
        char szName[RT_SIZEOFMEMB(PDMDEVREG, szName)];
        rc = SSMR3GetStrZ(pSSM, szName, sizeof(szName));
        if (RT_FAILURE(rc))
            return rc;
        uint32_t iInstance;
        rc = SSMR3GetU32(pSSM, &iInstance);
        if (RT_FAILURE(rc))
            return rc;

        /* Try locate it. */
        PPDMDEVINS pDevIns;
        for (pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextR3)
            if (   !strcmp(szName, pDevIns->pReg->szName)
                && pDevIns->iInstance == iInstance)
            {
                AssertLogRelMsgReturn(!(pDevIns->Internal.s.fIntFlags & PDMDEVINSINT_FLAGS_FOUND),
                                      ("%s/#%u\n", pDevIns->pReg->szName, pDevIns->iInstance),
                                      VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
                pDevIns->Internal.s.fIntFlags |= PDMDEVINSINT_FLAGS_FOUND;
                break;
            }
        if (!pDevIns)
        {
            LogRel(("Device '%s'/%d not found in current config\n", szName, iInstance));
            if (SSMR3HandleGetAfter(pSSM) != SSMAFTER_DEBUG_IT)
                return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Device '%s'/%d not found in current config"), szName, iInstance);
        }
    }

    /*
     * Check that no additional devices were configured.
     */
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextR3)
        if (!(pDevIns->Internal.s.fIntFlags & PDMDEVINSINT_FLAGS_FOUND))
        {
            LogRel(("Device '%s'/%d not found in the saved state\n", pDevIns->pReg->szName, pDevIns->iInstance));
            if (SSMR3HandleGetAfter(pSSM) != SSMAFTER_DEBUG_IT)
                return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Device '%s'/%d not found in the saved state"),
                                        pDevIns->pReg->szName, pDevIns->iInstance);
        }

    return VINF_SUCCESS;
}


/**
 * Worker for PDMR3PowerOn that deals with one driver.
 *
 * @param   pDrvIns             The driver instance.
 * @param   pszDevName          The parent device name.
 * @param   iDevInstance        The parent device instance number.
 * @param   iLun                The parent LUN number.
 */
DECLINLINE(int) pdmR3PowerOnDrv(PPDMDRVINS pDrvIns, const char *pszDevName, uint32_t iDevInstance, uint32_t iLun)
{
    Assert(pDrvIns->Internal.s.fVMSuspended);
    if (pDrvIns->pReg->pfnPowerOn)
    {
        LogFlow(("PDMR3PowerOn: Notifying - driver '%s'/%d on LUN#%d of device '%s'/%d\n",
                 pDrvIns->pReg->szName, pDrvIns->iInstance, iLun, pszDevName, iDevInstance));
        int rc = VINF_SUCCESS; pDrvIns->pReg->pfnPowerOn(pDrvIns);
        if (RT_FAILURE(rc))
        {
            LogRel(("PDMR3PowerOn: driver '%s'/%d on LUN#%d of device '%s'/%d -> %Rrc\n",
                    pDrvIns->pReg->szName, pDrvIns->iInstance, iLun, pszDevName, iDevInstance, rc));
            return rc;
        }
    }
    pDrvIns->Internal.s.fVMSuspended = false;
    return VINF_SUCCESS;
}


/**
 * Worker for PDMR3PowerOn that deals with one USB device instance.
 *
 * @returns VBox status code.
 * @param   pUsbIns             The USB device instance.
 */
DECLINLINE(int) pdmR3PowerOnUsb(PPDMUSBINS pUsbIns)
{
    Assert(pUsbIns->Internal.s.fVMSuspended);
    if (pUsbIns->pReg->pfnVMPowerOn)
    {
        LogFlow(("PDMR3PowerOn: Notifying - device '%s'/%d\n", pUsbIns->pReg->szName, pUsbIns->iInstance));
        int rc = VINF_SUCCESS; pUsbIns->pReg->pfnVMPowerOn(pUsbIns);
        if (RT_FAILURE(rc))
        {
            LogRel(("PDMR3PowerOn: device '%s'/%d -> %Rrc\n", pUsbIns->pReg->szName, pUsbIns->iInstance, rc));
            return rc;
        }
    }
    pUsbIns->Internal.s.fVMSuspended = false;
    return VINF_SUCCESS;
}


/**
 * Worker for PDMR3PowerOn that deals with one device instance.
 *
 * @returns VBox status code.
 * @param   pDevIns             The device instance.
 */
DECLINLINE(int) pdmR3PowerOnDev(PPDMDEVINS pDevIns)
{
    Assert(pDevIns->Internal.s.fIntFlags & PDMDEVINSINT_FLAGS_SUSPENDED);
    if (pDevIns->pReg->pfnPowerOn)
    {
        LogFlow(("PDMR3PowerOn: Notifying - device '%s'/%d\n", pDevIns->pReg->szName, pDevIns->iInstance));
        int rc = VINF_SUCCESS; pDevIns->pReg->pfnPowerOn(pDevIns);
        if (RT_FAILURE(rc))
        {
            LogRel(("PDMR3PowerOn: device '%s'/%d -> %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, rc));
            return rc;
        }
    }
    pDevIns->Internal.s.fIntFlags &= ~PDMDEVINSINT_FLAGS_SUSPENDED;
    return VINF_SUCCESS;
}


/**
 * This function will notify all the devices and their
 * attached drivers about the VM now being powered on.
 *
 * @param   pVM     VM Handle.
 */
VMMR3DECL(void) PDMR3PowerOn(PVM pVM)
{
    LogFlow(("PDMR3PowerOn:\n"));

    /*
     * Iterate thru the device instances and USB device instances,
     * processing the drivers associated with those.
     */
    int rc = VINF_SUCCESS;
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances;  pDevIns && RT_SUCCESS(rc);  pDevIns = pDevIns->Internal.s.pNextR3)
    {
        for (PPDMLUN pLun = pDevIns->Internal.s.pLunsR3;  pLun && RT_SUCCESS(rc);  pLun = pLun->pNext)
            for (PPDMDRVINS pDrvIns = pLun->pTop;  pDrvIns && RT_SUCCESS(rc);  pDrvIns = pDrvIns->Internal.s.pDown)
                rc = pdmR3PowerOnDrv(pDrvIns, pDevIns->pReg->szName, pDevIns->iInstance, pLun->iLun);
        if (RT_SUCCESS(rc))
            rc = pdmR3PowerOnDev(pDevIns);
    }

#ifdef VBOX_WITH_USB
    for (PPDMUSBINS pUsbIns = pVM->pdm.s.pUsbInstances;  pUsbIns && RT_SUCCESS(rc);  pUsbIns = pUsbIns->Internal.s.pNext)
    {
        for (PPDMLUN pLun = pUsbIns->Internal.s.pLuns;  pLun && RT_SUCCESS(rc);  pLun = pLun->pNext)
            for (PPDMDRVINS pDrvIns = pLun->pTop;  pDrvIns && RT_SUCCESS(rc);  pDrvIns = pDrvIns->Internal.s.pDown)
                rc = pdmR3PowerOnDrv(pDrvIns, pUsbIns->pReg->szName, pUsbIns->iInstance, pLun->iLun);
        if (RT_SUCCESS(rc))
            rc = pdmR3PowerOnUsb(pUsbIns);
    }
#endif

#ifdef VBOX_WITH_PDM_ASYNC_COMPLETION
    pdmR3AsyncCompletionResume(pVM);
#endif

    /*
     * Resume all threads.
     */
    if (RT_SUCCESS(rc))
        pdmR3ThreadResumeAll(pVM);

    /*
     * On failure, clean up via PDMR3Suspend.
     */
    if (RT_FAILURE(rc))
        PDMR3Suspend(pVM);

    LogFlow(("PDMR3PowerOn: returns %Rrc\n", rc));
    return /*rc*/;
}


/**
 * Initializes the asynchronous notifi stats structure.
 *
 * @param   pThis               The asynchronous notifification stats.
 * @param   pszOp               The name of the operation.
 */
static void pdmR3NotifyAsyncInit(PPDMNOTIFYASYNCSTATS pThis, const char *pszOp)
{
    pThis->uStartNsTs           = RTTimeNanoTS();
    pThis->cNsElapsedNextLog    = 0;
    pThis->cLoops               = 0;
    pThis->cAsync               = 0;
    pThis->pszOp                = pszOp;
    pThis->offList              = 0;
    pThis->szList[0]            = '\0';
}


/**
 * Begin a new loop, prepares to gather new stats.
 *
 * @param   pThis               The asynchronous notifification stats.
 */
static void pdmR3NotifyAsyncBeginLoop(PPDMNOTIFYASYNCSTATS pThis)
{
    pThis->cLoops++;
    pThis->cAsync       = 0;
    pThis->offList      = 0;
    pThis->szList[0]    = '\0';
}


/**
 * Records a device or USB device with a pending asynchronous notification.
 *
 * @param   pThis               The asynchronous notifification stats.
 * @param   pszName             The name of the thing.
 * @param   iInstance           The instance number.
 */
static void pdmR3NotifyAsyncAdd(PPDMNOTIFYASYNCSTATS pThis, const char *pszName, uint32_t iInstance)
{
    pThis->cAsync++;
    if (pThis->offList < sizeof(pThis->szList) - 4)
        pThis->offList += RTStrPrintf(&pThis->szList[pThis->offList], sizeof(pThis->szList) - pThis->offList,
                                      pThis->offList == 0 ? "%s/%u" : ", %s/%u",
                                      pszName, iInstance);
}


/**
 * Records the asynchronous completition of a reset, suspend or power off.
 *
 * @param   pThis               The asynchronous notifification stats.
 * @param   pszDrvName          The driver name.
 * @param   iDrvInstance        The driver instance number.
 * @param   pszDevName          The device or USB device name.
 * @param   iDevInstance        The device or USB device instance number.
 * @param   iLun                The LUN.
 */
static void pdmR3NotifyAsyncAddDrv(PPDMNOTIFYASYNCSTATS pThis, const char *pszDrvName, uint32_t iDrvInstance,
                                   const char *pszDevName, uint32_t iDevInstance, uint32_t iLun)
{
    pThis->cAsync++;
    if (pThis->offList < sizeof(pThis->szList) - 8)
        pThis->offList += RTStrPrintf(&pThis->szList[pThis->offList], sizeof(pThis->szList) - pThis->offList,
                                      pThis->offList == 0 ? "%s/%u/%u/%s/%u" : ", %s/%u/%u/%s/%u",
                                      pszDevName, iDevInstance, iLun, pszDrvName, iDrvInstance);
}


/**
 * Log the stats.
 *
 * @param   pThis               The asynchronous notifification stats.
 */
static void pdmR3NotifyAsyncLog(PPDMNOTIFYASYNCSTATS pThis)
{
    /*
     * Return if we shouldn't log at this point.
     * We log with an internval increasing from 0 sec to 60 sec.
     */
    if (!pThis->cAsync)
        return;

    uint64_t cNsElapsed = RTTimeNanoTS() - pThis->uStartNsTs;
    if (cNsElapsed < pThis->cNsElapsedNextLog)
        return;

    if (pThis->cNsElapsedNextLog == 0)
        pThis->cNsElapsedNextLog = RT_NS_1SEC;
    else if (pThis->cNsElapsedNextLog >= RT_NS_1MIN / 2)
        pThis->cNsElapsedNextLog = RT_NS_1MIN;
    else
        pThis->cNsElapsedNextLog *= 2;

    /*
     * Do the logging.
     */
    LogRel(("%s: after %5llu ms, %u loops: %u async tasks - %s\n",
            pThis->pszOp, cNsElapsed / RT_NS_1MS, pThis->cLoops, pThis->cAsync, pThis->szList));
}


/**
 * Wait for events and process pending requests.
 *
 * @param   pThis               The asynchronous notifification stats.
 * @param   pVM                 The VM handle.
 */
static void pdmR3NotifyAsyncWaitAndProcessRequests(PPDMNOTIFYASYNCSTATS pThis, PVM pVM)
{
    /** @todo This is utterly nuts and completely unsafe... will get back to it in a
     *        bit I hope... */
    VM_ASSERT_EMT0(pVM);
    int rc = VMR3AsyncPdmNotificationWaitU(&pVM->pUVM->aCpus[0]);
    AssertReleaseMsg(rc == VINF_SUCCESS, ("%Rrc - %s - %s\n", rc, pThis->pszOp, pThis->szList));

    rc = VMR3ReqProcessU(pVM->pUVM, VMCPUID_ANY);
    AssertReleaseMsg(rc == VINF_SUCCESS, ("%Rrc - %s - %s\n", rc, pThis->pszOp, pThis->szList));
    rc = VMR3ReqProcessU(pVM->pUVM, 0/*idDstCpu*/);
    AssertReleaseMsg(rc == VINF_SUCCESS, ("%Rrc - %s - %s\n", rc, pThis->pszOp, pThis->szList));
}


/**
 * Worker for PDMR3Reset that deals with one driver.
 *
 * @param   pDrvIns             The driver instance.
 * @param   pAsync              The structure for recording asynchronous
 *                              notification tasks.
 * @param   pszDevName          The parent device name.
 * @param   iDevInstance        The parent device instance number.
 * @param   iLun                The parent LUN number.
 */
DECLINLINE(bool) pdmR3ResetDrv(PPDMDRVINS pDrvIns, PPDMNOTIFYASYNCSTATS pAsync,
                               const char *pszDevName, uint32_t iDevInstance, uint32_t iLun)
{
    if (!pDrvIns->Internal.s.fVMReset)
    {
        pDrvIns->Internal.s.fVMReset = true;
        if (pDrvIns->pReg->pfnReset)
        {
            if (!pDrvIns->Internal.s.pfnAsyncNotify)
            {
                LogFlow(("PDMR3Reset: Notifying - driver '%s'/%d on LUN#%d of device '%s'/%d\n",
                         pDrvIns->pReg->szName, pDrvIns->iInstance, iLun, pszDevName, iDevInstance));
                pDrvIns->pReg->pfnReset(pDrvIns);
                if (pDrvIns->Internal.s.pfnAsyncNotify)
                    LogFlow(("PDMR3Reset: Async notification started - driver '%s'/%d on LUN#%d of device '%s'/%d\n",
                             pDrvIns->pReg->szName, pDrvIns->iInstance, iLun, pszDevName, iDevInstance));
            }
            else if (pDrvIns->Internal.s.pfnAsyncNotify(pDrvIns))
            {
                pDrvIns->Internal.s.pfnAsyncNotify = false;
                LogFlow(("PDMR3Reset: Async notification completed - driver '%s'/%d on LUN#%d of device '%s'/%d\n",
                         pDrvIns->pReg->szName, pDrvIns->iInstance, iLun, pszDevName, iDevInstance));
            }
            if (pDrvIns->Internal.s.pfnAsyncNotify)
            {
                pDrvIns->Internal.s.fVMReset = false;
                pdmR3NotifyAsyncAddDrv(pAsync, pDrvIns->Internal.s.pDrv->pReg->szName, pDrvIns->iInstance,
                                       pszDevName, iDevInstance, iLun);
                return false;
            }
        }
    }
    return true;
}


/**
 * Worker for PDMR3Reset that deals with one USB device instance.
 *
 * @param   pUsbIns             The USB device instance.
 * @param   pAsync              The structure for recording asynchronous
 *                              notification tasks.
 */
DECLINLINE(void) pdmR3ResetUsb(PPDMUSBINS pUsbIns, PPDMNOTIFYASYNCSTATS pAsync)
{
    if (!pUsbIns->Internal.s.fVMReset)
    {
        pUsbIns->Internal.s.fVMReset = true;
        if (pUsbIns->pReg->pfnVMReset)
        {
            if (!pUsbIns->Internal.s.pfnAsyncNotify)
            {
                LogFlow(("PDMR3Reset: Notifying - device '%s'/%d\n", pUsbIns->pReg->szName, pUsbIns->iInstance));
                pUsbIns->pReg->pfnVMReset(pUsbIns);
                if (pUsbIns->Internal.s.pfnAsyncNotify)
                    LogFlow(("PDMR3Reset: Async notification started - device '%s'/%d\n", pUsbIns->pReg->szName, pUsbIns->iInstance));
            }
            else if (pUsbIns->Internal.s.pfnAsyncNotify(pUsbIns))
            {
                LogFlow(("PDMR3Reset: Async notification completed - device '%s'/%d\n", pUsbIns->pReg->szName, pUsbIns->iInstance));
                pUsbIns->Internal.s.pfnAsyncNotify = NULL;
            }
            if (pUsbIns->Internal.s.pfnAsyncNotify)
            {
                pUsbIns->Internal.s.fVMReset = false;
                pdmR3NotifyAsyncAdd(pAsync, pUsbIns->Internal.s.pUsbDev->pReg->szName, pUsbIns->iInstance);
            }
        }
    }
}


/**
 * Worker for PDMR3Reset that deals with one device instance.
 *
 * @param   pDevIns             The device instance.
 * @param   pAsync              The structure for recording asynchronous
 *                              notification tasks.
 */
DECLINLINE(void) pdmR3ResetDev(PPDMDEVINS pDevIns, PPDMNOTIFYASYNCSTATS pAsync)
{
    if (!(pDevIns->Internal.s.fIntFlags & PDMDEVINSINT_FLAGS_RESET))
    {
        pDevIns->Internal.s.fIntFlags |= PDMDEVINSINT_FLAGS_RESET;
        if (pDevIns->pReg->pfnReset)
        {
            if (!pDevIns->Internal.s.pfnAsyncNotify)
            {
                LogFlow(("PDMR3Reset: Notifying - device '%s'/%d\n", pDevIns->pReg->szName, pDevIns->iInstance));
                pDevIns->pReg->pfnReset(pDevIns);
                if (pDevIns->Internal.s.pfnAsyncNotify)
                    LogFlow(("PDMR3Reset: Async notification started - device '%s'/%d\n", pDevIns->pReg->szName, pDevIns->iInstance));
            }
            else if (pDevIns->Internal.s.pfnAsyncNotify(pDevIns))
            {
                LogFlow(("PDMR3Reset: Async notification completed - device '%s'/%d\n", pDevIns->pReg->szName, pDevIns->iInstance));
                pDevIns->Internal.s.pfnAsyncNotify = NULL;
            }
            if (pDevIns->Internal.s.pfnAsyncNotify)
            {
                pDevIns->Internal.s.fIntFlags &= ~PDMDEVINSINT_FLAGS_RESET;
                pdmR3NotifyAsyncAdd(pAsync, pDevIns->Internal.s.pDevR3->pReg->szName, pDevIns->iInstance);
            }
        }
    }
}


/**
 * Resets a virtual CPU.
 *
 * Used by PDMR3Reset and CPU hot plugging.
 *
 * @param   pVCpu               The virtual CPU handle.
 */
VMMR3DECL(void) PDMR3ResetCpu(PVMCPU pVCpu)
{
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_APIC);
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_PIC);
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_NMI);
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_SMI);
}


/**
 * This function will notify all the devices and their attached drivers about
 * the VM now being reset.
 *
 * @param   pVM     VM Handle.
 */
VMMR3DECL(void) PDMR3Reset(PVM pVM)
{
    LogFlow(("PDMR3Reset:\n"));

    /*
     * Clear all the reset flags.
     */
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextR3)
    {
        pDevIns->Internal.s.fIntFlags &= ~PDMDEVINSINT_FLAGS_RESET;
        for (PPDMLUN pLun = pDevIns->Internal.s.pLunsR3; pLun; pLun = pLun->pNext)
            for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                pDrvIns->Internal.s.fVMReset = false;
    }
#ifdef VBOX_WITH_USB
    for (PPDMUSBINS pUsbIns = pVM->pdm.s.pUsbInstances; pUsbIns; pUsbIns = pUsbIns->Internal.s.pNext)
    {
        pUsbIns->Internal.s.fVMReset = false;
        for (PPDMLUN pLun = pUsbIns->Internal.s.pLuns; pLun; pLun = pLun->pNext)
            for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                pDrvIns->Internal.s.fVMReset = false;
    }
#endif

    /*
     * The outer loop repeats until there are no more async requests.
     */
    PDMNOTIFYASYNCSTATS     Async;
    pdmR3NotifyAsyncInit(&Async, "PDMR3Reset");
    for (;;)
    {
        pdmR3NotifyAsyncBeginLoop(&Async);

        /*
         * Iterate thru the device instances and USB device instances,
         * processing the drivers associated with those.
         */
        for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextR3)
        {
            unsigned const cAsyncStart = Async.cAsync;

            if (Async.cAsync == cAsyncStart)
                for (PPDMLUN pLun = pDevIns->Internal.s.pLunsR3; pLun; pLun = pLun->pNext)
                    for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                        if (!pdmR3ResetDrv(pDrvIns, &Async, pDevIns->pReg->szName, pDevIns->iInstance, pLun->iLun))
                            break;

                        if (Async.cAsync == cAsyncStart)
                pdmR3ResetDev(pDevIns, &Async);
        }

#ifdef VBOX_WITH_USB
        for (PPDMUSBINS pUsbIns = pVM->pdm.s.pUsbInstances; pUsbIns; pUsbIns = pUsbIns->Internal.s.pNext)
        {
            unsigned const cAsyncStart = Async.cAsync;

            for (PPDMLUN pLun = pUsbIns->Internal.s.pLuns; pLun; pLun = pLun->pNext)
                for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                    if (!pdmR3ResetDrv(pDrvIns, &Async, pUsbIns->pReg->szName, pUsbIns->iInstance, pLun->iLun))
                        break;

            if (Async.cAsync == cAsyncStart)
                pdmR3ResetUsb(pUsbIns, &Async);
        }
#endif
        if (!Async.cAsync)
            break;
        pdmR3NotifyAsyncLog(&Async);
        pdmR3NotifyAsyncWaitAndProcessRequests(&Async, pVM);
    }

    /*
     * Clear all pending interrupts and DMA operations.
     */
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
        PDMR3ResetCpu(&pVM->aCpus[idCpu]);
    VM_FF_CLEAR(pVM, VM_FF_PDM_DMA);

    LogFlow(("PDMR3Reset: returns void\n"));
}


/**
 * Worker for PDMR3Suspend that deals with one driver.
 *
 * @param   pDrvIns             The driver instance.
 * @param   pAsync              The structure for recording asynchronous
 *                              notification tasks.
 * @param   pszDevName          The parent device name.
 * @param   iDevInstance        The parent device instance number.
 * @param   iLun                The parent LUN number.
 */
DECLINLINE(bool) pdmR3SuspendDrv(PPDMDRVINS pDrvIns, PPDMNOTIFYASYNCSTATS pAsync,
                                 const char *pszDevName, uint32_t iDevInstance, uint32_t iLun)
{
    if (!pDrvIns->Internal.s.fVMSuspended)
    {
        pDrvIns->Internal.s.fVMSuspended = true;
        if (pDrvIns->pReg->pfnSuspend)
        {
            uint64_t cNsElapsed = RTTimeNanoTS();

            if (!pDrvIns->Internal.s.pfnAsyncNotify)
            {
                LogFlow(("PDMR3Suspend: Notifying - driver '%s'/%d on LUN#%d of device '%s'/%d\n",
                         pDrvIns->pReg->szName, pDrvIns->iInstance, iLun, pszDevName, iDevInstance));
                pDrvIns->pReg->pfnSuspend(pDrvIns);
                if (pDrvIns->Internal.s.pfnAsyncNotify)
                    LogFlow(("PDMR3Suspend: Async notification started - driver '%s'/%d on LUN#%d of device '%s'/%d\n",
                             pDrvIns->pReg->szName, pDrvIns->iInstance, iLun, pszDevName, iDevInstance));
            }
            else if (pDrvIns->Internal.s.pfnAsyncNotify(pDrvIns))
            {
                pDrvIns->Internal.s.pfnAsyncNotify = NULL;
                LogFlow(("PDMR3Suspend: Async notification completed - driver '%s'/%d on LUN#%d of device '%s'/%d\n",
                         pDrvIns->pReg->szName, pDrvIns->iInstance, iLun, pszDevName, iDevInstance));
            }

            cNsElapsed = RTTimeNanoTS() - cNsElapsed;
            if (cNsElapsed >= PDMSUSPEND_WARN_AT_NS)
                LogRel(("PDMR3Suspend: Driver '%s'/%d on LUN#%d of device '%s'/%d took %'llu ns to suspend\n",
                        pDrvIns->pReg->szName, pDrvIns->iInstance, iLun, pszDevName, iDevInstance, cNsElapsed));

            if (pDrvIns->Internal.s.pfnAsyncNotify)
            {
                pDrvIns->Internal.s.fVMSuspended = false;
                pdmR3NotifyAsyncAddDrv(pAsync, pDrvIns->Internal.s.pDrv->pReg->szName, pDrvIns->iInstance, pszDevName, iDevInstance, iLun);
                return false;
            }
        }
    }
    return true;
}


/**
 * Worker for PDMR3Suspend that deals with one USB device instance.
 *
 * @param   pUsbIns             The USB device instance.
 * @param   pAsync              The structure for recording asynchronous
 *                              notification tasks.
 */
DECLINLINE(void) pdmR3SuspendUsb(PPDMUSBINS pUsbIns, PPDMNOTIFYASYNCSTATS pAsync)
{
    if (!pUsbIns->Internal.s.fVMSuspended)
    {
        pUsbIns->Internal.s.fVMSuspended = true;
        if (pUsbIns->pReg->pfnVMSuspend)
        {
            uint64_t cNsElapsed = RTTimeNanoTS();

            if (!pUsbIns->Internal.s.pfnAsyncNotify)
            {
                LogFlow(("PDMR3Suspend: Notifying - USB device '%s'/%d\n", pUsbIns->pReg->szName, pUsbIns->iInstance));
                pUsbIns->pReg->pfnVMSuspend(pUsbIns);
                if (pUsbIns->Internal.s.pfnAsyncNotify)
                    LogFlow(("PDMR3Suspend: Async notification started - USB device '%s'/%d\n", pUsbIns->pReg->szName, pUsbIns->iInstance));
            }
            else if (pUsbIns->Internal.s.pfnAsyncNotify(pUsbIns))
            {
                LogFlow(("PDMR3Suspend: Async notification completed - USB device '%s'/%d\n", pUsbIns->pReg->szName, pUsbIns->iInstance));
                pUsbIns->Internal.s.pfnAsyncNotify = NULL;
            }
            if (pUsbIns->Internal.s.pfnAsyncNotify)
            {
                pUsbIns->Internal.s.fVMSuspended = false;
                pdmR3NotifyAsyncAdd(pAsync, pUsbIns->Internal.s.pUsbDev->pReg->szName, pUsbIns->iInstance);
            }

            cNsElapsed = RTTimeNanoTS() - cNsElapsed;
            if (cNsElapsed >= PDMSUSPEND_WARN_AT_NS)
                LogRel(("PDMR3Suspend: USB device '%s'/%d took %'llu ns to suspend\n",
                        pUsbIns->pReg->szName, pUsbIns->iInstance, cNsElapsed));
        }
    }
}


/**
 * Worker for PDMR3Suspend that deals with one device instance.
 *
 * @param   pDevIns             The device instance.
 * @param   pAsync              The structure for recording asynchronous
 *                              notification tasks.
 */
DECLINLINE(void) pdmR3SuspendDev(PPDMDEVINS pDevIns, PPDMNOTIFYASYNCSTATS pAsync)
{
    if (!(pDevIns->Internal.s.fIntFlags & PDMDEVINSINT_FLAGS_SUSPENDED))
    {
        pDevIns->Internal.s.fIntFlags |= PDMDEVINSINT_FLAGS_SUSPENDED;
        if (pDevIns->pReg->pfnSuspend)
        {
            uint64_t cNsElapsed = RTTimeNanoTS();

            if (!pDevIns->Internal.s.pfnAsyncNotify)
            {
                LogFlow(("PDMR3Suspend: Notifying - device '%s'/%d\n", pDevIns->pReg->szName, pDevIns->iInstance));
                pDevIns->pReg->pfnSuspend(pDevIns);
                if (pDevIns->Internal.s.pfnAsyncNotify)
                    LogFlow(("PDMR3Suspend: Async notification started - device '%s'/%d\n", pDevIns->pReg->szName, pDevIns->iInstance));
            }
            else if (pDevIns->Internal.s.pfnAsyncNotify(pDevIns))
            {
                LogFlow(("PDMR3Suspend: Async notification completed - device '%s'/%d\n", pDevIns->pReg->szName, pDevIns->iInstance));
                pDevIns->Internal.s.pfnAsyncNotify = NULL;
            }
            if (pDevIns->Internal.s.pfnAsyncNotify)
            {
                pDevIns->Internal.s.fIntFlags &= ~PDMDEVINSINT_FLAGS_SUSPENDED;
                pdmR3NotifyAsyncAdd(pAsync, pDevIns->Internal.s.pDevR3->pReg->szName, pDevIns->iInstance);
            }

            cNsElapsed = RTTimeNanoTS() - cNsElapsed;
            if (cNsElapsed >= PDMSUSPEND_WARN_AT_NS)
                LogRel(("PDMR3Suspend: device '%s'/%d took %'llu ns to suspend\n",
                        pDevIns->pReg->szName, pDevIns->iInstance, cNsElapsed));
        }
    }
}


/**
 * This function will notify all the devices and their attached drivers about
 * the VM now being suspended.
 *
 * @param   pVM     The VM Handle.
 * @thread  EMT(0)
 */
VMMR3DECL(void) PDMR3Suspend(PVM pVM)
{
    LogFlow(("PDMR3Suspend:\n"));
    VM_ASSERT_EMT0(pVM);
    uint64_t cNsElapsed = RTTimeNanoTS();

    /*
     * The outer loop repeats until there are no more async requests.
     *
     * Note! We depend on the suspended indicators to be in the desired state
     *       and we do not reset them before starting because this allows
     *       PDMR3PowerOn and PDMR3Resume to use PDMR3Suspend for cleaning up
     *       on failure.
     */
    PDMNOTIFYASYNCSTATS Async;
    pdmR3NotifyAsyncInit(&Async, "PDMR3Suspend");
    for (;;)
    {
        pdmR3NotifyAsyncBeginLoop(&Async);

        /*
         * Iterate thru the device instances and USB device instances,
         * processing the drivers associated with those.
         *
         * The attached drivers are normally processed first.  Some devices
         * (like DevAHCI) though needs to be notified before the drivers so
         * that it doesn't kick off any new requests after the drivers stopped
         * taking any. (DrvVD changes to read-only in this particular case.)
         */
        for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextR3)
        {
            unsigned const cAsyncStart = Async.cAsync;

            if (pDevIns->pReg->fFlags & PDM_DEVREG_FLAGS_FIRST_SUSPEND_NOTIFICATION)
                pdmR3SuspendDev(pDevIns, &Async);

            if (Async.cAsync == cAsyncStart)
                for (PPDMLUN pLun = pDevIns->Internal.s.pLunsR3; pLun; pLun = pLun->pNext)
                    for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                        if (!pdmR3SuspendDrv(pDrvIns, &Async, pDevIns->pReg->szName, pDevIns->iInstance, pLun->iLun))
                            break;

            if (    Async.cAsync == cAsyncStart
                && !(pDevIns->pReg->fFlags & PDM_DEVREG_FLAGS_FIRST_SUSPEND_NOTIFICATION))
                pdmR3SuspendDev(pDevIns, &Async);
        }

#ifdef VBOX_WITH_USB
        for (PPDMUSBINS pUsbIns = pVM->pdm.s.pUsbInstances; pUsbIns; pUsbIns = pUsbIns->Internal.s.pNext)
        {
            unsigned const cAsyncStart = Async.cAsync;

            for (PPDMLUN pLun = pUsbIns->Internal.s.pLuns; pLun; pLun = pLun->pNext)
                for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                    if (!pdmR3SuspendDrv(pDrvIns, &Async, pUsbIns->pReg->szName, pUsbIns->iInstance, pLun->iLun))
                        break;

            if (Async.cAsync == cAsyncStart)
                pdmR3SuspendUsb(pUsbIns, &Async);
        }
#endif
        if (!Async.cAsync)
            break;
        pdmR3NotifyAsyncLog(&Async);
        pdmR3NotifyAsyncWaitAndProcessRequests(&Async, pVM);
    }

    /*
     * Suspend all threads.
     */
    pdmR3ThreadSuspendAll(pVM);

    cNsElapsed = RTTimeNanoTS() - cNsElapsed;
    LogRel(("PDMR3Suspend: %'llu ns run time\n", cNsElapsed));
}


/**
 * Worker for PDMR3Resume that deals with one driver.
 *
 * @param   pDrvIns             The driver instance.
 * @param   pszDevName          The parent device name.
 * @param   iDevInstance        The parent device instance number.
 * @param   iLun                The parent LUN number.
 */
DECLINLINE(int) pdmR3ResumeDrv(PPDMDRVINS pDrvIns, const char *pszDevName, uint32_t iDevInstance, uint32_t iLun)
{
    Assert(pDrvIns->Internal.s.fVMSuspended);
    if (pDrvIns->pReg->pfnResume)
    {
        LogFlow(("PDMR3Resume: Notifying - driver '%s'/%d on LUN#%d of device '%s'/%d\n",
                 pDrvIns->pReg->szName, pDrvIns->iInstance, iLun, pszDevName, iDevInstance));
        int rc = VINF_SUCCESS; pDrvIns->pReg->pfnResume(pDrvIns);
        if (RT_FAILURE(rc))
        {
            LogRel(("PDMR3Resume: driver '%s'/%d on LUN#%d of device '%s'/%d -> %Rrc\n",
                    pDrvIns->pReg->szName, pDrvIns->iInstance, iLun, pszDevName, iDevInstance, rc));
            return rc;
        }
    }
    pDrvIns->Internal.s.fVMSuspended = false;
    return VINF_SUCCESS;
}


/**
 * Worker for PDMR3Resume that deals with one USB device instance.
 *
 * @returns VBox status code.
 * @param   pUsbIns             The USB device instance.
 */
DECLINLINE(int) pdmR3ResumeUsb(PPDMUSBINS pUsbIns)
{
    Assert(pUsbIns->Internal.s.fVMSuspended);
    if (pUsbIns->pReg->pfnVMResume)
    {
        LogFlow(("PDMR3Resume: Notifying - device '%s'/%d\n", pUsbIns->pReg->szName, pUsbIns->iInstance));
        int rc = VINF_SUCCESS; pUsbIns->pReg->pfnVMResume(pUsbIns);
        if (RT_FAILURE(rc))
        {
            LogRel(("PDMR3Resume: device '%s'/%d -> %Rrc\n", pUsbIns->pReg->szName, pUsbIns->iInstance, rc));
            return rc;
        }
    }
    pUsbIns->Internal.s.fVMSuspended = false;
    return VINF_SUCCESS;
}


/**
 * Worker for PDMR3Resume that deals with one device instance.
 *
 * @returns VBox status code.
 * @param   pDevIns             The device instance.
 */
DECLINLINE(int) pdmR3ResumeDev(PPDMDEVINS pDevIns)
{
    Assert(pDevIns->Internal.s.fIntFlags & PDMDEVINSINT_FLAGS_SUSPENDED);
    if (pDevIns->pReg->pfnResume)
    {
        LogFlow(("PDMR3Resume: Notifying - device '%s'/%d\n", pDevIns->pReg->szName, pDevIns->iInstance));
        int rc = VINF_SUCCESS; pDevIns->pReg->pfnResume(pDevIns);
        if (RT_FAILURE(rc))
        {
            LogRel(("PDMR3Resume: device '%s'/%d -> %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, rc));
            return rc;
        }
    }
    pDevIns->Internal.s.fIntFlags &= ~PDMDEVINSINT_FLAGS_SUSPENDED;
    return VINF_SUCCESS;
}


/**
 * This function will notify all the devices and their
 * attached drivers about the VM now being resumed.
 *
 * @param   pVM     VM Handle.
 */
VMMR3DECL(void) PDMR3Resume(PVM pVM)
{
    LogFlow(("PDMR3Resume:\n"));

    /*
     * Iterate thru the device instances and USB device instances,
     * processing the drivers associated with those.
     */
    int rc = VINF_SUCCESS;
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances;  pDevIns && RT_SUCCESS(rc);  pDevIns = pDevIns->Internal.s.pNextR3)
    {
        for (PPDMLUN pLun = pDevIns->Internal.s.pLunsR3;  pLun && RT_SUCCESS(rc);  pLun    = pLun->pNext)
            for (PPDMDRVINS pDrvIns = pLun->pTop;  pDrvIns && RT_SUCCESS(rc);  pDrvIns = pDrvIns->Internal.s.pDown)
                rc = pdmR3ResumeDrv(pDrvIns, pDevIns->pReg->szName, pDevIns->iInstance, pLun->iLun);
        if (RT_SUCCESS(rc))
            rc = pdmR3ResumeDev(pDevIns);
    }

#ifdef VBOX_WITH_USB
    for (PPDMUSBINS pUsbIns = pVM->pdm.s.pUsbInstances;  pUsbIns && RT_SUCCESS(rc);  pUsbIns = pUsbIns->Internal.s.pNext)
    {
        for (PPDMLUN pLun = pUsbIns->Internal.s.pLuns;  pLun && RT_SUCCESS(rc);  pLun = pLun->pNext)
            for (PPDMDRVINS pDrvIns = pLun->pTop;  pDrvIns && RT_SUCCESS(rc);  pDrvIns = pDrvIns->Internal.s.pDown)
                rc = pdmR3ResumeDrv(pDrvIns, pUsbIns->pReg->szName, pUsbIns->iInstance, pLun->iLun);
        if (RT_SUCCESS(rc))
            rc = pdmR3ResumeUsb(pUsbIns);
    }
#endif

    /*
     * Resume all threads.
     */
    if (RT_SUCCESS(rc))
        pdmR3ThreadResumeAll(pVM);

    /*
     * Resume the block cache.
     */
    if (RT_SUCCESS(rc))
        pdmR3BlkCacheResume(pVM);

    /*
     * On failure, clean up via PDMR3Suspend.
     */
    if (RT_FAILURE(rc))
        PDMR3Suspend(pVM);

    LogFlow(("PDMR3Resume: returns %Rrc\n", rc));
    return /*rc*/;
}


/**
 * Worker for PDMR3PowerOff that deals with one driver.
 *
 * @param   pDrvIns             The driver instance.
 * @param   pAsync              The structure for recording asynchronous
 *                              notification tasks.
 * @param   pszDevName          The parent device name.
 * @param   iDevInstance        The parent device instance number.
 * @param   iLun                The parent LUN number.
 */
DECLINLINE(bool) pdmR3PowerOffDrv(PPDMDRVINS pDrvIns, PPDMNOTIFYASYNCSTATS pAsync,
                                  const char *pszDevName, uint32_t iDevInstance, uint32_t iLun)
{
    if (!pDrvIns->Internal.s.fVMSuspended)
    {
        pDrvIns->Internal.s.fVMSuspended = true;
        if (pDrvIns->pReg->pfnPowerOff)
        {
            uint64_t cNsElapsed = RTTimeNanoTS();

            if (!pDrvIns->Internal.s.pfnAsyncNotify)
            {
                LogFlow(("PDMR3PowerOff: Notifying - driver '%s'/%d on LUN#%d of device '%s'/%d\n",
                         pDrvIns->pReg->szName, pDrvIns->iInstance, iLun, pszDevName, iDevInstance));
                pDrvIns->pReg->pfnPowerOff(pDrvIns);
                if (pDrvIns->Internal.s.pfnAsyncNotify)
                    LogFlow(("PDMR3PowerOff: Async notification started - driver '%s'/%d on LUN#%d of device '%s'/%d\n",
                             pDrvIns->pReg->szName, pDrvIns->iInstance, iLun, pszDevName, iDevInstance));
            }
            else if (pDrvIns->Internal.s.pfnAsyncNotify(pDrvIns))
            {
                pDrvIns->Internal.s.pfnAsyncNotify = false;
                LogFlow(("PDMR3PowerOff: Async notification completed - driver '%s'/%d on LUN#%d of device '%s'/%d\n",
                         pDrvIns->pReg->szName, pDrvIns->iInstance, iLun, pszDevName, iDevInstance));
            }

            cNsElapsed = RTTimeNanoTS() - cNsElapsed;
            if (cNsElapsed >= PDMPOWEROFF_WARN_AT_NS)
                LogRel(("PDMR3PowerOff: Driver '%s'/%d on LUN#%d of device '%s'/%d took %'llu ns to power off\n",
                        pDrvIns->pReg->szName, pDrvIns->iInstance, iLun, pszDevName, iDevInstance, cNsElapsed));

            if (pDrvIns->Internal.s.pfnAsyncNotify)
            {
                pDrvIns->Internal.s.fVMSuspended = false;
                pdmR3NotifyAsyncAddDrv(pAsync, pDrvIns->Internal.s.pDrv->pReg->szName, pDrvIns->iInstance,
                                       pszDevName, iDevInstance, iLun);
                return false;
            }
        }
    }
    return true;
}


/**
 * Worker for PDMR3PowerOff that deals with one USB device instance.
 *
 * @param   pUsbIns             The USB device instance.
 * @param   pAsync              The structure for recording asynchronous
 *                              notification tasks.
 */
DECLINLINE(void) pdmR3PowerOffUsb(PPDMUSBINS pUsbIns, PPDMNOTIFYASYNCSTATS pAsync)
{
    if (!pUsbIns->Internal.s.fVMSuspended)
    {
        pUsbIns->Internal.s.fVMSuspended = true;
        if (pUsbIns->pReg->pfnVMPowerOff)
        {
            uint64_t cNsElapsed = RTTimeNanoTS();

            if (!pUsbIns->Internal.s.pfnAsyncNotify)
            {
                LogFlow(("PDMR3PowerOff: Notifying - USB device '%s'/%d\n", pUsbIns->pReg->szName, pUsbIns->iInstance));
                pUsbIns->pReg->pfnVMPowerOff(pUsbIns);
                if (pUsbIns->Internal.s.pfnAsyncNotify)
                    LogFlow(("PDMR3PowerOff: Async notification started - USB device '%s'/%d\n", pUsbIns->pReg->szName, pUsbIns->iInstance));
            }
            else if (pUsbIns->Internal.s.pfnAsyncNotify(pUsbIns))
            {
                LogFlow(("PDMR3PowerOff: Async notification completed - USB device '%s'/%d\n", pUsbIns->pReg->szName, pUsbIns->iInstance));
                pUsbIns->Internal.s.pfnAsyncNotify = NULL;
            }
            if (pUsbIns->Internal.s.pfnAsyncNotify)
            {
                pUsbIns->Internal.s.fVMSuspended = false;
                pdmR3NotifyAsyncAdd(pAsync, pUsbIns->Internal.s.pUsbDev->pReg->szName, pUsbIns->iInstance);
            }

            cNsElapsed = RTTimeNanoTS() - cNsElapsed;
            if (cNsElapsed >= PDMPOWEROFF_WARN_AT_NS)
                LogRel(("PDMR3PowerOff: USB device '%s'/%d took %'llu ns to power off\n",
                        pUsbIns->pReg->szName, pUsbIns->iInstance, cNsElapsed));

        }
    }
}


/**
 * Worker for PDMR3PowerOff that deals with one device instance.
 *
 * @param   pDevIns             The device instance.
 * @param   pAsync              The structure for recording asynchronous
 *                              notification tasks.
 */
DECLINLINE(void) pdmR3PowerOffDev(PPDMDEVINS pDevIns, PPDMNOTIFYASYNCSTATS pAsync)
{
    if (!(pDevIns->Internal.s.fIntFlags & PDMDEVINSINT_FLAGS_SUSPENDED))
    {
        pDevIns->Internal.s.fIntFlags |= PDMDEVINSINT_FLAGS_SUSPENDED;
        if (pDevIns->pReg->pfnPowerOff)
        {
            uint64_t cNsElapsed = RTTimeNanoTS();

            if (!pDevIns->Internal.s.pfnAsyncNotify)
            {
                LogFlow(("PDMR3PowerOff: Notifying - device '%s'/%d\n", pDevIns->pReg->szName, pDevIns->iInstance));
                pDevIns->pReg->pfnPowerOff(pDevIns);
                if (pDevIns->Internal.s.pfnAsyncNotify)
                    LogFlow(("PDMR3PowerOff: Async notification started - device '%s'/%d\n", pDevIns->pReg->szName, pDevIns->iInstance));
            }
            else if (pDevIns->Internal.s.pfnAsyncNotify(pDevIns))
            {
                LogFlow(("PDMR3PowerOff: Async notification completed - device '%s'/%d\n", pDevIns->pReg->szName, pDevIns->iInstance));
                pDevIns->Internal.s.pfnAsyncNotify = NULL;
            }
            if (pDevIns->Internal.s.pfnAsyncNotify)
            {
                pDevIns->Internal.s.fIntFlags &= ~PDMDEVINSINT_FLAGS_SUSPENDED;
                pdmR3NotifyAsyncAdd(pAsync, pDevIns->Internal.s.pDevR3->pReg->szName, pDevIns->iInstance);
            }

            cNsElapsed = RTTimeNanoTS() - cNsElapsed;
            if (cNsElapsed >= PDMPOWEROFF_WARN_AT_NS)
                LogFlow(("PDMR3PowerOff: Device '%s'/%d took %'llu ns to power off\n",
                         pDevIns->pReg->szName, pDevIns->iInstance, cNsElapsed));
        }
    }
}


/**
 * This function will notify all the devices and their
 * attached drivers about the VM being powered off.
 *
 * @param   pVM     VM Handle.
 */
VMMR3DECL(void) PDMR3PowerOff(PVM pVM)
{
    LogFlow(("PDMR3PowerOff:\n"));
    uint64_t cNsElapsed = RTTimeNanoTS();

    /*
     * The outer loop repeats until there are no more async requests.
     */
    PDMNOTIFYASYNCSTATS Async;
    pdmR3NotifyAsyncInit(&Async, "PDMR3PowerOff");
    for (;;)
    {
        pdmR3NotifyAsyncBeginLoop(&Async);

        /*
         * Iterate thru the device instances and USB device instances,
         * processing the drivers associated with those.
         *
         * The attached drivers are normally processed first.  Some devices
         * (like DevAHCI) though needs to be notified before the drivers so
         * that it doesn't kick off any new requests after the drivers stopped
         * taking any. (DrvVD changes to read-only in this particular case.)
         */
        for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextR3)
        {
            unsigned const cAsyncStart = Async.cAsync;

            if (pDevIns->pReg->fFlags & PDM_DEVREG_FLAGS_FIRST_POWEROFF_NOTIFICATION)
                pdmR3PowerOffDev(pDevIns, &Async);

            if (Async.cAsync == cAsyncStart)
                for (PPDMLUN pLun = pDevIns->Internal.s.pLunsR3; pLun; pLun = pLun->pNext)
                    for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                        if (!pdmR3PowerOffDrv(pDrvIns, &Async, pDevIns->pReg->szName, pDevIns->iInstance, pLun->iLun))
                            break;

            if (    Async.cAsync == cAsyncStart
                && !(pDevIns->pReg->fFlags & PDM_DEVREG_FLAGS_FIRST_POWEROFF_NOTIFICATION))
                pdmR3PowerOffDev(pDevIns, &Async);
        }

#ifdef VBOX_WITH_USB
        for (PPDMUSBINS pUsbIns = pVM->pdm.s.pUsbInstances; pUsbIns; pUsbIns = pUsbIns->Internal.s.pNext)
        {
            unsigned const cAsyncStart = Async.cAsync;

            for (PPDMLUN pLun = pUsbIns->Internal.s.pLuns; pLun; pLun = pLun->pNext)
                for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                    if (!pdmR3PowerOffDrv(pDrvIns, &Async, pUsbIns->pReg->szName, pUsbIns->iInstance, pLun->iLun))
                        break;

            if (Async.cAsync == cAsyncStart)
                pdmR3PowerOffUsb(pUsbIns, &Async);
        }
#endif
        if (!Async.cAsync)
            break;
        pdmR3NotifyAsyncLog(&Async);
        pdmR3NotifyAsyncWaitAndProcessRequests(&Async, pVM);
    }

    /*
     * Suspend all threads.
     */
    pdmR3ThreadSuspendAll(pVM);

    cNsElapsed = RTTimeNanoTS() - cNsElapsed;
    LogRel(("PDMR3PowerOff: %'llu ns run time\n", cNsElapsed));
}


/**
 * Queries the base interface of a device instance.
 *
 * The caller can use this to query other interfaces the device implements
 * and use them to talk to the device.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pszDevice       Device name.
 * @param   iInstance       Device instance.
 * @param   ppBase          Where to store the pointer to the base device interface on success.
 * @remark  We're not doing any locking ATM, so don't try call this at times when the
 *          device chain is known to be updated.
 */
VMMR3DECL(int) PDMR3QueryDevice(PVM pVM, const char *pszDevice, unsigned iInstance, PPDMIBASE *ppBase)
{
    LogFlow(("PDMR3DeviceQuery: pszDevice=%p:{%s} iInstance=%u ppBase=%p\n", pszDevice, pszDevice, iInstance, ppBase));

    /*
     * Iterate registered devices looking for the device.
     */
    size_t cchDevice = strlen(pszDevice);
    for (PPDMDEV pDev = pVM->pdm.s.pDevs; pDev; pDev = pDev->pNext)
    {
        if (    pDev->cchName == cchDevice
            &&  !memcmp(pDev->pReg->szName, pszDevice, cchDevice))
        {
            /*
             * Iterate device instances.
             */
            for (PPDMDEVINS pDevIns = pDev->pInstances; pDevIns; pDevIns = pDevIns->Internal.s.pPerDeviceNextR3)
            {
                if (pDevIns->iInstance == iInstance)
                {
                    if (pDevIns->IBase.pfnQueryInterface)
                    {
                        *ppBase = &pDevIns->IBase;
                        LogFlow(("PDMR3DeviceQuery: return VINF_SUCCESS and *ppBase=%p\n", *ppBase));
                        return VINF_SUCCESS;
                    }

                    LogFlow(("PDMR3DeviceQuery: returns VERR_PDM_DEVICE_INSTANCE_NO_IBASE\n"));
                    return VERR_PDM_DEVICE_INSTANCE_NO_IBASE;
                }
            }

            LogFlow(("PDMR3DeviceQuery: returns VERR_PDM_DEVICE_INSTANCE_NOT_FOUND\n"));
            return VERR_PDM_DEVICE_INSTANCE_NOT_FOUND;
        }
    }

    LogFlow(("PDMR3QueryDevice: returns VERR_PDM_DEVICE_NOT_FOUND\n"));
    return VERR_PDM_DEVICE_NOT_FOUND;
}


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
 * @remark  We're not doing any locking ATM, so don't try call this at times when the
 *          device chain is known to be updated.
 */
VMMR3DECL(int) PDMR3QueryDeviceLun(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun, PPDMIBASE *ppBase)
{
    LogFlow(("PDMR3QueryLun: pszDevice=%p:{%s} iInstance=%u iLun=%u ppBase=%p\n",
             pszDevice, pszDevice, iInstance, iLun, ppBase));

    /*
     * Find the LUN.
     */
    PPDMLUN pLun;
    int rc = pdmR3DevFindLun(pVM, pszDevice, iInstance, iLun, &pLun);
    if (RT_SUCCESS(rc))
    {
        *ppBase = pLun->pBase;
        LogFlow(("PDMR3QueryDeviceLun: return VINF_SUCCESS and *ppBase=%p\n", *ppBase));
        return VINF_SUCCESS;
    }
    LogFlow(("PDMR3QueryDeviceLun: returns %Rrc\n", rc));
    return rc;
}


/**
 * Query the interface of the top level driver on a LUN.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pszDevice       Device name.
 * @param   iInstance       Device instance.
 * @param   iLun            The Logical Unit to obtain the interface of.
 * @param   ppBase          Where to store the base interface pointer.
 * @remark  We're not doing any locking ATM, so don't try call this at times when the
 *          device chain is known to be updated.
 */
VMMR3DECL(int) PDMR3QueryLun(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun, PPDMIBASE *ppBase)
{
    LogFlow(("PDMR3QueryLun: pszDevice=%p:{%s} iInstance=%u iLun=%u ppBase=%p\n",
             pszDevice, pszDevice, iInstance, iLun, ppBase));
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);

    /*
     * Find the LUN.
     */
    PPDMLUN pLun;
    int rc = pdmR3DevFindLun(pVM, pszDevice, iInstance, iLun, &pLun);
    if (RT_SUCCESS(rc))
    {
        if (pLun->pTop)
        {
            *ppBase = &pLun->pTop->IBase;
            LogFlow(("PDMR3QueryLun: return %Rrc and *ppBase=%p\n", VINF_SUCCESS, *ppBase));
            return VINF_SUCCESS;
        }
        rc = VERR_PDM_NO_DRIVER_ATTACHED_TO_LUN;
    }
    LogFlow(("PDMR3QueryLun: returns %Rrc\n", rc));
    return rc;
}


/**
 * Query the interface of a named driver on a LUN.
 *
 * If the driver appears more than once in the driver chain, the first instance
 * is returned.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pszDevice       Device name.
 * @param   iInstance       Device instance.
 * @param   iLun            The Logical Unit to obtain the interface of.
 * @param   pszDriver       The driver name.
 * @param   ppBase          Where to store the base interface pointer.
 *
 * @remark  We're not doing any locking ATM, so don't try call this at times when the
 *          device chain is known to be updated.
 */
VMMR3DECL(int) PDMR3QueryDriverOnLun(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun, const char *pszDriver, PPPDMIBASE ppBase)
{
    LogFlow(("PDMR3QueryDriverOnLun: pszDevice=%p:{%s} iInstance=%u iLun=%u pszDriver=%p:{%s} ppBase=%p\n",
             pszDevice, pszDevice, iInstance, iLun, pszDriver, pszDriver, ppBase));

    /*
     * Find the LUN.
     */
    PPDMLUN pLun;
    int rc = pdmR3DevFindLun(pVM, pszDevice, iInstance, iLun, &pLun);
    if (RT_SUCCESS(rc))
    {
        if (pLun->pTop)
        {
            for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                if (!strcmp(pDrvIns->pReg->szName, pszDriver))
                {
                    *ppBase = &pDrvIns->IBase;
                    LogFlow(("PDMR3QueryDriverOnLun: return %Rrc and *ppBase=%p\n", VINF_SUCCESS, *ppBase));
                    return VINF_SUCCESS;

                }
            rc = VERR_PDM_DRIVER_NOT_FOUND;
        }
        else
            rc = VERR_PDM_NO_DRIVER_ATTACHED_TO_LUN;
    }
    LogFlow(("PDMR3QueryDriverOnLun: returns %Rrc\n", rc));
    return rc;
}

/**
 * Executes pending DMA transfers.
 * Forced Action handler.
 *
 * @param   pVM             VM handle.
 */
VMMR3DECL(void) PDMR3DmaRun(PVM pVM)
{
    /* Note! Not really SMP safe; restrict it to VCPU 0. */
    if (VMMGetCpuId(pVM) != 0)
        return;

    if (VM_FF_TESTANDCLEAR(pVM, VM_FF_PDM_DMA))
    {
        if (pVM->pdm.s.pDmac)
        {
            bool fMore = pVM->pdm.s.pDmac->Reg.pfnRun(pVM->pdm.s.pDmac->pDevIns);
            if (fMore)
                VM_FF_SET(pVM, VM_FF_PDM_DMA);
        }
    }
}


/**
 * Service a VMMCALLRING3_PDM_LOCK call.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
VMMR3DECL(int) PDMR3LockCall(PVM pVM)
{
    return PDMR3CritSectEnterEx(&pVM->pdm.s.CritSect, true /* fHostCall */);
}


/**
 * Registers the VMM device heap
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   GCPhys          The physical address.
 * @param   pvHeap          Ring-3 pointer.
 * @param   cbSize          Size of the heap.
 */
VMMR3DECL(int) PDMR3RegisterVMMDevHeap(PVM pVM, RTGCPHYS GCPhys, RTR3PTR pvHeap, unsigned cbSize)
{
    Assert(pVM->pdm.s.pvVMMDevHeap == NULL);

    Log(("PDMR3RegisterVMMDevHeap %RGp %RHv %x\n", GCPhys, pvHeap, cbSize));
    pVM->pdm.s.pvVMMDevHeap     = pvHeap;
    pVM->pdm.s.GCPhysVMMDevHeap = GCPhys;
    pVM->pdm.s.cbVMMDevHeap     = cbSize;
    pVM->pdm.s.cbVMMDevHeapLeft = cbSize;
    return VINF_SUCCESS;
}


/**
 * Unregisters the VMM device heap
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   GCPhys          The physical address.
 */
VMMR3DECL(int) PDMR3UnregisterVMMDevHeap(PVM pVM, RTGCPHYS GCPhys)
{
    Assert(pVM->pdm.s.GCPhysVMMDevHeap == GCPhys);

    Log(("PDMR3UnregisterVMMDevHeap %RGp\n", GCPhys));
    pVM->pdm.s.pvVMMDevHeap     = NULL;
    pVM->pdm.s.GCPhysVMMDevHeap = NIL_RTGCPHYS;
    pVM->pdm.s.cbVMMDevHeap     = 0;
    pVM->pdm.s.cbVMMDevHeapLeft = 0;
    return VINF_SUCCESS;
}


/**
 * Allocates memory from the VMM device heap
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   cbSize          Allocation size.
 * @param   pv              Ring-3 pointer. (out)
 */
VMMR3DECL(int) PDMR3VMMDevHeapAlloc(PVM pVM, unsigned cbSize, RTR3PTR *ppv)
{
#ifdef DEBUG_bird
    if (!cbSize || cbSize > pVM->pdm.s.cbVMMDevHeapLeft)
        return VERR_NO_MEMORY;
#else
    AssertReturn(cbSize && cbSize <= pVM->pdm.s.cbVMMDevHeapLeft, VERR_NO_MEMORY);
#endif

    Log(("PDMR3VMMDevHeapAlloc %x\n", cbSize));

    /** @todo not a real heap as there's currently only one user. */
    *ppv = pVM->pdm.s.pvVMMDevHeap;
    pVM->pdm.s.cbVMMDevHeapLeft = 0;
    return VINF_SUCCESS;
}


/**
 * Frees memory from the VMM device heap
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pv              Ring-3 pointer.
 */
VMMR3DECL(int) PDMR3VMMDevHeapFree(PVM pVM, RTR3PTR pv)
{
    Log(("PDMR3VMMDevHeapFree %RHv\n", pv));

    /** @todo not a real heap as there's currently only one user. */
    pVM->pdm.s.cbVMMDevHeapLeft = pVM->pdm.s.cbVMMDevHeap;
    return VINF_SUCCESS;
}

