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
 * time when the VMM bit was a kind of vague.  'VM' also happend to be the name
 * of the per-VM instance structure (see vm.h), so it kind of made sense.
 * However as it turned out, VMM(.cpp) is almost empty all it provides in ring-3
 * is some minor functionally and some "routing" services.
 *
 * Fixing this is just a matter of some more or less straight forward
 * refactoring, the question is just when someone will get to it. Moving the EMT
 * would be a good start.
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
static int               vmR3CreateUVM(uint32_t cCpus, PUVM *ppUVM);
static int               vmR3CreateU(PUVM pUVM, uint32_t cCpus, PFNCFGMCONSTRUCTOR pfnCFGMConstructor, void *pvUserCFGM);
static int               vmR3InitRing3(PVM pVM, PUVM pUVM);
static int               vmR3InitVMCpu(PVM pVM);
static int               vmR3InitRing0(PVM pVM);
static int               vmR3InitGC(PVM pVM);
static int               vmR3InitDoCompleted(PVM pVM, VMINITCOMPLETED enmWhat);
static DECLCALLBACK(size_t) vmR3LogPrefixCallback(PRTLOGGER pLogger, char *pchBuf, size_t cchBuf, void *pvUser);
static DECLCALLBACK(int) vmR3PowerOff(PVM pVM);
static void              vmR3DestroyUVM(PUVM pUVM, uint32_t cMilliesEMTWait);
static void              vmR3AtDtor(PVM pVM);
static bool              vmR3ValidateStateTransition(VMSTATE enmStateOld, VMSTATE enmStateNew);
static void              vmR3DoAtState(PVM pVM, PUVM pUVM, VMSTATE enmStateNew, VMSTATE enmStateOld);
static int               vmR3TrySetState(PVM pVM, const char *pszWho, unsigned cTransitions, ...);
static void              vmR3SetStateLocked(PVM pVM, PUVM pUVM, VMSTATE enmStateNew, VMSTATE enmStateOld);
static void              vmR3SetState(PVM pVM, VMSTATE enmStateNew, VMSTATE enmStateOld);
static int               vmR3SetErrorU(PUVM pUVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...);


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
 * @param   cCpus               Number of virtual CPUs for the new VM.
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
VMMR3DECL(int)   VMR3Create(uint32_t cCpus, PFNVMATERROR pfnVMAtError, void *pvUserVM, PFNCFGMCONSTRUCTOR pfnCFGMConstructor, void *pvUserCFGM, PVM *ppVM)
{
    LogFlow(("VMR3Create: cCpus=%RU32 pfnVMAtError=%p pvUserVM=%p  pfnCFGMConstructor=%p pvUserCFGM=%p ppVM=%p\n",
             cCpus, pfnVMAtError, pvUserVM, pfnCFGMConstructor, pvUserCFGM, ppVM));

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
    AssertLogRelMsgReturn(cCpus > 0 && cCpus <= VMM_MAX_CPU_COUNT, ("%RU32\n", cCpus), VERR_TOO_MANY_CPUS);

    /*
     * Create the UVM so we can register the at-error callback
     * and consoliate a bit of cleanup code.
     */
    PUVM pUVM = NULL;                   /* shuts up gcc */
    int rc = vmR3CreateUVM(cCpus, &pUVM);
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
            rc = VMR3ReqCallU(pUVM, VMCPUID_ANY, &pReq, RT_INDEFINITE_WAIT, VMREQFLAGS_VBOX_STATUS,
                              (PFNRT)vmR3CreateU, 4, pUVM, cCpus, pfnCFGMConstructor, pvUserCFGM);
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
                AssertMsgFailed(("VMR3ReqCallU failed rc=%Rrc\n", rc));

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

#ifdef RT_OS_LINUX
                case VERR_SUPDRV_COMPONENT_NOT_FOUND:
                    pszError = N_("One of the kernel modules was not successfully loaded. Make sure "
                                  "that no kernel modules from an older version of VirtualBox exist. "
                                  "Then try to recompile and reload the kernel modules by executing "
                                  "'/etc/init.d/vboxdrv setup' as root");
                    break;
#endif

                case VERR_RAW_MODE_INVALID_SMP:
                    pszError = N_("VT-x/AMD-V is either not available on your host or disabled. "
                                  "VirtualBox requires this hardware extension to emulate more than one "
                                  "guest CPU");
                    break;

                case VERR_SUPDRV_KERNEL_TOO_OLD_FOR_VTX:
#ifdef RT_OS_LINUX
                    pszError = N_("Because the host kernel is too old, VirtualBox cannot enable the VT-x "
                                  "extension. Either upgrade your kernel to Linux 2.6.13 or later or disable "
                                  "the VT-x extension in the VM settings. Note that without VT-x you have "
                                  "to reduce the number of guest CPUs to one");
#else
                    pszError = N_("Because the host kernel is too old, VirtualBox cannot enable the VT-x "
                                  "extension. Either upgrade your kernel or disable the VT-x extension in the "
                                  "VM settings. Note that without VT-x you have to reduce the number of guest "
                                  "CPUs to one");
#endif
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
    PUVM pUVM = (PUVM)RTMemPageAllocZ(RT_OFFSETOF(UVM, aCpus[cCpus]));
    AssertReturn(pUVM, VERR_NO_MEMORY);
    pUVM->u32Magic = UVM_MAGIC;
    pUVM->cCpus = cCpus;

    AssertCompile(sizeof(pUVM->vm.s) <= sizeof(pUVM->vm.padding));

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
            pUVM->aCpus[i].vm.s.EventSemWait = NIL_RTSEMEVENT;
        for (i = 0; i < cCpus; i++)
        {
            rc = RTSemEventCreate(&pUVM->aCpus[i].vm.s.EventSemWait);
            if (RT_FAILURE(rc))
                break;
        }
        if (RT_SUCCESS(rc))
        {
            rc = RTCritSectInit(&pUVM->vm.s.AtStateCritSect);
            if (RT_SUCCESS(rc))
            {
                rc = RTCritSectInit(&pUVM->vm.s.AtErrorCritSect);
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
                                    rc = RTThreadCreateF(&pUVM->aCpus[i].vm.s.ThreadEMT, vmR3EmulationThread, &pUVM->aCpus[i], _1M,
                                                         RTTHREADTYPE_EMULATION, RTTHREADFLAGS_WAITABLE,
                                                         cCpus > 1 ? "EMT-%u" : "EMT", i);
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
                    RTCritSectDelete(&pUVM->vm.s.AtErrorCritSect);
                }
                RTCritSectDelete(&pUVM->vm.s.AtStateCritSect);
            }
        }
        for (i = 0; i < cCpus; i++)
        {
            RTSemEventDestroy(pUVM->aCpus[i].vm.s.EventSemWait);
            pUVM->aCpus[i].vm.s.EventSemWait = NIL_RTSEMEVENT;
        }
        RTTlsFree(pUVM->vm.s.idxTLS);
    }
    RTMemPageFree(pUVM);
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
    rc = SUPR3CallVMMR0Ex(NIL_RTR0PTR, NIL_VMCPUID, VMMR0_DO_GVMM_CREATE_VM, 0, &CreateVMReq.Hdr);
    if (RT_SUCCESS(rc))
    {
        PVM pVM = pUVM->pVM = CreateVMReq.pVMR3;
        AssertRelease(VALID_PTR(pVM));
        AssertRelease(pVM->pVMR0 == CreateVMReq.pVMR0);
        AssertRelease(pVM->pSession == pUVM->vm.s.pSession);
        AssertRelease(pVM->cCpus == cCpus);
        AssertRelease(pVM->offVMCPU == RT_UOFFSETOF(VM, aCpus));

        Log(("VMR3Create: Created pUVM=%p pVM=%p pVMR0=%p hSelf=%#x cCpus=%RU32\n",
             pUVM, pVM, pVM->pVMR0, pVM->hSelf, pVM->cCpus));

        /*
         * Initialize the VM structure and our internal data (VMINT).
         */
        pVM->pUVM = pUVM;

        for (VMCPUID i = 0; i < pVM->cCpus; i++)
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
                    AssertLogRelMsgFailed(("Configuration error: \"NumCPUs\"=%RU32 and VMR3CreateVM::cCpus=%RU32 does not match!\n",
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
                                        vmR3SetState(pVM, VMSTATE_CREATED, VMSTATE_CREATING);
                                        pUVM->pNext = g_pUVMsHead;
                                        g_pUVMsHead = pUVM;

#ifdef LOG_ENABLED
                                        RTLogSetCustomPrefixCallback(NULL, vmR3LogPrefixCallback, pUVM);
#endif
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

        /*
         * Drop all references to VM and the VMCPU structures, then
         * tell GVMM to destroy the VM.
         */
        pUVM->pVM = NULL;
        for (VMCPUID i = 0; i < pUVM->cCpus; i++)
        {
            pUVM->aCpus[i].pVM = NULL;
            pUVM->aCpus[i].pVCpu = NULL;
        }
        Assert(pUVM->vm.s.enmHaltMethod == VMHALTMETHOD_BOOTSTRAP);

        if (pUVM->cCpus > 1)
        {
            /* Poke the other EMTs since they may have stale pVM and pVCpu references
               on the stack (see VMR3WaitU for instance) if they've been awakened after
               VM creation. */
            for (VMCPUID i = 1; i < pUVM->cCpus; i++)
                VMR3NotifyCpuFFU(&pUVM->aCpus[i], 0);
            RTThreadSleep(RT_MIN(100 + 25 *(pUVM->cCpus - 1), 500)); /* very sophisticated */
        }

        int rc2 = SUPR3CallVMMR0Ex(CreateVMReq.pVMR0, 0 /*idCpu*/, VMMR0_DO_GVMM_DESTROY_VM, 0, NULL);
        AssertRC(rc2);
    }
    else
        vmR3SetErrorU(pUVM, rc, RT_SRC_POS, N_("VM creation failed (GVMM)"));

    LogFlow(("vmR3CreateU: returns %Rrc\n", rc));
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
    int rc = SUPR3CallVMMR0Ex(pVM->pVMR0, idCpu, VMMR0_DO_GVMM_REGISTER_VMCPU, 0, NULL);
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
    for (VMCPUID idCpu = 1; idCpu < pVM->cCpus; idCpu++)
    {
        rc = VMR3ReqCallWaitU(pUVM, idCpu, (PFNRT)vmR3RegisterEMT, 2, pVM, idCpu);
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

        for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
        {
            rc = STAMR3RegisterF(pVM, &pUVM->aCpus[idCpu].vm.s.StatHaltYield,  STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling halted state yielding.", "/PROF/VM/CPU%d/Halt/Yield", idCpu);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pUVM->aCpus[idCpu].vm.s.StatHaltBlock,  STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling halted state blocking.", "/PROF/VM/CPU%d/Halt/Block", idCpu);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pUVM->aCpus[idCpu].vm.s.StatHaltTimers, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling halted state timer tasks.", "/PROF/VM/CPU%d/Halt/Timers", idCpu);
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
 * Logger callback for inserting a custom prefix.
 *
 * @returns Number of chars written.
 * @param   pLogger             The logger.
 * @param   pchBuf              The output buffer.
 * @param   cchBuf              The output buffer size.
 * @param   pvUser              Pointer to the UVM structure.
 */
static DECLCALLBACK(size_t) vmR3LogPrefixCallback(PRTLOGGER pLogger, char *pchBuf, size_t cchBuf, void *pvUser)
{
    AssertReturn(cchBuf >= 2, 0);
    PUVM        pUVM   = (PUVM)pvUser;
    PUVMCPU     pUVCpu = (PUVMCPU)RTTlsGet(pUVM->vm.s.idxTLS);
    if (pUVCpu)
    {
        static const char s_szHex[17] = "0123456789abcdef";
        VMCPUID const     idCpu       = pUVCpu->idCpu;
        pchBuf[1] = s_szHex[ idCpu       & 15];
        pchBuf[0] = s_szHex[(idCpu >> 4) & 15];
    }
    else
    {
        pchBuf[0] = 'x';
        pchBuf[1] = 'y';
    }

    return 2;
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
 * @thread  EMT
 */
static DECLCALLBACK(int) vmR3PowerOn(PVM pVM)
{
    LogFlow(("vmR3PowerOn: pVM=%p\n", pVM));

    /*
     * EMT(0) does the actual power on work *before* the other EMTs
     * get here, they just need to set their state to STARTED so they
     * get out of the EMT loop and into EM.
     */
    PVMCPU pVCpu = VMMGetCpu(pVM);
    VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED);
    if (pVCpu->idCpu != 0)
        return VINF_SUCCESS;

    /*
     * Try change the state.
     */
    int rc = vmR3TrySetState(pVM, "VMR3PowerOff", 1, VMSTATE_POWERING_ON, VMSTATE_CREATED);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Change the state, notify the components and resume the execution.
     */
    PDMR3PowerOn(pVM);
    vmR3SetState(pVM, VMSTATE_RUNNING, VMSTATE_POWERING_ON);

    return VINF_SUCCESS;
}


/**
 * Powers on the virtual machine.
 *
 * @returns VBox status code.
 *
 * @param   pVM         The VM to power on.
 *
 * @thread      Any thread.
 * @vmstate     Created
 * @vmstateto   PoweringOn, Running
 */
VMMR3DECL(int) VMR3PowerOn(PVM pVM)
{
    LogFlow(("VMR3PowerOn: pVM=%p\n", pVM));
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);

    /*
     * Forward the request to the EMTs (EMT(0) first as it does all the
     * work upfront).
     */
    int rc = VMR3ReqCallWaitU(pVM->pUVM, VMCPUID_ALL, (PFNRT)vmR3PowerOn, 1, pVM);
    LogFlow(("VMR3PowerOn: returns %Rrc\n", rc));
    return rc;
}


/**
 * EMT worker for vmR3SuspendCommon.
 *
 * @returns VBox strict status code.
 * @retval  VINF_EM_SUSPEND.
 * @retval  VERR_VM_INVALID_VM_STATE.
 *
 * @param   pVM     VM to suspend.
 * @param   fFatal  Whether it's a fatal error or normal suspend.
 *
 * @thread  EMT
 */
static DECLCALLBACK(int) vmR3Suspend(PVM pVM, bool fFatal)
{
    LogFlow(("vmR3Suspend: pVM=%p\n", pVM));

    /*
     * The first EMT switches the state to suspending.
     */
    PVMCPU pVCpu = VMMGetCpu(pVM);
    if (pVCpu->idCpu == pVM->cCpus - 1)
    {
        int rc = vmR3TrySetState(pVM, "VMR3Suspend", 2,
                                 VMSTATE_SUSPENDING, VMSTATE_RUNNING,
                                 VMSTATE_SUSPENDING_LS, VMSTATE_RUNNING_LS);
        if (RT_FAILURE(rc))
            return rc;
    }

    VMSTATE enmVMState = VMR3GetState(pVM);
    AssertMsgReturn(    enmVMState == VMSTATE_SUSPENDING
                    ||  enmVMState == VMSTATE_SUSPENDING_LS,
                    ("%s\n", VMR3GetStateName(enmVMState)),
                    VERR_INTERNAL_ERROR_5);

    /*
     * EMT(0) does the actually suspending *after* all the other CPUs has
     * been thru here.
     */
    if (pVCpu->idCpu == 0)
    {
        /* Perform suspend notification. */
        PDMR3Suspend(pVM);

        /*
         * Change to the final state. Live saving makes this a wee bit more
         * complicated than one would like.
         */
        PUVM pUVM = pVM->pUVM;
        RTCritSectEnter(&pUVM->vm.s.AtStateCritSect);
        VMSTATE enmVMState = pVM->enmVMState;
        if (enmVMState != VMSTATE_SUSPENDING_LS)
            vmR3SetStateLocked(pVM, pUVM, fFatal ? VMSTATE_FATAL_ERROR : VMSTATE_SUSPENDED, VMSTATE_SUSPENDING);
        else if (!fFatal)
            vmR3SetStateLocked(pVM, pUVM, VMSTATE_SUSPENDED_LS, VMSTATE_SUSPENDING_LS);
        else
        {
            vmR3SetStateLocked(pVM, pUVM, VMSTATE_FATAL_ERROR_LS, VMSTATE_SUSPENDING_LS);
            SSMR3Cancel(pVM);
        }
        RTCritSectLeave(&pUVM->vm.s.AtStateCritSect);
    }

    return VINF_EM_SUSPEND;
}


/**
 * Common worker for VMR3Suspend and vmR3SetRuntimeErrorCommon.
 *
 * They both suspends the VM, but the latter ends up in the VMSTATE_FATAL_ERROR
 * instead of VMSTATE_SUSPENDED.
 *
 * @returns VBox strict status code.
 * @param   pVM                 The VM handle.
 * @param   fFatal              Whether it's a fatal error or not.
 *
 * @thread      Any thread.
 * @vmstate     Running or RunningLS
 * @vmstateto   Suspending + Suspended/FatalError or SuspendingLS +
 *              SuspendedLS/FatalErrorLS
 */
static int vmR3SuspendCommon(PVM pVM, bool fFatal)
{
    /*
     * Forward the operation to EMT in reverse order so EMT(0) can do the
     * actual suspending after the other ones have stopped running guest code.
     */
    return VMR3ReqCallWaitU(pVM->pUVM, VMCPUID_ALL_REVERSE, (PFNRT)vmR3Suspend, 2, pVM, fFatal);
}


/**
 * Suspends a running VM.
 *
 * @returns VBox status code. When called on EMT, this will be a strict status
 *          code that has to be propagated up the call stack.
 *
 * @param   pVM     The VM to suspend.
 *
 * @thread      Any thread.
 * @vmstate     Running or RunningLS
 * @vmstateto   Suspending + Suspended or SuspendingLS + SuspendedLS
 */
VMMR3DECL(int) VMR3Suspend(PVM pVM)
{
    LogFlow(("VMR3Suspend: pVM=%p\n", pVM));
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    int rc = vmR3SuspendCommon(pVM, false /*fFatal*/);
    LogFlow(("VMR3Suspend: returns %Rrc\n", rc));
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
     * EMT(0) does all the work *before* the others wake up.
     */
    PVMCPU pVCpu = VMMGetCpu(pVM);
    if (pVCpu->idCpu == 0)
    {
        int rc = vmR3TrySetState(pVM, "VMR3Resume", 1, VMSTATE_RESUMING, VMSTATE_SUSPENDED);
        if (RT_FAILURE(rc))
            return rc;

        /* Perform resume notifications. */
        PDMR3Resume(pVM);

        /* Advance to the final state. */
        vmR3SetState(pVM, VMSTATE_RUNNING, VMSTATE_RESUMING);
    }

    /** @todo there is a race here: Someone could suspend, power off, raise a fatal
     *        error (both kinds), save the vm, or start a live save operation before
     *        we get here on all CPUs. Only safe way is a cross call, or to make
     *        the last thread flip the state from Resuming to Running. While the
     *        latter seems easy and perhaps more attractive, the former might be
     *        better wrt TSC/TM... */
    AssertMsgReturn(VMR3GetState(pVM) == VMSTATE_RUNNING, ("%s\n", VMR3GetStateName(VMR3GetState(pVM))), VERR_VM_INVALID_VM_STATE);
    return VINF_EM_RESUME;
}




/**
 * Resume VM execution.
 *
 * @returns VBox status code. When called on EMT, this will be a strict status
 *          code that has to be propagated up the call stack.
 *
 * @param   pVM         The VM to resume.
 *
 * @thread      Any thread.
 * @vmstate     Suspended
 * @vmstateto   Running
 */
VMMR3DECL(int) VMR3Resume(PVM pVM)
{
    LogFlow(("VMR3Resume: pVM=%p\n", pVM));
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);

    /*
     * Forward the request to the EMTs (EMT(0) first as it does all the
     * work upfront).
     */
    int rc = VMR3ReqCallWaitU(pVM->pUVM, VMCPUID_ALL, (PFNRT)vmR3Resume, 1, pVM);
    LogFlow(("VMR3Resume: returns %Rrc\n", rc));
    return rc;
}


/**
 * Worker for VMR3Save that validates the state and calls SSMR3Save.
 *
 * @returns VBox status code.
 *
 * @param   pVM             The VM handle.
 * @param   pszFilename     The name of the save state file.
 * @param   enmAfter        What to do afterwards.
 * @param   pfnProgress     Progress callback. Optional.
 * @param   pvUser          User argument for the progress callback.
 * @param   ppSSM           Where to return the saved state handle in case of a
 *                          live snapshot scenario.
 * @thread  EMT
 */
static DECLCALLBACK(int) vmR3Save(PVM pVM, const char *pszFilename, SSMAFTER enmAfter, PFNVMPROGRESS pfnProgress, void *pvUser, PSSMHANDLE *ppSSM)
{
    LogFlow(("vmR3Save: pVM=%p pszFilename=%p:{%s} enmAfter=%d pfnProgress=%p pvUser=%p ppSSM=%p\n", pVM, pszFilename, pszFilename, enmAfter, pfnProgress, pvUser, ppSSM));

    /*
     * Validate input.
     */
    AssertPtr(pszFilename);
    AssertPtr(pVM);
    Assert(enmAfter == SSMAFTER_DESTROY || enmAfter == SSMAFTER_CONTINUE);
    AssertPtr(ppSSM);
    *ppSSM = NULL;

    /*
     * Change the state and perform/start the saving.
     */
    int rc = vmR3TrySetState(pVM, "VMR3Save", 2,
                             VMSTATE_SAVING, VMSTATE_SUSPENDED,
                             VMSTATE_RUNNING, VMSTATE_RUNNING_LS);
    if (rc == 1)
    {
        rc = SSMR3Save(pVM, pszFilename, enmAfter, pfnProgress, pvUser);
        vmR3SetState(pVM, VMSTATE_SUSPENDED, VMSTATE_SAVING);
    }
    else if (rc == 2)
    {
        rc = SSMR3LiveToFile(pVM, pszFilename, enmAfter, pfnProgress, pvUser, ppSSM);
        /* (We're not subject to cancellation just yet.) */
    }
    else
        Assert(RT_FAILURE(rc));
    return rc;
}


/**
 * Worker for VMR3Save to clean up a SSMR3LiveDoStep1 failure.
 *
 * We failed after hitting the RunningLS state, but before trying to suspend the
 * VM before vmR3SaveLiveStep2.  There are a number of state transisions in this
 * state, some, like ResetLS, that requires some special handling.  (ResetLS is
 * the excuse for doing this all on EMT(0).
 *
 * @returns VBox status code.
 *
 * @param   pVM             The VM handle.
 * @param   pSSM            The handle of saved state operation. This will be
 *                          closed.
 * @thread  EMT(0)
 */
static DECLCALLBACK(int) vmR3SaveLiveStep1Cleanup(PVM pVM, PSSMHANDLE pSSM)
{
    LogFlow(("vmR3SaveLiveStep2: pVM=%p pSSM=%p\n", pVM, pSSM));
    VM_ASSERT_EMT0(pVM);

    /*
     * Finish the SSM state first (or move the ssmR3SetCancellable call),
     * then change the state out of the *LS variants.
     */
    int rc = SSMR3LiveDone(pSSM);
    int rc2 = vmR3TrySetState(pVM, "vmR3SaveLiveStep1Cleanup", 8,
                              VMSTATE_RUNNING,           VMSTATE_RUNNING_LS,
                              VMSTATE_RUNNING,           VMSTATE_RESET_LS,
                              VMSTATE_SUSPENDING,        VMSTATE_SUSPENDING_LS, /* external*/
                              VMSTATE_GURU_MEDITATION,   VMSTATE_GURU_MEDITATION_LS,
                              VMSTATE_FATAL_ERROR,       VMSTATE_FATAL_ERROR_LS,
                              VMSTATE_POWERING_OFF,      VMSTATE_POWERING_OFF_LS,
                              VMSTATE_OFF,               VMSTATE_OFF_LS,
                              VMSTATE_DEBUGGING,         VMSTATE_DEBUGGING_LS);
    if (RT_SUCCESS(rc))
    {
        if (RT_SUCCESS(rc2))
            rc = rc2 == 2 /* ResetLS -> Running */ ? VINF_EM_RESUME : VINF_SUCCESS;
        else
            rc = rc2;
    }
/** @todo VMR3Reset during live save (ResetLS, ResettingLS) needs to be
 *        redone. We should suspend the VM after resetting the state, not
 *        cancelling the save operation. In the live migration scenario we
 *        would already have transfered most of the state and the little that
 *        remains after a reset isn't going to be very big and it's not worth
 *        making special paths for this. In the live snapshot case, there
 *        would be a gain in that we wouldn't require a potentially large saved
 *        state file. But that could be handled on VMR3Save return and size
 *        shouldn't matter much as already mentioned..
 *
 *        Will address this tomorrow. */
    return rc;
}


/**
 * Worker for VMR3Save continues a live save on EMT(0).
 *
 * @returns VBox status code.
 *
 * @param   pVM             The VM handle.
 * @param   pSSM            The handle of saved state operation.
 * @thread  EMT(0)
 */
static DECLCALLBACK(int) vmR3SaveLiveStep2(PVM pVM, PSSMHANDLE pSSM)
{
    LogFlow(("vmR3SaveLiveStep2: pVM=%p pSSM=%p\n", pVM, pSSM));
    VM_ASSERT_EMT0(pVM);

    vmR3SetState(pVM, VMSTATE_SAVING, VMSTATE_SUSPENDED_LS);

    int rc  = SSMR3LiveDoStep2(pSSM);
    int rc2 = SSMR3LiveDone(pSSM);
    if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
        rc = rc2;

    vmR3SetState(pVM, VMSTATE_SUSPENDED, VMSTATE_SAVING);

    return rc;
}



/**
 * Save current VM state.
 *
 * Can be used for both saving the state and creating snapshots.
 *
 * When called for a VM in the Running state, the saved state is created live
 * and the VM is only suspended when the final part of the saving is preformed.
 * The VM state will not be restored to Running in this case and it's up to the
 * caller to call VMR3Resume if this is desirable.  (The rational is that the
 * caller probably wish to reconfigure the disks before resuming the VM.)
 *
 * @returns VBox status code.
 *
 * @param   pVM                 The VM which state should be saved.
 * @param   pszFilename         The name of the save state file.
 * @param   fContinueAfterwards Whether continue execution afterwards or not.
 *                              When in doubt, set this to true.
 * @param   pfnProgress         Progress callback. Optional.
 * @param   pvUser              User argument for the progress callback.
 *
 * @thread      Non-EMT.
 * @vmstate     Suspended or Running
 * @vmstateto   Saving+Suspended or
 *              RunningLS+SuspeningLS+SuspendedLS+Saving+Suspended.
 */
VMMR3DECL(int) VMR3Save(PVM pVM, const char *pszFilename, bool fContinueAfterwards, PFNVMPROGRESS pfnProgress, void *pvUser)
{
    LogFlow(("VMR3Save: pVM=%p pszFilename=%p:{%s} fContinueAfterwards=%RTbool pfnProgress=%p pvUser=%p\n",
             pVM, pszFilename, pszFilename, fContinueAfterwards, pfnProgress, pvUser));

    /*
     * Validate input.
     */
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    VM_ASSERT_OTHER_THREAD(pVM);
    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);
    AssertReturn(*pszFilename, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pfnProgress, VERR_INVALID_POINTER);

    /*
     * Request the operation in EMT(0).
     */
    SSMAFTER    enmAfter = fContinueAfterwards ? SSMAFTER_CONTINUE : SSMAFTER_DESTROY;
    PSSMHANDLE  pSSM;
    int rc = VMR3ReqCallWaitU(pVM->pUVM, 0 /*idDstCpu*/,
                              (PFNRT)vmR3Save, 6, pVM, pszFilename, enmAfter, pfnProgress, pvUser, &pSSM);
    if (    RT_SUCCESS(rc)
        &&  pSSM)
    {
        /*
         * Live snapshot.
         *
         * The state handling here is kind of tricky, doing it on EMT(0)
         * helps abit. See the VMSTATE diagram for details. The EMT(0) calls
         * consumes the pSSM handle and calls SSMR3LiveDone.
         */
        rc = SSMR3LiveDoStep1(pSSM);
        if (RT_SUCCESS(rc))
            rc = vmR3SuspendCommon(pVM, false /*fFatal*/); /** @todo this races external VMR3Suspend calls and may cause trouble (goes for any VMCPUID_ALL* calls messing with the state in the handler). */
        if (RT_SUCCESS(rc))
            rc = VMR3ReqCallWaitU(pVM->pUVM, 0 /*idDstCpu*/, (PFNRT)vmR3SaveLiveStep2, 2, pVM, pSSM);
        else
        {
            int rc2 = VMR3ReqCallWait(pVM, 0 /*idDstCpu*/, (PFNRT)vmR3SaveLiveStep1Cleanup, 2, pVM, pSSM);
            AssertLogRelRC(rc2);
        }
    }

    LogFlow(("VMR3Save: returns %Rrc\n", rc));
    return rc;
}


/**
 * Loads a new VM state.
 *
 * To restore a saved state on VM startup, call this function and then
 * resume the VM instead of powering it on.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pszFilename     The name of the save state file.
 * @param   pfnProgress     Progress callback. Optional.
 * @param   pvUser          User argument for the progress callback.
 * @thread  EMT.
 */
static DECLCALLBACK(int) vmR3Load(PVM pVM, const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser)
{
    LogFlow(("vmR3Load: pVM=%p pszFilename=%p:{%s} pfnProgress=%p pvUser=%p\n", pVM, pszFilename, pszFilename, pfnProgress, pvUser));

    /*
     * Validate input (paranoia).
     */
    AssertPtr(pVM);
    AssertPtr(pszFilename);

    /*
     * Change the state and perform the load.
     *
     * Always perform a relocation round afterwards to make sure hypervisor
     * selectors and such are correct.
     */
    int rc = vmR3TrySetState(pVM, "VMR3Load", 2,
                             VMSTATE_LOADING, VMSTATE_CREATED,
                             VMSTATE_LOADING, VMSTATE_SUSPENDED);
    if (RT_FAILURE(rc))
        return rc;

    rc = SSMR3Load(pVM, pszFilename, SSMAFTER_RESUME, pfnProgress,  pvUser);
    if (RT_SUCCESS(rc))
    {
        VMR3Relocate(pVM, 0 /*offDelta*/);
        vmR3SetState(pVM, VMSTATE_SUSPENDED, VMSTATE_LOADING);
    }
    else
    {
        vmR3SetState(pVM, VMSTATE_LOAD_FAILURE, VMSTATE_LOADING);
        rc = VMSetError(pVM, rc, RT_SRC_POS,
                        N_("Unable to restore the virtual machine's saved state from '%s'.  It may be damaged or from an older version of VirtualBox.  Please discard the saved state before starting the virtual machine"),
                        pszFilename);
    }

    return rc;
}


/**
 * Loads a VM state into a newly created VM or a one that is suspended.
 *
 * To restore a saved state on VM startup, call this function and then resume
 * the VM instead of powering it on.
 *
 * @returns VBox status code.
 *
 * @param   pVM             The VM handle.
 * @param   pszFilename     The name of the save state file.
 * @param   pfnProgress     Progress callback. Optional.
 * @param   pvUser          User argument for the progress callback.
 *
 * @thread      Any thread.
 * @vmstate     Created, Suspended
 * @vmstateto   Loading+Suspended
 */
VMMR3DECL(int) VMR3Load(PVM pVM, const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser)
{
    LogFlow(("VMR3Load: pVM=%p pszFilename=%p:{%s} pfnProgress=%p pvUser=%p\n", pVM, pszFilename, pszFilename, pfnProgress, pvUser));

    /*
     * Validate input.
     */
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);

    /*
     * Forward the request to EMT(0).
     */
    int rc = VMR3ReqCallWaitU(pVM->pUVM, 0 /*idDstCpu*/,
                              (PFNRT)vmR3Load, 4, pVM, pszFilename, pfnProgress, pvUser);
    LogFlow(("VMR3Load: returns %Rrc\n", rc));
    return rc;
}


/**
 * Worker for VMR3PowerOff that does the actually powering off on EMT(0) after
 * cycling thru the other EMTs first.
 *
 * @returns VBox strict status code.
 *
 * @param   pVM     The VM handle.
 *
 * @thread      EMT.
 */
static DECLCALLBACK(int) vmR3PowerOff(PVM pVM)
{
    LogFlow(("vmR3PowerOff: pVM=%p\n", pVM));

    /*
     * The first EMT thru here will change the state to PoweringOff.
     */
    PVMCPU pVCpu = VMMGetCpu(pVM);
    if (pVCpu->idCpu == pVM->cCpus - 1)
    {
        int rc = vmR3TrySetState(pVM, "VMR3PowerOff", 10,
                                 VMSTATE_POWERING_OFF,    VMSTATE_RUNNING,
                                 VMSTATE_POWERING_OFF,    VMSTATE_SUSPENDED,
                                 VMSTATE_POWERING_OFF,    VMSTATE_DEBUGGING,
                                 VMSTATE_POWERING_OFF,    VMSTATE_LOAD_FAILURE,
                                 VMSTATE_POWERING_OFF,    VMSTATE_GURU_MEDITATION,
                                 VMSTATE_POWERING_OFF,    VMSTATE_FATAL_ERROR,
                                 VMSTATE_POWERING_OFF_LS, VMSTATE_RUNNING_LS,
                                 VMSTATE_POWERING_OFF_LS, VMSTATE_DEBUGGING_LS,
                                 VMSTATE_POWERING_OFF_LS, VMSTATE_GURU_MEDITATION_LS,
                                 VMSTATE_POWERING_OFF_LS, VMSTATE_FATAL_ERROR_LS);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Check the state.
     */
    VMSTATE enmVMState = VMR3GetState(pVM);
    AssertMsgReturn(   enmVMState == VMSTATE_POWERING_OFF
                    || enmVMState == VMSTATE_POWERING_OFF_LS,
                    ("%s\n", VMR3GetStateName(enmVMState)),
                    VERR_VM_INVALID_VM_STATE);

    /*
     * EMT(0) does the actual power off work here *after* all the other EMTs
     * have been thru and entered the STOPPED state.
     */
    VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STOPPED);
    if (pVCpu->idCpu == 0)
    {
        /*
         * For debugging purposes, we will log a summary of the guest state at this point.
         */
        if (enmVMState != VMSTATE_GURU_MEDITATION)
        {
            /** @todo SMP support? */
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
#if 1 // "temporary" while debugging #1589
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
         * Perform the power off notifications and advance the state to
         * Off or OffLS.
         */
        PDMR3PowerOff(pVM);

        PUVM pUVM = pVM->pUVM;
        RTCritSectEnter(&pUVM->vm.s.AtStateCritSect);
        enmVMState = pVM->enmVMState;
        if (enmVMState == VMSTATE_POWERING_OFF_LS)
            vmR3SetStateLocked(pVM, pUVM, VMSTATE_OFF_LS, VMSTATE_POWERING_OFF_LS);
        else
            vmR3SetStateLocked(pVM, pUVM, VMSTATE_OFF,    VMSTATE_POWERING_OFF);
        RTCritSectLeave(&pUVM->vm.s.AtStateCritSect);
    }
    return VINF_EM_OFF;
}


/**
 * Power off the VM.
 *
 * @returns VBox status code. When called on EMT, this will be a strict status
 *          code that has to be propagated up the call stack.
 *
 * @param   pVM     The handle of the VM to be powered off.
 *
 * @thread      Any thread.
 * @vmstate     Suspended, Running, Guru Meditation, Load Failure
 * @vmstateto   Off or OffLS
 */
VMMR3DECL(int)   VMR3PowerOff(PVM pVM)
{
    LogFlow(("VMR3PowerOff: pVM=%p\n", pVM));
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);

    /*
     * Forward the request to the EMTs in reverse order, making all the other
     * EMTs stop working before EMT(0) comes and does the actual powering off.
     */
    int rc = VMR3ReqCallWaitU(pVM->pUVM, VMCPUID_ALL_REVERSE, (PFNRT)vmR3PowerOff, 1, pVM);
    LogFlow(("VMR3PowerOff: returns %Rrc\n", rc));
    return rc;
}


/**
 * Destroys the VM.
 *
 * The VM must be powered off (or never really powered on) to call this
 * function. The VM handle is destroyed and can no longer be used up successful
 * return.
 *
 * @returns VBox status code.
 *
 * @param   pVM     The handle of the VM which should be destroyed.
 *
 * @thread      EMT(0) or any none emulation thread.
 * @vmstate     Off, Created
 * @vmstateto   N/A
 */
VMMR3DECL(int) VMR3Destroy(PVM pVM)
{
    LogFlow(("VMR3Destroy: pVM=%p\n", pVM));

    /*
     * Validate input.
     */
    if (!pVM)
        return VERR_INVALID_PARAMETER;
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    Assert(VMMGetCpuId(pVM) == 0 || VMMGetCpuId(pVM) == NIL_VMCPUID);

    /*
     * Change VM state to destroying and unlink the VM.
     */
    int rc = vmR3TrySetState(pVM, "VMR3Destroy", 1, VMSTATE_DESTROYING, VMSTATE_OFF);
    if (RT_FAILURE(rc))
        return rc;

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
     */
    vmR3AtDtor(pVM);

    /*
     * EMT(0) does the final cleanup, so if we're it calling VMR3Destroy then
     * we'll have to postpone parts of it till later.  Otherwise, call
     * vmR3Destroy on each of the EMTs in ending with EMT(0) doing the bulk
     * of the cleanup.
     */
    if (VMMGetCpuId(pVM) == 0)
    {
        pUVM->vm.s.fEMTDoesTheCleanup = true;
        pUVM->vm.s.fTerminateEMT = true;
        VM_FF_SET(pVM, VM_FF_TERMINATE);

        /* Terminate the other EMTs. */
        for (VMCPUID idCpu = 1; idCpu < pVM->cCpus; idCpu++)
        {
            int rc = VMR3ReqCallWaitU(pUVM, idCpu, (PFNRT)vmR3Destroy, 1, pVM);
            AssertLogRelRC(rc);
        }
    }
    else
    {
        /* vmR3Destroy on all EMTs, ending with EMT(0). */
        int rc = VMR3ReqCallWaitU(pUVM, VMCPUID_ALL_REVERSE, (PFNRT)vmR3Destroy, 1, pVM);
        AssertLogRelRC(rc);

        /* Wait for EMTs and destroy the UVM. */
        vmR3DestroyUVM(pUVM, 30000);
    }

    LogFlow(("VMR3Destroy: returns VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}


/**
 * Internal destruction worker.
 *
 * This is either called from VMR3Destroy via VMR3ReqCallU or from
 * vmR3EmulationThreadWithId when EMT(0) terminates after having called
 * VMR3Destroy().
 *
 * When called on EMT(0), it will performed the great bulk of the destruction.
 * When called on the other EMTs, they will do nothing and the whole purpose is
 * to return VINF_EM_TERMINATE so they break out of their run loops.
 *
 * @returns VINF_EM_TERMINATE.
 * @param   pVM     The VM handle.
 */
DECLCALLBACK(int) vmR3Destroy(PVM pVM)
{
    PUVM   pUVM  = pVM->pUVM;
    PVMCPU pVCpu = VMMGetCpu(pVM);
    Assert(pVCpu);
    LogFlow(("vmR3Destroy: pVM=%p pUVM=%p pVCpu=%p idCpu=%u\n", pVM, pUVM, pVCpu, pVCpu->idCpu));

    /*
     * Only VCPU 0 does the full cleanup.
     */
    if (pVCpu->idCpu == 0)
    {

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
        pUVM->vm.s.pvDBGC = NULL;
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
        SSMR3Term(pVM);
        rc = PDMR3CritSectTerm(pVM);
        AssertRC(rc);
        rc = MMR3Term(pVM);
        AssertRC(rc);

        /*
         * We're done in this thread (EMT).
         */
        ASMAtomicUoWriteBool(&pUVM->vm.s.fTerminateEMT, true);
        ASMAtomicWriteU32(&pVM->fGlobalForcedActions, VM_FF_TERMINATE);
        LogFlow(("vmR3Destroy: returning %Rrc\n", VINF_EM_TERMINATE));
    }
    return VINF_EM_TERMINATE;
}


/**
 * Called at the end of the EMT procedure to take care of the final cleanup.
 *
 * Currently only EMT(0) will do work here.  It will destroy the shared VM
 * structure if it is still around.  If EMT(0) was the caller of VMR3Destroy it
 * will destroy UVM and nothing will be left behind upon exit.  But if some
 * other thread is calling VMR3Destroy, they will clean up UVM after all EMTs
 * has exitted.
 *
 * @param   pUVM        The UVM handle.
 * @param   idCpu       The virtual CPU id.
 */
void vmR3DestroyFinalBitFromEMT(PUVM pUVM, VMCPUID idCpu)
{
    /*
     * Only EMT(0) has work to do here.
     */
    if (idCpu != 0)
        return;
    Assert(   !pUVM->pVM
           || VMMGetCpuId(pUVM->pVM) == 0);

    /*
     * If we have a shared VM structure, change its state to Terminated and
     * tell GVMM to destroy it.
     */
    if (pUVM->pVM)
    {
        vmR3SetState(pUVM->pVM, VMSTATE_TERMINATED, VMSTATE_DESTROYING);
        int rc = SUPR3CallVMMR0Ex(pUVM->pVM->pVMR0, 0 /*idCpu*/, VMMR0_DO_GVMM_DESTROY_VM, 0, NULL);
        AssertLogRelRC(rc);
        pUVM->pVM = NULL;
    }

    /*
     * If EMT(0) called VMR3Destroy, then it will destroy UVM as well.
     */
    if (pUVM->vm.s.fEMTDoesTheCleanup)
        vmR3DestroyUVM(pUVM, 30000);
}


/**
 * Destroys the UVM portion.
 *
 * This is called as the final step in the VM destruction or as the cleanup
 * in case of a creation failure. If EMT(0) called VMR3Destroy, meaning
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
        int rc = SUPR3Term(false /*fForced*/);
        AssertRC(rc);
        pUVM->vm.s.pSession = NIL_RTR0PTR;
    }

    /*
     * Destroy the MM heap and free the UVM structure.
     */
    MMR3TermUVM(pUVM);
    STAMR3TermUVM(pUVM);

#ifdef LOG_ENABLED
    RTLogSetCustomPrefixCallback(NULL, NULL, NULL);
#endif
    RTTlsFree(pUVM->vm.s.idxTLS);

    ASMAtomicUoWriteU32(&pUVM->u32Magic, UINT32_MAX);
    RTMemPageFree(pUVM);

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
 * This is called by the emulation threads as a response to the
 * reset request issued by VMR3Reset().
 *
 * @returns VBox status code.
 * @param   pVM     VM to reset.
 */
static DECLCALLBACK(int) vmR3Reset(PVM pVM)
{
    int    rcRet = VINF_EM_RESET;
    PVMCPU pVCpu = VMMGetCpu(pVM);

    /*
     * The first EMT will try change the state to resetting.
     * We do the live save cancellation inside the state critsect because it
     * is cleaner and safer.
     */
    if (pVCpu->idCpu == pVM->cCpus - 1)
    {
        PUVM pUVM = pVM->pUVM;
        RTCritSectEnter(&pUVM->vm.s.AtStateCritSect);
        int rc = vmR3TrySetState(pVM, "VMR3Reset", 3,
                                 VMSTATE_RESETTING,     VMSTATE_RUNNING,
                                 VMSTATE_RESETTING,     VMSTATE_SUSPENDED,
                                 VMSTATE_RESETTING_LS,  VMSTATE_RUNNING_LS);
        if (rc == 3)
            SSMR3Cancel(pVM);
        RTCritSectLeave(&pUVM->vm.s.AtStateCritSect);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Check the state.
     */
    VMSTATE enmVMState = VMR3GetState(pVM);
    AssertLogRelMsgReturn(   enmVMState == VMSTATE_RESETTING
                          || enmVMState == VMSTATE_RESETTING_LS,
                          ("%s\n", VMR3GetStateName(enmVMState)),
                          VERR_VM_INVALID_VM_STATE);

    /*
     * EMT(0) does the full cleanup *after* all the other EMTs has been
     * thru here and been told to enter the EMSTATE_WAIT_SIPI state.
     *
     * Because there are per-cpu reset routines and order may/is important,
     * the following sequence looks a bit ugly...
     */
    if (pVCpu->idCpu == 0)
        vmR3CheckIntegrity(pVM);

    /* Reset the VCpu state. */
    VMCPU_ASSERT_STATE(pVCpu, VMCPUSTATE_STARTED);

    /* Clear all pending forced actions. */
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_ALL_MASK & ~VMCPU_FF_REQUEST);

    /*
     * Reset the VM components.
     */
    if (pVCpu->idCpu == 0)
    {
        PATMR3Reset(pVM);
        CSAMR3Reset(pVM);
        PGMR3Reset(pVM);                    /* We clear VM RAM in PGMR3Reset. It's vital PDMR3Reset is executed
                                             * _afterwards_. E.g. ACPI sets up RAM tables during init/reset. */
        MMR3Reset(pVM);
        PDMR3Reset(pVM);
        SELMR3Reset(pVM);
        TRPMR3Reset(pVM);
        REMR3Reset(pVM);
        IOMR3Reset(pVM);
        CPUMR3Reset(pVM);
    }
    CPUMR3ResetCpu(pVCpu);
    if (pVCpu->idCpu == 0)
    {
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
         * Since EMT(0) is the last to go thru here, it will advance the state.
         */
        PUVM pUVM = pVM->pUVM;
        RTCritSectEnter(&pUVM->vm.s.AtStateCritSect);
        enmVMState = pVM->enmVMState;
        if (enmVMState == VMSTATE_RESETTING)
        {
            if (pUVM->vm.s.enmPrevVMState == VMSTATE_SUSPENDED)
                vmR3SetStateLocked(pVM, pUVM, VMSTATE_SUSPENDED, VMSTATE_RESETTING);
            else
                vmR3SetStateLocked(pVM, pUVM, VMSTATE_RUNNING,   VMSTATE_RESETTING);
        }
        else
        {
            /** @todo EMT(0) should not execute code if the state is
             *        VMSTATE_RESETTING_LS... This requires adding
             *        VINF_EM_RESET_AND_SUSPEND. Can be done later. */
            vmR3SetStateLocked(pVM, pUVM, VMSTATE_RESET_LS, VMSTATE_RESETTING_LS);
            rcRet = VINF_EM_RESET/*_AND_SUSPEND*/;
        }
        RTCritSectLeave(&pUVM->vm.s.AtStateCritSect);

        vmR3CheckIntegrity(pVM);
    }

    return rcRet;
}


/**
 * Reset the current VM.
 *
 * @returns VBox status code.
 * @param   pVM     VM to reset.
 */
VMMR3DECL(int) VMR3Reset(PVM pVM)
{
    LogFlow(("VMR3Reset:\n"));
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);

    /*
     * Forward the query on
     * Queue reset request to the emulation thread
     * and wait for it to be processed. (in reverse order as VCPU 0 does the real cleanup)
     */
    int rc = VMR3ReqCallWaitU(pVM->pUVM, VMCPUID_ALL_REVERSE, (PFNRT)vmR3Reset, 1, pVM);
    AssertLogRelRC(rc);
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
        case VMSTATE_LOADING:           return "LOADING";
        case VMSTATE_POWERING_ON:       return "POWERING_ON";
        case VMSTATE_RESUMING:          return "RESUMING";
        case VMSTATE_RUNNING:           return "RUNNING";
        case VMSTATE_RUNNING_LS:        return "RUNNING_LS";
        case VMSTATE_RESETTING:         return "RESETTING";
        case VMSTATE_RESETTING_LS:      return "RESETTING_LS";
        case VMSTATE_RESET_LS:          return "RESET_LS";
        case VMSTATE_SUSPENDED:         return "SUSPENDED";
        case VMSTATE_SUSPENDED_LS:      return "SUSPENDED_LS";
        case VMSTATE_SUSPENDING:        return "SUSPENDING";
        case VMSTATE_SUSPENDING_LS:     return "SUSPENDING_LS";
        case VMSTATE_SAVING:            return "SAVING";
        case VMSTATE_DEBUGGING:         return "DEBUGGING";
        case VMSTATE_DEBUGGING_LS:      return "DEBUGGING_LS";
        case VMSTATE_POWERING_OFF:      return "POWERING_OFF";
        case VMSTATE_POWERING_OFF_LS:   return "POWERING_OFF_LS";
        case VMSTATE_FATAL_ERROR:       return "FATAL_ERROR";
        case VMSTATE_FATAL_ERROR_LS:    return "FATAL_ERROR_LS";
        case VMSTATE_GURU_MEDITATION:   return "GURU_MEDITATION";
        case VMSTATE_GURU_MEDITATION_LS:return "GURU_MEDITATION_LS";
        case VMSTATE_LOAD_FAILURE:      return "LOAD_FAILURE";
        case VMSTATE_OFF:               return "OFF";
        case VMSTATE_DESTROYING:        return "DESTROYING";
        case VMSTATE_TERMINATED:        return "TERMINATED";

        default:
            AssertMsgFailed(("Unknown state %d\n", enmState));
            return "Unknown!\n";
    }
}


/**
 * Validates the state transition in strict builds.
 *
 * @returns true if valid, false if not.
 *
 * @param   enmStateOld         The old (current) state.
 * @param   enmStateNew         The proposed new state.
 *
 * @remarks The reference for this is found in doc/vp/VMM.vpp, the VMSTATE
 *          diagram (under State Machine Diagram).
 */
static bool vmR3ValidateStateTransition(VMSTATE enmStateOld, VMSTATE enmStateNew)
{
#ifdef VBOX_STRICT
    switch (enmStateOld)
    {
        case VMSTATE_CREATING:
            AssertMsgReturn(enmStateNew == VMSTATE_CREATED, ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_CREATED:
            AssertMsgReturn(   enmStateNew == VMSTATE_LOADING
                            || enmStateNew == VMSTATE_POWERING_ON
                            || enmStateNew == VMSTATE_POWERING_OFF
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_LOADING:
            AssertMsgReturn(   enmStateNew == VMSTATE_SUSPENDED
                            || enmStateNew == VMSTATE_LOAD_FAILURE
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_POWERING_ON:
            AssertMsgReturn(   enmStateNew == VMSTATE_RUNNING
                            || enmStateNew == VMSTATE_FATAL_ERROR /*?*/
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_RESUMING:
            AssertMsgReturn(   enmStateNew == VMSTATE_RUNNING
                            || enmStateNew == VMSTATE_FATAL_ERROR /*?*/
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_RUNNING:
            AssertMsgReturn(   enmStateNew == VMSTATE_POWERING_OFF
                            || enmStateNew == VMSTATE_SUSPENDING
                            || enmStateNew == VMSTATE_RESETTING
                            || enmStateNew == VMSTATE_RUNNING_LS
                            || enmStateNew == VMSTATE_DEBUGGING
                            || enmStateNew == VMSTATE_FATAL_ERROR
                            || enmStateNew == VMSTATE_GURU_MEDITATION
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_RUNNING_LS:
            AssertMsgReturn(   enmStateNew == VMSTATE_POWERING_OFF_LS
                            || enmStateNew == VMSTATE_SUSPENDING_LS
                            || enmStateNew == VMSTATE_RESETTING_LS
                            || enmStateNew == VMSTATE_RUNNING
                            || enmStateNew == VMSTATE_DEBUGGING_LS
                            || enmStateNew == VMSTATE_FATAL_ERROR_LS
                            || enmStateNew == VMSTATE_GURU_MEDITATION_LS
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_RESETTING:
            AssertMsgReturn(enmStateNew == VMSTATE_RUNNING, ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_RESETTING_LS:
            AssertMsgReturn(   enmStateNew == VMSTATE_RUNNING
                            || enmStateNew == VMSTATE_RESET_LS
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_RESET_LS:
            AssertMsgReturn(enmStateNew == VMSTATE_RUNNING, ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_SUSPENDING:
            AssertMsgReturn(enmStateNew == VMSTATE_SUSPENDED, ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_SUSPENDING_LS:
            AssertMsgReturn(   enmStateNew == VMSTATE_SUSPENDING
                            || enmStateNew == VMSTATE_SUSPENDED_LS
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_SUSPENDED:
            AssertMsgReturn(   enmStateNew == VMSTATE_POWERING_OFF
                            || enmStateNew == VMSTATE_SAVING
                            || enmStateNew == VMSTATE_RESETTING
                            || enmStateNew == VMSTATE_RESUMING
                            || enmStateNew == VMSTATE_LOADING
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_SUSPENDED_LS:
            AssertMsgReturn(   enmStateNew == VMSTATE_SUSPENDED_LS
                            || enmStateNew == VMSTATE_SUSPENDED
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_SAVING:
            AssertMsgReturn(enmStateNew == VMSTATE_SUSPENDED, ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_DEBUGGING:
            AssertMsgReturn(   enmStateNew == VMSTATE_RUNNING
                            || enmStateNew == VMSTATE_POWERING_OFF
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_DEBUGGING_LS:
            AssertMsgReturn(   enmStateNew == VMSTATE_DEBUGGING
                            || enmStateNew == VMSTATE_RUNNING_LS
                            || enmStateNew == VMSTATE_POWERING_OFF_LS
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_POWERING_OFF:
            AssertMsgReturn(enmStateNew == VMSTATE_OFF, ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_POWERING_OFF_LS:
            AssertMsgReturn(   enmStateNew == VMSTATE_POWERING_OFF
                            || enmStateNew == VMSTATE_OFF_LS
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_OFF:
            AssertMsgReturn(enmStateNew == VMSTATE_DESTROYING, ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_OFF_LS:
            AssertMsgReturn(enmStateNew == VMSTATE_OFF, ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_FATAL_ERROR:
            AssertMsgReturn(enmStateNew == VMSTATE_POWERING_OFF, ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_FATAL_ERROR_LS:
            AssertMsgReturn(   enmStateNew == VMSTATE_FATAL_ERROR
                            || enmStateNew == VMSTATE_POWERING_OFF_LS
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_GURU_MEDITATION:
            AssertMsgReturn(   enmStateNew == VMSTATE_DEBUGGING
                            || enmStateNew == VMSTATE_POWERING_OFF
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_GURU_MEDITATION_LS:
            AssertMsgReturn(   enmStateNew == VMSTATE_GURU_MEDITATION
                            || enmStateNew == VMSTATE_POWERING_OFF_LS
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_LOAD_FAILURE:
            AssertMsgReturn(enmStateNew == VMSTATE_POWERING_OFF, ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_DESTROYING:
            AssertMsgReturn(enmStateNew == VMSTATE_TERMINATED, ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_TERMINATED:
        default:
            AssertMsgFailedReturn(("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;
    }
#endif /* VBOX_STRICT */
    return true;
}


/**
 * Does the state change callouts.
 *
 * The caller owns the AtStateCritSect.
 *
 * @param   pVM                 The VM handle.
 * @param   pUVM                The UVM handle.
 * @param   enmStateNew         The New state.
 * @param   enmStateOld         The old state.
 */
static void vmR3DoAtState(PVM pVM, PUVM pUVM, VMSTATE enmStateNew, VMSTATE enmStateOld)
{
    LogRel(("Changing the VM state from '%s' to '%s'.\n", VMR3GetStateName(enmStateOld),  VMR3GetStateName(enmStateNew)));

    for (PVMATSTATE pCur = pUVM->vm.s.pAtState; pCur; pCur = pCur->pNext)
    {
        pCur->pfnAtState(pVM, enmStateNew, enmStateOld, pCur->pvUser);
        if (    enmStateNew     != VMSTATE_DESTROYING
            &&  pVM->enmVMState == VMSTATE_DESTROYING)
            break;
        AssertMsg(pVM->enmVMState == enmStateNew,
                  ("You are not allowed to change the state while in the change callback, except "
                   "from destroying the VM. There are restrictions in the way the state changes "
                   "are propagated up to the EM execution loop and it makes the program flow very "
                   "difficult to follow. (%s, expected %s, old %s)\n",
                   VMR3GetStateName(pVM->enmVMState), VMR3GetStateName(enmStateNew),
                   VMR3GetStateName(enmStateOld)));
    }
}


/**
 * Sets the current VM state, with the AtStatCritSect already entered.
 *
 * @param   pVM                 The VM handle.
 * @param   pUVM                The UVM handle.
 * @param   enmStateNew         The new state.
 * @param   enmStateOld         The old state.
 */
static void vmR3SetStateLocked(PVM pVM, PUVM pUVM, VMSTATE enmStateNew, VMSTATE enmStateOld)
{
    vmR3ValidateStateTransition(enmStateOld, enmStateNew);

    AssertMsg(pVM->enmVMState == enmStateOld,
              ("%s != %s\n", VMR3GetStateName(pVM->enmVMState), VMR3GetStateName(enmStateOld)));
    pUVM->vm.s.enmPrevVMState = enmStateOld;
    pVM->enmVMState           = enmStateNew;

    vmR3DoAtState(pVM, pUVM, enmStateNew, enmStateOld);
}


/**
 * Sets the current VM state.
 *
 * @param   pVM             VM handle.
 * @param   enmStateNew     The new state.
 * @param   enmStateOld     The old state (for asserting only).
 */
static void vmR3SetState(PVM pVM, VMSTATE enmStateNew, VMSTATE enmStateOld)
{
    PUVM pUVM = pVM->pUVM;
    RTCritSectEnter(&pUVM->vm.s.AtStateCritSect);

    AssertMsg(pVM->enmVMState == enmStateOld,
              ("%s != %s\n", VMR3GetStateName(pVM->enmVMState), VMR3GetStateName(enmStateOld)));
    vmR3SetStateLocked(pVM, pUVM, enmStateNew, pVM->enmVMState);

    RTCritSectLeave(&pUVM->vm.s.AtStateCritSect);
}


/**
 * Tries to perform a state transition.
 *
 * @returns The 1-based ordinal of the succeeding transition.
 *          VERR_VM_INVALID_VM_STATE and Assert+LogRel on failure.
 *
 * @param   pVM                 The VM handle.
 * @param   pszWho              Who is trying to change it.
 * @param   cTransitions        The number of transitions in the ellipsis.
 * @param   ...                 Transition pairs; new, old.
 */
static int vmR3TrySetState(PVM pVM, const char *pszWho, unsigned cTransitions, ...)
{
    va_list va;
    VMSTATE enmStateNew = VMSTATE_CREATED;
    VMSTATE enmStateOld = VMSTATE_CREATED;

#ifdef VBOX_STRICT
    /*
     * Validate the input first.
     */
    va_start(va, cTransitions);
    for (unsigned i = 0; i < cTransitions; i++)
    {
        enmStateNew = (VMSTATE)va_arg(va, /*VMSTATE*/int);
        enmStateOld = (VMSTATE)va_arg(va, /*VMSTATE*/int);
        vmR3ValidateStateTransition(enmStateOld, enmStateNew);
    }
    va_end(va);
#endif

    /*
     * Grab the lock and see if any of the proposed transisions works out.
     */
    va_start(va, cTransitions);
    int     rc          = VERR_VM_INVALID_VM_STATE;
    PUVM    pUVM        = pVM->pUVM;
    RTCritSectEnter(&pUVM->vm.s.AtStateCritSect);

    VMSTATE enmStateCur = pVM->enmVMState;

    for (unsigned i = 0; i < cTransitions; i++)
    {
        enmStateNew = (VMSTATE)va_arg(va, /*VMSTATE*/int);
        enmStateOld = (VMSTATE)va_arg(va, /*VMSTATE*/int);
        if (enmStateCur == enmStateOld)
        {
            vmR3SetStateLocked(pVM, pUVM, enmStateNew, enmStateOld);
            rc = i + 1;
            break;
        }
    }

    if (RT_FAILURE(rc))
    {
        /*
         * Complain about it.
         */
        if (cTransitions == 1)
            LogRel(("%s: %s -> %s failed, because the VM state is actually %s\n",
                    pszWho, VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew), VMR3GetStateName(enmStateCur)));
        else
        {
            va_end(va);
            va_start(va, cTransitions);
            LogRel(("%s:\n", pszWho));
            for (unsigned i = 0; i < cTransitions; i++)
            {
                enmStateNew = (VMSTATE)va_arg(va, /*VMSTATE*/int);
                enmStateOld = (VMSTATE)va_arg(va, /*VMSTATE*/int);
                LogRel(("%s%s -> %s",
                        i ? ", " : " ", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)));
            }
            LogRel((" failed, because the VM state is actually %s\n", VMR3GetStateName(enmStateCur)));
        }

        VMSetError(pVM, VERR_VM_INVALID_VM_STATE, RT_SRC_POS,
                   N_("%s failed because the VM state is %s instead of %s"),
                   VMR3GetStateName(enmStateCur), VMR3GetStateName(enmStateOld));
        AssertMsgFailed(("%s: %s -> %s failed, state is actually %s\n",
                         pszWho, VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew), VMR3GetStateName(enmStateCur)));
    }

    RTCritSectLeave(&pUVM->vm.s.AtStateCritSect);
    va_end(va);
    Assert(rc > 0 || rc < 0);
    return rc;
}


/**
 * Flag a guru meditation ... a hack.
 *
 * @param   pVM             The VM handle
 *
 * @todo    Rewrite this part. The guru meditation should be flagged
 *          immediately by the VMM and not by VMEmt.cpp when it's all over.
 */
void vmR3SetGuruMeditation(PVM pVM)
{
    PUVM pUVM = pVM->pUVM;
    RTCritSectEnter(&pUVM->vm.s.AtStateCritSect);

    VMSTATE enmStateCur = pVM->enmVMState;
    if (enmStateCur == VMSTATE_RUNNING)
        vmR3SetStateLocked(pVM, pUVM, VMSTATE_GURU_MEDITATION, VMSTATE_RUNNING);
    else if (enmStateCur == VMSTATE_RUNNING_LS)
    {
        vmR3SetStateLocked(pVM, pUVM, VMSTATE_GURU_MEDITATION_LS, VMSTATE_RUNNING_LS);
        SSMR3Cancel(pVM);
    }

    RTCritSectLeave(&pUVM->vm.s.AtStateCritSect);
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
VMMR3DECL(int) VMR3AtStateRegister(PVM pVM, PFNVMATSTATE pfnAtState, void *pvUser)
{
    LogFlow(("VMR3AtStateRegister: pfnAtState=%p pvUser=%p\n", pfnAtState, pvUser));

    /*
     * Validate input.
     */
    AssertPtrReturn(pfnAtState, VERR_INVALID_PARAMETER);
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);

    /*
     * Allocate a new record.
     */
    PUVM pUVM = pVM->pUVM;
    PVMATSTATE pNew = (PVMATSTATE)MMR3HeapAllocU(pUVM, MM_TAG_VM, sizeof(*pNew));
    if (!pNew)
        return VERR_NO_MEMORY;

    /* fill */
    pNew->pfnAtState = pfnAtState;
    pNew->pvUser     = pvUser;

    /* insert */
    RTCritSectEnter(&pUVM->vm.s.AtStateCritSect);
    pNew->pNext      = *pUVM->vm.s.ppAtStateNext;
    *pUVM->vm.s.ppAtStateNext = pNew;
    pUVM->vm.s.ppAtStateNext = &pNew->pNext;
    RTCritSectLeave(&pUVM->vm.s.AtStateCritSect);

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
VMMR3DECL(int) VMR3AtStateDeregister(PVM pVM, PFNVMATSTATE pfnAtState, void *pvUser)
{
    LogFlow(("VMR3AtStateDeregister: pfnAtState=%p pvUser=%p\n", pfnAtState, pvUser));

    /*
     * Validate input.
     */
    AssertPtrReturn(pfnAtState, VERR_INVALID_PARAMETER);
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);

    PUVM pUVM = pVM->pUVM;
    RTCritSectEnter(&pUVM->vm.s.AtStateCritSect);

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
        RTCritSectLeave(&pUVM->vm.s.AtStateCritSect);
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

    RTCritSectLeave(&pUVM->vm.s.AtStateCritSect);

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

    /*
     * Validate input.
     */
    AssertPtrReturn(pfnAtError, VERR_INVALID_PARAMETER);
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);

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
    RTCritSectEnter(&pUVM->vm.s.AtErrorCritSect);
    pNew->pNext      = *pUVM->vm.s.ppAtErrorNext;
    *pUVM->vm.s.ppAtErrorNext = pNew;
    pUVM->vm.s.ppAtErrorNext = &pNew->pNext;
    RTCritSectLeave(&pUVM->vm.s.AtErrorCritSect);

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
VMMR3DECL(int) VMR3AtErrorDeregister(PVM pVM, PFNVMATERROR pfnAtError, void *pvUser)
{
    LogFlow(("VMR3AtErrorDeregister: pfnAtError=%p pvUser=%p\n", pfnAtError, pvUser));

    /*
     * Validate input.
     */
    AssertPtrReturn(pfnAtError, VERR_INVALID_PARAMETER);
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);

    PUVM pUVM = pVM->pUVM;
    RTCritSectEnter(&pUVM->vm.s.AtErrorCritSect);

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
        RTCritSectLeave(&pUVM->vm.s.AtErrorCritSect);
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

    RTCritSectLeave(&pUVM->vm.s.AtErrorCritSect);

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
    PUVM pUVM = pVM->pUVM;
    RTCritSectEnter(&pUVM->vm.s.AtErrorCritSect);
    for (PVMATERROR pCur = pUVM->vm.s.pAtError; pCur; pCur = pCur->pNext)
        vmR3SetErrorWorkerDoCall(pVM, pCur, rc, RT_SRC_POS_ARGS, "%s", pszMessage);
    RTCritSectLeave(&pUVM->vm.s.AtErrorCritSect);
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
    RTCritSectEnter(&pUVM->vm.s.AtErrorCritSect);
    for (PVMATERROR pCur = pUVM->vm.s.pAtError; pCur; pCur = pCur->pNext)
    {
        va_list va2;
        va_copy(va2, *pArgs);
        pCur->pfnAtError(pUVM->pVM, pCur->pvUser, rc, RT_SRC_POS_ARGS, pszFormat, va2);
        va_end(va2);
    }
    RTCritSectLeave(&pUVM->vm.s.AtErrorCritSect);
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
    AssertPtrReturn(pfnAtRuntimeError, VERR_INVALID_PARAMETER);
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);

    /*
     * Allocate a new record.
     */
    PUVM pUVM = pVM->pUVM;
    PVMATRUNTIMEERROR pNew = (PVMATRUNTIMEERROR)MMR3HeapAllocU(pUVM, MM_TAG_VM, sizeof(*pNew));
    if (!pNew)
        return VERR_NO_MEMORY;

    /* fill */
    pNew->pfnAtRuntimeError = pfnAtRuntimeError;
    pNew->pvUser            = pvUser;

    /* insert */
    RTCritSectEnter(&pUVM->vm.s.AtErrorCritSect);
    pNew->pNext             = *pUVM->vm.s.ppAtRuntimeErrorNext;
    *pUVM->vm.s.ppAtRuntimeErrorNext = pNew;
    pUVM->vm.s.ppAtRuntimeErrorNext = &pNew->pNext;
    RTCritSectLeave(&pUVM->vm.s.AtErrorCritSect);

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
    AssertPtrReturn(pfnAtRuntimeError, VERR_INVALID_PARAMETER);
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);

    PUVM pUVM = pVM->pUVM;
    RTCritSectEnter(&pUVM->vm.s.AtErrorCritSect);

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
        RTCritSectLeave(&pUVM->vm.s.AtErrorCritSect);
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

    RTCritSectLeave(&pUVM->vm.s.AtErrorCritSect);

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
        rc = vmR3SuspendCommon(pVM, true /*fFatal*/);
    else if (fFlags & VMSETRTERR_FLAGS_SUSPEND)
        rc = vmR3SuspendCommon(pVM, false /*fFatal*/);

    /*
     * Do the callback round.
     */
    PUVM pUVM = pVM->pUVM;
    RTCritSectEnter(&pUVM->vm.s.AtErrorCritSect);
    for (PVMATRUNTIMEERROR pCur = pUVM->vm.s.pAtRuntimeError; pCur; pCur = pCur->pNext)
    {
        va_list va;
        va_copy(va, *pVa);
        pCur->pfnAtRuntimeError(pVM, pCur->pvUser, fFlags, pszErrorId, pszFormat, va);
        va_end(va);
    }
    RTCritSectLeave(&pUVM->vm.s.AtErrorCritSect);

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
 * @param   pszMessage      The error message residing the MM heap.
 *
 * @thread  EMT
 */
DECLCALLBACK(int) vmR3SetRuntimeError(PVM pVM, uint32_t fFlags, const char *pszErrorId, char *pszMessage)
{
#if 0 /** @todo make copy of the error msg. */
    /*
     * Make a copy of the message.
     */
    va_list va2;
    va_copy(va2, *pVa);
    vmSetRuntimeErrorCopy(pVM, fFlags, pszErrorId, pszFormat, va2);
    va_end(va2);
#endif

    /*
     * Join paths with VMR3SetRuntimeErrorWorker.
     */
    int rc = vmR3SetRuntimeErrorCommonF(pVM, fFlags, pszErrorId, "%s", pszMessage);
    MMR3HeapFree(pszMessage);
    return rc;
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

