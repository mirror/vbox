/* $Id$ */
/** @file
 * PDM - Pluggable Device Manager.
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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
 * create a binary interface that can be supported accross releases and to
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
 * The PCI device repeates ths pfnGetRCHelpers call in it's relocation method
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
 * operation during init and hotplug.  PDM may query some interfaces during
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
 * As mentioned earlier, PDM is the location of any usful constrcts that doesn't
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
 * asynchronous I/O.  For reasons of performance and complexcity this does not
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
 * same name.  It adds the posibility to use critical sections in ring-0 and
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
 * ring-3 by EMT, and optinally forcing a delayed or ASAP return to ring-3.  The
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
#include <VBox/pdm.h>
#include <VBox/mm.h>
#include <VBox/pgm.h>
#include <VBox/ssm.h>
#include <VBox/vm.h>
#include <VBox/uvm.h>
#include <VBox/vmm.h>
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
#define PDM_SAVED_STATE_VERSION     3


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) pdmR3Save(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int) pdmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version);
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
    pUVM->pdm.s.pModules = NULL;
    return VINF_SUCCESS;
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

    /*
     * Init the structure.
     */
    pVM->pdm.s.offVM = RT_OFFSETOF(VM, pdm.s);
    pVM->pdm.s.GCPhysVMMDevHeap = NIL_RTGCPHYS;

    /*
     * Initialize sub compontents.
     */
    int rc = pdmR3CritSectInit(pVM);
    if (RT_SUCCESS(rc))
    {
        rc = PDMR3CritSectInit(pVM, &pVM->pdm.s.CritSect, "PDM");
        if (RT_SUCCESS(rc))
            rc = pdmR3LdrInitU(pVM->pUVM);
        if (RT_SUCCESS(rc))
        {
            rc = pdmR3DrvInit(pVM);
            if (RT_SUCCESS(rc))
            {
                rc = pdmR3DevInit(pVM);
                if (RT_SUCCESS(rc))
                {
#ifdef VBOX_WITH_PDM_ASYNC_COMPLETION
                    rc = pdmR3AsyncCompletionInit(pVM);
                    if (RT_SUCCESS(rc))
#endif
                    {
                        /*
                         * Register the saved state data unit.
                         */
                        rc = SSMR3RegisterInternal(pVM, "pdm", 1, PDM_SAVED_STATE_VERSION, 128,
                                                   NULL, pdmR3Save, NULL,
                                                   pdmR3LoadPrep, pdmR3Load, NULL);
                        if (RT_SUCCESS(rc))
                        {
                            LogFlow(("PDM: Successfully initialized\n"));
                            return rc;
                        }

                    }
                }
            }
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
     * Devices.
     */
    PCPDMDEVHLPRC pDevHlpRC;
    int rc = PDMR3LdrGetSymbolRC(pVM, NULL, "g_pdmRCDevHlp", &pDevHlpRC);
    AssertReleaseMsgRC(rc, ("rc=%Rrc when resolving g_pdmRCDevHlp\n", rc));
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextR3)
    {
        if (pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_RC)
        {
            pDevIns->pDevHlpRC = pDevHlpRC;
            pDevIns->pvInstanceDataRC = MMHyperR3ToRC(pVM, pDevIns->pvInstanceDataR3);
            pDevIns->Internal.s.pVMRC = pVM->pVMRC;
            if (pDevIns->Internal.s.pPciBusR3)
                pDevIns->Internal.s.pPciBusRC = MMHyperR3ToRC(pVM, pDevIns->Internal.s.pPciBusR3);
            if (pDevIns->Internal.s.pPciDeviceR3)
                pDevIns->Internal.s.pPciDeviceRC = MMHyperR3ToRC(pVM, pDevIns->Internal.s.pPciDeviceR3);
            if (pDevIns->pDevReg->pfnRelocate)
            {
                LogFlow(("PDMR3Relocate: Relocating device '%s'/%d\n",
                         pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
                pDevIns->pDevReg->pfnRelocate(pDevIns, offDelta);
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

            if (pDrvIns->pDrvReg->pfnDestruct)
            {
                LogFlow(("pdmR3DevTerm: Destroying - driver '%s'/%d on LUN#%d of device '%s'/%d\n",
                         pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, pLun->iLun, pszDevice, iInstance));
                pDrvIns->pDrvReg->pfnDestruct(pDrvIns);
            }

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
    AssertMsg(pVM->pdm.s.offVM, ("bad init order!\n"));

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
        pdmR3TermLuns(pVM, pUsbIns->Internal.s.pLuns, pUsbIns->pUsbReg->szDeviceName, pUsbIns->iInstance);

        if (pUsbIns->pUsbReg->pfnDestruct)
        {
            LogFlow(("pdmR3DevTerm: Destroying - device '%s'/%d\n",
                     pUsbIns->pUsbReg->szDeviceName, pUsbIns->iInstance));
            pUsbIns->pUsbReg->pfnDestruct(pUsbIns);
        }

        //TMR3TimerDestroyUsb(pVM, pUsbIns);
        //SSMR3DeregisterUsb(pVM, pUsbIns, NULL, 0);
        pdmR3ThreadDestroyUsb(pVM, pUsbIns);
    }

    /* then the 'normal' ones. */
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextR3)
    {
        pdmR3TermLuns(pVM, pDevIns->Internal.s.pLunsR3, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance);

        if (pDevIns->pDevReg->pfnDestruct)
        {
            LogFlow(("pdmR3DevTerm: Destroying - device '%s'/%d\n",
                     pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
            pDevIns->pDevReg->pfnDestruct(pDevIns);
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
}





/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) pdmR3Save(PVM pVM, PSSMHANDLE pSSM)
{
    LogFlow(("pdmR3Save:\n"));

    /*
     * Save interrupt and DMA states.
     */
    for (unsigned idCpu=0;idCpu<pVM->cCPUs;idCpu++)
    {
        PVMCPU pVCpu = &pVM->aCpus[idCpu];
        SSMR3PutUInt(pSSM, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_APIC));
        SSMR3PutUInt(pSSM, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_PIC));
    }
    SSMR3PutUInt(pSSM, VM_FF_ISSET(pVM, VM_FF_PDM_DMA));

    /*
     * Save the list of device instances so we can check that
     * they're all still there when we load the state and that
     * nothing new have been added.
     */
    /** @todo We might have to filter out some device classes, like USB attached devices. */
    uint32_t i = 0;
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextR3, i++)
    {
        SSMR3PutU32(pSSM, i);
        SSMR3PutStrZ(pSSM, pDevIns->pDevReg->szDeviceName);
        SSMR3PutU32(pSSM, pDevIns->iInstance);
    }
    return SSMR3PutU32(pSSM, ~0); /* terminator */
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
    LogFlow(("pdmR3LoadPrep: %s%s%s%s\n",
             VM_FF_ISSET(pVM, VM_FF_PDM_QUEUES)     ? " VM_FF_PDM_QUEUES" : "",
             VM_FF_ISSET(pVM, VM_FF_PDM_DMA)        ? " VM_FF_PDM_DMA" : ""
             ));
#ifdef LOG_ENABLED
    for (unsigned idCpu=0;idCpu<pVM->cCPUs;idCpu++)
    {
        PVMCPU pVCpu = &pVM->aCpus[idCpu];
        LogFlow(("pdmR3LoadPrep: VCPU %d %s%s%s%s\n", idCpu,
                VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_APIC) ? " VMCPU_FF_INTERRUPT_APIC" : "",
                VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_PIC)  ? " VMCPU_FF_INTERRUPT_PIC" : ""
                ));
    }
#endif

    /*
     * In case there is work pending that will raise an interrupt,
     * start a DMA transfer, or release a lock. (unlikely)
     */
    if (VM_FF_ISSET(pVM, VM_FF_PDM_QUEUES))
        PDMR3QueueFlushAll(pVM);

    /* Clear the FFs. */
    for (unsigned idCpu=0;idCpu<pVM->cCPUs;idCpu++)
    {
        PVMCPU pVCpu = &pVM->aCpus[idCpu];
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_APIC);
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_PIC);
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
 * @param   u32Version      Data layout version.
 */
static DECLCALLBACK(int) pdmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version)
{
    int rc;

    LogFlow(("pdmR3Load:\n"));

    /*
     * Validate version.
     */
    if (u32Version != PDM_SAVED_STATE_VERSION)
    {
        AssertMsgFailed(("pdmR3Load: Invalid version u32Version=%d!\n", u32Version));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    /*
     * Load the interrupt and DMA states.
     */
    for (unsigned idCpu=0;idCpu<pVM->cCPUs;idCpu++)
    {
        PVMCPU pVCpu = &pVM->aCpus[idCpu];

        /* APIC interrupt */
        RTUINT fInterruptPending = 0;
        rc = SSMR3GetUInt(pSSM, &fInterruptPending);
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
        rc = SSMR3GetUInt(pSSM, &fInterruptPending);
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
    }

    /* DMA pending */
    RTUINT fDMAPending = 0;
    rc = SSMR3GetUInt(pSSM, &fDMAPending);
    if (RT_FAILURE(rc))
        return rc;
    if (fDMAPending & ~1)
    {
        AssertMsgFailed(("fDMAPending=%#x\n", fDMAPending));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
    AssertRelease(!VM_FF_ISSET(pVM, VM_FF_PDM_DMA));
    if (fDMAPending)
        VM_FF_SET(pVM, VM_FF_PDM_DMA);

    /*
     * Load the list of devices and verify that they are all there.
     *
     * We boldly ASSUME that the order is fixed and that it's a good, this
     * makes it way easier to validate...
     */
    uint32_t i = 0;
    PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances;
    for (;;pDevIns = pDevIns->Internal.s.pNextR3, i++)
    {
        /* Get the separator / terminator. */
        uint32_t    u32Sep;
        int rc = SSMR3GetU32(pSSM, &u32Sep);
        if (RT_FAILURE(rc))
            return rc;
        if (u32Sep == (uint32_t)~0)
            break;
        if (u32Sep != i)
            AssertMsgFailedReturn(("Out of seqence. u32Sep=%#x i=%#x\n", u32Sep, i), VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

        /* get the name and instance number. */
        char szDeviceName[sizeof(pDevIns->pDevReg->szDeviceName)];
        rc = SSMR3GetStrZ(pSSM, szDeviceName, sizeof(szDeviceName));
        if (RT_FAILURE(rc))
            return rc;
        RTUINT iInstance;
        rc = SSMR3GetUInt(pSSM, &iInstance);
        if (RT_FAILURE(rc))
            return rc;

        /* compare */
        if (!pDevIns)
        {
            LogRel(("Device '%s'/%d not found in current config\n", szDeviceName, iInstance));
            if (SSMR3HandleGetAfter(pSSM) != SSMAFTER_DEBUG_IT)
                AssertFailedReturn(VERR_SSM_LOAD_CONFIG_MISMATCH);
            break;
        }
        if (    strcmp(szDeviceName, pDevIns->pDevReg->szDeviceName)
            ||  pDevIns->iInstance != iInstance)
        {
            LogRel(("u32Sep=%d loaded '%s'/%d  configured '%s'/%d\n",
                    u32Sep, szDeviceName, iInstance, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
            if (SSMR3HandleGetAfter(pSSM) != SSMAFTER_DEBUG_IT)
                AssertFailedReturn(VERR_SSM_LOAD_CONFIG_MISMATCH);
        }
    }

    /*
     * Too many devices?
     */
    if (pDevIns)
    {
        LogRel(("Device '%s'/%d not found in saved state\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
        if (SSMR3HandleGetAfter(pSSM) != SSMAFTER_DEBUG_IT)
            AssertFailedReturn(VERR_SSM_LOAD_CONFIG_MISMATCH);
    }

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
     * Iterate the device instances.
     * The attached drivers are processed first.
     */
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextR3)
    {
        for (PPDMLUN pLun = pDevIns->Internal.s.pLunsR3; pLun; pLun = pLun->pNext)
            /** @todo Inverse the order here? */
            for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                if (pDrvIns->pDrvReg->pfnPowerOn)
                {
                    LogFlow(("PDMR3PowerOn: Notifying - driver '%s'/%d on LUN#%d of device '%s'/%d\n",
                             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, pLun->iLun, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
                    pDrvIns->pDrvReg->pfnPowerOn(pDrvIns);
                }

        if (pDevIns->pDevReg->pfnPowerOn)
        {
            LogFlow(("PDMR3PowerOn: Notifying - device '%s'/%d\n",
                     pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
            pDevIns->pDevReg->pfnPowerOn(pDevIns);
        }
    }

#ifdef VBOX_WITH_USB
    for (PPDMUSBINS pUsbIns = pVM->pdm.s.pUsbInstances; pUsbIns; pUsbIns = pUsbIns->Internal.s.pNext)
    {
        for (PPDMLUN pLun = pUsbIns->Internal.s.pLuns; pLun; pLun = pLun->pNext)
            for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                if (pDrvIns->pDrvReg->pfnPowerOn)
                {
                    LogFlow(("PDMR3PowerOn: Notifying - driver '%s'/%d on LUN#%d of usb device '%s'/%d\n",
                             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, pLun->iLun, pUsbIns->pUsbReg->szDeviceName, pUsbIns->iInstance));
                    pDrvIns->pDrvReg->pfnPowerOn(pDrvIns);
                }

        if (pUsbIns->pUsbReg->pfnVMPowerOn)
        {
            LogFlow(("PDMR3PowerOn: Notifying - device '%s'/%d\n",
                     pUsbIns->pUsbReg->szDeviceName, pUsbIns->iInstance));
            pUsbIns->pUsbReg->pfnVMPowerOn(pUsbIns);
        }
    }
#endif

    /*
     * Resume all threads.
     */
    pdmR3ThreadResumeAll(pVM);

    LogFlow(("PDMR3PowerOn: returns void\n"));
}




/**
 * This function will notify all the devices and their
 * attached drivers about the VM now being reset.
 *
 * @param   pVM     VM Handle.
 */
VMMR3DECL(void) PDMR3Reset(PVM pVM)
{
    LogFlow(("PDMR3Reset:\n"));

    /*
     * Clear all pending interrupts and DMA operations.
     */
    for (unsigned idCpu=0;idCpu<pVM->cCPUs;idCpu++)
    {
        PVMCPU pVCpu = &pVM->aCpus[idCpu];
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_APIC);
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_PIC);
    }
    VM_FF_CLEAR(pVM, VM_FF_PDM_DMA);

    /*
     * Iterate the device instances.
     * The attached drivers are processed first.
     */
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextR3)
    {
        for (PPDMLUN pLun = pDevIns->Internal.s.pLunsR3; pLun; pLun = pLun->pNext)
            /** @todo Inverse the order here? */
            for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                if (pDrvIns->pDrvReg->pfnReset)
                {
                    LogFlow(("PDMR3Reset: Notifying - driver '%s'/%d on LUN#%d of device '%s'/%d\n",
                             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, pLun->iLun, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
                    pDrvIns->pDrvReg->pfnReset(pDrvIns);
                }

        if (pDevIns->pDevReg->pfnReset)
        {
            LogFlow(("PDMR3Reset: Notifying - device '%s'/%d\n",
                     pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
            pDevIns->pDevReg->pfnReset(pDevIns);
        }
    }

#ifdef VBOX_WITH_USB
    for (PPDMUSBINS pUsbIns = pVM->pdm.s.pUsbInstances; pUsbIns; pUsbIns = pUsbIns->Internal.s.pNext)
    {
        for (PPDMLUN pLun = pUsbIns->Internal.s.pLuns; pLun; pLun = pLun->pNext)
            for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                if (pDrvIns->pDrvReg->pfnReset)
                {
                    LogFlow(("PDMR3Reset: Notifying - driver '%s'/%d on LUN#%d of usb device '%s'/%d\n",
                             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, pLun->iLun, pUsbIns->pUsbReg->szDeviceName, pUsbIns->iInstance));
                    pDrvIns->pDrvReg->pfnReset(pDrvIns);
                }

        if (pUsbIns->pUsbReg->pfnVMReset)
        {
            LogFlow(("PDMR3Reset: Notifying - device '%s'/%d\n",
                     pUsbIns->pUsbReg->szDeviceName, pUsbIns->iInstance));
            pUsbIns->pUsbReg->pfnVMReset(pUsbIns);
        }
    }
#endif

    LogFlow(("PDMR3Reset: returns void\n"));
}


/**
 * This function will notify all the devices and their
 * attached drivers about the VM now being reset.
 *
 * @param   pVM     VM Handle.
 */
VMMR3DECL(void) PDMR3Suspend(PVM pVM)
{
    LogFlow(("PDMR3Suspend:\n"));

    /*
     * Iterate the device instances.
     * The attached drivers are processed first.
     */
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextR3)
    {
        /*
         * Some devices need to be notified first that the VM is suspended to ensure that that there are no pending
         * requests from the guest which are still processed. Calling the drivers before these requests are finished
         * might lead to errors otherwise. One example is the SATA controller which might still have I/O requests
         * pending. But DrvVD sets the files into readonly mode and every request will fail then.
         */
        if (pDevIns->pDevReg->pfnSuspend && (pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_FIRST_SUSPEND_NOTIFICATION))
        {
            LogFlow(("PDMR3Suspend: Notifying - device '%s'/%d\n",
                     pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
            pDevIns->pDevReg->pfnSuspend(pDevIns);
        }

        for (PPDMLUN pLun = pDevIns->Internal.s.pLunsR3; pLun; pLun = pLun->pNext)
            for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                if (pDrvIns->pDrvReg->pfnSuspend)
                {
                    LogFlow(("PDMR3Suspend: Notifying - driver '%s'/%d on LUN#%d of device '%s'/%d\n",
                             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, pLun->iLun, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
                    pDrvIns->pDrvReg->pfnSuspend(pDrvIns);
                }

        /* Don't call the suspend notification again if it was already called. */
        if (pDevIns->pDevReg->pfnSuspend && !(pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_FIRST_SUSPEND_NOTIFICATION))
        {
            LogFlow(("PDMR3Suspend: Notifying - device '%s'/%d\n",
                     pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
            pDevIns->pDevReg->pfnSuspend(pDevIns);
        }
    }

#ifdef VBOX_WITH_USB
    for (PPDMUSBINS pUsbIns = pVM->pdm.s.pUsbInstances; pUsbIns; pUsbIns = pUsbIns->Internal.s.pNext)
    {
        for (PPDMLUN pLun = pUsbIns->Internal.s.pLuns; pLun; pLun = pLun->pNext)
            for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                if (pDrvIns->pDrvReg->pfnSuspend)
                {
                    LogFlow(("PDMR3Suspend: Notifying - driver '%s'/%d on LUN#%d of usb device '%s'/%d\n",
                             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, pLun->iLun, pUsbIns->pUsbReg->szDeviceName, pUsbIns->iInstance));
                    pDrvIns->pDrvReg->pfnSuspend(pDrvIns);
                }

        if (pUsbIns->pUsbReg->pfnVMSuspend)
        {
            LogFlow(("PDMR3Suspend: Notifying - device '%s'/%d\n",
                     pUsbIns->pUsbReg->szDeviceName, pUsbIns->iInstance));
            pUsbIns->pUsbReg->pfnVMSuspend(pUsbIns);
        }
    }
#endif

    /*
     * Suspend all threads.
     */
    pdmR3ThreadSuspendAll(pVM);

    LogFlow(("PDMR3Suspend: returns void\n"));
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
     * Iterate the device instances.
     * The attached drivers are processed first.
     */
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextR3)
    {
        for (PPDMLUN pLun = pDevIns->Internal.s.pLunsR3; pLun; pLun = pLun->pNext)
            for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                if (pDrvIns->pDrvReg->pfnResume)
                {
                    LogFlow(("PDMR3Resume: Notifying - driver '%s'/%d on LUN#%d of device '%s'/%d\n",
                             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, pLun->iLun, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
                    pDrvIns->pDrvReg->pfnResume(pDrvIns);
                }

        if (pDevIns->pDevReg->pfnResume)
        {
            LogFlow(("PDMR3Resume: Notifying - device '%s'/%d\n",
                     pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
            pDevIns->pDevReg->pfnResume(pDevIns);
        }
    }

#ifdef VBOX_WITH_USB
    for (PPDMUSBINS pUsbIns = pVM->pdm.s.pUsbInstances; pUsbIns; pUsbIns = pUsbIns->Internal.s.pNext)
    {
        for (PPDMLUN pLun = pUsbIns->Internal.s.pLuns; pLun; pLun = pLun->pNext)
            for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                if (pDrvIns->pDrvReg->pfnResume)
                {
                    LogFlow(("PDMR3Resume: Notifying - driver '%s'/%d on LUN#%d of usb device '%s'/%d\n",
                             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, pLun->iLun, pUsbIns->pUsbReg->szDeviceName, pUsbIns->iInstance));
                    pDrvIns->pDrvReg->pfnResume(pDrvIns);
                }

        if (pUsbIns->pUsbReg->pfnVMResume)
        {
            LogFlow(("PDMR3Resume: Notifying - device '%s'/%d\n",
                     pUsbIns->pUsbReg->szDeviceName, pUsbIns->iInstance));
            pUsbIns->pUsbReg->pfnVMResume(pUsbIns);
        }
    }
#endif

    /*
     * Resume all threads.
     */
    pdmR3ThreadResumeAll(pVM);

    LogFlow(("PDMR3Resume: returns void\n"));
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

    /*
     * Iterate the device instances.
     * The attached drivers are processed first.
     */
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextR3)
    {

        if (pDevIns->pDevReg->pfnPowerOff && (pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_FIRST_POWEROFF_NOTIFICATION))
        {
            LogFlow(("PDMR3PowerOff: Notifying - device '%s'/%d\n",
                     pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
            pDevIns->pDevReg->pfnPowerOff(pDevIns);
        }

        for (PPDMLUN pLun = pDevIns->Internal.s.pLunsR3; pLun; pLun = pLun->pNext)
            for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                if (pDrvIns->pDrvReg->pfnPowerOff)
                {
                    LogFlow(("PDMR3PowerOff: Notifying - driver '%s'/%d on LUN#%d of device '%s'/%d\n",
                             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, pLun->iLun, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
                    pDrvIns->pDrvReg->pfnPowerOff(pDrvIns);
                }

        if (pDevIns->pDevReg->pfnPowerOff && !(pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_FIRST_POWEROFF_NOTIFICATION))
        {
            LogFlow(("PDMR3PowerOff: Notifying - device '%s'/%d\n",
                     pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
            pDevIns->pDevReg->pfnPowerOff(pDevIns);
        }
    }

#ifdef VBOX_WITH_USB
    for (PPDMUSBINS pUsbIns = pVM->pdm.s.pUsbInstances; pUsbIns; pUsbIns = pUsbIns->Internal.s.pNext)
    {
        for (PPDMLUN pLun = pUsbIns->Internal.s.pLuns; pLun; pLun = pLun->pNext)
            for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                if (pDrvIns->pDrvReg->pfnPowerOff)
                {
                    LogFlow(("PDMR3PowerOff: Notifying - driver '%s'/%d on LUN#%d of usb device '%s'/%d\n",
                             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, pLun->iLun, pUsbIns->pUsbReg->szDeviceName, pUsbIns->iInstance));
                    pDrvIns->pDrvReg->pfnPowerOff(pDrvIns);
                }

        if (pUsbIns->pUsbReg->pfnVMPowerOff)
        {
            LogFlow(("PDMR3PowerOff: Notifying - device '%s'/%d\n",
                     pUsbIns->pUsbReg->szDeviceName, pUsbIns->iInstance));
            pUsbIns->pUsbReg->pfnVMPowerOff(pUsbIns);
        }
    }
#endif

    /*
     * Suspend all threads.
     */
    pdmR3ThreadSuspendAll(pVM);

    LogFlow(("PDMR3PowerOff: returns void\n"));
}


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
            &&  !memcmp(pDev->pDevReg->szDeviceName, pszDevice, cchDevice))
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

    if (VM_FF_TESTANDCLEAR(pVM, VM_FF_PDM_DMA_BIT))
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
 * Service a VMMCALLHOST_PDM_LOCK call.
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

/**
 * Release the PDM lock if owned by the current VCPU
 *
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(void) PDMR3ReleaseOwnedLocks(PVM pVM)
{
    if (PDMCritSectIsOwner(&pVM->pdm.s.CritSect))
        PDMCritSectLeave(&pVM->pdm.s.CritSect);
}
