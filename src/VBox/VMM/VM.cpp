/* $Id$ */
/** @file
 * VM - Virtual Machine
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

/** @page pg_vm     VM API
 *
 * This is the encapsulating bit.  It provides the APIs that Main and VBoxBFE
 * use to create a VMM instance for running a guest in.  It also provides
 * facilities for queuing request for execution in EMT (serialization purposes
 * mostly) and for reporting error back to the VMM user (Main/VBoxBFE).
 *
 *
 * @section sec_vm_design   Design Critique / Things To Do
 *
 * In hindsight this component is a big design mistake, all this stuff really
 * belongs in the VMM component.  It just seemed like a kind of ok idea at a
 * time when the VMM bit was a bit vague.  'VM' also happend to be the name of
 * the per-VM instance structure (see vm.h), so it kind of made sense.  However
 * as it turned out, VMM(.cpp) is almost empty all it provides in ring-3 is some
 * minor functionally and some "routing" services.
 *
 * Fixing this is just a matter of some more or less straight forward
 * refactoring, the question is just when someone will get to it.
 *
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
#ifdef VBOX_WITH_VMI
# include <VBox/parav.h>
#endif
#include <VBox/csam.h>
#include <VBox/iom.h>
#include <VBox/ssm.h>
#include <VBox/hwaccm.h>
#include "VMInternal.h"
#include <VBox/vm.h>
#include <VBox/uvm.h>

#include <VBox/sup.h>
#include <VBox/dbg.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <iprt/env.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>


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
static PUVM         g_pUVMsHead = NULL;

/** Pointer to the list of at VM destruction callbacks. */
static PVMATDTOR    g_pVMAtDtorHead = NULL;
/** Lock the g_pVMAtDtorHead list. */
#define VM_ATDTOR_LOCK() do { } while (0)
/** Unlock the g_pVMAtDtorHead list. */
#define VM_ATDTOR_UNLOCK() do { } while (0)


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int               vmR3CreateUVM(uint32_t cCPUs, PUVM *ppUVM);
static int               vmR3CreateU(PUVM pUVM, uint32_t cCPUs, PFNCFGMCONSTRUCTOR pfnCFGMConstructor, void *pvUserCFGM);
static int               vmR3InitRing3(PVM pVM, PUVM pUVM);
static int               vmR3InitVMCpu(PVM pVM);
static int               vmR3InitRing0(PVM pVM);
static int               vmR3InitGC(PVM pVM);
static int               vmR3InitDoCompleted(PVM pVM, VMINITCOMPLETED enmWhat);
static DECLCALLBACK(int) vmR3PowerOn(PVM pVM);
static DECLCALLBACK(int) vmR3Suspend(PVM pVM);
static DECLCALLBACK(int) vmR3Resume(PVM pVM);
static DECLCALLBACK(int) vmR3Save(PVM pVM, const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser);
static DECLCALLBACK(int) vmR3Load(PVM pVM, const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser);
static DECLCALLBACK(int) vmR3PowerOff(PVM pVM);
static void              vmR3DestroyUVM(PUVM pUVM, uint32_t cMilliesEMTWait);
static void              vmR3AtDtor(PVM pVM);
static int               vmR3AtResetU(PUVM pUVM);
static DECLCALLBACK(int) vmR3Reset(PVM pVM);
static DECLCALLBACK(int) vmR3AtStateRegisterU(PUVM pUVM, PFNVMATSTATE pfnAtState, void *pvUser);
static DECLCALLBACK(int) vmR3AtStateDeregisterU(PUVM pUVM, PFNVMATSTATE pfnAtState, void *pvUser);
static DECLCALLBACK(int) vmR3AtErrorRegisterU(PUVM pUVM, PFNVMATERROR pfnAtError, void *pvUser);
static DECLCALLBACK(int) vmR3AtErrorDeregisterU(PUVM pUVM, PFNVMATERROR pfnAtError, void *pvUser);
static int               vmR3SetErrorU(PUVM pUVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...);
static DECLCALLBACK(int) vmR3AtRuntimeErrorRegisterU(PUVM pUVM, PFNVMATRUNTIMEERROR pfnAtRuntimeError, void *pvUser);
static DECLCALLBACK(int) vmR3AtRuntimeErrorDeregisterU(PUVM pUVM, PFNVMATRUNTIMEERROR pfnAtRuntimeError, void *pvUser);


/**
 * Do global VMM init.
 *
 * @returns VBox status code.
 */
VMMR3DECL(int)   VMR3GlobalInit(void)
{
    /*
     * Only once.
     */
    static bool volatile s_fDone = false;
    if (s_fDone)
        return VINF_SUCCESS;

    /*
     * We're done.
     */
    s_fDone = true;
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
 * @param   cCPUs               Number of virtual CPUs for the new VM.
 * @param   pfnVMAtError        Pointer to callback function for setting VM
 *                              errors. This was added as an implicit call to
 *                              VMR3AtErrorRegister() since there is no way the
 *                              caller can get to the VM handle early enough to
 *                              do this on its own.
 *                              This is called in the context of an EMT.
 * @param   pvUserVM            The user argument passed to pfnVMAtError.
 * @param   pfnCFGMConstructor  Pointer to callback function for constructing the VM configuration tree.
 *                              This is called in the context of an EMT0.
 * @param   pvUserCFGM          The user argument passed to pfnCFGMConstructor.
 * @param   ppVM                Where to store the 'handle' of the created VM.
 */
VMMR3DECL(int)   VMR3Create(uint32_t cCPUs, PFNVMATERROR pfnVMAtError, void *pvUserVM, PFNCFGMCONSTRUCTOR pfnCFGMConstructor, void *pvUserCFGM, PVM *ppVM)
{
    LogFlow(("VMR3Create: cCPUs=%RU32 pfnVMAtError=%p pvUserVM=%p  pfnCFGMConstructor=%p pvUserCFGM=%p ppVM=%p\n", cCPUs, pfnVMAtError, pvUserVM, pfnCFGMConstructor, pvUserCFGM, ppVM));

    /*
     * Because of the current hackiness of the applications
     * we'll have to initialize global stuff from here.
     * Later the applications will take care of this in a proper way.
     */
    static bool fGlobalInitDone = false;
    if (!fGlobalInitDone)
    {
        int rc = VMR3GlobalInit();
        if (RT_FAILURE(rc))
            return rc;
        fGlobalInitDone = true;
    }

    /*
     * Validate input.
     */
    AssertLogRelMsgReturn(cCPUs > 0 && cCPUs <= VMM_MAX_CPU_COUNT, ("%RU32\n", cCPUs), VERR_INVALID_PARAMETER);

    /*
     * Create the UVM so we can register the at-error callback
     * and consoliate a bit of cleanup code.
     */
    PUVM pUVM;
    int rc = vmR3CreateUVM(cCPUs, &pUVM);
    if (RT_FAILURE(rc))
        return rc;
    if (pfnVMAtError)
        rc = VMR3AtErrorRegisterU(pUVM, pfnVMAtError, pvUserVM);
    if (RT_SUCCESS(rc))
    {
        /*
         * Initialize the support library creating the session for this VM.
         */
        rc = SUPR3Init(&pUVM->vm.s.pSession);
        if (RT_SUCCESS(rc))
        {
            /*
             * Call vmR3CreateU in the EMT thread and wait for it to finish.
             *
             * Note! VMCPUID_ANY is used here because VMR3ReqQueueU would have trouble
             *       submitting a request to a specific VCPU without a pVM. So, to make
             *       sure init is running on EMT(0), vmR3EmulationThreadWithId makes sure
             *       that only EMT(0) is servicing VMCPUID_ANY requests when pVM is NULL.
             */
            PVMREQ pReq;
            rc = VMR3ReqCallU(pUVM, VMCPUID_ANY, &pReq, RT_INDEFINITE_WAIT, 0, (PFNRT)vmR3CreateU, 4,
                              pUVM, cCPUs, pfnCFGMConstructor, pvUserCFGM);
            if (RT_SUCCESS(rc))
            {
                rc = pReq->iStatus;
                VMR3ReqFree(pReq);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Success!
                     */
                    *ppVM = pUVM->pVM;
                    LogFlow(("VMR3Create: returns VINF_SUCCESS *ppVM=%p\n", *ppVM));
                    return VINF_SUCCESS;
                }
            }
            else
                AssertMsgFailed(("VMR3ReqCall failed rc=%Rrc\n", rc));

            /*
             * An error occurred during VM creation. Set the error message directly
             * using the initial callback, as the callback list doesn't exist yet.
             */
            const char *pszError = NULL;
            switch (rc)
            {
                case VERR_VMX_IN_VMX_ROOT_MODE:
#ifdef RT_OS_LINUX
                    pszError = N_("VirtualBox can't operate in VMX root mode. "
                                  "Please disable the KVM kernel extension, recompile your kernel and reboot");
#else
                    pszError = N_("VirtualBox can't operate in VMX root mode. Please close all other virtualization programs.");
#endif
                    break;

                case VERR_VERSION_MISMATCH:
                    pszError = N_("VMMR0 driver version mismatch. Please terminate all VMs, make sure that "
                                  "VBoxNetDHCP is not running and try again. If you still get this error, "
                                  "re-install VirtualBox");
                    break;

                default:
                    pszError = N_("Unknown error creating VM");
                    break;
            }
            vmR3SetErrorU(pUVM, rc, RT_SRC_POS, pszError, rc);
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
                    pszError = N_("VirtualBox kernel driver not loaded");
#endif
                    break;
                case VERR_VM_DRIVER_OPEN_ERROR:
                    pszError = N_("VirtualBox kernel driver cannot be opened");
                    break;
                case VERR_VM_DRIVER_NOT_ACCESSIBLE:
#ifdef VBOX_WITH_HARDENING
                    /* This should only happen if the executable wasn't hardened - bad code/build. */
                    pszError = N_("VirtualBox kernel driver not accessible, permission problem. "
                                  "Re-install VirtualBox. If you are building it yourself, you "
                                  "should make sure it installed correctly and that the setuid "
                                  "bit is set on the executables calling VMR3Create.");
#else
                    /* This should only happen when mixing builds or with the usual /dev/vboxdrv access issues. */
# if defined(RT_OS_DARWIN)
                    pszError = N_("VirtualBox KEXT is not accessible, permission problem. "
                                  "If you have built VirtualBox yourself, make sure that you do not "
                                  "have the vboxdrv KEXT from a different build or installation loaded.");
# elif defined(RT_OS_LINUX)
                    pszError = N_("VirtualBox kernel driver is not accessible, permission problem. "
                                  "If you have built VirtualBox yourself, make sure that you do "
                                  "not have the vboxdrv kernel module from a different build or "
                                  "installation loaded. Also, make sure the vboxdrv udev rule gives "
                                  "you the permission you need to access the device.");
# elif defined(RT_OS_WINDOWS)
                    pszError = N_("VirtualBox kernel driver is not accessible, permission problem.");
# else /* solaris, freebsd, ++. */
                    pszError = N_("VirtualBox kernel module is not accessible, permission problem. "
                                  "If you have built VirtualBox yourself, make sure that you do "
                                  "not have the vboxdrv kernel module from a different install loaded.");
# endif
#endif
                    break;
                case VERR_INVALID_HANDLE: /** @todo track down and fix this error. */
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
                    pszError = N_("Unknown error initializing kernel driver");
                    AssertMsgFailed(("Add error message for rc=%d (%Rrc)\n", rc, rc));
            }
            vmR3SetErrorU(pUVM, rc, RT_SRC_POS, pszError, rc);
        }
    }

    /* cleanup */
    vmR3DestroyUVM(pUVM, 2000);
    LogFlow(("VMR3Create: returns %Rrc\n", rc));
    return rc;
}


/**
 * Creates the UVM.
 *
 * This will not initialize the support library even if vmR3DestroyUVM
 * will terminate that.
 *
 * @returns VBox status code.
 * @param   cCpus   Number of virtual CPUs
 * @param   ppUVM   Where to store the UVM pointer.
 */
static int vmR3CreateUVM(uint32_t cCpus, PUVM *ppUVM)
{
    uint32_t i;

    /*
     * Create and initialize the UVM.
     */
    PUVM pUVM = (PUVM)RTMemAllocZ(RT_OFFSETOF(UVM, aCpus[cCpus]));
    AssertReturn(pUVM, VERR_NO_MEMORY);
    pUVM->u32Magic = UVM_MAGIC;
    pUVM->cCpus = cCpus;

    AssertCompile(sizeof(pUVM->vm.s) <= sizeof(pUVM->vm.padding));

    pUVM->vm.s.ppAtResetNext = &pUVM->vm.s.pAtReset;
    pUVM->vm.s.ppAtStateNext = &pUVM->vm.s.pAtState;
    pUVM->vm.s.ppAtErrorNext = &pUVM->vm.s.pAtError;
    pUVM->vm.s.ppAtRuntimeErrorNext = &pUVM->vm.s.pAtRuntimeError;

    pUVM->vm.s.enmHaltMethod = VMHALTMETHOD_BOOTSTRAP;

    /* Initialize the VMCPU array in the UVM. */
    for (i = 0; i < cCpus; i++)
    {
        pUVM->aCpus[i].pUVM   = pUVM;
        pUVM->aCpus[i].idCpu  = i;
    }

    /* Allocate a TLS entry to store the VMINTUSERPERVMCPU pointer. */
    int rc = RTTlsAllocEx(&pUVM->vm.s.idxTLS, NULL);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        /* Allocate a halt method event semaphore for each VCPU. */
        for (i = 0; i < cCpus; i++)
        {
            rc = RTSemEventCreate(&pUVM->aCpus[i].vm.s.EventSemWait);
            if (RT_FAILURE(rc))
                break;
        }

        if (RT_SUCCESS(rc))
        {
            /*
             * Init fundamental (sub-)components - STAM, MMR3Heap and PDMLdr.
             */
            rc = STAMR3InitUVM(pUVM);
            if (RT_SUCCESS(rc))
            {
                rc = MMR3InitUVM(pUVM);
                if (RT_SUCCESS(rc))
                {
                    rc = PDMR3InitUVM(pUVM);
                    if (RT_SUCCESS(rc))
                    {
                        /*
                         * Start the emulation threads for all VMCPUs.
                         */
                        for (i = 0; i < cCpus; i++)
                        {
                            rc = RTThreadCreate(&pUVM->aCpus[i].vm.s.ThreadEMT, vmR3EmulationThread, &pUVM->aCpus[i], _1M,
                                                RTTHREADTYPE_EMULATION, RTTHREADFLAGS_WAITABLE, "EMT");
                            if (RT_FAILURE(rc))
                                break;

                            pUVM->aCpus[i].vm.s.NativeThreadEMT = RTThreadGetNative(pUVM->aCpus[i].vm.s.ThreadEMT);
                        }

                        if (RT_SUCCESS(rc))
                        {
                            *ppUVM = pUVM;
                            return VINF_SUCCESS;
                        }

                        /* bail out. */
                        while (i-- > 0)
                        {
                            /** @todo rainy day: terminate the EMTs. */
                        }
                        PDMR3TermUVM(pUVM);
                    }
                    MMR3TermUVM(pUVM);
                }
                STAMR3TermUVM(pUVM);
            }
            for (i = 0; i < cCpus; i++)
            {
                RTSemEventDestroy(pUVM->aCpus[i].vm.s.EventSemWait);
                pUVM->aCpus[i].vm.s.EventSemWait = NIL_RTSEMEVENT;
            }
        }
        RTTlsFree(pUVM->vm.s.idxTLS);
    }
    RTMemFree(pUVM);
    return rc;
}


/**
 * Creates and initializes the VM.
 *
 * @thread EMT
 */
static int vmR3CreateU(PUVM pUVM, uint32_t cCpus, PFNCFGMCONSTRUCTOR pfnCFGMConstructor, void *pvUserCFGM)
{
    int rc = VINF_SUCCESS;

    /*
     * Load the VMMR0.r0 module so that we can call GVMMR0CreateVM.
     */
    rc = PDMR3LdrLoadVMMR0U(pUVM);
    if (RT_FAILURE(rc))
    {
        /** @todo we need a cleaner solution for this (VERR_VMX_IN_VMX_ROOT_MODE).
          * bird: what about moving the message down here? Main picks the first message, right? */
        if (rc == VERR_VMX_IN_VMX_ROOT_MODE)
            return rc;  /* proper error message set later on */
        return vmR3SetErrorU(pUVM, rc, RT_SRC_POS, N_("Failed to load VMMR0.r0"));
    }

    /*
     * Request GVMM to create a new VM for us.
     */
    GVMMCREATEVMREQ CreateVMReq;
    CreateVMReq.Hdr.u32Magic    = SUPVMMR0REQHDR_MAGIC;
    CreateVMReq.Hdr.cbReq       = sizeof(CreateVMReq);
    CreateVMReq.pSession        = pUVM->vm.s.pSession;
    CreateVMReq.pVMR0           = NIL_RTR0PTR;
    CreateVMReq.pVMR3           = NULL;
    CreateVMReq.cCpus           = cCpus;
    rc = SUPCallVMMR0Ex(NIL_RTR0PTR, 0 /* VCPU 0 */, VMMR0_DO_GVMM_CREATE_VM, 0, &CreateVMReq.Hdr);
    if (RT_SUCCESS(rc))
    {
        PVM pVM = pUVM->pVM = CreateVMReq.pVMR3;
        AssertRelease(VALID_PTR(pVM));
        AssertRelease(pVM->pVMR0 == CreateVMReq.pVMR0);
        AssertRelease(pVM->pSession == pUVM->vm.s.pSession);
        AssertRelease(pVM->cCPUs == cCpus);
        AssertRelease(pVM->offVMCPU == RT_UOFFSETOF(VM, aCpus));

        Log(("VMR3Create: Created pUVM=%p pVM=%p pVMR0=%p hSelf=%#x cCPUs=%RU32\n",
             pUVM, pVM, pVM->pVMR0, pVM->hSelf, pVM->cCPUs));

        /*
         * Initialize the VM structure and our internal data (VMINT).
         */
        pVM->pUVM = pUVM;

        for (uint32_t i = 0; i < pVM->cCPUs; i++)
        {
            pVM->aCpus[i].pUVCpu        = &pUVM->aCpus[i];
            pVM->aCpus[i].idCpu         = i;
            pVM->aCpus[i].hNativeThread = pUVM->aCpus[i].vm.s.NativeThreadEMT;
            Assert(pVM->aCpus[i].hNativeThread != NIL_RTNATIVETHREAD);

            pUVM->aCpus[i].pVCpu        = &pVM->aCpus[i];
            pUVM->aCpus[i].pVM          = pVM;
        }


        /*
         * Init the configuration.
         */
        rc = CFGMR3Init(pVM, pfnCFGMConstructor, pvUserCFGM);
        if (RT_SUCCESS(rc))
        {
            rc = CFGMR3QueryBoolDef(CFGMR3GetRoot(pVM), "HwVirtExtForced", &pVM->fHwVirtExtForced, false);
            if (RT_SUCCESS(rc) && pVM->fHwVirtExtForced)
                pVM->fHWACCMEnabled = true;

            /*
             * If executing in fake suplib mode disable RR3 and RR0 in the config.
             */
            const char *psz = RTEnvGet("VBOX_SUPLIB_FAKE");
            if (psz && !strcmp(psz, "fake"))
            {
                CFGMR3RemoveValue(CFGMR3GetRoot(pVM), "RawR3Enabled");
                CFGMR3InsertInteger(CFGMR3GetRoot(pVM), "RawR3Enabled", 0);
                CFGMR3RemoveValue(CFGMR3GetRoot(pVM), "RawR0Enabled");
                CFGMR3InsertInteger(CFGMR3GetRoot(pVM), "RawR0Enabled", 0);
            }

            /*
             * Make sure the CPU count in the config data matches.
             */
            if (RT_SUCCESS(rc))
            {
                uint32_t cCPUsCfg;
                rc = CFGMR3QueryU32Def(CFGMR3GetRoot(pVM), "NumCPUs", &cCPUsCfg, 1);
                AssertLogRelMsgRC(rc, ("Configuration error: Querying \"NumCPUs\" as integer failed, rc=%Rrc\n", rc));
                if (RT_SUCCESS(rc) && cCPUsCfg != cCpus)
                {
                    AssertLogRelMsgFailed(("Configuration error: \"NumCPUs\"=%RU32 and VMR3CreateVM::cCPUs=%RU32 does not match!\n",
                                           cCPUsCfg, cCpus));
                    rc = VERR_INVALID_PARAMETER;
                }
            }
            if (RT_SUCCESS(rc))
            {
                /*
                 * Init the ring-3 components and ring-3 per cpu data, finishing it off
                 * by a relocation round (intermediate context finalization will do this).
                 */
                rc = vmR3InitRing3(pVM, pUVM);
                if (RT_SUCCESS(rc))
                {
                    rc = vmR3InitVMCpu(pVM);
                    if (RT_SUCCESS(rc))
                        rc = PGMR3FinalizeMappings(pVM);
                    if (RT_SUCCESS(rc))
                    {

                        LogFlow(("Ring-3 init succeeded\n"));

                        /*
                         * Init the Ring-0 components.
                         */
                        rc = vmR3InitRing0(pVM);
                        if (RT_SUCCESS(rc))
                        {
                            /* Relocate again, because some switcher fixups depends on R0 init results. */
                            VMR3Relocate(pVM, 0);

#ifdef VBOX_WITH_DEBUGGER
                            /*
                             * Init the tcp debugger console if we're building
                             * with debugger support.
                             */
                            void *pvUser = NULL;
                            rc = DBGCTcpCreate(pVM, &pvUser);
                            if (    RT_SUCCESS(rc)
                                ||  rc == VERR_NET_ADDRESS_IN_USE)
                            {
                                pUVM->vm.s.pvDBGC = pvUser;
#endif
                                /*
                                 * Init the Guest Context components.
                                 */
                                rc = vmR3InitGC(pVM);
                                if (RT_SUCCESS(rc))
                                {
                                    /*
                                    * Now we can safely set the VM halt method to default.
                                    */
                                    rc = vmR3SetHaltMethodU(pUVM, VMHALTMETHOD_DEFAULT);
                                    if (RT_SUCCESS(rc))
                                    {
                                        /*
                                        * Set the state and link into the global list.
                                        */
                                        vmR3SetState(pVM, VMSTATE_CREATED);
                                        pUVM->pNext = g_pUVMsHead;
                                        g_pUVMsHead = pUVM;
                                        return VINF_SUCCESS;
                                    }
                                }
#ifdef VBOX_WITH_DEBUGGER
                                DBGCTcpTerminate(pVM, pUVM->vm.s.pvDBGC);
                                pUVM->vm.s.pvDBGC = NULL;
                            }
#endif
                            //..
                        }
                    }
                    vmR3Destroy(pVM);
                }
            }
            //..

            /* Clean CFGM. */
            int rc2 = CFGMR3Term(pVM);
            AssertRC(rc2);
        }

        /* Tell GVMM that it can destroy the VM now. */
        int rc2 = SUPCallVMMR0Ex(CreateVMReq.pVMR0, 0 /* VCPU 0 */, VMMR0_DO_GVMM_DESTROY_VM, 0, NULL);
        AssertRC(rc2);
        pUVM->pVM = NULL;
    }
    else
        vmR3SetErrorU(pUVM, rc, RT_SRC_POS, N_("VM creation failed (GVMM)"));

    LogFlow(("vmR3Create: returns %Rrc\n", rc));
    return rc;
}

/**
 * Register the calling EMT with GVM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   idCpu       The Virtual CPU ID.
 */
static DECLCALLBACK(int) vmR3RegisterEMT(PVM pVM, VMCPUID idCpu)
{
    Assert(VMMGetCpuId(pVM) == idCpu);
    int rc = SUPCallVMMR0Ex(pVM->pVMR0, idCpu, VMMR0_DO_GVMM_REGISTER_VMCPU, 0, NULL);
    if (RT_FAILURE(rc))
        LogRel(("idCpu=%u rc=%Rrc\n", idCpu, rc));
    return rc;
}


/**
 * Initializes all R3 components of the VM
 */
static int vmR3InitRing3(PVM pVM, PUVM pUVM)
{
    int rc;

    /*
     * Register the other EMTs with GVM.
     */
    for (VMCPUID idCpu = 1; idCpu < pVM->cCPUs; idCpu++)
    {
        PVMREQ pReq;
        rc = VMR3ReqCallU(pUVM, idCpu, &pReq, RT_INDEFINITE_WAIT, 0 /*fFlags*/,
                          (PFNRT)vmR3RegisterEMT, 2, pVM, idCpu);
        if (RT_SUCCESS(rc))
            rc = pReq->iStatus;
        VMR3ReqFree(pReq);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Init all R3 components, the order here might be important.
     */
    rc = MMR3Init(pVM);
    if (RT_SUCCESS(rc))
    {
        STAM_REG(pVM, &pVM->StatTotalInGC,          STAMTYPE_PROFILE_ADV, "/PROF/VM/InGC",          STAMUNIT_TICKS_PER_CALL,    "Profiling the total time spent in GC.");
        STAM_REG(pVM, &pVM->StatSwitcherToGC,       STAMTYPE_PROFILE_ADV, "/PROF/VM/SwitchToGC",    STAMUNIT_TICKS_PER_CALL,    "Profiling switching to GC.");
        STAM_REG(pVM, &pVM->StatSwitcherToHC,       STAMTYPE_PROFILE_ADV, "/PROF/VM/SwitchToHC",    STAMUNIT_TICKS_PER_CALL,    "Profiling switching to HC.");
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

        for (unsigned iCpu=0;iCpu<pVM->cCPUs;iCpu++)
        {
            rc = STAMR3RegisterF(pVM, &pUVM->aCpus[iCpu].vm.s.StatHaltYield,  STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling halted state yielding.", "/PROF/VM/CPU%d/Halt/Yield", iCpu);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pUVM->aCpus[iCpu].vm.s.StatHaltBlock,  STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling halted state blocking.", "/PROF/VM/CPU%d/Halt/Block", iCpu);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pUVM->aCpus[iCpu].vm.s.StatHaltTimers, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling halted state timer tasks.", "/PROF/VM/CPU%d/Halt/Timers", iCpu);
            AssertRC(rc);
        }

        STAM_REG(pVM, &pUVM->vm.s.StatReqAllocNew,   STAMTYPE_COUNTER,     "/VM/Req/AllocNew",       STAMUNIT_OCCURENCES,        "Number of VMR3ReqAlloc returning a new packet.");
        STAM_REG(pVM, &pUVM->vm.s.StatReqAllocRaces, STAMTYPE_COUNTER,     "/VM/Req/AllocRaces",     STAMUNIT_OCCURENCES,        "Number of VMR3ReqAlloc causing races.");
        STAM_REG(pVM, &pUVM->vm.s.StatReqAllocRecycled, STAMTYPE_COUNTER,  "/VM/Req/AllocRecycled",  STAMUNIT_OCCURENCES,        "Number of VMR3ReqAlloc returning a recycled packet.");
        STAM_REG(pVM, &pUVM->vm.s.StatReqFree,       STAMTYPE_COUNTER,     "/VM/Req/Free",           STAMUNIT_OCCURENCES,        "Number of VMR3ReqFree calls.");
        STAM_REG(pVM, &pUVM->vm.s.StatReqFreeOverflow, STAMTYPE_COUNTER,   "/VM/Req/FreeOverflow",   STAMUNIT_OCCURENCES,        "Number of times the request was actually freed.");

        rc = CPUMR3Init(pVM);
        if (RT_SUCCESS(rc))
        {
            rc = HWACCMR3Init(pVM);
            if (RT_SUCCESS(rc))
            {
                rc = PGMR3Init(pVM);
                if (RT_SUCCESS(rc))
                {
                    rc = REMR3Init(pVM);
                    if (RT_SUCCESS(rc))
                    {
                        rc = MMR3InitPaging(pVM);
                        if (RT_SUCCESS(rc))
                            rc = TMR3Init(pVM);
                        if (RT_SUCCESS(rc))
                        {
                            rc = VMMR3Init(pVM);
                            if (RT_SUCCESS(rc))
                            {
                                rc = SELMR3Init(pVM);
                                if (RT_SUCCESS(rc))
                                {
                                    rc = TRPMR3Init(pVM);
                                    if (RT_SUCCESS(rc))
                                    {
                                        rc = CSAMR3Init(pVM);
                                        if (RT_SUCCESS(rc))
                                        {
                                            rc = PATMR3Init(pVM);
                                            if (RT_SUCCESS(rc))
                                            {
#ifdef VBOX_WITH_VMI
                                                rc = PARAVR3Init(pVM);
                                                if (RT_SUCCESS(rc))
                                                {
#endif
                                                    rc = IOMR3Init(pVM);
                                                    if (RT_SUCCESS(rc))
                                                    {
                                                        rc = EMR3Init(pVM);
                                                        if (RT_SUCCESS(rc))
                                                        {
                                                            rc = DBGFR3Init(pVM);
                                                            if (RT_SUCCESS(rc))
                                                            {
                                                                rc = PDMR3Init(pVM);
                                                                if (RT_SUCCESS(rc))
                                                                {
                                                                    rc = PGMR3InitDynMap(pVM);
                                                                    if (RT_SUCCESS(rc))
                                                                        rc = MMR3HyperInitFinalize(pVM);
                                                                    if (RT_SUCCESS(rc))
                                                                        rc = PATMR3InitFinalize(pVM);
                                                                    if (RT_SUCCESS(rc))
                                                                        rc = PGMR3InitFinalize(pVM);
                                                                    if (RT_SUCCESS(rc))
                                                                        rc = SELMR3InitFinalize(pVM);
                                                                    if (RT_SUCCESS(rc))
                                                                        rc = TMR3InitFinalize(pVM);
                                                                    if (RT_SUCCESS(rc))
                                                                        rc = VMMR3InitFinalize(pVM);
                                                                    if (RT_SUCCESS(rc))
                                                                        rc = REMR3InitFinalize(pVM);
                                                                    if (RT_SUCCESS(rc))
                                                                        rc = vmR3InitDoCompleted(pVM, VMINITCOMPLETED_RING3);
                                                                    if (RT_SUCCESS(rc))
                                                                    {
                                                                        LogFlow(("vmR3InitRing3: returns %Rrc\n", VINF_SUCCESS));
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
#ifdef VBOX_WITH_VMI
                                                    int rc2 = PARAVR3Term(pVM);
                                                    AssertRC(rc2);
                                                }
#endif
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

    LogFlow(("vmR3InitRing3: returns %Rrc\n", rc));
    return rc;
}


/**
 * Initializes all VM CPU components of the VM
 */
static int vmR3InitVMCpu(PVM pVM)
{
    int rc = VINF_SUCCESS;
    int rc2;

    rc = CPUMR3InitCPU(pVM);
    if (RT_SUCCESS(rc))
    {
        rc = HWACCMR3InitCPU(pVM);
        if (RT_SUCCESS(rc))
        {
            rc = PGMR3InitCPU(pVM);
            if (RT_SUCCESS(rc))
            {
                rc = TMR3InitCPU(pVM);
                if (RT_SUCCESS(rc))
                {
                    rc = VMMR3InitCPU(pVM);
                    if (RT_SUCCESS(rc))
                    {
                        rc = EMR3InitCPU(pVM);
                        if (RT_SUCCESS(rc))
                        {
                            LogFlow(("vmR3InitVMCpu: returns %Rrc\n", VINF_SUCCESS));
                            return VINF_SUCCESS;
                        }

                        rc2 = VMMR3TermCPU(pVM);
                        AssertRC(rc2);
                    }
                    rc2 = TMR3TermCPU(pVM);
                    AssertRC(rc2);
                }
                rc2 = PGMR3TermCPU(pVM);
                AssertRC(rc2);
            }
            rc2 = HWACCMR3TermCPU(pVM);
            AssertRC(rc2);
        }
        rc2 = CPUMR3TermCPU(pVM);
        AssertRC(rc2);
    }
    LogFlow(("vmR3InitVMCpu: returns %Rrc\n", rc));
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
    const char *psz = RTEnvGet("VBOX_SUPLIB_FAKE");
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
    if (RT_SUCCESS(rc))
        rc = vmR3InitDoCompleted(pVM, VMINITCOMPLETED_RING0);

    /** todo: move this to the VMINITCOMPLETED_RING0 notification handler once implemented */
    if (RT_SUCCESS(rc))
        rc = HWACCMR3InitFinalizeR0(pVM);

    LogFlow(("vmR3InitRing0: returns %Rrc\n", rc));
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
    const char *psz = RTEnvGet("VBOX_SUPLIB_FAKE");
    if (!psz || strcmp(psz, "fake"))
    {
        /*
         * Call the VMMR0 component and let it do the init.
         */
        rc = VMMR3InitRC(pVM);
    }
    else
        Log(("vmR3InitGC: skipping because of VBOX_SUPLIB_FAKE=fake\n"));

    /*
     * Do notifications and return.
     */
    if (RT_SUCCESS(rc))
        rc = vmR3InitDoCompleted(pVM, VMINITCOMPLETED_GC);
    LogFlow(("vmR3InitGC: returns %Rrc\n", rc));
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
VMMR3DECL(void)   VMR3Relocate(PVM pVM, RTGCINTPTR offDelta)
{
    LogFlow(("VMR3Relocate: offDelta=%RGv\n", offDelta));

    /*
     * The order here is very important!
     */
    PGMR3Relocate(pVM, offDelta);
    PDMR3LdrRelocateU(pVM->pUVM, offDelta);
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
VMMR3DECL(int)   VMR3PowerOn(PVM pVM)
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
    int rc = VMR3ReqCall(pVM, 0 /* VCPU 0 */, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3PowerOn, 1, pVM);
    if (RT_SUCCESS(rc))
    {
        rc = pReq->iStatus;
        VMR3ReqFree(pReq);
    }

    LogFlow(("VMR3PowerOn: returns %Rrc\n", rc));
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
VMMR3DECL(int) VMR3Suspend(PVM pVM)
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
     * Request the operation in EMT. (in reverse order as VCPU 0 does the actual work)
     */
    PVMREQ pReq = NULL;
    int rc = VMR3ReqCall(pVM, VMCPUID_ALL_REVERSE, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3Suspend, 1, pVM);
    if (RT_SUCCESS(rc))
    {
        rc = pReq->iStatus;
        VMR3ReqFree(pReq);
    }
    else
        Assert(pReq == NULL);

    LogFlow(("VMR3Suspend: returns %Rrc\n", rc));
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
VMMR3DECL(int) VMR3SuspendNoSave(PVM pVM)
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

    PVMCPU pVCpu = VMMGetCpu(pVM);
    /* Only VCPU 0 does the actual work. */
    if (pVCpu->idCpu != 0)
        return VINF_EM_SUSPEND;

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
VMMR3DECL(int)   VMR3Resume(PVM pVM)
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
     * Request the operation in EMT. (in VCPU order as VCPU 0 does the actual work)
     */
    PVMREQ pReq = NULL;
    int rc = VMR3ReqCall(pVM, VMCPUID_ALL, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3Resume, 1, pVM);
    if (RT_SUCCESS(rc))
    {
        rc = pReq->iStatus;
        VMR3ReqFree(pReq);
    }
    else
        Assert(pReq == NULL);

    LogFlow(("VMR3Resume: returns %Rrc\n", rc));
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

    PVMCPU pVCpu = VMMGetCpu(pVM);
    /* Only VCPU 0 does the actual work. */
    if (pVCpu->idCpu != 0)
        return VINF_EM_RESUME;

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
VMMR3DECL(int) VMR3Save(PVM pVM, const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser)
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
    int rc = VMR3ReqCall(pVM, 0 /* VCPU 0 */, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3Save, 4, pVM, pszFilename, pfnProgress, pvUser);
    if (RT_SUCCESS(rc))
    {
        rc = pReq->iStatus;
        VMR3ReqFree(pReq);
    }

    LogFlow(("VMR3Save: returns %Rrc\n", rc));
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
VMMR3DECL(int)   VMR3Load(PVM pVM, const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser)
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
    int rc = VMR3ReqCall(pVM, 0 /* VCPU 0 */, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3Load, 4, pVM, pszFilename, pfnProgress, pvUser);
    if (RT_SUCCESS(rc))
    {
        rc = pReq->iStatus;
        VMR3ReqFree(pReq);
    }

    LogFlow(("VMR3Load: returns %Rrc\n", rc));
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
    if (RT_SUCCESS(rc))
    {
        /* Not paranoia anymore; the saved guest might use different hypervisor selectors. We must call VMR3Relocate. */
        VMR3Relocate(pVM, 0);
        vmR3SetState(pVM, VMSTATE_SUSPENDED);
    }
    else
    {
        vmR3SetState(pVM, VMSTATE_LOAD_FAILURE);
        rc = VMSetError(pVM, rc, RT_SRC_POS, N_("Unable to restore the virtual machine's saved state from '%s'.  It may be damaged or from an older version of VirtualBox.  Please discard the saved state before starting the virtual machine"), pszFilename);
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
 * @vmstate     Suspended, Running, Guru Meditation, Load Failure
 * @vmstateto   Off
 */
VMMR3DECL(int)   VMR3PowerOff(PVM pVM)
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
     * Request the operation in EMT. (in reverse order as VCPU 0 does the actual work)
     */
    PVMREQ pReq = NULL;
    int rc = VMR3ReqCall(pVM, VMCPUID_ALL_REVERSE, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3PowerOff, 1, pVM);
    if (RT_SUCCESS(rc))
    {
        rc = pReq->iStatus;
        VMR3ReqFree(pReq);
    }
    else
        Assert(pReq == NULL);

    LogFlow(("VMR3PowerOff: returns %Rrc\n", rc));
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

    PVMCPU pVCpu = VMMGetCpu(pVM);
    /* Only VCPU 0 does the actual work. */
    if (pVCpu->idCpu != 0)
        return VINF_EM_TERMINATE;

    /*
     * For debugging purposes, we will log a summary of the guest state at this point.
     */
    if (pVM->enmVMState != VMSTATE_GURU_MEDITATION)
    {
        /* @todo SMP support? */
        PVMCPU pVCpu = VMMGetCpu(pVM);

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
        uint32_t esp = CPUMGetGuestESP(pVCpu);
        if (    CPUMGetGuestSS(pVCpu) == 0
            &&  esp < _64K)
        {
            uint8_t abBuf[PAGE_SIZE];
            RTLogRelPrintf("***\n"
                           "ss:sp=0000:%04x ", esp);
            uint32_t Start = esp & ~(uint32_t)63;
            int rc = PGMPhysSimpleReadGCPhys(pVM, abBuf, Start, 0x100);
            if (RT_SUCCESS(rc))
                RTLogRelPrintf("0000:%04x TO 0000:%04x:\n"
                               "%.*Rhxd\n",
                               Start, Start + 0x100 - 1,
                               0x100, abBuf);
            else
                RTLogRelPrintf("rc=%Rrc\n", rc);

            /* grub ... */
            if (esp < 0x2000 && esp > 0x1fc0)
            {
                rc = PGMPhysSimpleReadGCPhys(pVM, abBuf, 0x8000, 0x800);
                if (RT_SUCCESS(rc))
                    RTLogRelPrintf("0000:8000 TO 0000:87ff:\n"
                                   "%.*Rhxd\n",
                                   0x800, abBuf);
            }
            /* microsoft cdrom hang ... */
            if (true)
            {
                rc = PGMPhysSimpleReadGCPhys(pVM, abBuf, 0x8000, 0x200);
                if (RT_SUCCESS(rc))
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
 * @returns VBox status code.
 * @param   pVM     VM which should be destroyed.
 * @thread      Any thread but the emulation thread.
 * @vmstate     Off, Created
 * @vmstateto   N/A
 */
VMMR3DECL(int)   VMR3Destroy(PVM pVM)
{
    LogFlow(("VMR3Destroy: pVM=%p\n", pVM));

    /*
     * Validate input.
     */
    if (!pVM)
        return VERR_INVALID_PARAMETER;
    AssertPtrReturn(pVM, VERR_INVALID_POINTER);
    AssertMsgReturn(   pVM->enmVMState == VMSTATE_OFF
                    || pVM->enmVMState == VMSTATE_CREATED,
                    ("Invalid VM state %d\n", pVM->enmVMState),
                    VERR_VM_INVALID_VM_STATE);

    /*
     * Change VM state to destroying and unlink the VM.
     */
    vmR3SetState(pVM, VMSTATE_DESTROYING);

    /** @todo lock this when we start having multiple machines in a process... */
    PUVM pUVM = pVM->pUVM; AssertPtr(pUVM);
    if (g_pUVMsHead == pUVM)
        g_pUVMsHead = pUVM->pNext;
    else
    {
        PUVM pPrev = g_pUVMsHead;
        while (pPrev && pPrev->pNext != pUVM)
            pPrev = pPrev->pNext;
        AssertMsgReturn(pPrev, ("pUVM=%p / pVM=%p  is INVALID!\n", pUVM, pVM), VERR_INVALID_PARAMETER);

        pPrev->pNext = pUVM->pNext;
    }
    pUVM->pNext = NULL;

    /*
     * Notify registered at destruction listeners.
     * (That's the debugger console.)
     */
    vmR3AtDtor(pVM);

    /*
     * If we are the EMT of VCPU 0, then we'll delay the cleanup till later.
     */
    if (VMMGetCpuId(pVM) == 0)
    {
        pUVM->vm.s.fEMTDoesTheCleanup = true;
        pUVM->vm.s.fTerminateEMT = true;
        VM_FF_SET(pVM, VM_FF_TERMINATE);

        /* Inform all other VCPUs too. */
        for (VMCPUID idCpu = 1; idCpu < pVM->cCPUs; idCpu++)
        {
            /*
             * Request EMT to do the larger part of the destruction.
             */
            PVMREQ pReq = NULL;
            int rc = VMR3ReqCallU(pUVM, idCpu, &pReq, RT_INDEFINITE_WAIT, 0, (PFNRT)vmR3Destroy, 1, pVM);
            if (RT_SUCCESS(rc))
                rc = pReq->iStatus;
            AssertRC(rc);
            VMR3ReqFree(pReq);
        }
    }
    else
    {
        /*
         * Request EMT to do the larger part of the destruction. (in reverse order as VCPU 0 does the real cleanup)
         */
        PVMREQ pReq = NULL;
        int rc = VMR3ReqCallU(pUVM, VMCPUID_ALL_REVERSE, &pReq, RT_INDEFINITE_WAIT, 0, (PFNRT)vmR3Destroy, 1, pVM);
        if (RT_SUCCESS(rc))
            rc = pReq->iStatus;
        AssertRC(rc);
        VMR3ReqFree(pReq);

        /*
         * Now do the final bit where the heap and VM structures are freed up.
         * This will also wait 30 secs for the emulation threads to terminate.
         */
        vmR3DestroyUVM(pUVM, 30000);
    }

    LogFlow(("VMR3Destroy: returns VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}


/**
 * Internal destruction worker.
 *
 * This will do nearly all of the job, including sacking the EMT.
 *
 * @returns VBox status.
 * @param   pVM     VM handle.
 */
DECLCALLBACK(int) vmR3Destroy(PVM pVM)
{
    PUVM pUVM = pVM->pUVM;
    PVMCPU pVCpu = VMMGetCpu(pVM);

    NOREF(pUVM);
    LogFlow(("vmR3Destroy: pVM=%p pUVM=%p\n", pVM, pUVM));
    VM_ASSERT_EMT(pVM);

    /* Only VCPU 0 does the full cleanup. */
    if (pVCpu->idCpu != 0)
        return VINF_EM_TERMINATE;

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
#ifdef VBOX_WITH_DEBUGGER
    rc = DBGCTcpTerminate(pVM, pUVM->vm.s.pvDBGC);
    pVM->pUVM->vm.s.pvDBGC = NULL;
#endif
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
    rc = PDMR3CritSectTerm(pVM);
    AssertRC(rc);
    rc = MMR3Term(pVM);
    AssertRC(rc);

    /*
     * We're done in this thread (EMT).
     */
    ASMAtomicUoWriteBool(&pVM->pUVM->vm.s.fTerminateEMT, true);
    ASMAtomicWriteU32(&pVM->fGlobalForcedActions, VM_FF_TERMINATE);
    LogFlow(("vmR3Destroy: returning %Rrc\n", VINF_EM_TERMINATE));
    return VINF_EM_TERMINATE;
}


/**
 * Destroys the shared VM structure, leaving only UVM behind.
 *
 * This is called by EMT right before terminating.
 *
 * @param   pUVM        The UVM handle.
 */
void vmR3DestroyFinalBitFromEMT(PUVM pUVM)
{
    if (pUVM->pVM)
    {
        PVMCPU pVCpu = VMMGetCpu(pUVM->pVM);

        /* VCPU 0 does all the cleanup work. */
        if (pVCpu->idCpu != 0)
            return;

        /*
         * Modify state and then terminate MM.
         * (MM must be delayed until this point so we don't destroy the callbacks and the request packet.)
         */
        vmR3SetState(pUVM->pVM, VMSTATE_TERMINATED);

        /*
         * Tell GVMM to destroy the VM and free its resources.
         */
        int rc = SUPCallVMMR0Ex(pUVM->pVM->pVMR0, 0 /* VCPU 0 */, VMMR0_DO_GVMM_DESTROY_VM, 0, NULL);
        AssertRC(rc);
        pUVM->pVM = NULL;
    }

    /*
     * Did an EMT call VMR3Destroy and end up having to do all the work?
     */
    if (pUVM->vm.s.fEMTDoesTheCleanup)
        vmR3DestroyUVM(pUVM, 30000);
}


/**
 * Destroys the UVM portion.
 *
 * This is called as the final step in the VM destruction or as the cleanup
 * in case of a creation failure. If EMT called VMR3Destroy, meaning
 * VMINTUSERPERVM::fEMTDoesTheCleanup is true, it will call this as
 * vmR3DestroyFinalBitFromEMT completes.
 *
 * @param   pVM             VM Handle.
 * @param   cMilliesEMTWait The number of milliseconds to wait for the emulation
 *                          threads.
 */
static void vmR3DestroyUVM(PUVM pUVM, uint32_t cMilliesEMTWait)
{
    /*
     * Signal termination of each the emulation threads and
     * wait for them to complete.
     */
    /* Signal them. */
    ASMAtomicUoWriteBool(&pUVM->vm.s.fTerminateEMT, true);
    for (VMCPUID i = 0; i < pUVM->cCpus; i++)
    {
        ASMAtomicUoWriteBool(&pUVM->aCpus[i].vm.s.fTerminateEMT, true);
        if (pUVM->pVM)
            VM_FF_SET(pUVM->pVM, VM_FF_TERMINATE);
        VMR3NotifyGlobalFFU(pUVM, VMNOTIFYFF_FLAGS_DONE_REM);
        RTSemEventSignal(pUVM->aCpus[i].vm.s.EventSemWait);
    }

    /* Wait for them. */
    uint64_t    NanoTS = RTTimeNanoTS();
    RTTHREAD    hSelf  = RTThreadSelf();
    ASMAtomicUoWriteBool(&pUVM->vm.s.fTerminateEMT, true);
    for (VMCPUID i = 0; i < pUVM->cCpus; i++)
    {
        RTTHREAD hThread = pUVM->aCpus[i].vm.s.ThreadEMT;
        if (    hThread != NIL_RTTHREAD
            &&  hThread != hSelf)
        {
            uint64_t cMilliesElapsed = (RTTimeNanoTS() - NanoTS) / 1000000;
            int rc2 = RTThreadWait(hThread,
                                   cMilliesElapsed < cMilliesEMTWait
                                   ? RT_MAX(cMilliesEMTWait - cMilliesElapsed, 2000)
                                   : 2000,
                                   NULL);
            if (rc2 == VERR_TIMEOUT) /* avoid the assertion when debugging. */
                rc2 = RTThreadWait(hThread, 1000, NULL);
            AssertLogRelMsgRC(rc2, ("i=%u rc=%Rrc\n", i, rc2));
            if (RT_SUCCESS(rc2))
                pUVM->aCpus[0].vm.s.ThreadEMT = NIL_RTTHREAD;
        }
    }

    /* Cleanup the semaphores. */
    for (VMCPUID i = 0; i < pUVM->cCpus; i++)
    {
        RTSemEventDestroy(pUVM->aCpus[i].vm.s.EventSemWait);
        pUVM->aCpus[i].vm.s.EventSemWait = NIL_RTSEMEVENT;
    }

    /*
     * Free the event semaphores associated with the request packets.
     */
    unsigned cReqs = 0;
    for (unsigned i = 0; i < RT_ELEMENTS(pUVM->vm.s.apReqFree); i++)
    {
        PVMREQ pReq = pUVM->vm.s.apReqFree[i];
        pUVM->vm.s.apReqFree[i] = NULL;
        for (; pReq; pReq = pReq->pNext, cReqs++)
        {
            pReq->enmState = VMREQSTATE_INVALID;
            RTSemEventDestroy(pReq->EventSem);
        }
    }
    Assert(cReqs == pUVM->vm.s.cReqFree); NOREF(cReqs);

    /*
     * Kill all queued requests. (There really shouldn't be any!)
     */
    for (unsigned i = 0; i < 10; i++)
    {
        PVMREQ pReqHead = (PVMREQ)ASMAtomicXchgPtr((void *volatile *)&pUVM->vm.s.pReqs, NULL);
        AssertMsg(!pReqHead, ("This isn't supposed to happen! VMR3Destroy caller has to serialize this.\n"));
        if (!pReqHead)
            break;
        for (PVMREQ pReq = pReqHead; pReq; pReq = pReq->pNext)
        {
            ASMAtomicUoWriteSize(&pReq->iStatus, VERR_INTERNAL_ERROR);
            ASMAtomicWriteSize(&pReq->enmState, VMREQSTATE_INVALID);
            RTSemEventSignal(pReq->EventSem);
            RTThreadSleep(2);
            RTSemEventDestroy(pReq->EventSem);
        }
        /* give them a chance to respond before we free the request memory. */
        RTThreadSleep(32);
    }

    /*
     * Now all queued VCPU requests (again, there shouldn't be any).
     */
    for (VMCPUID i = 0; i < pUVM->cCpus; i++)
    {
        PUVMCPU pUVCpu = &pUVM->aCpus[i];

        for (unsigned i = 0; i < 10; i++)
        {
            PVMREQ pReqHead = (PVMREQ)ASMAtomicXchgPtr((void *volatile *)&pUVCpu->vm.s.pReqs, NULL);
            AssertMsg(!pReqHead, ("This isn't supposed to happen! VMR3Destroy caller has to serialize this.\n"));
            if (!pReqHead)
                break;
            for (PVMREQ pReq = pReqHead; pReq; pReq = pReq->pNext)
            {
                ASMAtomicUoWriteSize(&pReq->iStatus, VERR_INTERNAL_ERROR);
                ASMAtomicWriteSize(&pReq->enmState, VMREQSTATE_INVALID);
                RTSemEventSignal(pReq->EventSem);
                RTThreadSleep(2);
                RTSemEventDestroy(pReq->EventSem);
            }
            /* give them a chance to respond before we free the request memory. */
            RTThreadSleep(32);
        }
    }

    /*
     * Make sure the VMMR0.r0 module and whatever else is unloaded.
     */
    PDMR3TermUVM(pUVM);

    /*
     * Terminate the support library if initialized.
     */
    if (pUVM->vm.s.pSession)
    {
        int rc = SUPTerm();
        AssertRC(rc);
        pUVM->vm.s.pSession = NIL_RTR0PTR;
    }

    /*
     * Destroy the MM heap and free the UVM structure.
     */
    MMR3TermUVM(pUVM);
    STAMR3TermUVM(pUVM);

    RTTlsFree(pUVM->vm.s.idxTLS);

    ASMAtomicUoWriteU32(&pUVM->u32Magic, UINT32_MAX);
    RTMemFree(pUVM);

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
VMMR3DECL(PVM)   VMR3EnumVMs(PVM pVMPrev)
{
    /*
     * This is quick and dirty. It has issues with VM being
     * destroyed during the enumeration.
     */
    PUVM pNext;
    if (pVMPrev)
        pNext = pVMPrev->pUVM->pNext;
    else
        pNext = g_pUVMsHead;
    return pNext ? pNext->pVM : NULL;
}


/**
 * Registers an at VM destruction callback.
 *
 * @returns VBox status code.
 * @param   pfnAtDtor       Pointer to callback.
 * @param   pvUser          User argument.
 */
VMMR3DECL(int)   VMR3AtDtorRegister(PFNVMATDTOR pfnAtDtor, void *pvUser)
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
VMMR3DECL(int)   VMR3AtDtorDeregister(PFNVMATDTOR pfnAtDtor)
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
VMMR3DECL(int)   VMR3Reset(PVM pVM)
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
     * and wait for it to be processed. (in reverse order as VCPU 0 does the real cleanup)
     */
    PVMREQ pReq = NULL;
    rc = VMR3ReqCall(pVM, VMCPUID_ALL_REVERSE, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3Reset, 1, pVM);
    /** @note Can this really happen?? */
    while (rc == VERR_TIMEOUT)
        rc = VMR3ReqWait(pReq, RT_INDEFINITE_WAIT);

    if (RT_SUCCESS(rc))
        rc = pReq->iStatus;
    AssertRC(rc);
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
    PVMCPU pVCpu = VMMGetCpu(pVM);

    /* Only VCPU 0 does the full cleanup. */
    if (pVCpu->idCpu != 0)
        return VINF_EM_RESET;

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
    vmR3AtResetU(pVM->pUVM);
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
 * @param   pUVM        Pointe to the user mode VM structure.
 */
static int vmR3AtResetU(PUVM pUVM)
{
    /*
     * Walk the list and call them all.
     */
    int rc = VINF_SUCCESS;
    for (PVMATRESET pCur = pUVM->vm.s.pAtReset; pCur; pCur = pCur->pNext)
    {
        /* do the call */
        switch (pCur->enmType)
        {
            case VMATRESETTYPE_DEV:
                rc = pCur->u.Dev.pfnCallback(pCur->u.Dev.pDevIns, pCur->pvUser);
                break;
            case VMATRESETTYPE_INTERNAL:
                rc = pCur->u.Internal.pfnCallback(pUVM->pVM, pCur->pvUser);
                break;
            case VMATRESETTYPE_EXTERNAL:
                pCur->u.External.pfnCallback(pCur->pvUser);
                break;
            default:
                AssertMsgFailed(("Invalid at-reset type %d!\n", pCur->enmType));
                return VERR_INTERNAL_ERROR;
        }

        if (RT_FAILURE(rc))
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
static int vmr3AtResetRegisterU(PUVM pUVM, void *pvUser, const char *pszDesc, PVMATRESET *ppNew)
{
    /*
     * Allocate restration structure.
     */
    PVMATRESET pNew = (PVMATRESET)MMR3HeapAllocU(pUVM, MM_TAG_VM,  sizeof(*pNew));
    if (pNew)
    {
        /* fill data. */
        pNew->pszDesc   = pszDesc;
        pNew->pvUser    = pvUser;

        /* insert */
        pNew->pNext     = *pUVM->vm.s.ppAtResetNext;
        *pUVM->vm.s.ppAtResetNext = pNew;
        pUVM->vm.s.ppAtResetNext = &pNew->pNext;

        *ppNew = pNew;
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
VMMR3DECL(int)   VMR3AtResetRegister(PVM pVM, PPDMDEVINS pDevInst, PFNVMATRESET pfnCallback, void *pvUser, const char *pszDesc)
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
    int rc = vmr3AtResetRegisterU(pVM->pUVM, pvUser, pszDesc, &pNew);
    if (RT_SUCCESS(rc))
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
VMMR3DECL(int)   VMR3AtResetRegisterInternal(PVM pVM, PFNVMATRESETINT pfnCallback, void *pvUser, const char *pszDesc)
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
    int rc = vmr3AtResetRegisterU(pVM->pUVM, pvUser, pszDesc, &pNew);
    if (RT_SUCCESS(rc))
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
VMMR3DECL(int)   VMR3AtResetRegisterExternal(PVM pVM, PFNVMATRESETEXT pfnCallback, void *pvUser, const char *pszDesc)
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
    int rc = vmr3AtResetRegisterU(pVM->pUVM, pvUser, pszDesc, &pNew);
    if (RT_SUCCESS(rc))
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
 * @param   pUVM            Pointer to the user mode VM structure.
 * @param   pCur            The one to free.
 * @param   pPrev           The one before pCur.
 */
static PVMATRESET vmr3AtResetFreeU(PUVM pUVM, PVMATRESET pCur, PVMATRESET pPrev)
{
    /*
     * Unlink it.
     */
    PVMATRESET pNext = pCur->pNext;
    if (pPrev)
    {
        pPrev->pNext = pNext;
        if (!pNext)
            pUVM->vm.s.ppAtResetNext = &pPrev->pNext;
    }
    else
    {
        pUVM->vm.s.pAtReset = pNext;
        if (!pNext)
            pUVM->vm.s.ppAtResetNext = &pUVM->vm.s.pAtReset;
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
 * @param   pDevIns         Device instance.
 * @param   pfnCallback     Callback function.
 */
VMMR3DECL(int)   VMR3AtResetDeregister(PVM pVM, PPDMDEVINS pDevIns, PFNVMATRESET pfnCallback)
{
    int         rc = VERR_VM_ATRESET_NOT_FOUND;
    PVMATRESET  pPrev = NULL;
    PVMATRESET  pCur = pVM->pUVM->vm.s.pAtReset;
    while (pCur)
    {
        if (    pCur->enmType == VMATRESETTYPE_DEV
            &&  pCur->u.Dev.pDevIns == pDevIns
            &&  (   !pfnCallback
                 || pCur->u.Dev.pfnCallback == pfnCallback))
        {
            pCur = vmr3AtResetFreeU(pVM->pUVM, pCur, pPrev);
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
VMMR3DECL(int)   VMR3AtResetDeregisterInternal(PVM pVM, PFNVMATRESETINT pfnCallback)
{
    int         rc = VERR_VM_ATRESET_NOT_FOUND;
    PVMATRESET  pPrev = NULL;
    PVMATRESET  pCur = pVM->pUVM->vm.s.pAtReset;
    while (pCur)
    {
        if (    pCur->enmType == VMATRESETTYPE_INTERNAL
            &&  pCur->u.Internal.pfnCallback == pfnCallback)
        {
            pCur = vmr3AtResetFreeU(pVM->pUVM, pCur, pPrev);
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
VMMR3DECL(int)   VMR3AtResetDeregisterExternal(PVM pVM, PFNVMATRESETEXT pfnCallback)
{
    int         rc = VERR_VM_ATRESET_NOT_FOUND;
    PVMATRESET  pPrev = NULL;
    PVMATRESET  pCur = pVM->pUVM->vm.s.pAtReset;
    while (pCur)
    {
        if (    pCur->enmType == VMATRESETTYPE_INTERNAL
            &&  pCur->u.External.pfnCallback == pfnCallback)
        {
            pCur = vmr3AtResetFreeU(pVM->pUVM, pCur, pPrev);
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
VMMR3DECL(VMSTATE) VMR3GetState(PVM pVM)
{
    return pVM->enmVMState;
}


/**
 * Gets the state name string for a VM state.
 *
 * @returns Pointer to the state name. (readonly)
 * @param   enmState        The state.
 */
VMMR3DECL(const char *) VMR3GetStateName(VMSTATE enmState)
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
        case VMSTATE_GURU_MEDITATION:   return "GURU_MEDITATION";
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
    /*
     * Validate state machine transitions before doing the actual change.
     */
    VMSTATE enmStateOld = pVM->enmVMState;
    switch (enmStateOld)
    {
        case VMSTATE_OFF:
            Assert(enmStateNew != VMSTATE_GURU_MEDITATION);
            break;

        default:
            /** @todo full validation. */
            break;
    }

    pVM->enmVMState = enmStateNew;
    LogRel(("Changing the VM state from '%s' to '%s'.\n", VMR3GetStateName(enmStateOld),  VMR3GetStateName(enmStateNew)));

    /*
     * Call the at state change callbacks.
     */
    for (PVMATSTATE pCur = pVM->pUVM->vm.s.pAtState; pCur; pCur = pCur->pNext)
    {
        pCur->pfnAtState(pVM, enmStateNew, enmStateOld, pCur->pvUser);
        if (    pVM->enmVMState != enmStateNew
            &&  pVM->enmVMState == VMSTATE_DESTROYING)
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
VMMR3DECL(int)   VMR3AtStateRegister(PVM pVM, PFNVMATSTATE pfnAtState, void *pvUser)
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
    int rc = VMR3ReqCall(pVM, VMCPUID_ANY, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3AtStateRegisterU, 3, pVM->pUVM, pfnAtState, pvUser);
    if (RT_FAILURE(rc))
        return rc;
    rc = pReq->iStatus;
    VMR3ReqFree(pReq);

    LogFlow(("VMR3AtStateRegister: returns %Rrc\n", rc));
    return rc;
}


/**
 * Registers a VM state change callback.
 *
 * @returns VBox status code.
 * @param   pUVM            Pointer to the user mode VM structure.
 * @param   pfnAtState      Pointer to callback.
 * @param   pvUser          User argument.
 * @thread  EMT
 */
static DECLCALLBACK(int) vmR3AtStateRegisterU(PUVM pUVM, PFNVMATSTATE pfnAtState, void *pvUser)
{
    /*
     * Allocate a new record.
     */

    PVMATSTATE pNew = (PVMATSTATE)MMR3HeapAllocU(pUVM, MM_TAG_VM, sizeof(*pNew));
    if (!pNew)
        return VERR_NO_MEMORY;

    /* fill */
    pNew->pfnAtState = pfnAtState;
    pNew->pvUser     = pvUser;

    /* insert */
    pNew->pNext      = *pUVM->vm.s.ppAtStateNext;
    *pUVM->vm.s.ppAtStateNext = pNew;
    pUVM->vm.s.ppAtStateNext = &pNew->pNext;

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
VMMR3DECL(int)   VMR3AtStateDeregister(PVM pVM, PFNVMATSTATE pfnAtState, void *pvUser)
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
    int rc = VMR3ReqCall(pVM, VMCPUID_ANY, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3AtStateDeregisterU, 3, pVM->pUVM, pfnAtState, pvUser);
    if (RT_FAILURE(rc))
        return rc;
    rc = pReq->iStatus;
    VMR3ReqFree(pReq);

    LogFlow(("VMR3AtStateDeregister: returns %Rrc\n", rc));
    return rc;
}


/**
 * Deregisters a VM state change callback.
 *
 * @returns VBox status code.
 * @param   pUVM            Pointer to the user mode VM structure.
 * @param   pfnAtState      Pointer to callback.
 * @param   pvUser          User argument.
 * @thread  EMT
 */
static DECLCALLBACK(int) vmR3AtStateDeregisterU(PUVM pUVM, PFNVMATSTATE pfnAtState, void *pvUser)
{
    LogFlow(("vmR3AtStateDeregisterU: pfnAtState=%p pvUser=%p\n", pfnAtState, pvUser));

    /*
     * Search the list for the entry.
     */
    PVMATSTATE pPrev = NULL;
    PVMATSTATE pCur = pUVM->vm.s.pAtState;
    while (     pCur
           &&   (   pCur->pfnAtState != pfnAtState
                 || pCur->pvUser != pvUser))
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
            pUVM->vm.s.ppAtStateNext = &pPrev->pNext;
    }
    else
    {
        pUVM->vm.s.pAtState = pCur->pNext;
        if (!pCur->pNext)
            pUVM->vm.s.ppAtStateNext = &pUVM->vm.s.pAtState;
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
VMMR3DECL(int)   VMR3AtErrorRegister(PVM pVM, PFNVMATERROR pfnAtError, void *pvUser)
{
    return VMR3AtErrorRegisterU(pVM->pUVM, pfnAtError, pvUser);
}


/**
 * Registers a VM error callback.
 *
 * @returns VBox status code.
 * @param   pUVM            The VM handle.
 * @param   pfnAtError      Pointer to callback.
 * @param   pvUser          User argument.
 * @thread  Any.
 */
VMMR3DECL(int)   VMR3AtErrorRegisterU(PUVM pUVM, PFNVMATERROR pfnAtError, void *pvUser)
{
    LogFlow(("VMR3AtErrorRegister: pfnAtError=%p pvUser=%p\n", pfnAtError, pvUser));
    AssertPtrReturn(pfnAtError, VERR_INVALID_PARAMETER);

    /*
     * Make sure we're in EMT (to avoid the logging).
     */
    PVMREQ pReq;
    int rc = VMR3ReqCallU(pUVM, VMCPUID_ANY, &pReq, RT_INDEFINITE_WAIT, 0, (PFNRT)vmR3AtErrorRegisterU, 3, pUVM, pfnAtError, pvUser);
    if (RT_FAILURE(rc))
        return rc;
    rc = pReq->iStatus;
    VMR3ReqFree(pReq);

    LogFlow(("VMR3AtErrorRegister: returns %Rrc\n", rc));
    return rc;
}


/**
 * Registers a VM error callback.
 *
 * @returns VBox status code.
 * @param   pUVM            Pointer to the user mode VM structure.
 * @param   pfnAtError      Pointer to callback.
 * @param   pvUser          User argument.
 * @thread  EMT
 */
static DECLCALLBACK(int) vmR3AtErrorRegisterU(PUVM pUVM, PFNVMATERROR pfnAtError, void *pvUser)
{
    /*
     * Allocate a new record.
     */

    PVMATERROR pNew = (PVMATERROR)MMR3HeapAllocU(pUVM, MM_TAG_VM, sizeof(*pNew));
    if (!pNew)
        return VERR_NO_MEMORY;

    /* fill */
    pNew->pfnAtError = pfnAtError;
    pNew->pvUser     = pvUser;

    /* insert */
    pNew->pNext      = *pUVM->vm.s.ppAtErrorNext;
    *pUVM->vm.s.ppAtErrorNext = pNew;
    pUVM->vm.s.ppAtErrorNext = &pNew->pNext;

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
VMMR3DECL(int)   VMR3AtErrorDeregister(PVM pVM, PFNVMATERROR pfnAtError, void *pvUser)
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
    int rc = VMR3ReqCall(pVM, VMCPUID_ANY, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3AtErrorDeregisterU, 3, pVM->pUVM, pfnAtError, pvUser);
    if (RT_FAILURE(rc))
        return rc;
    rc = pReq->iStatus;
    VMR3ReqFree(pReq);

    LogFlow(("VMR3AtErrorDeregister: returns %Rrc\n", rc));
    return rc;
}


/**
 * Deregisters a VM error callback.
 *
 * @returns VBox status code.
 * @param   pUVM            Pointer to the user mode VM structure.
 * @param   pfnAtError      Pointer to callback.
 * @param   pvUser          User argument.
 * @thread  EMT
 */
static DECLCALLBACK(int)    vmR3AtErrorDeregisterU(PUVM pUVM, PFNVMATERROR pfnAtError, void *pvUser)
{
    LogFlow(("vmR3AtErrorDeregisterU: pfnAtError=%p pvUser=%p\n", pfnAtError, pvUser));

    /*
     * Search the list for the entry.
     */
    PVMATERROR pPrev = NULL;
    PVMATERROR pCur = pUVM->vm.s.pAtError;
    while (     pCur
           &&   (   pCur->pfnAtError != pfnAtError
                 || pCur->pvUser != pvUser))
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
            pUVM->vm.s.ppAtErrorNext = &pPrev->pNext;
    }
    else
    {
        pUVM->vm.s.pAtError = pCur->pNext;
        if (!pCur->pNext)
            pUVM->vm.s.ppAtErrorNext = &pUVM->vm.s.pAtError;
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
VMMR3DECL(void) VMR3SetErrorWorker(PVM pVM)
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
    for (PVMATERROR pCur = pVM->pUVM->vm.s.pAtError; pCur; pCur = pCur->pNext)
        vmR3SetErrorWorkerDoCall(pVM, pCur, rc, RT_SRC_POS_ARGS, "%s", pszMessage);
}


/**
 * Creation time wrapper for vmR3SetErrorUV.
 *
 * @returns rc.
 * @param   pUVM            Pointer to the user mode VM structure.
 * @param   rc              The VBox status code.
 * @param   RT_SRC_POS_DECL The source position of this error.
 * @param   pszFormat       Format string.
 * @param   ...             The arguments.
 * @thread  Any thread.
 */
static int vmR3SetErrorU(PUVM pUVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    vmR3SetErrorUV(pUVM, rc, pszFile, iLine, pszFunction, pszFormat, &va);
    va_end(va);
    return rc;
}


/**
 * Worker which calls everyone listening to the VM error messages.
 *
 * @param   pUVM            Pointer to the user mode VM structure.
 * @param   rc              The VBox status code.
 * @param   RT_SRC_POS_DECL The source position of this error.
 * @param   pszFormat       Format string.
 * @param   pArgs           Pointer to the format arguments.
 * @thread  EMT
 */
DECLCALLBACK(void) vmR3SetErrorUV(PUVM pUVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list *pArgs)
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
    RTLogPrintf("\n");
#endif

    /*
     * Make a copy of the message.
     */
    if (pUVM->pVM)
        vmSetErrorCopy(pUVM->pVM, rc, RT_SRC_POS_ARGS, pszFormat, *pArgs);

    /*
     * Call the at error callbacks.
     */
    for (PVMATERROR pCur = pUVM->vm.s.pAtError; pCur; pCur = pCur->pNext)
    {
        va_list va2;
        va_copy(va2, *pArgs);
        pCur->pfnAtError(pUVM->pVM, pCur->pvUser, rc, RT_SRC_POS_ARGS, pszFormat, va2);
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
VMMR3DECL(int)   VMR3AtRuntimeErrorRegister(PVM pVM, PFNVMATRUNTIMEERROR pfnAtRuntimeError, void *pvUser)
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
    int rc = VMR3ReqCall(pVM, VMCPUID_ANY, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3AtRuntimeErrorRegisterU, 3, pVM->pUVM, pfnAtRuntimeError, pvUser);
    if (RT_FAILURE(rc))
        return rc;
    rc = pReq->iStatus;
    VMR3ReqFree(pReq);

    LogFlow(("VMR3AtRuntimeErrorRegister: returns %Rrc\n", rc));
    return rc;
}


/**
 * Registers a VM runtime error callback.
 *
 * @returns VBox status code.
 * @param   pUVM            Pointer to the user mode VM structure.
 * @param   pfnAtRuntimeError   Pointer to callback.
 * @param   pvUser              User argument.
 * @thread  EMT
 */
static DECLCALLBACK(int)    vmR3AtRuntimeErrorRegisterU(PUVM pUVM, PFNVMATRUNTIMEERROR pfnAtRuntimeError, void *pvUser)
{
    /*
     * Allocate a new record.
     */

    PVMATRUNTIMEERROR pNew = (PVMATRUNTIMEERROR)MMR3HeapAllocU(pUVM, MM_TAG_VM, sizeof(*pNew));
    if (!pNew)
        return VERR_NO_MEMORY;

    /* fill */
    pNew->pfnAtRuntimeError = pfnAtRuntimeError;
    pNew->pvUser            = pvUser;

    /* insert */
    pNew->pNext             = *pUVM->vm.s.ppAtRuntimeErrorNext;
    *pUVM->vm.s.ppAtRuntimeErrorNext = pNew;
    pUVM->vm.s.ppAtRuntimeErrorNext = &pNew->pNext;

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
VMMR3DECL(int)   VMR3AtRuntimeErrorDeregister(PVM pVM, PFNVMATRUNTIMEERROR pfnAtRuntimeError, void *pvUser)
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
    int rc = VMR3ReqCall(pVM, VMCPUID_ANY, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3AtRuntimeErrorDeregisterU, 3, pVM->pUVM, pfnAtRuntimeError, pvUser);
    if (RT_FAILURE(rc))
        return rc;
    rc = pReq->iStatus;
    VMR3ReqFree(pReq);

    LogFlow(("VMR3AtRuntimeErrorDeregister: returns %Rrc\n", rc));
    return rc;
}


/**
 * Deregisters a VM runtime error callback.
 *
 * @returns VBox status code.
 * @param   pUVM            Pointer to the user mode VM structure.
 * @param   pfnAtRuntimeError   Pointer to callback.
 * @param   pvUser              User argument.
 * @thread  EMT
 */
static DECLCALLBACK(int)    vmR3AtRuntimeErrorDeregisterU(PUVM pUVM, PFNVMATRUNTIMEERROR pfnAtRuntimeError, void *pvUser)
{
    LogFlow(("vmR3AtRuntimeErrorDeregisterU: pfnAtRuntimeError=%p pvUser=%p\n", pfnAtRuntimeError, pvUser));

    /*
     * Search the list for the entry.
     */
    PVMATRUNTIMEERROR pPrev = NULL;
    PVMATRUNTIMEERROR pCur = pUVM->vm.s.pAtRuntimeError;
    while (     pCur
           &&   (   pCur->pfnAtRuntimeError != pfnAtRuntimeError
                 || pCur->pvUser != pvUser))
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
            pUVM->vm.s.ppAtRuntimeErrorNext = &pPrev->pNext;
    }
    else
    {
        pUVM->vm.s.pAtRuntimeError = pCur->pNext;
        if (!pCur->pNext)
            pUVM->vm.s.ppAtRuntimeErrorNext = &pUVM->vm.s.pAtRuntimeError;
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
 * Worker for VMR3SetRuntimeErrorWorker and vmR3SetRuntimeErrorV.
 *
 * This does the common parts after the error has been saved / retrieved.
 *
 * @returns VBox status code with modifications, see VMSetRuntimeErrorV.
 *
 * @param   pVM             The VM handle.
 * @param   fFlags          The error flags.
 * @param   pszErrorId      Error ID string.
 * @param   pszFormat       Format string.
 * @param   pVa             Pointer to the format arguments.
 */
static int vmR3SetRuntimeErrorCommon(PVM pVM, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, va_list *pVa)
{
    LogRel(("VM: Raising runtime error '%s' (fFlags=%#x)\n", pszErrorId, fFlags));

    /*
     * Take actions before the call.
     */
    int rc = VINF_SUCCESS;
    if (fFlags & VMSETRTERR_FLAGS_FATAL)
        /** @todo Add some special VM state for the FATAL variant that isn't resumable.
         *        It's too risky for 2.2.0, do after branching. */
        rc = VMR3SuspendNoSave(pVM);
    else if (fFlags & VMSETRTERR_FLAGS_SUSPEND)
        rc = VMR3Suspend(pVM);

    /*
     * Do the callback round.
     */
    for (PVMATRUNTIMEERROR pCur = pVM->pUVM->vm.s.pAtRuntimeError; pCur; pCur = pCur->pNext)
    {
        va_list va;
        va_copy(va, *pVa);
        pCur->pfnAtRuntimeError(pVM, pCur->pvUser, fFlags, pszErrorId, pszFormat, va);
        va_end(va);
    }

    return rc;
}


/**
 * Ellipsis to va_list wrapper for calling vmR3SetRuntimeErrorCommon.
 */
static int vmR3SetRuntimeErrorCommonF(PVM pVM, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    int rc = vmR3SetRuntimeErrorCommon(pVM, fFlags, pszErrorId, pszFormat, &va);
    va_end(va);
    return rc;
}


/**
 * This is a worker function for RC and Ring-0 calls to VMSetError and
 * VMSetErrorV.
 *
 * The message is found in VMINT.
 *
 * @returns VBox status code, see VMSetRuntimeError.
 * @param   pVM             The VM handle.
 * @thread  EMT.
 */
VMMR3DECL(int) VMR3SetRuntimeErrorWorker(PVM pVM)
{
    VM_ASSERT_EMT(pVM);
    AssertReleaseMsgFailed(("And we have a winner! You get to implement Ring-0 and GC VMSetRuntimeErrorV! Congrats!\n"));

    /*
     * Unpack the error (if we managed to format one).
     */
    const char     *pszErrorId = "SetRuntimeError";
    const char     *pszMessage = "No message!";
    uint32_t        fFlags     = VMSETRTERR_FLAGS_FATAL;
    PVMRUNTIMEERROR pErr       = pVM->vm.s.pRuntimeErrorR3;
    if (pErr)
    {
        AssertCompile(sizeof(const char) == sizeof(uint8_t));
        if (pErr->offErrorId)
            pszErrorId = (const char *)pErr + pErr->offErrorId;
        if (pErr->offMessage)
            pszMessage = (const char *)pErr + pErr->offMessage;
        fFlags = pErr->fFlags;
    }

    /*
     * Join cause with vmR3SetRuntimeErrorV.
     */
    return vmR3SetRuntimeErrorCommonF(pVM, fFlags, pszErrorId, "%s", pszMessage);
}


/**
 * Worker for VMSetRuntimeErrorV for doing the job on EMT in ring-3.
 *
 * @returns VBox status code with modifications, see VMSetRuntimeErrorV.
 *
 * @param   pVM             The VM handle.
 * @param   fFlags          The error flags.
 * @param   pszErrorId      Error ID string.
 * @param   pszFormat       Format string.
 * @param   pVa             Pointer to the format arguments.
 *
 * @thread  EMT
 */
DECLCALLBACK(int) vmR3SetRuntimeErrorV(PVM pVM, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, va_list *pVa)
{
    /*
     * Make a copy of the message.
     */
    va_list va2;
    va_copy(va2, *pVa);
    vmSetRuntimeErrorCopy(pVM, fFlags, pszErrorId, pszFormat, va2);
    va_end(va2);

    /*
     * Join paths with VMR3SetRuntimeErrorWorker.
     */
    return vmR3SetRuntimeErrorCommon(pVM, fFlags, pszErrorId, pszFormat, pVa);
}


/**
 * Gets the ID virtual of the virtual CPU assoicated with the calling thread.
 *
 * @returns The CPU ID. NIL_VMCPUID if the thread isn't an EMT.
 *
 * @param   pVM             The VM handle.
 */
VMMR3DECL(RTCPUID) VMR3GetVMCPUId(PVM pVM)
{
    PUVMCPU pUVCpu = (PUVMCPU)RTTlsGet(pVM->pUVM->vm.s.idxTLS);
    return pUVCpu
         ? pUVCpu->idCpu
         : NIL_VMCPUID;
}


/**
 * Returns the native handle of the current EMT VMCPU thread.
 *
 * @returns Handle if this is an EMT thread; NIL_RTNATIVETHREAD otherwise
 * @param   pVM             The VM handle.
 * @thread  EMT
 */
VMMR3DECL(RTNATIVETHREAD) VMR3GetVMCPUNativeThread(PVM pVM)
{
    PUVMCPU pUVCpu = (PUVMCPU)RTTlsGet(pVM->pUVM->vm.s.idxTLS);

    if (!pUVCpu)
        return NIL_RTNATIVETHREAD;

    return pUVCpu->vm.s.NativeThreadEMT;
}


/**
 * Returns the native handle of the current EMT VMCPU thread.
 *
 * @returns Handle if this is an EMT thread; NIL_RTNATIVETHREAD otherwise
 * @param   pVM             The VM handle.
 * @thread  EMT
 */
VMMR3DECL(RTNATIVETHREAD) VMR3GetVMCPUNativeThreadU(PUVM pUVM)
{
    PUVMCPU pUVCpu = (PUVMCPU)RTTlsGet(pUVM->vm.s.idxTLS);

    if (!pUVCpu)
        return NIL_RTNATIVETHREAD;

    return pUVCpu->vm.s.NativeThreadEMT;
}


/**
 * Returns the handle of the current EMT VMCPU thread.
 *
 * @returns Handle if this is an EMT thread; NIL_RTNATIVETHREAD otherwise
 * @param   pVM             The VM handle.
 * @thread  EMT
 */
VMMR3DECL(RTTHREAD) VMR3GetVMCPUThread(PVM pVM)
{
    PUVMCPU pUVCpu = (PUVMCPU)RTTlsGet(pVM->pUVM->vm.s.idxTLS);

    if (!pUVCpu)
        return NIL_RTTHREAD;

    return pUVCpu->vm.s.ThreadEMT;
}


/**
 * Returns the handle of the current EMT VMCPU thread.
 *
 * @returns Handle if this is an EMT thread; NIL_RTNATIVETHREAD otherwise
 * @param   pVM             The VM handle.
 * @thread  EMT
 */
VMMR3DECL(RTTHREAD) VMR3GetVMCPUThreadU(PUVM pUVM)
{
    PUVMCPU pUVCpu = (PUVMCPU)RTTlsGet(pUVM->vm.s.idxTLS);

    if (!pUVCpu)
        return NIL_RTTHREAD;

    return pUVCpu->vm.s.ThreadEMT;
}

