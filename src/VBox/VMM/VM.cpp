/* $Id$ */
/** @file
 * VM - Virtual Machine
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_VM
#include <VBox/cfgm.h>
#include <VBox/vmm.h>
#include <VBox/gvmm.h>
#include <VBox/mm.h>
#include <VBox/cpum.h>
#include <VBox/selm.h>
#include <VBox/trpm.h>
#include <VBox/dbgf.h>
#include <VBox/pgm.h>
#include <VBox/pdmapi.h>
#include <VBox/pdmcritsect.h>
#include <VBox/em.h>
#include <VBox/rem.h>
#include <VBox/tm.h>
#include <VBox/stam.h>
#include <VBox/patm.h>
#include <VBox/csam.h>
#include <VBox/iom.h>
#include <VBox/ssm.h>
#include <VBox/hwaccm.h>
#include "VMInternal.h"
#include <VBox/vm.h>

#include <VBox/sup.h>
#include <VBox/dbg.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>

#include <stdlib.h> /* getenv */


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * VM destruction callback registration record.
 */
typedef struct VMATDTOR
{
    /** Pointer to the next record in the list. */
    struct VMATDTOR        *pNext;
    /** Pointer to the callback function. */
    PFNVMATDTOR             pfnAtDtor;
    /** The user argument. */
    void                   *pvUser;
} VMATDTOR;
/** Pointer to a VM destruction callback registration record. */
typedef VMATDTOR *PVMATDTOR;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Pointer to the list of VMs. */
static PVM          g_pVMsHead;

/** Pointer to the list of at VM destruction callbacks. */
static PVMATDTOR    g_pVMAtDtorHead;
/** Lock the g_pVMAtDtorHead list. */
#define VM_ATDTOR_LOCK() do { } while (0)
/** Unlock the g_pVMAtDtorHead list. */
#define VM_ATDTOR_UNLOCK() do { } while (0)

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int  vmR3Create(PVM pVM, PFNVMATERROR pfnVMAtError, void *pvUserVM, PFNCFGMCONSTRUCTOR pfnCFGMConstructor, void *pvUserCFGM);
static void vmR3CallVMAtError(PFNVMATERROR pfnVMAtError, void *pvUser, int rc, RT_SRC_POS_DECL, const char *pszError, ...);
static int  vmR3InitRing3(PVM pVM);
static int  vmR3InitRing0(PVM pVM);
static int  vmR3InitGC(PVM pVM);
static int  vmR3InitDoCompleted(PVM pVM, VMINITCOMPLETED enmWhat);
static DECLCALLBACK(int) vmR3PowerOn(PVM pVM);
static DECLCALLBACK(int) vmR3Suspend(PVM pVM);
static DECLCALLBACK(int) vmR3Resume(PVM pVM);
static DECLCALLBACK(int) vmR3Save(PVM pVM, const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser);
static DECLCALLBACK(int) vmR3Load(PVM pVM, const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser);
static DECLCALLBACK(int) vmR3PowerOff(PVM pVM);
static void vmR3AtDtor(PVM pVM);
static int  vmR3AtReset(PVM pVM);
static DECLCALLBACK(int) vmR3Reset(PVM pVM);
static DECLCALLBACK(int) vmR3AtStateRegister(PVM pVM, PFNVMATSTATE pfnAtState, void *pvUser);
static DECLCALLBACK(int) vmR3AtStateDeregister(PVM pVM, PFNVMATSTATE pfnAtState, void *pvUser);
static DECLCALLBACK(int) vmR3AtErrorRegister(PVM pVM, PFNVMATERROR pfnAtError, void *pvUser);
static DECLCALLBACK(int) vmR3AtErrorDeregister(PVM pVM, PFNVMATERROR pfnAtError, void *pvUser);
static DECLCALLBACK(int) vmR3AtRuntimeErrorRegister(PVM pVM, PFNVMATRUNTIMEERROR pfnAtRuntimeError, void *pvUser);
static DECLCALLBACK(int) vmR3AtRuntimeErrorDeregister(PVM pVM, PFNVMATRUNTIMEERROR pfnAtRuntimeError, void *pvUser);


/**
 * Do global VMM init.
 *
 * @returns VBox status code.
 */
VMR3DECL(int)   VMR3GlobalInit(void)
{
    /*
     * Only once.
     */
    static bool fDone = false;
    if (fDone)
        return VINF_SUCCESS;

    /*
     * We're done.
     */
    fDone = true;
    return VINF_SUCCESS;
}



/**
 * Creates a virtual machine by calling the supplied configuration constructor.
 *
 * On successful returned the VM is powered, i.e. VMR3PowerOn() should be
 * called to start the execution.
 *
 * @returns 0 on success.
 * @returns VBox error code on failure.
 * @param   pfnVMAtError        Pointer to callback function for setting VM errors.
 *                              This is called in the EM.
 * @param   pvUserVM            The user argument passed to pfnVMAtError.
 * @param   pfnCFGMConstructor  Pointer to callback function for constructing the VM configuration tree.
 *                              This is called in the EM.
 * @param   pvUserCFGM          The user argument passed to pfnCFGMConstructor.
 * @param   ppVM                Where to store the 'handle' of the created VM.
 */
VMR3DECL(int)   VMR3Create(PFNVMATERROR pfnVMAtError, void *pvUserVM, PFNCFGMCONSTRUCTOR pfnCFGMConstructor, void *pvUserCFGM, PVM *ppVM)
{
    LogFlow(("VMR3Create: pfnVMAtError=%p pvUserVM=%p  pfnCFGMConstructor=%p pvUserCFGM=%p ppVM=%p\n", pfnVMAtError, pvUserVM, pfnCFGMConstructor, pvUserCFGM, ppVM));

    /*
     * Because of the current hackiness of the applications
     * we'll have to initialize global stuff from here.
     * Later the applications will take care of this in a proper way.
     */
    static bool fGlobalInitDone = false;
    if (!fGlobalInitDone)
    {
        int rc = VMR3GlobalInit();
        if (VBOX_FAILURE(rc))
            return rc;
        fGlobalInitDone = true;
    }

    /*
     * Init support library and load the VMMR0.r0 module.
     */
    PSUPDRVSESSION pSession = 0;
    int rc = SUPInit(&pSession, 0);
    if (VBOX_SUCCESS(rc))
    {
        /** @todo This is isn't very nice, it would be preferrable to move the loader bits
         * out of the VM structure and into a ring-3 only thing. There's a big deal of the
         * error path that we now won't unload the VMMR0.r0 module in. This isn't such a
         * big deal right now, but I'll have to get back to this later. (bird) */
        void *pvVMMR0Opaque;
        rc = PDMR3LdrLoadVMMR0(&pvVMMR0Opaque);
        if (RT_SUCCESS(rc))
        {
            /*
             * Request GVMM to create a new VM for us.
             */
            GVMMCREATEVMREQ CreateVMReq;
            CreateVMReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
            CreateVMReq.Hdr.cbReq = sizeof(CreateVMReq);
            CreateVMReq.pSession = pSession;
            CreateVMReq.pVMR0 = NIL_RTR0PTR;
            CreateVMReq.pVMR3 = NULL;
            rc = SUPCallVMMR0Ex(NIL_RTR0PTR, VMMR0_DO_GVMM_CREATE_VM, 0, &CreateVMReq.Hdr);
            if (RT_SUCCESS(rc))
            {
                PVM pVM = CreateVMReq.pVMR3;
                AssertRelease(VALID_PTR(pVM));
                Log(("VMR3Create: Created pVM=%p pVMR0=%p\n", pVM, pVM->pVMR0));
                PDMR3LdrLoadVMMR0Part2(pVM, pvVMMR0Opaque);

                /*
                 * Do basic init of the VM structure.
                 */
                pVM->vm.s.offVM = RT_OFFSETOF(VM, vm.s);
                pVM->vm.s.ppAtResetNext = &pVM->vm.s.pAtReset;
                pVM->vm.s.ppAtStateNext = &pVM->vm.s.pAtState;
                pVM->vm.s.ppAtErrorNext = &pVM->vm.s.pAtError;
                pVM->vm.s.ppAtRuntimeErrorNext = &pVM->vm.s.pAtRuntimeError;
                rc = RTSemEventCreate(&pVM->vm.s.EventSemWait);
                AssertRCReturn(rc, rc);

                /*
                 * Initialize STAM.
                 */
                rc = STAMR3Init(pVM);
                if (VBOX_SUCCESS(rc))
                {
                    /*
                     * Create the EMT thread, it will start up and wait for requests to process.
                     */
                    VMEMULATIONTHREADARGS Args;
                    Args.pVM = pVM;
                    rc = RTThreadCreate(&pVM->ThreadEMT, vmR3EmulationThread, &Args, _1M,
                                        RTTHREADTYPE_EMULATION, RTTHREADFLAGS_WAITABLE, "EMT");
                    if (VBOX_SUCCESS(rc))
                    {
                        /*
                         * Issue a VM Create request and wait for it to complete.
                         */
                        PVMREQ pReq;
                        rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3Create, 5,
                                         pVM, pfnVMAtError, pvUserVM, pfnCFGMConstructor, pvUserCFGM);
                        if (VBOX_SUCCESS(rc))
                        {
                            rc = pReq->iStatus;
                            VMR3ReqFree(pReq);
                            if (VBOX_SUCCESS(rc))
                            {
                                *ppVM = pVM;
                                LogFlow(("VMR3Create: returns VINF_SUCCESS *ppVM=%p\n", pVM));
                                return VINF_SUCCESS;
                            }

                            AssertMsgFailed(("vmR3Create failed rc=%Vrc\n", rc));
                        }
                        else
                            AssertMsgFailed(("VMR3ReqCall failed rc=%Vrc\n", rc));

                        /*
                         * An error occurred during VM creation. Set the error message directly
                         * using the initial callback, as the callback list doesn't exist yet.
                         */
                        const char *pszError;
                        switch (rc)
                        {
                            case VERR_VMX_IN_VMX_ROOT_MODE:
#ifdef RT_OS_LINUX
                                pszError = N_("VirtualBox can't operate in VMX root mode. "
                                              "Please disable the KVM kernel extension, recompile your kernel and reboot");
#else
                                pszError = N_("VirtualBox can't operate in VMX root mode");
#endif
                                break;
                            default:
                                pszError = N_("Unknown error creating VM (%Vrc)");
                                AssertMsgFailed(("Add error message for rc=%d (%Vrc)\n", rc, rc));
                        }
                        vmR3CallVMAtError(pfnVMAtError, pvUserVM, rc, RT_SRC_POS, pszError, rc);

                        /* Forcefully terminate the emulation thread. */
                        VM_FF_SET(pVM, VM_FF_TERMINATE);
                        VMR3NotifyFF(pVM, false);
                        RTThreadWait(pVM->ThreadEMT, 1000, NULL);
                    }

                    int rc2 = STAMR3Term(pVM);
                    AssertRC(rc2);
                }

                /* cleanup the heap. */
                int rc2 = MMR3Term(pVM);
                AssertRC(rc2);

                /* Tell GVMM that it can destroy the VM now. */
                rc2 = SUPCallVMMR0Ex(CreateVMReq.pVMR0, VMMR0_DO_GVMM_DESTROY_VM, 0, NULL);
                AssertRC(rc2);
            }
            else
            {
                PDMR3LdrLoadVMMR0Part2(NULL, pvVMMR0Opaque);
                vmR3CallVMAtError(pfnVMAtError, pvUserVM, rc, RT_SRC_POS, N_("VM creation failed"));
                AssertMsgFailed(("GMMR0CreateVMReq returned %Rrc\n", rc));
            }
        }
        else
        {
            vmR3CallVMAtError(pfnVMAtError, pvUserVM, rc, RT_SRC_POS, N_("Failed to load VMMR0.r0"));
            AssertMsgFailed(("PDMR3LdrLoadVMMR0 returned %Rrc\n", rc));
        }

        /* terminate SUPLib */
        int rc2 = SUPTerm(false);
        AssertRC(rc2);
    }
    else
    {
        /*
         * An error occurred at support library initialization time (before the
         * VM could be created). Set the error message directly using the
         * initial callback, as the callback list doesn't exist yet.
         */
        const char *pszError;
        switch (rc)
        {
            case VERR_VM_DRIVER_LOAD_ERROR:
#ifdef RT_OS_LINUX
                pszError = N_("VirtualBox kernel driver not loaded. The vboxdrv kernel module "
		              "was either not loaded or /dev/vboxdrv is not set up properly. "
			      "Re-setup the kernel module by executing "
			      "'/etc/init.d/vboxdrv setup' as root");
#else
                pszError = N_("VirtualBox kernel driver not loaded.");
#endif
                break;
            case VERR_VM_DRIVER_OPEN_ERROR:
                pszError = N_("VirtualBox kernel driver cannot be opened");
                break;
            case VERR_VM_DRIVER_NOT_ACCESSIBLE:
#ifdef RT_OS_LINUX
                pszError = N_("The VirtualBox kernel driver is not accessible to the current "
		              "user. Make sure that the user has write permissions for "
			      "/dev/vboxdrv by adding them to the vboxusers groups. You "
			      "will need to logout for the change to take effect.");
#else
                pszError = N_("VirtualBox kernel driver not accessible, permission problem");
#endif
                break;
            case VERR_VM_DRIVER_NOT_INSTALLED:
#ifdef RT_OS_LINUX
                pszError = N_("VirtualBox kernel driver not installed. The vboxdrv kernel module "
		              "was either not loaded or /dev/vboxdrv was not created for some "
			      "reason. Re-setup the kernel module by executing "
			      "'/etc/init.d/vboxdrv setup' as root");
#else
                pszError = N_("VirtualBox kernel driver not installed");
#endif
                break;
            case VERR_NO_MEMORY:
                pszError = N_("VirtualBox support library out of memory");
                break;
            case VERR_VERSION_MISMATCH:
            case VERR_VM_DRIVER_VERSION_MISMATCH:
                pszError = N_("The VirtualBox support driver which is running is from a different "
                              "version of VirtualBox.  You can correct this by stopping all "
                              "running instances of VirtualBox and reinstalling the software.");
                break;
            default:
                pszError = N_("Unknown error initializing kernel driver (%Vrc)");
                AssertMsgFailed(("Add error message for rc=%d (%Vrc)\n", rc, rc));
        }
        vmR3CallVMAtError(pfnVMAtError, pvUserVM, rc, RT_SRC_POS, pszError, rc);
    }

    LogFlow(("VMR3Create: returns %Vrc\n", rc));
    return rc;
}


/**
 * Wrapper for getting a correct va_list.
 */
static void vmR3CallVMAtError(PFNVMATERROR pfnVMAtError, void *pvUser, int rc, RT_SRC_POS_DECL, const char *pszError, ...)
{
    if (!pfnVMAtError)
        return;
    va_list va;
    va_start(va, pszError);
    pfnVMAtError(NULL, pvUser, rc, RT_SRC_POS_ARGS, pszError, va);
    va_end(va);
}


/**
 * Initializes the VM.
 */
static int vmR3Create(PVM pVM, PFNVMATERROR pfnVMAtError, void *pvUserVM, PFNCFGMCONSTRUCTOR pfnCFGMConstructor, void *pvUserCFGM)
{
    int rc = VINF_SUCCESS;

    /* Register error callback if specified. */
    if (pfnVMAtError)
        rc = VMR3AtErrorRegister(pVM, pfnVMAtError, pvUserVM);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Init the configuration.
         */
        rc = CFGMR3Init(pVM, pfnCFGMConstructor, pvUserCFGM);
        if (VBOX_SUCCESS(rc))
        {
            /*
             * If executing in fake suplib mode disable RR3 and RR0 in the config.
             */
            const char *psz = getenv("VBOX_SUPLIB_FAKE");
            if (psz && !strcmp(psz, "fake"))
            {
                CFGMR3RemoveValue(CFGMR3GetRoot(pVM), "RawR3Enabled");
                CFGMR3InsertInteger(CFGMR3GetRoot(pVM), "RawR3Enabled", 0);
                CFGMR3RemoveValue(CFGMR3GetRoot(pVM), "RawR0Enabled");
                CFGMR3InsertInteger(CFGMR3GetRoot(pVM), "RawR0Enabled", 0);
            }

            /*
             * Check if the required minimum of resources are available.
             */
            /** @todo Check if the required minimum of resources are available. */
            if (VBOX_SUCCESS(rc))
            {
                /*
                 * Init the Ring-3 components and do a round of relocations with 0 delta.
                 */
                rc = vmR3InitRing3(pVM);
                if (VBOX_SUCCESS(rc))
                {
                    VMR3Relocate(pVM, 0);
                    LogFlow(("Ring-3 init succeeded\n"));

                    /*
                     * Init the Ring-0 components.
                     */
                    rc = vmR3InitRing0(pVM);
                    if (VBOX_SUCCESS(rc))
                    {
                        /* Relocate again, because some switcher fixups depends on R0 init results. */
                        VMR3Relocate(pVM, 0);

                        /*
                         * Init the tcp debugger console if we're building
                         * with debugger support.
                         */
                        void *pvUser = NULL;
                        rc = DBGCTcpCreate(pVM, &pvUser);
                        if (    VBOX_SUCCESS(rc)
                            ||  rc == VERR_NET_ADDRESS_IN_USE)
                        {
                            pVM->vm.s.pvDBGC = pvUser;

                            /*
                             * Init the Guest Context components.
                             */
                            rc = vmR3InitGC(pVM);
                            if (VBOX_SUCCESS(rc))
                            {
                                /*
                                 * Set the state and link into the global list.
                                 */
                                vmR3SetState(pVM, VMSTATE_CREATED);
                                pVM->pNext = g_pVMsHead;
                                g_pVMsHead = pVM;
                                return VINF_SUCCESS;
                            }
                            DBGCTcpTerminate(pVM, pVM->vm.s.pvDBGC);
                            pVM->vm.s.pvDBGC = NULL;
                        }
                        //..
                    }
                    vmR3Destroy(pVM);
                }
                //..
            }

            /* Clean CFGM. */
            int rc2 = CFGMR3Term(pVM);
            AssertRC(rc2);
        }
        //..
    }

    LogFlow(("vmR3Create: returns %Vrc\n", rc));
    return rc;
}



/**
 * Initializes all R3 components of the VM
 */
static int vmR3InitRing3(PVM pVM)
{
    int rc;

    /*
     * Init all R3 components, the order here might be important.
     */
    rc = vmR3SetHaltMethod(pVM, VMHALTMETHOD_DEFAULT);
    AssertRCReturn(rc, rc);

    rc = MMR3Init(pVM);
    if (VBOX_SUCCESS(rc))
    {
        STAM_REG(pVM, &pVM->StatTotalInGC,          STAMTYPE_PROFILE_ADV, "/PROF/VM/InGC",          STAMUNIT_TICKS_PER_CALL,    "Profiling the total time spent in GC.");
        STAM_REG(pVM, &pVM->StatSwitcherToGC,       STAMTYPE_PROFILE_ADV, "/PROF/VM/SwitchToGC",    STAMUNIT_TICKS_PER_CALL,    "Profiling switching to GC.");
        STAM_REG(pVM, &pVM->StatSwitcherToHC,       STAMTYPE_PROFILE_ADV, "/PROF/VM/SwitchToHC",    STAMUNIT_TICKS_PER_CALL,    "Profiling switching to HC.");

        STAM_REL_REG(pVM, &pVM->vm.s.StatHaltYield, STAMTYPE_PROFILE,     "/PROF/VM/Halt/Yield",    STAMUNIT_TICKS_PER_CALL,    "Profiling halted state yielding.");
        STAM_REL_REG(pVM, &pVM->vm.s.StatHaltBlock, STAMTYPE_PROFILE,     "/PROF/VM/Halt/Block",    STAMUNIT_TICKS_PER_CALL,    "Profiling halted state blocking.");
        STAM_REL_REG(pVM, &pVM->vm.s.StatHaltTimers,STAMTYPE_PROFILE,     "/PROF/VM/Halt/Timers",   STAMUNIT_TICKS_PER_CALL,    "Profiling halted state timer tasks.");
        STAM_REL_REG(pVM, &pVM->vm.s.StatHaltPoll,  STAMTYPE_PROFILE,     "/PROF/VM/Halt/Poll",     STAMUNIT_TICKS_PER_CALL,    "Profiling halted state poll tasks.");

        STAM_REG(pVM, &pVM->StatSwitcherSaveRegs,   STAMTYPE_PROFILE_ADV, "/VM/Switcher/ToGC/SaveRegs", STAMUNIT_TICKS_PER_CALL,"Profiling switching to GC.");
        STAM_REG(pVM, &pVM->StatSwitcherSysEnter,   STAMTYPE_PROFILE_ADV, "/VM/Switcher/ToGC/SysEnter", STAMUNIT_TICKS_PER_CALL,"Profiling switching to GC.");
        STAM_REG(pVM, &pVM->StatSwitcherDebug,      STAMTYPE_PROFILE_ADV, "/VM/Switcher/ToGC/Debug",    STAMUNIT_TICKS_PER_CALL,"Profiling switching to GC.");
        STAM_REG(pVM, &pVM->StatSwitcherCR0,        STAMTYPE_PROFILE_ADV, "/VM/Switcher/ToGC/CR0",  STAMUNIT_TICKS_PER_CALL,    "Profiling switching to GC.");
        STAM_REG(pVM, &pVM->StatSwitcherCR4,        STAMTYPE_PROFILE_ADV, "/VM/Switcher/ToGC/CR4",  STAMUNIT_TICKS_PER_CALL,    "Profiling switching to GC.");
        STAM_REG(pVM, &pVM->StatSwitcherLgdt,       STAMTYPE_PROFILE_ADV, "/VM/Switcher/ToGC/Lgdt", STAMUNIT_TICKS_PER_CALL,    "Profiling switching to GC.");
        STAM_REG(pVM, &pVM->StatSwitcherLidt,       STAMTYPE_PROFILE_ADV, "/VM/Switcher/ToGC/Lidt", STAMUNIT_TICKS_PER_CALL,    "Profiling switching to GC.");
        STAM_REG(pVM, &pVM->StatSwitcherLldt,       STAMTYPE_PROFILE_ADV, "/VM/Switcher/ToGC/Lldt", STAMUNIT_TICKS_PER_CALL,    "Profiling switching to GC.");
        STAM_REG(pVM, &pVM->StatSwitcherTSS,        STAMTYPE_PROFILE_ADV, "/VM/Switcher/ToGC/TSS",  STAMUNIT_TICKS_PER_CALL,    "Profiling switching to GC.");
        STAM_REG(pVM, &pVM->StatSwitcherJmpCR3,     STAMTYPE_PROFILE_ADV, "/VM/Switcher/ToGC/JmpCR3",   STAMUNIT_TICKS_PER_CALL,"Profiling switching to GC.");
        STAM_REG(pVM, &pVM->StatSwitcherRstrRegs,   STAMTYPE_PROFILE_ADV, "/VM/Switcher/ToGC/RstrRegs", STAMUNIT_TICKS_PER_CALL,"Profiling switching to GC.");

        STAM_REG(pVM, &pVM->vm.s.StatReqAllocNew,   STAMTYPE_COUNTER,     "/VM/Req/AllocNew",       STAMUNIT_OCCURENCES,        "Number of VMR3ReqAlloc returning a new packet.");
        STAM_REG(pVM, &pVM->vm.s.StatReqAllocRaces, STAMTYPE_COUNTER,     "/VM/Req/AllocRaces",     STAMUNIT_OCCURENCES,        "Number of VMR3ReqAlloc causing races.");
        STAM_REG(pVM, &pVM->vm.s.StatReqAllocRecycled, STAMTYPE_COUNTER,  "/VM/Req/AllocRecycled",  STAMUNIT_OCCURENCES,        "Number of VMR3ReqAlloc returning a recycled packet.");
        STAM_REG(pVM, &pVM->vm.s.StatReqFree,       STAMTYPE_COUNTER,     "/VM/Req/Free",           STAMUNIT_OCCURENCES,        "Number of VMR3ReqFree calls.");
        STAM_REG(pVM, &pVM->vm.s.StatReqFreeOverflow, STAMTYPE_COUNTER,   "/VM/Req/FreeOverflow",   STAMUNIT_OCCURENCES,        "Number of times the request was actually freed.");

        rc = CPUMR3Init(pVM);
        if (VBOX_SUCCESS(rc))
        {
            rc = HWACCMR3Init(pVM);
            if (VBOX_SUCCESS(rc))
            {
                rc = PGMR3Init(pVM);
                if (VBOX_SUCCESS(rc))
                {
                    rc = REMR3Init(pVM);
                    if (VBOX_SUCCESS(rc))
                    {
                        rc = MMR3InitPaging(pVM);
                        if (VBOX_SUCCESS(rc))
                            rc = TMR3Init(pVM);
                        if (VBOX_SUCCESS(rc))
                        {
                            rc = VMMR3Init(pVM);
                            if (VBOX_SUCCESS(rc))
                            {
                                rc = SELMR3Init(pVM);
                                if (VBOX_SUCCESS(rc))
                                {
                                    rc = TRPMR3Init(pVM);
                                    if (VBOX_SUCCESS(rc))
                                    {
                                        rc = CSAMR3Init(pVM);
                                        if (VBOX_SUCCESS(rc))
                                        {
                                            rc = PATMR3Init(pVM);
                                            if (VBOX_SUCCESS(rc))
                                            {
                                                rc = IOMR3Init(pVM);
                                                if (VBOX_SUCCESS(rc))
                                                {
                                                    rc = EMR3Init(pVM);
                                                    if (VBOX_SUCCESS(rc))
                                                    {
                                                        rc = DBGFR3Init(pVM);
                                                        if (VBOX_SUCCESS(rc))
                                                        {
                                                            rc = PDMR3Init(pVM);
                                                            if (VBOX_SUCCESS(rc))
                                                            {
                                                                rc = PGMR3InitDynMap(pVM);
                                                                if (VBOX_SUCCESS(rc))
                                                                    rc = MMR3HyperInitFinalize(pVM);
                                                                if (VBOX_SUCCESS(rc))
                                                                    rc = PATMR3InitFinalize(pVM);
                                                                if (VBOX_SUCCESS(rc))
                                                                    rc = PGMR3InitFinalize(pVM);
                                                                if (VBOX_SUCCESS(rc))
                                                                    rc = SELMR3InitFinalize(pVM);
                                                                if (VBOX_SUCCESS(rc))
                                                                    rc = VMMR3InitFinalize(pVM);
                                                                if (VBOX_SUCCESS(rc))
                                                                    rc = vmR3InitDoCompleted(pVM, VMINITCOMPLETED_RING3);
                                                                if (VBOX_SUCCESS(rc))
                                                                {
                                                                    LogFlow(("vmR3InitRing3: returns %Vrc\n", VINF_SUCCESS));
                                                                    return VINF_SUCCESS;
                                                                }
                                                                int rc2 = PDMR3Term(pVM);
                                                                AssertRC(rc2);
                                                            }
                                                            int rc2 = DBGFR3Term(pVM);
                                                            AssertRC(rc2);
                                                        }
                                                        int rc2 = EMR3Term(pVM);
                                                        AssertRC(rc2);
                                                    }
                                                    int rc2 = IOMR3Term(pVM);
                                                    AssertRC(rc2);
                                                }
                                                int rc2 = PATMR3Term(pVM);
                                                AssertRC(rc2);
                                            }
                                            int rc2 = CSAMR3Term(pVM);
                                            AssertRC(rc2);
                                        }
                                        int rc2 = TRPMR3Term(pVM);
                                        AssertRC(rc2);
                                    }
                                    int rc2 = SELMR3Term(pVM);
                                    AssertRC(rc2);
                                }
                                int rc2 = VMMR3Term(pVM);
                                AssertRC(rc2);
                            }
                            int rc2 = TMR3Term(pVM);
                            AssertRC(rc2);
                        }
                        int rc2 = REMR3Term(pVM);
                        AssertRC(rc2);
                    }
                    int rc2 = PGMR3Term(pVM);
                    AssertRC(rc2);
                }
                int rc2 = HWACCMR3Term(pVM);
                AssertRC(rc2);
            }
            //int rc2 = CPUMR3Term(pVM);
            //AssertRC(rc2);
        }
        /* MMR3Term is not called here because it'll kill the heap. */
    }

    LogFlow(("vmR3InitRing3: returns %Vrc\n", rc));
    return rc;
}


/**
 * Initializes all R0 components of the VM
 */
static int vmR3InitRing0(PVM pVM)
{
    LogFlow(("vmR3InitRing0:\n"));

    /*
     * Check for FAKE suplib mode.
     */
    int rc = VINF_SUCCESS;
    const char *psz = getenv("VBOX_SUPLIB_FAKE");
    if (!psz || strcmp(psz, "fake"))
    {
        /*
         * Call the VMMR0 component and let it do the init.
         */
        rc = VMMR3InitR0(pVM);
    }
    else
        Log(("vmR3InitRing0: skipping because of VBOX_SUPLIB_FAKE=fake\n"));

    /*
     * Do notifications and return.
     */
    if (VBOX_SUCCESS(rc))
        rc = vmR3InitDoCompleted(pVM, VMINITCOMPLETED_RING0);
    LogFlow(("vmR3InitRing0: returns %Vrc\n", rc));
    return rc;
}


/**
 * Initializes all GC components of the VM
 */
static int vmR3InitGC(PVM pVM)
{
    LogFlow(("vmR3InitGC:\n"));

    /*
     * Check for FAKE suplib mode.
     */
    int rc = VINF_SUCCESS;
    const char *psz = getenv("VBOX_SUPLIB_FAKE");
    if (!psz || strcmp(psz, "fake"))
    {
        /*
         * Call the VMMR0 component and let it do the init.
         */
        rc = VMMR3InitGC(pVM);
    }
    else
        Log(("vmR3InitGC: skipping because of VBOX_SUPLIB_FAKE=fake\n"));

    /*
     * Do notifications and return.
     */
    if (VBOX_SUCCESS(rc))
        rc = vmR3InitDoCompleted(pVM, VMINITCOMPLETED_GC);
    LogFlow(("vmR3InitGC: returns %Vrc\n", rc));
    return rc;
}


/**
 * Do init completed notifications.
 * This notifications can fail.
 *
 * @param   pVM         The VM handle.
 * @param   enmWhat     What's completed.
 */
static int vmR3InitDoCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{

    return VINF_SUCCESS;
}


/**
 * Calls the relocation functions for all VMM components so they can update
 * any GC pointers. When this function is called all the basic VM members
 * have been updated  and the actual memory relocation have been done
 * by the PGM/MM.
 *
 * This is used both on init and on runtime relocations.
 *
 * @param   pVM         VM handle.
 * @param   offDelta    Relocation delta relative to old location.
 */
VMR3DECL(void)   VMR3Relocate(PVM pVM, RTGCINTPTR offDelta)
{
    LogFlow(("VMR3Relocate: offDelta=%VGv\n", offDelta));

    /*
     * The order here is very important!
     */
    PGMR3Relocate(pVM, offDelta);
    PDMR3LdrRelocate(pVM, offDelta);
    PGMR3Relocate(pVM, 0);              /* Repeat after PDM relocation. */
    CPUMR3Relocate(pVM);
    HWACCMR3Relocate(pVM);
    SELMR3Relocate(pVM);
    VMMR3Relocate(pVM, offDelta);
    SELMR3Relocate(pVM);                /* !hack! fix stack! */
    TRPMR3Relocate(pVM, offDelta);
    PATMR3Relocate(pVM);
    CSAMR3Relocate(pVM, offDelta);
    IOMR3Relocate(pVM, offDelta);
    EMR3Relocate(pVM);
    TMR3Relocate(pVM, offDelta);
    DBGFR3Relocate(pVM, offDelta);
    PDMR3Relocate(pVM, offDelta);
}



/**
 * Power on the virtual machine.
 *
 * @returns 0 on success.
 * @returns VBox error code on failure.
 * @param   pVM         VM to power on.
 * @thread      Any thread.
 * @vmstate     Created
 * @vmstateto   Running
 */
VMR3DECL(int)   VMR3PowerOn(PVM pVM)
{
    LogFlow(("VMR3PowerOn: pVM=%p\n", pVM));

    /*
     * Validate input.
     */
    if (!pVM)
    {
        AssertMsgFailed(("Invalid VM pointer\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Request the operation in EMT.
     */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3PowerOn, 1, pVM);
    if (VBOX_SUCCESS(rc))
    {
        rc = pReq->iStatus;
        VMR3ReqFree(pReq);
    }

    LogFlow(("VMR3PowerOn: returns %Vrc\n", rc));
    return rc;
}


/**
 * Power on the virtual machine.
 *
 * @returns 0 on success.
 * @returns VBox error code on failure.
 * @param   pVM         VM to power on.
 * @thread  EMT
 */
static DECLCALLBACK(int) vmR3PowerOn(PVM pVM)
{
    LogFlow(("vmR3PowerOn: pVM=%p\n", pVM));

    /*
     * Validate input.
     */
    if (pVM->enmVMState != VMSTATE_CREATED)
    {
        AssertMsgFailed(("Invalid VM state %d\n", pVM->enmVMState));
        return VERR_VM_INVALID_VM_STATE;
    }

    /*
     * Change the state, notify the components and resume the execution.
     */
    vmR3SetState(pVM, VMSTATE_RUNNING);
    PDMR3PowerOn(pVM);

    return VINF_SUCCESS;
}


/**
 * Suspends a running VM.
 *
 * @returns 0 on success.
 * @returns VBox error code on failure.
 * @param   pVM     VM to suspend.
 * @thread      Any thread.
 * @vmstate     Running
 * @vmstateto   Suspended
 */
VMR3DECL(int) VMR3Suspend(PVM pVM)
{
    LogFlow(("VMR3Suspend: pVM=%p\n", pVM));

    /*
     * Validate input.
     */
    if (!pVM)
    {
        AssertMsgFailed(("Invalid VM pointer\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Request the operation in EMT.
     */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3Suspend, 1, pVM);
    if (VBOX_SUCCESS(rc))
    {
        rc = pReq->iStatus;
        VMR3ReqFree(pReq);
    }

    LogFlow(("VMR3Suspend: returns %Vrc\n", rc));
    return rc;
}


/**
 * Suspends a running VM and prevent state saving until the VM is resumed or stopped.
 *
 * @returns 0 on success.
 * @returns VBox error code on failure.
 * @param   pVM     VM to suspend.
 * @thread      Any thread.
 * @vmstate     Running
 * @vmstateto   Suspended
 */
VMR3DECL(int) VMR3SuspendNoSave(PVM pVM)
{
    pVM->vm.s.fPreventSaveState = true;
    return VMR3Suspend(pVM);
}


/**
 * Suspends a running VM.
 *
 * @returns 0 on success.
 * @returns VBox error code on failure.
 * @param   pVM     VM to suspend.
 * @thread  EMT
 */
static DECLCALLBACK(int) vmR3Suspend(PVM pVM)
{
    LogFlow(("vmR3Suspend: pVM=%p\n", pVM));

    /*
     * Validate input.
     */
    if (pVM->enmVMState != VMSTATE_RUNNING)
    {
        AssertMsgFailed(("Invalid VM state %d\n", pVM->enmVMState));
        return VERR_VM_INVALID_VM_STATE;
    }

    /*
     * Change the state, notify the components and resume the execution.
     */
    vmR3SetState(pVM, VMSTATE_SUSPENDED);
    PDMR3Suspend(pVM);

    return VINF_EM_SUSPEND;
}


/**
 * Resume VM execution.
 *
 * @returns 0 on success.
 * @returns VBox error code on failure.
 * @param   pVM         The VM to resume.
 * @thread      Any thread.
 * @vmstate     Suspended
 * @vmstateto   Running
 */
VMR3DECL(int)   VMR3Resume(PVM pVM)
{
    LogFlow(("VMR3Resume: pVM=%p\n", pVM));

    /*
     * Validate input.
     */
    if (!pVM)
    {
        AssertMsgFailed(("Invalid VM pointer\n"));
        return VERR_INVALID_PARAMETER;
    }

        /*
     * Request the operation in EMT.
     */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3Resume, 1, pVM);
    if (VBOX_SUCCESS(rc))
    {
        rc = pReq->iStatus;
        VMR3ReqFree(pReq);
    }

    LogFlow(("VMR3Resume: returns %Vrc\n", rc));
    return rc;
}


/**
 * Resume VM execution.
 *
 * @returns 0 on success.
 * @returns VBox error code on failure.
 * @param   pVM         The VM to resume.
 * @thread  EMT
 */
static DECLCALLBACK(int) vmR3Resume(PVM pVM)
{
    LogFlow(("vmR3Resume: pVM=%p\n", pVM));

    /*
     * Validate input.
     */
    if (pVM->enmVMState != VMSTATE_SUSPENDED)
    {
        AssertMsgFailed(("Invalid VM state %d\n", pVM->enmVMState));
        return VERR_VM_INVALID_VM_STATE;
    }

    /*
     * Change the state, notify the components and resume the execution.
     */
    pVM->vm.s.fPreventSaveState = false;
    vmR3SetState(pVM, VMSTATE_RUNNING);
    PDMR3Resume(pVM);

    return VINF_EM_RESUME;
}


/**
 * Save current VM state.
 *
 * To save and terminate the VM, the VM must be suspended before the call.
 *
 * @returns 0 on success.
 * @returns VBox error code on failure.
 * @param   pVM             VM which state should be saved.
 * @param   pszFilename     Name of the save state file.
 * @param   pfnProgress     Progress callback. Optional.
 * @param   pvUser          User argument for the progress callback.
 * @thread      Any thread.
 * @vmstate     Suspended
 * @vmstateto   Unchanged state.
 */
VMR3DECL(int) VMR3Save(PVM pVM, const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser)
{
    LogFlow(("VMR3Save: pVM=%p pszFilename=%p:{%s} pfnProgress=%p pvUser=%p\n", pVM, pszFilename, pszFilename, pfnProgress, pvUser));

    /*
     * Validate input.
     */
    if (!pVM)
    {
        AssertMsgFailed(("Invalid VM pointer\n"));
        return VERR_INVALID_PARAMETER;
    }
    if (!pszFilename)
    {
        AssertMsgFailed(("Must specify a filename to save the state to, wise guy!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Request the operation in EMT.
     */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3Save, 4, pVM, pszFilename, pfnProgress, pvUser);
    if (VBOX_SUCCESS(rc))
    {
        rc = pReq->iStatus;
        VMR3ReqFree(pReq);
    }

    LogFlow(("VMR3Save: returns %Vrc\n", rc));
    return rc;
}


/**
 * Save current VM state.
 *
 * To save and terminate the VM, the VM must be suspended before the call.
 *
 * @returns 0 on success.
 * @returns VBox error code on failure.
 * @param   pVM             VM which state should be saved.
 * @param   pszFilename     Name of the save state file.
 * @param   pfnProgress     Progress callback. Optional.
 * @param   pvUser          User argument for the progress callback.
 * @thread  EMT
 */
static DECLCALLBACK(int) vmR3Save(PVM pVM, const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser)
{
    LogFlow(("vmR3Save: pVM=%p pszFilename=%p:{%s} pfnProgress=%p pvUser=%p\n", pVM, pszFilename, pszFilename, pfnProgress, pvUser));

    /*
     * Validate input.
     */
    if (pVM->enmVMState != VMSTATE_SUSPENDED)
    {
        AssertMsgFailed(("Invalid VM state %d\n", pVM->enmVMState));
        return VERR_VM_INVALID_VM_STATE;
    }

    /* If we are in an inconsistent state, then we don't allow state saving. */
    if (pVM->vm.s.fPreventSaveState)
    {
        LogRel(("VMM: vmR3Save: saving the VM state is not allowed at this moment\n"));
        return VERR_VM_SAVE_STATE_NOT_ALLOWED;
    }

    /*
     * Change the state and perform the save.
     */
    /** @todo implement progress support in SSM */
    vmR3SetState(pVM, VMSTATE_SAVING);
    int rc = SSMR3Save(pVM, pszFilename, SSMAFTER_CONTINUE, pfnProgress,  pvUser);
    vmR3SetState(pVM, VMSTATE_SUSPENDED);

    return rc;
}


/**
 * Loads a new VM state.
 *
 * To restore a saved state on VM startup, call this function and then
 * resume the VM instead of powering it on.
 *
 * @returns 0 on success.
 * @returns VBox error code on failure.
 * @param   pVM             VM which state should be saved.
 * @param   pszFilename     Name of the save state file.
 * @param   pfnProgress     Progress callback. Optional.
 * @param   pvUser          User argument for the progress callback.
 * @thread      Any thread.
 * @vmstate     Created, Suspended
 * @vmstateto   Suspended
 */
VMR3DECL(int)   VMR3Load(PVM pVM, const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser)
{
    LogFlow(("VMR3Load: pVM=%p pszFilename=%p:{%s} pfnProgress=%p pvUser=%p\n", pVM, pszFilename, pszFilename, pfnProgress, pvUser));

    /*
     * Validate input.
     */
    if (!pVM)
    {
        AssertMsgFailed(("Invalid VM pointer\n"));
        return VERR_INVALID_PARAMETER;
    }
    if (!pszFilename)
    {
        AssertMsgFailed(("Must specify a filename to load the state from, wise guy!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Request the operation in EMT.
     */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3Load, 4, pVM, pszFilename, pfnProgress, pvUser);
    if (VBOX_SUCCESS(rc))
    {
        rc = pReq->iStatus;
        VMR3ReqFree(pReq);
    }

    LogFlow(("VMR3Load: returns %Vrc\n", rc));
    return rc;
}


/**
 * Loads a new VM state.
 *
 * To restore a saved state on VM startup, call this function and then
 * resume the VM instead of powering it on.
 *
 * @returns 0 on success.
 * @returns VBox error code on failure.
 * @param   pVM             VM which state should be saved.
 * @param   pszFilename     Name of the save state file.
 * @param   pfnProgress     Progress callback. Optional.
 * @param   pvUser          User argument for the progress callback.
 * @thread  EMT.
 */
static DECLCALLBACK(int) vmR3Load(PVM pVM, const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser)
{
    LogFlow(("vmR3Load: pVM=%p pszFilename=%p:{%s} pfnProgress=%p pvUser=%p\n", pVM, pszFilename, pszFilename, pfnProgress, pvUser));

    /*
     * Validate input.
     */
    if (    pVM->enmVMState != VMSTATE_SUSPENDED
        &&  pVM->enmVMState != VMSTATE_CREATED)
    {
        AssertMsgFailed(("Invalid VM state %d\n", pVM->enmVMState));
        return VMSetError(pVM, VERR_VM_INVALID_VM_STATE, RT_SRC_POS, N_("Invalid VM state (%s) for restoring state from '%s'"),
                          VMR3GetStateName(pVM->enmVMState), pszFilename);
    }

    /*
     * Change the state and perform the load.
     */
    vmR3SetState(pVM, VMSTATE_LOADING);
    int rc = SSMR3Load(pVM, pszFilename, SSMAFTER_RESUME, pfnProgress,  pvUser);
    if (VBOX_SUCCESS(rc))
    {
        /* Not paranoia anymore; the saved guest might use different hypervisor selectors. We must call VMR3Relocate. */
        VMR3Relocate(pVM, 0);
        vmR3SetState(pVM, VMSTATE_SUSPENDED);
    }
    else
    {
        vmR3SetState(pVM, VMSTATE_LOAD_FAILURE);
        rc = VMSetError(pVM, rc, RT_SRC_POS, N_("Failed to restore VM state from '%s' (%Vrc)"), pszFilename, rc);
    }

    return rc;
}


/**
 * Power Off the VM.
 *
 * @returns 0 on success.
 * @returns VBox error code on failure.
 * @param   pVM     VM which should be destroyed.
 * @thread      Any thread.
 * @vmstate     Suspended, Running, Guru Mediation, Load Failure
 * @vmstateto   Off
 */
VMR3DECL(int)   VMR3PowerOff(PVM pVM)
{
    LogFlow(("VMR3PowerOff: pVM=%p\n", pVM));

    /*
     * Validate input.
     */
    if (!pVM)
    {
        AssertMsgFailed(("Invalid VM pointer\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Request the operation in EMT.
     */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3PowerOff, 1, pVM);
    if (VBOX_SUCCESS(rc))
    {
        rc = pReq->iStatus;
        VMR3ReqFree(pReq);
    }

    LogFlow(("VMR3PowerOff: returns %Vrc\n", rc));
    return rc;
}


/**
 * Power Off the VM.
 *
 * @returns 0 on success.
 * @returns VBox error code on failure.
 * @param   pVM     VM which should be destroyed.
 * @thread      EMT.
 */
static DECLCALLBACK(int) vmR3PowerOff(PVM pVM)
{
    LogFlow(("vmR3PowerOff: pVM=%p\n", pVM));

    /*
     * Validate input.
     */
    if (    pVM->enmVMState != VMSTATE_RUNNING
        &&  pVM->enmVMState != VMSTATE_SUSPENDED
        &&  pVM->enmVMState != VMSTATE_LOAD_FAILURE
        &&  pVM->enmVMState != VMSTATE_GURU_MEDITATION)
    {
        AssertMsgFailed(("Invalid VM state %d\n", pVM->enmVMState));
        return VERR_VM_INVALID_VM_STATE;
    }

    /*
     * For debugging purposes, we will log a summary of the guest state at this point.
     */
    if (pVM->enmVMState != VMSTATE_GURU_MEDITATION)
    {
        /** @todo make the state dumping at VMR3PowerOff optional. */
        RTLogRelPrintf("****************** Guest state at power off ******************\n");
        DBGFR3Info(pVM, "cpumguest", "verbose", DBGFR3InfoLogRelHlp());
        RTLogRelPrintf("***\n");
        DBGFR3Info(pVM, "mode", NULL, DBGFR3InfoLogRelHlp());
        RTLogRelPrintf("***\n");
        DBGFR3Info(pVM, "activetimers", NULL, DBGFR3InfoLogRelHlp());
        RTLogRelPrintf("***\n");
        DBGFR3Info(pVM, "gdt", NULL, DBGFR3InfoLogRelHlp());
        /** @todo dump guest call stack. */
#if 1 // temporary while debugging #1589
        RTLogRelPrintf("***\n");
        uint32_t esp = CPUMGetGuestESP(pVM);
        if (    CPUMGetGuestSS(pVM) == 0
            &&  esp < _64K)
        {
            uint8_t abBuf[PAGE_SIZE];
            RTLogRelPrintf("***\n"
                           "ss:sp=0000:%04x ", esp);
            uint32_t Start = esp & ~(uint32_t)63;
            int rc = PGMPhysReadGCPhys(pVM, abBuf, Start, 0x100);
            if (VBOX_SUCCESS(rc))
                RTLogRelPrintf("0000:%04x TO 0000:%04x:\n"
                               "%.*Rhxd\n",
                               Start, Start + 0x100 - 1,
                               0x100, abBuf);
            else
                RTLogRelPrintf("rc=%Vrc\n", rc);

            /* grub ... */
            if (esp < 0x2000 && esp > 0x1fc0)
            {
                rc = PGMPhysReadGCPhys(pVM, abBuf, 0x8000, 0x800);
                if (VBOX_SUCCESS(rc))
                    RTLogRelPrintf("0000:8000 TO 0000:87ff:\n"
                                   "%.*Rhxd\n",
                                   0x800, abBuf);
            }
            /* microsoft cdrom hang ... */
            if (true)
            {
                rc = PGMPhysReadGCPhys(pVM, abBuf, 0x8000, 0x200);
                if (VBOX_SUCCESS(rc))
                    RTLogRelPrintf("2000:0000 TO 2000:01ff:\n"
                                   "%.*Rhxd\n",
                                   0x200, abBuf);
            }
        }
#endif
        RTLogRelPrintf("************** End of Guest state at power off ***************\n");
    }

    /*
     * Change the state to OFF and notify the components.
     */
    vmR3SetState(pVM, VMSTATE_OFF);
    PDMR3PowerOff(pVM);

    return VINF_EM_OFF;
}


/**
 * Destroys the VM.
 * The VM must be powered off (or never really powered on) to call this function.
 * The VM handle is destroyed and can no longer be used up successful return.
 *
 * @returns 0 on success.
 * @returns VBox error code on failure.
 * @param   pVM     VM which should be destroyed.
 * @thread      Any thread but the emulation thread.
 * @vmstate     Off, Created
 * @vmstateto   N/A
 */
VMR3DECL(int)   VMR3Destroy(PVM pVM)
{
    LogFlow(("VMR3Destroy: pVM=%p\n", pVM));

    /*
     * Validate input.
     */
    if (!pVM)
        return VERR_INVALID_PARAMETER;
    if (    pVM->enmVMState != VMSTATE_OFF
        &&  pVM->enmVMState != VMSTATE_CREATED)
    {
        AssertMsgFailed(("Invalid VM state %d\n", pVM->enmVMState));
        return VERR_VM_INVALID_VM_STATE;
    }

    /*
     * Unlink the VM and change it's state to destroying.
     */
/** @todo lock this when we start having multiple machines in a process... */
    PVM pPrev = NULL;
    PVM pCur = g_pVMsHead;
    while (pCur && pCur != pVM)
    {
        pPrev = pCur;
        pCur = pCur->pNext;
    }
    if (!pCur)
    {
        AssertMsgFailed(("pVM=%p is INVALID!\n", pVM));
        return VERR_INVALID_PARAMETER;
    }
    if (pPrev)
        pPrev->pNext = pCur->pNext;
    else
        g_pVMsHead = pCur->pNext;

    vmR3SetState(pVM, VMSTATE_DESTROYING);


    /*
     * Notify registered at destruction listeners.
     * (That's the debugger console.)
     */
    vmR3AtDtor(pVM);

    pVM->pNext = g_pVMsHead;
    g_pVMsHead = pVM;

    /*
     * If we are the EMT we'll delay the cleanup till later.
     */
    if (VM_IS_EMT(pVM))
    {
        pVM->vm.s.fEMTDoesTheCleanup = true;
        VM_FF_SET(pVM, VM_FF_TERMINATE);
    }
    else
    {
        /*
         * Request EMT to do the larger part of the destruction.
         */
        PVMREQ pReq = NULL;
        int rc = VMR3ReqCall(pVM, &pReq, 0, (PFNRT)vmR3Destroy, 1, pVM);
        while (rc == VERR_TIMEOUT)
            rc = VMR3ReqWait(pReq, RT_INDEFINITE_WAIT);
        if (VBOX_SUCCESS(rc))
            rc = pReq->iStatus;
        VMR3ReqFree(pReq);

        /*
         * Wait for the EMT thread to terminate.
         */
        VM_FF_SET(pVM, VM_FF_TERMINATE);
        uint64_t u64Start = RTTimeMilliTS();
        do
        {
            VMR3NotifyFF(pVM, false);
            rc = RTThreadWait(pVM->ThreadEMT, 1000, NULL);
        } while (   RTTimeMilliTS() - u64Start < 30000 /* 30 sec */
                 && rc == VERR_TIMEOUT);
        AssertMsgRC(rc, ("EMT thread wait failed, rc=%Vrc\n", rc));

        /*
         * Now do the final bit where the heap and VM structures are freed up.
         */
        vmR3DestroyFinalBit(pVM);
    }

    LogFlow(("VMR3Destroy: returns VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}


/**
 * Internal destruction worker. This will do nearly all of the
 * job, including quitting the emulation thread.
 *
 * @returns VBox status.
 * @param   pVM     VM handle.
 */
DECLCALLBACK(int) vmR3Destroy(PVM pVM)
{
    LogFlow(("vmR3Destroy: pVM=%p\n", pVM));
    VM_ASSERT_EMT(pVM);

    /*
     * Dump statistics to the log.
     */
#if defined(VBOX_WITH_STATISTICS) || defined(LOG_ENABLED)
    RTLogFlags(NULL, "nodisabled nobuffered");
#endif
#ifdef VBOX_WITH_STATISTICS
    STAMR3Dump(pVM, "*");
#else
    LogRel(("************************* Statistics *************************\n"));
    STAMR3DumpToReleaseLog(pVM, "*");
    LogRel(("********************* End of statistics **********************\n"));
#endif

    /*
     * Destroy the VM components.
     */
    int rc = TMR3Term(pVM);
    AssertRC(rc);
    rc = DBGCTcpTerminate(pVM, pVM->vm.s.pvDBGC);
    pVM->vm.s.pvDBGC = NULL;
    AssertRC(rc);
    rc = DBGFR3Term(pVM);
    AssertRC(rc);
    rc = PDMR3Term(pVM);
    AssertRC(rc);
    rc = EMR3Term(pVM);
    AssertRC(rc);
    rc = IOMR3Term(pVM);
    AssertRC(rc);
    rc = CSAMR3Term(pVM);
    AssertRC(rc);
    rc = PATMR3Term(pVM);
    AssertRC(rc);
    rc = TRPMR3Term(pVM);
    AssertRC(rc);
    rc = SELMR3Term(pVM);
    AssertRC(rc);
    rc = REMR3Term(pVM);
    AssertRC(rc);
    rc = HWACCMR3Term(pVM);
    AssertRC(rc);
    rc = PGMR3Term(pVM);
    AssertRC(rc);
    rc = VMMR3Term(pVM); /* Terminates the ring-0 code! */
    AssertRC(rc);
    rc = CPUMR3Term(pVM);
    AssertRC(rc);
    rc = STAMR3Term(pVM);
    AssertRC(rc);
    rc = PDMR3CritSectTerm(pVM);
    AssertRC(rc);
    /* MM is destroyed later in vmR3DestroyFinalBit() for heap reasons. */

    /*
     * We're done in this thread.
     */
    pVM->fForcedActions = VM_FF_TERMINATE;
    LogFlow(("vmR3Destroy: returning %Vrc\n", VINF_EM_TERMINATE));
    return VINF_EM_TERMINATE;
}


/**
 * Does the final part of the VM destruction.
 * This is called by EMT in it's final stage or by the VMR3Destroy caller.
 *
 * @param   pVM     VM Handle.
 */
void vmR3DestroyFinalBit(PVM pVM)
{
    /*
     * Free the event semaphores associated with the request packets.
     */
    unsigned cReqs = 0;
    for (unsigned i = 0; i < ELEMENTS(pVM->vm.s.apReqFree); i++)
    {
        PVMREQ pReq = pVM->vm.s.apReqFree[i];
        pVM->vm.s.apReqFree[i] = NULL;
        for (; pReq; pReq = pReq->pNext, cReqs++)
        {
            pReq->enmState = VMREQSTATE_INVALID;
            RTSemEventDestroy(pReq->EventSem);
        }
    }
    Assert(cReqs == pVM->vm.s.cReqFree); NOREF(cReqs);

    /*
     * Kill all queued requests. (There really shouldn't be any!)
     */
    for (unsigned i = 0; i < 10; i++)
    {
        PVMREQ pReqHead = (PVMREQ)ASMAtomicXchgPtr((void *volatile *)&pVM->vm.s.pReqs, NULL);
        AssertMsg(!pReqHead, ("This isn't supposed to happen! VMR3Destroy caller has to serialize this.\n"));
        if (!pReqHead)
            break;
        for (PVMREQ pReq = pReqHead; pReq; pReq = pReq->pNext)
        {
            ASMAtomicXchgSize(&pReq->iStatus, VERR_INTERNAL_ERROR);
            ASMAtomicXchgSize(&pReq->enmState, VMREQSTATE_INVALID);
            RTSemEventSignal(pReq->EventSem);
            RTThreadSleep(2);
            RTSemEventDestroy(pReq->EventSem);
        }
        /* give them a chance to respond before we free the request memory. */
        RTThreadSleep(32);
    }

    /*
     * Modify state and then terminate MM.
     * (MM must be delayed until this point so we don't destroy the callbacks and the request packet.)
     */
    vmR3SetState(pVM, VMSTATE_TERMINATED);
    int rc = MMR3Term(pVM);
    AssertRC(rc);

    /*
     * Tell GVMM that it can destroy the VM now.
     */
    rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_GVMM_DESTROY_VM, 0, NULL);
    AssertRC(rc);
    rc = SUPTerm();
    AssertRC(rc);

    RTLogFlush(NULL);
}


/**
 * Enumerates the VMs in this process.
 *
 * @returns Pointer to the next VM.
 * @returns NULL when no more VMs.
 * @param   pVMPrev     The previous VM
 *                      Use NULL to start the enumeration.
 */
VMR3DECL(PVM)   VMR3EnumVMs(PVM pVMPrev)
{
    /*
     * This is quick and dirty. It has issues with VM being
     * destroyed during the enumeration.
     */
    if (pVMPrev)
        return pVMPrev->pNext;
    return g_pVMsHead;
}


/**
 * Registers an at VM destruction callback.
 *
 * @returns VBox status code.
 * @param   pfnAtDtor       Pointer to callback.
 * @param   pvUser          User argument.
 */
VMR3DECL(int)   VMR3AtDtorRegister(PFNVMATDTOR pfnAtDtor, void *pvUser)
{
    /*
     * Check if already registered.
     */
    VM_ATDTOR_LOCK();
    PVMATDTOR   pCur = g_pVMAtDtorHead;
    while (pCur)
    {
        if (pfnAtDtor == pCur->pfnAtDtor)
        {
            VM_ATDTOR_UNLOCK();
            AssertMsgFailed(("Already registered at destruction callback %p!\n", pfnAtDtor));
            return VERR_INVALID_PARAMETER;
        }

        /* next */
        pCur = pCur->pNext;
    }
    VM_ATDTOR_UNLOCK();

    /*
     * Allocate new entry.
     */
    PVMATDTOR   pVMAtDtor = (PVMATDTOR)RTMemAlloc(sizeof(*pVMAtDtor));
    if (!pVMAtDtor)
        return VERR_NO_MEMORY;

    VM_ATDTOR_LOCK();
    pVMAtDtor->pfnAtDtor = pfnAtDtor;
    pVMAtDtor->pvUser    = pvUser;
    pVMAtDtor->pNext     = g_pVMAtDtorHead;
    g_pVMAtDtorHead      = pVMAtDtor;
    VM_ATDTOR_UNLOCK();

    return VINF_SUCCESS;
}


/**
 * Deregisters an at VM destruction callback.
 *
 * @returns VBox status code.
 * @param   pfnAtDtor       Pointer to callback.
 */
VMR3DECL(int)   VMR3AtDtorDeregister(PFNVMATDTOR pfnAtDtor)
{
    /*
     * Find it, unlink it and free it.
     */
    VM_ATDTOR_LOCK();
    PVMATDTOR   pPrev = NULL;
    PVMATDTOR   pCur = g_pVMAtDtorHead;
    while (pCur)
    {
        if (pfnAtDtor == pCur->pfnAtDtor)
        {
            if (pPrev)
                pPrev->pNext = pCur->pNext;
            else
                g_pVMAtDtorHead = pCur->pNext;
            pCur->pNext = NULL;
            VM_ATDTOR_UNLOCK();

            RTMemFree(pCur);
            return VINF_SUCCESS;
        }

        /* next */
        pPrev = pCur;
        pCur = pCur->pNext;
    }
    VM_ATDTOR_UNLOCK();

    return VERR_INVALID_PARAMETER;
}


/**
 * Walks the list of at VM destructor callbacks.
 * @param   pVM     The VM which is about to be destroyed.
 */
static void vmR3AtDtor(PVM pVM)
{
    /*
     * Find it, unlink it and free it.
     */
    VM_ATDTOR_LOCK();
    for (PVMATDTOR pCur = g_pVMAtDtorHead; pCur; pCur = pCur->pNext)
        pCur->pfnAtDtor(pVM, pCur->pvUser);
    VM_ATDTOR_UNLOCK();
}


/**
 * Reset the current VM.
 *
 * @returns VBox status code.
 * @param   pVM     VM to reset.
 */
VMR3DECL(int)   VMR3Reset(PVM pVM)
{
    int rc = VINF_SUCCESS;

    /*
     * Check the state.
     */
    if (!pVM)
        return VERR_INVALID_PARAMETER;
    if (    pVM->enmVMState != VMSTATE_RUNNING
        &&  pVM->enmVMState != VMSTATE_SUSPENDED)
    {
        AssertMsgFailed(("Invalid VM state %d\n", pVM->enmVMState));
        return VERR_VM_INVALID_VM_STATE;
    }

    /*
     * Queue reset request to the emulation thread
     * and wait for it to be processed.
     */
    PVMREQ pReq = NULL;
    rc = VMR3ReqCall(pVM, &pReq, 0, (PFNRT)vmR3Reset, 1, pVM);
    while (rc == VERR_TIMEOUT)
        rc = VMR3ReqWait(pReq, RT_INDEFINITE_WAIT);
    if (VBOX_SUCCESS(rc))
        rc = pReq->iStatus;
    VMR3ReqFree(pReq);

    return rc;
}


/**
 * Worker which checks integrity of some internal structures.
 * This is yet another attempt to track down that AVL tree crash.
 */
static void vmR3CheckIntegrity(PVM pVM)
{
#ifdef VBOX_STRICT
    int rc = PGMR3CheckIntegrity(pVM);
    AssertReleaseRC(rc);
#endif
}


/**
 * Reset request processor.
 *
 * This is called by the emulation thread as a response to the
 * reset request issued by VMR3Reset().
 *
 * @returns VBox status code.
 * @param   pVM     VM to reset.
 */
static DECLCALLBACK(int) vmR3Reset(PVM pVM)
{
    /*
     * As a safety precaution we temporarily change the state while resetting.
     * (If VMR3Reset was not called from EMT we might have change state... let's ignore that fact for now.)
     */
    VMSTATE enmVMState = pVM->enmVMState;
    Assert(enmVMState == VMSTATE_SUSPENDED || enmVMState == VMSTATE_RUNNING);
    vmR3SetState(pVM, VMSTATE_RESETTING);
    vmR3CheckIntegrity(pVM);


    /*
     * Reset the VM components.
     */
    PATMR3Reset(pVM);
    CSAMR3Reset(pVM);
    PGMR3Reset(pVM);                    /* We clear VM RAM in PGMR3Reset. It's vital PDMR3Reset is executed
                                         * _afterwards_. E.g. ACPI sets up RAM tables during init/reset. */
    MMR3Reset(pVM);
    PDMR3Reset(pVM);
    SELMR3Reset(pVM);
    TRPMR3Reset(pVM);
    vmR3AtReset(pVM);
    REMR3Reset(pVM);
    IOMR3Reset(pVM);
    CPUMR3Reset(pVM);
    TMR3Reset(pVM);
    EMR3Reset(pVM);
    HWACCMR3Reset(pVM);                 /* This must come *after* PATM, CSAM, CPUM, SELM and TRPM. */

#ifdef LOG_ENABLED
    /*
     * Debug logging.
     */
    RTLogPrintf("\n\nThe VM was reset:\n");
    DBGFR3Info(pVM, "cpum", "verbose", NULL);
#endif

    /*
     * Restore the state.
     */
    vmR3CheckIntegrity(pVM);
    Assert(pVM->enmVMState == VMSTATE_RESETTING);
    vmR3SetState(pVM, enmVMState);

    return VINF_EM_RESET;
}


/**
 * Walks the list of at VM reset callbacks and calls them
 *
 * @returns VBox status code.
 *          Any failure is fatal.
 * @param   pVM     The VM which is being reset.
 */
static int vmR3AtReset(PVM pVM)
{
    /*
     * Walk the list and call them all.
     */
    int rc = VINF_SUCCESS;
    for (PVMATRESET pCur = pVM->vm.s.pAtReset; pCur; pCur = pCur->pNext)
    {
        /* do the call */
        switch (pCur->enmType)
        {
            case VMATRESETTYPE_DEV:
                rc = pCur->u.Dev.pfnCallback(pCur->u.Dev.pDevIns, pCur->pvUser);
                break;
            case VMATRESETTYPE_INTERNAL:
                rc = pCur->u.Internal.pfnCallback(pVM, pCur->pvUser);
                break;
            case VMATRESETTYPE_EXTERNAL:
                pCur->u.External.pfnCallback(pCur->pvUser);
                break;
            default:
                AssertMsgFailed(("Invalid at-reset type %d!\n", pCur->enmType));
                return VERR_INTERNAL_ERROR;
        }

        if (VBOX_FAILURE(rc))
        {
            AssertMsgFailed(("At-reset handler %s failed with rc=%d\n", pCur->pszDesc, rc));
            return rc;
        }
    }

    return VINF_SUCCESS;
}


/**
 * Internal registration function
 */
static int vmr3AtResetRegister(PVM pVM, void *pvUser, const char *pszDesc, PVMATRESET *ppNew)
{
    /*
     * Allocate restration structure.
     */
    PVMATRESET pNew = (PVMATRESET)MMR3HeapAlloc(pVM, MM_TAG_VM,  sizeof(*pNew));
    if (pNew)
    {
        /* fill data. */
        pNew->pNext     = NULL;
        pNew->pszDesc   = pszDesc;
        pNew->pvUser    = pvUser;

        /* insert */
        *pVM->vm.s.ppAtResetNext = pNew;
        pVM->vm.s.ppAtResetNext = &pNew->pNext;

        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


/**
 * Registers an at VM reset callback.
 *
 * @returns VBox status code.
 * @param   pVM             The VM.
 * @param   pDevInst        Device instance.
 * @param   pfnCallback     Callback function.
 * @param   pvUser          User argument.
 * @param   pszDesc         Description (optional).
 */
VMR3DECL(int)   VMR3AtResetRegister(PVM pVM, PPDMDEVINS pDevInst, PFNVMATRESET pfnCallback, void *pvUser, const char *pszDesc)
{
    /*
     * Validate.
     */
    if (!pDevInst)
    {
        AssertMsgFailed(("pDevIns is NULL!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Create the new entry.
     */
    PVMATRESET pNew;
    int rc = vmr3AtResetRegister(pVM, pvUser, pszDesc, &pNew);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Fill in type data.
         */
        pNew->enmType               = VMATRESETTYPE_DEV;
        pNew->u.Dev.pfnCallback     = pfnCallback;
        pNew->u.Dev.pDevIns         = pDevInst;
    }

    return rc;
}


/**
 * Registers an at VM reset internal callback.
 *
 * @returns VBox status code.
 * @param   pVM             The VM.
 * @param   pfnCallback     Callback function.
 * @param   pvUser          User argument.
 * @param   pszDesc         Description (optional).
 */
VMR3DECL(int)   VMR3AtResetRegisterInternal(PVM pVM, PFNVMATRESETINT pfnCallback, void *pvUser, const char *pszDesc)
{
    /*
     * Validate.
     */
    if (!pfnCallback)
    {
        AssertMsgFailed(("pfnCallback is NULL!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Create the new entry.
     */
    PVMATRESET pNew;
    int rc = vmr3AtResetRegister(pVM, pvUser, pszDesc, &pNew);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Fill in type data.
         */
        pNew->enmType                   = VMATRESETTYPE_INTERNAL;
        pNew->u.Internal.pfnCallback    = pfnCallback;
    }

    return rc;
}


/**
 * Registers an at VM reset external callback.
 *
 * @returns VBox status code.
 * @param   pVM             The VM.
 * @param   pfnCallback     Callback function.
 * @param   pvUser          User argument.
 * @param   pszDesc         Description (optional).
 */
VMR3DECL(int)   VMR3AtResetRegisterExternal(PVM pVM, PFNVMATRESETEXT pfnCallback, void *pvUser, const char *pszDesc)
{
    /*
     * Validate.
     */
    if (!pfnCallback)
    {
        AssertMsgFailed(("pfnCallback is NULL!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Create the new entry.
     */
    PVMATRESET pNew;
    int rc = vmr3AtResetRegister(pVM, pvUser, pszDesc, &pNew);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Fill in type data.
         */
        pNew->enmType                   = VMATRESETTYPE_EXTERNAL;
        pNew->u.External.pfnCallback    = pfnCallback;
    }

    return rc;
}


/**
 * Unlinks and frees a callback.
 *
 * @returns Pointer to the next callback structure.
 * @param   pVM             The VM.
 * @param   pCur            The one to free.
 * @param   pPrev           The one before pCur.
 */
static PVMATRESET vmr3AtResetFree(PVM pVM, PVMATRESET pCur, PVMATRESET pPrev)
{
    /*
     * Unlink it.
     */
    PVMATRESET pNext = pCur->pNext;
    if (pPrev)
    {
        pPrev->pNext = pNext;
        if (!pNext)
            pVM->vm.s.ppAtResetNext = &pPrev->pNext;
    }
    else
    {
        pVM->vm.s.pAtReset = pNext;
        if (!pNext)
            pVM->vm.s.ppAtResetNext = &pVM->vm.s.pAtReset;
    }

    /*
     * Free it.
     */
    MMR3HeapFree(pCur);

    return pNext;
}


/**
 * Deregisters an at VM reset callback.
 *
 * @returns VBox status code.
 * @param   pVM             The VM.
 * @param   pDevInst        Device instance.
 * @param   pfnCallback     Callback function.
 */
VMR3DECL(int)   VMR3AtResetDeregister(PVM pVM, PPDMDEVINS pDevInst, PFNVMATRESET pfnCallback)
{
    int         rc = VERR_VM_ATRESET_NOT_FOUND;
    PVMATRESET  pPrev = NULL;
    PVMATRESET  pCur = pVM->vm.s.pAtReset;
    while (pCur)
    {
        if (    pCur->enmType == VMATRESETTYPE_DEV
            &&  pCur->u.Dev.pDevIns == pDevInst
            &&  (!pfnCallback || pCur->u.Dev.pfnCallback == pfnCallback))
        {
            pCur = vmr3AtResetFree(pVM, pCur, pPrev);
            rc = VINF_SUCCESS;
        }
        else
        {
            pPrev = pCur;
            pCur = pCur->pNext;
        }
    }

    AssertRC(rc);
    return rc;
}


/**
 * Deregisters an at VM reset internal callback.
 *
 * @returns VBox status code.
 * @param   pVM             The VM.
 * @param   pfnCallback     Callback function.
 */
VMR3DECL(int)   VMR3AtResetDeregisterInternal(PVM pVM, PFNVMATRESETINT pfnCallback)
{
    int         rc = VERR_VM_ATRESET_NOT_FOUND;
    PVMATRESET  pPrev = NULL;
    PVMATRESET  pCur = pVM->vm.s.pAtReset;
    while (pCur)
    {
        if (    pCur->enmType == VMATRESETTYPE_INTERNAL
            &&  pCur->u.Internal.pfnCallback == pfnCallback)
        {
            pCur = vmr3AtResetFree(pVM, pCur, pPrev);
            rc = VINF_SUCCESS;
        }
        else
        {
            pPrev = pCur;
            pCur = pCur->pNext;
        }
    }

    AssertRC(rc);
    return rc;
}


/**
 * Deregisters an at VM reset external callback.
 *
 * @returns VBox status code.
 * @param   pVM             The VM.
 * @param   pfnCallback     Callback function.
 */
VMR3DECL(int)   VMR3AtResetDeregisterExternal(PVM pVM, PFNVMATRESETEXT pfnCallback)
{
    int         rc = VERR_VM_ATRESET_NOT_FOUND;
    PVMATRESET  pPrev = NULL;
    PVMATRESET  pCur = pVM->vm.s.pAtReset;
    while (pCur)
    {
        if (    pCur->enmType == VMATRESETTYPE_INTERNAL
            &&  pCur->u.External.pfnCallback == pfnCallback)
        {
            pCur = vmr3AtResetFree(pVM, pCur, pPrev);
            rc = VINF_SUCCESS;
        }
        else
        {
            pPrev = pCur;
            pCur = pCur->pNext;
        }
    }

    AssertRC(rc);
    return rc;
}


/**
 * Gets the current VM state.
 *
 * @returns The current VM state.
 * @param   pVM             VM handle.
 * @thread  Any
 */
VMR3DECL(VMSTATE) VMR3GetState(PVM pVM)
{
    return pVM->enmVMState;
}


/**
 * Gets the state name string for a VM state.
 *
 * @returns Pointer to the state name. (readonly)
 * @param   enmState        The state.
 */
VMR3DECL(const char *) VMR3GetStateName(VMSTATE enmState)
{
    switch (enmState)
    {
        case VMSTATE_CREATING:          return "CREATING";
        case VMSTATE_CREATED:           return "CREATED";
        case VMSTATE_RUNNING:           return "RUNNING";
        case VMSTATE_LOADING:           return "LOADING";
        case VMSTATE_LOAD_FAILURE:      return "LOAD_FAILURE";
        case VMSTATE_SAVING:            return "SAVING";
        case VMSTATE_SUSPENDED:         return "SUSPENDED";
        case VMSTATE_RESETTING:         return "RESETTING";
        case VMSTATE_GURU_MEDITATION:   return "GURU_MEDIATION";
        case VMSTATE_OFF:               return "OFF";
        case VMSTATE_DESTROYING:        return "DESTROYING";
        case VMSTATE_TERMINATED:        return "TERMINATED";
        default:
            AssertMsgFailed(("Unknown state %d\n", enmState));
            return "Unknown!\n";
    }
}


/**
 * Sets the current VM state.
 *
 * @returns The current VM state.
 * @param   pVM             VM handle.
 * @param   enmStateNew     The new state.
 */
void vmR3SetState(PVM pVM, VMSTATE enmStateNew)
{
    VMSTATE enmStateOld = pVM->enmVMState;
    pVM->enmVMState = enmStateNew;
    LogRel(("Changing the VM state from '%s' to '%s'.\n", VMR3GetStateName(enmStateOld),  VMR3GetStateName(enmStateNew)));

    /*
     * Call the at state change callbacks.
     */
    for (PVMATSTATE pCur = pVM->vm.s.pAtState; pCur; pCur = pCur->pNext)
    {
        pCur->pfnAtState(pVM, enmStateNew, enmStateOld, pCur->pvUser);
        if (pVM->enmVMState == VMSTATE_DESTROYING)
            break;
        AssertMsg(pVM->enmVMState == enmStateNew,
                  ("You are not allowed to change the state while in the change callback, except "
                   "from destroying the VM. There are restrictions in the way the state changes "
                   "are propagated up to the EM execution loop and it makes the program flow very "
                   "difficult to follow.\n"));
    }
}


/**
 * Registers a VM state change callback.
 *
 * You are not allowed to call any function which changes the VM state from a
 * state callback, except VMR3Destroy().
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pfnAtState      Pointer to callback.
 * @param   pvUser          User argument.
 * @thread  Any.
 */
VMR3DECL(int)   VMR3AtStateRegister(PVM pVM, PFNVMATSTATE pfnAtState, void *pvUser)
{
    LogFlow(("VMR3AtStateRegister: pfnAtState=%p pvUser=%p\n", pfnAtState, pvUser));

    /*
     * Validate input.
     */
    if (!pfnAtState)
    {
        AssertMsgFailed(("callback is required\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Make sure we're in EMT (to avoid the logging).
     */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3AtStateRegister, 3, pVM, pfnAtState, pvUser);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pReq->iStatus;
    VMR3ReqFree(pReq);

    LogFlow(("VMR3AtStateRegister: returns %Vrc\n", rc));
    return rc;
}


/**
 * Registers a VM state change callback.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pfnAtState      Pointer to callback.
 * @param   pvUser          User argument.
 * @thread  EMT
 */
static DECLCALLBACK(int) vmR3AtStateRegister(PVM pVM, PFNVMATSTATE pfnAtState, void *pvUser)
{
    /*
     * Allocate a new record.
     */

    PVMATSTATE pNew = (PVMATSTATE)MMR3HeapAlloc(pVM, MM_TAG_VM, sizeof(*pNew));
    if (!pNew)
        return VERR_NO_MEMORY;

    /* fill */
    pNew->pfnAtState = pfnAtState;
    pNew->pvUser     = pvUser;
    pNew->pNext      = NULL;

    /* insert */
    *pVM->vm.s.ppAtStateNext = pNew;
    pVM->vm.s.ppAtStateNext = &pNew->pNext;

    return VINF_SUCCESS;
}


/**
 * Deregisters a VM state change callback.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pfnAtState      Pointer to callback.
 * @param   pvUser          User argument.
 * @thread  Any.
 */
VMR3DECL(int)   VMR3AtStateDeregister(PVM pVM, PFNVMATSTATE pfnAtState, void *pvUser)
{
    LogFlow(("VMR3AtStateDeregister: pfnAtState=%p pvUser=%p\n", pfnAtState, pvUser));

    /*
     * Validate input.
     */
    if (!pfnAtState)
    {
        AssertMsgFailed(("callback is required\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Make sure we're in EMT (to avoid the logging).
     */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3AtStateDeregister, 3, pVM, pfnAtState, pvUser);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pReq->iStatus;
    VMR3ReqFree(pReq);

    LogFlow(("VMR3AtStateDeregister: returns %Vrc\n", rc));
    return rc;
}


/**
 * Deregisters a VM state change callback.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pfnAtState      Pointer to callback.
 * @param   pvUser          User argument.
 * @thread  EMT
 */
static DECLCALLBACK(int) vmR3AtStateDeregister(PVM pVM, PFNVMATSTATE pfnAtState, void *pvUser)
{
    LogFlow(("vmR3AtStateDeregister: pfnAtState=%p pvUser=%p\n", pfnAtState, pvUser));

    /*
     * Search the list for the entry.
     */
    PVMATSTATE pPrev = NULL;
    PVMATSTATE pCur = pVM->vm.s.pAtState;
    while (     pCur
           &&   pCur->pfnAtState == pfnAtState
           &&   pCur->pvUser == pvUser)
    {
        pPrev = pCur;
        pCur = pCur->pNext;
    }
    if (!pCur)
    {
        AssertMsgFailed(("pfnAtState=%p was not found\n", pfnAtState));
        return VERR_FILE_NOT_FOUND;
    }

    /*
     * Unlink it.
     */
    if (pPrev)
    {
        pPrev->pNext = pCur->pNext;
        if (!pCur->pNext)
            pVM->vm.s.ppAtStateNext = &pPrev->pNext;
    }
    else
    {
        pVM->vm.s.pAtState = pCur->pNext;
        if (!pCur->pNext)
            pVM->vm.s.ppAtStateNext = &pVM->vm.s.pAtState;
    }

    /*
     * Free it.
     */
    pCur->pfnAtState = NULL;
    pCur->pNext = NULL;
    MMR3HeapFree(pCur);

    return VINF_SUCCESS;
}


/**
 * Registers a VM error callback.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pfnAtError      Pointer to callback.
 * @param   pvUser          User argument.
 * @thread  Any.
 */
VMR3DECL(int)   VMR3AtErrorRegister(PVM pVM, PFNVMATERROR pfnAtError, void *pvUser)
{
    LogFlow(("VMR3AtErrorRegister: pfnAtError=%p pvUser=%p\n", pfnAtError, pvUser));

    /*
     * Validate input.
     */
    if (!pfnAtError)
    {
        AssertMsgFailed(("callback is required\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Make sure we're in EMT (to avoid the logging).
     */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3AtErrorRegister, 3, pVM, pfnAtError, pvUser);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pReq->iStatus;
    VMR3ReqFree(pReq);

    LogFlow(("VMR3AtErrorRegister: returns %Vrc\n", rc));
    return rc;
}


/**
 * Registers a VM error callback.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pfnAtError      Pointer to callback.
 * @param   pvUser          User argument.
 * @thread  EMT
 */
static DECLCALLBACK(int)    vmR3AtErrorRegister(PVM pVM, PFNVMATERROR pfnAtError, void *pvUser)
{
    /*
     * Allocate a new record.
     */

    PVMATERROR pNew = (PVMATERROR)MMR3HeapAlloc(pVM, MM_TAG_VM, sizeof(*pNew));
    if (!pNew)
        return VERR_NO_MEMORY;

    /* fill */
    pNew->pfnAtError = pfnAtError;
    pNew->pvUser     = pvUser;
    pNew->pNext      = NULL;

    /* insert */
    *pVM->vm.s.ppAtErrorNext = pNew;
    pVM->vm.s.ppAtErrorNext = &pNew->pNext;

    return VINF_SUCCESS;
}


/**
 * Deregisters a VM error callback.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pfnAtError      Pointer to callback.
 * @param   pvUser          User argument.
 * @thread  Any.
 */
VMR3DECL(int)   VMR3AtErrorDeregister(PVM pVM, PFNVMATERROR pfnAtError, void *pvUser)
{
    LogFlow(("VMR3AtErrorDeregister: pfnAtError=%p pvUser=%p\n", pfnAtError, pvUser));

    /*
     * Validate input.
     */
    if (!pfnAtError)
    {
        AssertMsgFailed(("callback is required\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Make sure we're in EMT (to avoid the logging).
     */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3AtErrorDeregister, 3, pVM, pfnAtError, pvUser);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pReq->iStatus;
    VMR3ReqFree(pReq);

    LogFlow(("VMR3AtErrorDeregister: returns %Vrc\n", rc));
    return rc;
}


/**
 * Deregisters a VM error callback.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pfnAtError      Pointer to callback.
 * @param   pvUser          User argument.
 * @thread  EMT
 */
static DECLCALLBACK(int)    vmR3AtErrorDeregister(PVM pVM, PFNVMATERROR pfnAtError, void *pvUser)
{
    LogFlow(("vmR3AtErrorDeregister: pfnAtError=%p pvUser=%p\n", pfnAtError, pvUser));

    /*
     * Search the list for the entry.
     */
    PVMATERROR pPrev = NULL;
    PVMATERROR pCur = pVM->vm.s.pAtError;
    while (     pCur
           &&   pCur->pfnAtError == pfnAtError
           &&   pCur->pvUser == pvUser)
    {
        pPrev = pCur;
        pCur = pCur->pNext;
    }
    if (!pCur)
    {
        AssertMsgFailed(("pfnAtError=%p was not found\n", pfnAtError));
        return VERR_FILE_NOT_FOUND;
    }

    /*
     * Unlink it.
     */
    if (pPrev)
    {
        pPrev->pNext = pCur->pNext;
        if (!pCur->pNext)
            pVM->vm.s.ppAtErrorNext = &pPrev->pNext;
    }
    else
    {
        pVM->vm.s.pAtError = pCur->pNext;
        if (!pCur->pNext)
            pVM->vm.s.ppAtErrorNext = &pVM->vm.s.pAtError;
    }

    /*
     * Free it.
     */
    pCur->pfnAtError = NULL;
    pCur->pNext = NULL;
    MMR3HeapFree(pCur);

    return VINF_SUCCESS;
}


/**
 * Ellipsis to va_list wrapper for calling pfnAtError.
 */
static void vmR3SetErrorWorkerDoCall(PVM pVM, PVMATERROR pCur, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    pCur->pfnAtError(pVM, pCur->pvUser, rc, RT_SRC_POS_ARGS, pszFormat, va);
    va_end(va);
}


/**
 * This is a worker function for GC and Ring-0 calls to VMSetError and VMSetErrorV.
 * The message is found in VMINT.
 *
 * @param   pVM             The VM handle.
 * @thread  EMT.
 */
VMR3DECL(void) VMR3SetErrorWorker(PVM pVM)
{
    VM_ASSERT_EMT(pVM);
    AssertReleaseMsgFailed(("And we have a winner! You get to implement Ring-0 and GC VMSetErrorV! Contrats!\n"));

    /*
     * Unpack the error (if we managed to format one).
     */
    PVMERROR pErr = pVM->vm.s.pErrorR3;
    const char *pszFile = NULL;
    const char *pszFunction = NULL;
    uint32_t    iLine = 0;
    const char *pszMessage;
    int32_t     rc = VERR_MM_HYPER_NO_MEMORY;
    if (pErr)
    {
        AssertCompile(sizeof(const char) == sizeof(uint8_t));
        if (pErr->offFile)
            pszFile = (const char *)pErr + pErr->offFile;
        iLine = pErr->iLine;
        if (pErr->offFunction)
            pszFunction = (const char *)pErr + pErr->offFunction;
        if (pErr->offMessage)
            pszMessage = (const char *)pErr + pErr->offMessage;
        else
            pszMessage = "No message!";
    }
    else
        pszMessage = "No message! (Failed to allocate memory to put the error message in!)";

    /*
     * Call the at error callbacks.
     */
    for (PVMATERROR pCur = pVM->vm.s.pAtError; pCur; pCur = pCur->pNext)
        vmR3SetErrorWorkerDoCall(pVM, pCur, rc, RT_SRC_POS_ARGS, "%s", pszMessage);
}


/**
 * Worker which calls everyone listening to the VM error messages.
 *
 * @param   pVM             The VM handle.
 * @param   rc              The VBox status code.
 * @param   RT_SRC_POS_DECL The source position of this error.
 * @param   pszFormat       Format string.
 * @param   pArgs           Pointer to the format arguments.
 * @thread  EMT
 */
DECLCALLBACK(void) vmR3SetErrorV(PVM pVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list *pArgs)
{
#ifdef LOG_ENABLED
    /*
     * Log the error.
     */
    RTLogPrintf("VMSetError: %s(%d) %s\n", pszFile, iLine, pszFunction);
    va_list va3;
    va_copy(va3, *pArgs);
    RTLogPrintfV(pszFormat, va3);
    va_end(va3);
#endif

    /*
     * Make a copy of the message.
     */
    vmSetErrorCopy(pVM, rc, RT_SRC_POS_ARGS, pszFormat, *pArgs);

    /*
     * Call the at error callbacks.
     */
    for (PVMATERROR pCur = pVM->vm.s.pAtError; pCur; pCur = pCur->pNext)
    {
        va_list va2;
        va_copy(va2, *pArgs);
        pCur->pfnAtError(pVM, pCur->pvUser, rc, RT_SRC_POS_ARGS, pszFormat, va2);
        va_end(va2);
    }
}


/**
 * Registers a VM runtime error callback.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pfnAtRuntimeError   Pointer to callback.
 * @param   pvUser              User argument.
 * @thread  Any.
 */
VMR3DECL(int)   VMR3AtRuntimeErrorRegister(PVM pVM, PFNVMATRUNTIMEERROR pfnAtRuntimeError, void *pvUser)
{
    LogFlow(("VMR3AtRuntimeErrorRegister: pfnAtRuntimeError=%p pvUser=%p\n", pfnAtRuntimeError, pvUser));

    /*
     * Validate input.
     */
    if (!pfnAtRuntimeError)
    {
        AssertMsgFailed(("callback is required\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Make sure we're in EMT (to avoid the logging).
     */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3AtRuntimeErrorRegister, 3, pVM, pfnAtRuntimeError, pvUser);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pReq->iStatus;
    VMR3ReqFree(pReq);

    LogFlow(("VMR3AtRuntimeErrorRegister: returns %Vrc\n", rc));
    return rc;
}


/**
 * Registers a VM runtime error callback.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pfnAtRuntimeError   Pointer to callback.
 * @param   pvUser              User argument.
 * @thread  EMT
 */
static DECLCALLBACK(int)    vmR3AtRuntimeErrorRegister(PVM pVM, PFNVMATRUNTIMEERROR pfnAtRuntimeError, void *pvUser)
{
    /*
     * Allocate a new record.
     */

    PVMATRUNTIMEERROR pNew = (PVMATRUNTIMEERROR)MMR3HeapAlloc(pVM, MM_TAG_VM, sizeof(*pNew));
    if (!pNew)
        return VERR_NO_MEMORY;

    /* fill */
    pNew->pfnAtRuntimeError = pfnAtRuntimeError;
    pNew->pvUser            = pvUser;
    pNew->pNext             = NULL;

    /* insert */
    *pVM->vm.s.ppAtRuntimeErrorNext = pNew;
    pVM->vm.s.ppAtRuntimeErrorNext = &pNew->pNext;

    return VINF_SUCCESS;
}


/**
 * Deregisters a VM runtime error callback.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pfnAtRuntimeError   Pointer to callback.
 * @param   pvUser              User argument.
 * @thread  Any.
 */
VMR3DECL(int)   VMR3AtRuntimeErrorDeregister(PVM pVM, PFNVMATRUNTIMEERROR pfnAtRuntimeError, void *pvUser)
{
    LogFlow(("VMR3AtRuntimeErrorDeregister: pfnAtRuntimeError=%p pvUser=%p\n", pfnAtRuntimeError, pvUser));

    /*
     * Validate input.
     */
    if (!pfnAtRuntimeError)
    {
        AssertMsgFailed(("callback is required\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Make sure we're in EMT (to avoid the logging).
     */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3AtRuntimeErrorDeregister, 3, pVM, pfnAtRuntimeError, pvUser);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pReq->iStatus;
    VMR3ReqFree(pReq);

    LogFlow(("VMR3AtRuntimeErrorDeregister: returns %Vrc\n", rc));
    return rc;
}


/**
 * Deregisters a VM runtime error callback.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pfnAtRuntimeError   Pointer to callback.
 * @param   pvUser              User argument.
 * @thread  EMT
 */
static DECLCALLBACK(int)    vmR3AtRuntimeErrorDeregister(PVM pVM, PFNVMATRUNTIMEERROR pfnAtRuntimeError, void *pvUser)
{
    LogFlow(("vmR3AtRuntimeErrorDeregister: pfnAtRuntimeError=%p pvUser=%p\n", pfnAtRuntimeError, pvUser));

    /*
     * Search the list for the entry.
     */
    PVMATRUNTIMEERROR pPrev = NULL;
    PVMATRUNTIMEERROR pCur = pVM->vm.s.pAtRuntimeError;
    while (     pCur
           &&   pCur->pfnAtRuntimeError == pfnAtRuntimeError
           &&   pCur->pvUser == pvUser)
    {
        pPrev = pCur;
        pCur = pCur->pNext;
    }
    if (!pCur)
    {
        AssertMsgFailed(("pfnAtRuntimeError=%p was not found\n", pfnAtRuntimeError));
        return VERR_FILE_NOT_FOUND;
    }

    /*
     * Unlink it.
     */
    if (pPrev)
    {
        pPrev->pNext = pCur->pNext;
        if (!pCur->pNext)
            pVM->vm.s.ppAtRuntimeErrorNext = &pPrev->pNext;
    }
    else
    {
        pVM->vm.s.pAtRuntimeError = pCur->pNext;
        if (!pCur->pNext)
            pVM->vm.s.ppAtRuntimeErrorNext = &pVM->vm.s.pAtRuntimeError;
    }

    /*
     * Free it.
     */
    pCur->pfnAtRuntimeError = NULL;
    pCur->pNext = NULL;
    MMR3HeapFree(pCur);

    return VINF_SUCCESS;
}


/**
 * Ellipsis to va_list wrapper for calling pfnAtRuntimeError.
 */
static void vmR3SetRuntimeErrorWorkerDoCall(PVM pVM, PVMATRUNTIMEERROR pCur, bool fFatal,
                                            const char *pszErrorID,
                                            const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    pCur->pfnAtRuntimeError(pVM, pCur->pvUser, fFatal, pszErrorID, pszFormat, va);
    va_end(va);
}


/**
 * This is a worker function for GC and Ring-0 calls to VMSetError and VMSetErrorV.
 * The message is found in VMINT.
 *
 * @param   pVM             The VM handle.
 * @thread  EMT.
 */
VMR3DECL(void) VMR3SetRuntimeErrorWorker(PVM pVM)
{
    VM_ASSERT_EMT(pVM);
    AssertReleaseMsgFailed(("And we have a winner! You get to implement Ring-0 and GC VMSetRuntimeErrorV! Contrats!\n"));

    /*
     * Unpack the error (if we managed to format one).
     */
    PVMRUNTIMEERROR pErr = pVM->vm.s.pRuntimeErrorR3;
    const char *pszErrorID = NULL;
    const char *pszMessage;
    bool        fFatal = false;
    if (pErr)
    {
        AssertCompile(sizeof(const char) == sizeof(uint8_t));
        if (pErr->offErrorID)
            pszErrorID = (const char *)pErr + pErr->offErrorID;
        if (pErr->offMessage)
            pszMessage = (const char *)pErr + pErr->offMessage;
        else
            pszMessage = "No message!";
        fFatal = pErr->fFatal;
    }
    else
        pszMessage = "No message! (Failed to allocate memory to put the error message in!)";

    /*
     * Call the at runtime error callbacks.
     */
    for (PVMATRUNTIMEERROR pCur = pVM->vm.s.pAtRuntimeError; pCur; pCur = pCur->pNext)
        vmR3SetRuntimeErrorWorkerDoCall(pVM, pCur, fFatal, pszErrorID, "%s", pszMessage);
}


/**
 * Worker which calls everyone listening to the VM runtime error messages.
 *
 * @param   pVM             The VM handle.
 * @param   fFatal          Whether it is a fatal error or not.
 * @param   pszErrorID      Error ID string.
 * @param   pszFormat       Format string.
 * @param   pArgs           Pointer to the format arguments.
 * @thread  EMT
 */
DECLCALLBACK(void) vmR3SetRuntimeErrorV(PVM pVM, bool fFatal,
                                        const char *pszErrorID,
                                        const char *pszFormat, va_list *pArgs)
{
    /*
     * Make a copy of the message.
     */
    vmSetRuntimeErrorCopy(pVM, fFatal, pszErrorID, pszFormat, *pArgs);

    /*
     * Call the at error callbacks.
     */
    for (PVMATRUNTIMEERROR pCur = pVM->vm.s.pAtRuntimeError; pCur; pCur = pCur->pNext)
    {
        va_list va2;
        va_copy(va2, *pArgs);
        pCur->pfnAtRuntimeError(pVM, pCur->pvUser, fFatal, pszErrorID, pszFormat, va2);
        va_end(va2);
    }
}

