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


/** @page   pg_pdm      PDM - The Pluggable Device Manager
 *
 * VBox is designed to be very configurable, i.e. the ability to select
 * virtual devices and configure them uniquely for a VM. For this reason
 * virtual devices are not statically linked with the VMM but loaded and
 * linked at runtime thru the Configuration Manager (CFGM). PDM will use
 * CFGM to enumerate devices which needs loading and instantiation.
 *
 *
 * @section sec_pdm_dev      The Pluggable Device
 *
 * Devices register themselves when the module containing them is loaded.
 * PDM will call an entry point 'VBoxDevicesRegister' when loading a device
 * module. The device module will then use the supplied callback table to
 * check the VMM version and to register its devices. Each device have an
 * unique (for the configured VM) name (string). The name is not only used
 * in PDM but in CFGM - to organize device and device instance settings - and
 * by anyone who wants to do ioctls to the device.
 *
 * When all device modules have been successfully loaded PDM will instantiate
 * those devices which are configured for the VM. Mark that this might mean
 * creating several instances of some devices. When instantiating a device
 * PDM provides device instance memory and a callback table with the VM APIs
 * which the device instance is trusted with.
 *
 * Some devices are trusted devices, most are not. The trusted devices are
 * an integrated part of the VM and can obtain the VM handle from their
 * device instance handles, thus enabling them to call any VM api. Untrusted
 * devices are can only use the callbacks provided during device
 * instantiation.
 *
 * The guest context extention (optional) of a device is initialized as part
 * of the GC init. A device marks in it's registration structure that it have
 * a GC part, in which module and which name the entry point have. PDM will
 * use its loader facilities to load this module into GC and to find the
 * specified entry point.
 *
 * When writing a GC extention the programmer must keep in mind that this
 * code will be relocated, so that using global/static pointer variables
 * won't work.
 *
 *
 * @section sec_pdm_drv      The Pluggable Drivers
 *
 * The VM devices are often accessing host hardware or OS facilities. For
 * most devices these facilities can be abstracted in one or more levels.
 * These abstractions are called drivers.
 *
 * For instance take a DVD/CD drive. This can be connected to a SCSI
 * controller, EIDE controller or SATA controller. The basics of the
 * DVD/CD drive implementation remains the same - eject, insert,
 * read, seek, and such. (For the scsi case, you might wanna speak SCSI
 * directly to, but that can of course be fixed.) So, it makes much sense to
 * have a generic CD/DVD driver which implements this.
 *
 * Then the media 'inserted' into the DVD/CD drive can be a ISO image, or
 * it can be read from a real CD or DVD drive (there are probably other
 * custom formats someone could desire to read or construct too). So, it
 * would make sense to have abstracted interfaces for dealing with this
 * in a generic way so the cdrom unit doesn't have to implement it all.
 * Thus we have created the CDROM/DVD media driver family.
 *
 * So, for this example the IDE controller #1 (i.e. secondary) will have
 * the DVD/CD Driver attached to it's LUN #0 (master). When a media is mounted
 * the DVD/CD Driver will have a ISO, NativeCD, NativeDVD or RAW (media) Driver
 * attached.
 *
 * It is possible to configure many levels of drivers inserting filters, loggers,
 * or whatever you desire into the chain.
 *
 *
 * @subsection sec_pdm_drv_interfaces   Interfaces
 *
 * The pluggable drivers exposes one standard interface (callback table) which
 * is used to construct, destruct, attach, detach, and query other interfaces.
 * A device will query the interfaces required for it's operation during init
 * and hotplug. PDM will query some interfaces during runtime mounting too.
 *
 * ... list interfaces ...
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
static DECLCALLBACK(void) pdmR3PollerTimer(PVM pVM, PTMTIMER pTimer, void *pvUser);



/**
 * Initializes the PDM part of the UVM.
 *
 * This doesn't really do much right now but has to be here for the sake
 * of completeness.
 *
 * @returns VBox status code.
 * @param   pUVM        Pointer to the user mode VM structure.
 */
PDMR3DECL(int) PDMR3InitUVM(PUVM pUVM)
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
PDMR3DECL(int) PDMR3Init(PVM pVM)
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

    int rc = TMR3TimerCreateInternal(pVM, TMCLOCK_VIRTUAL, pdmR3PollerTimer, NULL, "PDM Poller", &pVM->pdm.s.pTimerPollers);
    AssertRC(rc);

    /*
     * Initialize sub compontents.
     */
    rc = pdmR3CritSectInit(pVM);
    if (VBOX_SUCCESS(rc))
    {
#ifdef VBOX_WITH_PDM_LOCK
        rc = PDMR3CritSectInit(pVM, &pVM->pdm.s.CritSect, "PDM");
        if (VBOX_SUCCESS(rc))
#endif
            rc = pdmR3LdrInitU(pVM->pUVM);
        if (VBOX_SUCCESS(rc))
        {
            rc = pdmR3DrvInit(pVM);
            if (VBOX_SUCCESS(rc))
            {
                rc = pdmR3DevInit(pVM);
                if (VBOX_SUCCESS(rc))
                {
#ifdef VBOX_WITH_PDM_ASYNC_COMPLETION
                    rc = pdmR3AsyncCompletionInit(pVM);
                    if (VBOX_SUCCESS(rc))
#endif
                    {
                        /*
                         * Register the saved state data unit.
                         */
                        rc = SSMR3RegisterInternal(pVM, "pdm", 1, PDM_SAVED_STATE_VERSION, 128,
                                                   NULL, pdmR3Save, NULL,
                                                   pdmR3LoadPrep, pdmR3Load, NULL);
                        if (VBOX_SUCCESS(rc))
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
    LogFlow(("PDMR3Init: returns %Vrc\n", rc));
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
PDMR3DECL(void) PDMR3Relocate(PVM pVM, RTGCINTPTR offDelta)
{
    LogFlow(("PDMR3Relocate\n"));

    /*
     * Queues.
     */
    pdmR3QueueRelocate(pVM, offDelta);
    pVM->pdm.s.pDevHlpQueueGC = PDMQueueGCPtr(pVM->pdm.s.pDevHlpQueueHC);

    /*
     * Critical sections.
     */
    pdmR3CritSectRelocate(pVM);

    /*
     * The registered PIC.
     */
    if (pVM->pdm.s.Pic.pDevInsGC)
    {
        pVM->pdm.s.Pic.pDevInsGC            += offDelta;
        pVM->pdm.s.Pic.pfnSetIrqGC          += offDelta;
        pVM->pdm.s.Pic.pfnGetInterruptGC    += offDelta;
    }

    /*
     * The registered APIC.
     */
    if (pVM->pdm.s.Apic.pDevInsGC)
    {
        pVM->pdm.s.Apic.pDevInsGC           += offDelta;
        pVM->pdm.s.Apic.pfnGetInterruptGC   += offDelta;
        pVM->pdm.s.Apic.pfnSetBaseGC        += offDelta;
        pVM->pdm.s.Apic.pfnGetBaseGC        += offDelta;
        pVM->pdm.s.Apic.pfnSetTPRGC         += offDelta;
        pVM->pdm.s.Apic.pfnGetTPRGC         += offDelta;
        pVM->pdm.s.Apic.pfnBusDeliverGC     += offDelta;
    }

    /*
     * The registered I/O APIC.
     */
    if (pVM->pdm.s.IoApic.pDevInsGC)
    {
        pVM->pdm.s.IoApic.pDevInsGC         += offDelta;
        pVM->pdm.s.IoApic.pfnSetIrqGC       += offDelta;
    }

    /*
     * The register PCI Buses.
     */
    for (unsigned i = 0; i < ELEMENTS(pVM->pdm.s.aPciBuses); i++)
    {
        if (pVM->pdm.s.aPciBuses[i].pDevInsGC)
        {
            pVM->pdm.s.aPciBuses[i].pDevInsGC   += offDelta;
            pVM->pdm.s.aPciBuses[i].pfnSetIrqGC += offDelta;
        }
    }

    /*
     * Devices.
     */
    RCPTRTYPE(PCPDMDEVHLPGC) pDevHlpGC;
    int rc = PDMR3GetSymbolGC(pVM, NULL, "g_pdmGCDevHlp", &pDevHlpGC);
    AssertReleaseMsgRC(rc, ("rc=%Vrc when resolving g_pdmGCDevHlp\n", rc));
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextHC)
    {
        if (pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_GC)
        {
            pDevIns->pDevHlpGC = pDevHlpGC;
            pDevIns->pvInstanceDataGC = MMHyperR3ToGC(pVM, pDevIns->pvInstanceDataR3);
            pDevIns->pvInstanceDataR0 = MMHyperR3ToR0(pVM, pDevIns->pvInstanceDataR3);
            pDevIns->Internal.s.pVMGC = pVM->pVMGC;
            if (pDevIns->Internal.s.pPciBusHC)
                pDevIns->Internal.s.pPciBusGC = MMHyperR3ToGC(pVM, pDevIns->Internal.s.pPciBusHC);
            if (pDevIns->Internal.s.pPciDeviceHC)
                pDevIns->Internal.s.pPciDeviceGC = MMHyperR3ToGC(pVM, pDevIns->Internal.s.pPciDeviceHC);
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
PDMR3DECL(int) PDMR3Term(PVM pVM)
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
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextHC)
    {
        pdmR3TermLuns(pVM, pDevIns->Internal.s.pLunsHC, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance);

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

#ifdef VBOX_WITH_PDM_LOCK
    /*
     * Destroy the PDM lock.
     */
    PDMR3CritSectDelete(&pVM->pdm.s.CritSect);
#endif

    LogFlow(("PDMR3Term: returns %Vrc\n", VINF_SUCCESS));
    return VINF_SUCCESS;
}


/**
 * Terminates the PDM part of the UVM.
 *
 * This will unload any modules left behind.
 *
 * @param   pUVM        Pointer to the user mode VM structure.
 */
PDMR3DECL(void) PDMR3TermUVM(PUVM pUVM)
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
    SSMR3PutUInt(pSSM, VM_FF_ISSET(pVM, VM_FF_INTERRUPT_APIC));
    SSMR3PutUInt(pSSM, VM_FF_ISSET(pVM, VM_FF_INTERRUPT_PIC));
    SSMR3PutUInt(pSSM, VM_FF_ISSET(pVM, VM_FF_PDM_DMA));

    /*
     * Save the list of device instances so we can check that
     * they're all still there when we load the state and that
     * nothing new have been added.
     */
    /** @todo We might have to filter out some device classes, like USB attached devices. */
    uint32_t i = 0;
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextHC, i++)
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
             VM_FF_ISSET(pVM, VM_FF_PDM_DMA)        ? " VM_FF_PDM_DMA" : "",
             VM_FF_ISSET(pVM, VM_FF_INTERRUPT_APIC) ? " VM_FF_INTERRUPT_APIC" : "",
             VM_FF_ISSET(pVM, VM_FF_INTERRUPT_PIC)  ? " VM_FF_INTERRUPT_PIC" : ""
             ));

    /*
     * In case there is work pending that will raise an interrupt,
     * start a DMA transfer, or release a lock. (unlikely)
     */
    if (VM_FF_ISSET(pVM, VM_FF_PDM_QUEUES))
        PDMR3QueueFlushAll(pVM);

    /* Clear the FFs. */
    VM_FF_CLEAR(pVM, VM_FF_INTERRUPT_APIC);
    VM_FF_CLEAR(pVM, VM_FF_INTERRUPT_PIC);
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
    LogFlow(("pdmR3Load:\n"));

    /*
     * Validate version.
     */
    if (u32Version != PDM_SAVED_STATE_VERSION)
    {
        Log(("pdmR3Load: Invalid version u32Version=%d!\n", u32Version));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    /*
     * Load the interrupt and DMA states.
     */
    /* APIC interrupt */
    RTUINT fInterruptPending = 0;
    int rc = SSMR3GetUInt(pSSM, &fInterruptPending);
    if (VBOX_FAILURE(rc))
        return rc;
    if (fInterruptPending & ~1)
    {
        AssertMsgFailed(("fInterruptPending=%#x (APIC)\n", fInterruptPending));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
    AssertRelease(!VM_FF_ISSET(pVM, VM_FF_INTERRUPT_APIC));
    if (fInterruptPending)
        VM_FF_SET(pVM, VM_FF_INTERRUPT_APIC);

    /* PIC interrupt */
    fInterruptPending = 0;
    rc = SSMR3GetUInt(pSSM, &fInterruptPending);
    if (VBOX_FAILURE(rc))
        return rc;
    if (fInterruptPending & ~1)
    {
        AssertMsgFailed(("fInterruptPending=%#x (PIC)\n", fInterruptPending));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
    AssertRelease(!VM_FF_ISSET(pVM, VM_FF_INTERRUPT_PIC));
    if (fInterruptPending)
        VM_FF_SET(pVM, VM_FF_INTERRUPT_PIC);

    /* DMA pending */
    RTUINT fDMAPending = 0;
    rc = SSMR3GetUInt(pSSM, &fDMAPending);
    if (VBOX_FAILURE(rc))
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
    for (;;pDevIns = pDevIns->Internal.s.pNextHC, i++)
    {
        /* Get the separator / terminator. */
        uint32_t    u32Sep;
        int rc = SSMR3GetU32(pSSM, &u32Sep);
        if (VBOX_FAILURE(rc))
            return rc;
        if (u32Sep == (uint32_t)~0)
            break;
        if (u32Sep != i)
            AssertMsgFailedReturn(("Out of seqence. u32Sep=%#x i=%#x\n", u32Sep, i), VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

        /* get the name and instance number. */
        char szDeviceName[sizeof(pDevIns->pDevReg->szDeviceName)];
        rc = SSMR3GetStrZ(pSSM, szDeviceName, sizeof(szDeviceName));
        if (VBOX_FAILURE(rc))
            return rc;
        RTUINT iInstance;
        rc = SSMR3GetUInt(pSSM, &iInstance);
        if (VBOX_FAILURE(rc))
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
PDMR3DECL(void) PDMR3PowerOn(PVM pVM)
{
    LogFlow(("PDMR3PowerOn:\n"));

    /*
     * Iterate the device instances.
     * The attached drivers are processed first.
     */
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextHC)
    {
        for (PPDMLUN pLun = pDevIns->Internal.s.pLunsHC; pLun; pLun = pLun->pNext)
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
PDMR3DECL(void) PDMR3Reset(PVM pVM)
{
    LogFlow(("PDMR3Reset:\n"));

    /*
     * Clear all pending interrupts and DMA operations.
     */
    VM_FF_CLEAR(pVM, VM_FF_INTERRUPT_APIC);
    VM_FF_CLEAR(pVM, VM_FF_INTERRUPT_PIC);
    VM_FF_CLEAR(pVM, VM_FF_PDM_DMA);

    /*
     * Iterate the device instances.
     * The attached drivers are processed first.
     */
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextHC)
    {
        for (PPDMLUN pLun = pDevIns->Internal.s.pLunsHC; pLun; pLun = pLun->pNext)
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
PDMR3DECL(void) PDMR3Suspend(PVM pVM)
{
    LogFlow(("PDMR3Suspend:\n"));

    /*
     * Iterate the device instances.
     * The attached drivers are processed first.
     */
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextHC)
    {
        for (PPDMLUN pLun = pDevIns->Internal.s.pLunsHC; pLun; pLun = pLun->pNext)
            for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                if (pDrvIns->pDrvReg->pfnSuspend)
                {
                    LogFlow(("PDMR3Suspend: Notifying - driver '%s'/%d on LUN#%d of device '%s'/%d\n",
                             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, pLun->iLun, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
                    pDrvIns->pDrvReg->pfnSuspend(pDrvIns);
                }

        if (pDevIns->pDevReg->pfnSuspend)
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
PDMR3DECL(void) PDMR3Resume(PVM pVM)
{
    LogFlow(("PDMR3Resume:\n"));

    /*
     * Iterate the device instances.
     * The attached drivers are processed first.
     */
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextHC)
    {
        for (PPDMLUN pLun = pDevIns->Internal.s.pLunsHC; pLun; pLun = pLun->pNext)
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
PDMR3DECL(void) PDMR3PowerOff(PVM pVM)
{
    LogFlow(("PDMR3PowerOff:\n"));

    /*
     * Iterate the device instances.
     * The attached drivers are processed first.
     */
    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextHC)
    {
        for (PPDMLUN pLun = pDevIns->Internal.s.pLunsHC; pLun; pLun = pLun->pNext)
            for (PPDMDRVINS pDrvIns = pLun->pTop; pDrvIns; pDrvIns = pDrvIns->Internal.s.pDown)
                if (pDrvIns->pDrvReg->pfnPowerOff)
                {
                    LogFlow(("PDMR3PowerOff: Notifying - driver '%s'/%d on LUN#%d of device '%s'/%d\n",
                             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, pLun->iLun, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
                    pDrvIns->pDrvReg->pfnPowerOff(pDrvIns);
                }

        if (pDevIns->pDevReg->pfnPowerOff)
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
PDMR3DECL(int) PDMR3QueryDevice(PVM pVM, const char *pszDevice, unsigned iInstance, PPDMIBASE *ppBase)
{
    LogFlow(("PDMR3DeviceQuery: pszDevice=%p:{%s} iInstance=%u ppBase=%p\n", pszDevice, pszDevice, iInstance, ppBase));

    /*
     * Iterate registered devices looking for the device.
     */
    RTUINT cchDevice = strlen(pszDevice);
    for (PPDMDEV pDev = pVM->pdm.s.pDevs; pDev; pDev = pDev->pNext)
    {
        if (    pDev->cchName == cchDevice
            &&  !memcmp(pDev->pDevReg->szDeviceName, pszDevice, cchDevice))
        {
            /*
             * Iterate device instances.
             */
            for (PPDMDEVINS pDevIns = pDev->pInstances; pDevIns; pDevIns = pDevIns->Internal.s.pPerDeviceNextHC)
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
PDMR3DECL(int) PDMR3QueryDeviceLun(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun, PPDMIBASE *ppBase)
{
    LogFlow(("PDMR3QueryLun: pszDevice=%p:{%s} iInstance=%u iLun=%u ppBase=%p\n",
             pszDevice, pszDevice, iInstance, iLun, ppBase));

    /*
     * Find the LUN.
     */
    PPDMLUN pLun;
    int rc = pdmR3DevFindLun(pVM, pszDevice, iInstance, iLun, &pLun);
    if (VBOX_SUCCESS(rc))
    {
        *ppBase = pLun->pBase;
        LogFlow(("PDMR3QueryDeviceLun: return VINF_SUCCESS and *ppBase=%p\n", *ppBase));
        return VINF_SUCCESS;
    }
    LogFlow(("PDMR3QueryDeviceLun: returns %Vrc\n", rc));
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
PDMR3DECL(int) PDMR3QueryLun(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun, PPDMIBASE *ppBase)
{
    LogFlow(("PDMR3QueryLun: pszDevice=%p:{%s} iInstance=%u iLun=%u ppBase=%p\n",
             pszDevice, pszDevice, iInstance, iLun, ppBase));

    /*
     * Find the LUN.
     */
    PPDMLUN pLun;
    int rc = pdmR3DevFindLun(pVM, pszDevice, iInstance, iLun, &pLun);
    if (VBOX_SUCCESS(rc))
    {
        if (pLun->pTop)
        {
            *ppBase = &pLun->pTop->IBase;
            LogFlow(("PDMR3QueryLun: return %Vrc and *ppBase=%p\n", VINF_SUCCESS, *ppBase));
            return VINF_SUCCESS;
        }
        rc = VERR_PDM_NO_DRIVER_ATTACHED_TO_LUN;
    }
    LogFlow(("PDMR3QueryLun: returns %Vrc\n", rc));
    return rc;
}

/**
 * Executes pending DMA transfers.
 * Forced Action handler.
 *
 * @param   pVM             VM handle.
 */
PDMR3DECL(void) PDMR3DmaRun(PVM pVM)
{
    VM_FF_CLEAR(pVM, VM_FF_PDM_DMA);
    if (pVM->pdm.s.pDmac)
    {
        bool fMore = pVM->pdm.s.pDmac->Reg.pfnRun(pVM->pdm.s.pDmac->pDevIns);
        if (fMore)
            VM_FF_SET(pVM, VM_FF_PDM_DMA);
    }
}


/**
 * Call polling function.
 *
 * @param   pVM             VM handle.
 */
PDMR3DECL(void) PDMR3Poll(PVM pVM)
{
    /* This is temporary hack and shall be removed ASAP! */
    unsigned iPoller = pVM->pdm.s.cPollers;
    if (iPoller)
    {
        while (iPoller-- > 0)
            pVM->pdm.s.apfnPollers[iPoller](pVM->pdm.s.aDrvInsPollers[iPoller]);
        TMTimerSetMillies(pVM->pdm.s.pTimerPollers, 3);
    }
}


/**
 * Internal timer callback function.
 *
 * @param   pVM             The VM.
 * @param   pTimer          The timer handle.
 * @param   pvUser          User argument specified upon timer creation.
 */
static DECLCALLBACK(void) pdmR3PollerTimer(PVM pVM, PTMTIMER pTimer, void *pvUser)
{
    PDMR3Poll(pVM);
}


/**
 * Service a VMMCALLHOST_PDM_LOCK call.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
PDMR3DECL(int) PDMR3LockCall(PVM pVM)
{
#ifdef VBOX_WITH_PDM_LOCK
    return PDMR3CritSectEnterEx(&pVM->pdm.s.CritSect, true /* fHostCall */);
#else
    return VINF_SUCCESS;
#endif
}

