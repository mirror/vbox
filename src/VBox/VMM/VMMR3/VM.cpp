/* $Id$ */
/** @file
 * VM - Virtual Machine
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
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
 * time when the VMM bit was a kind of vague.  'VM' also happened to be the name
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
#include <VBox/vmm/cfgm.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/gvmm.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/selm.h>
#include <VBox/vmm/trpm.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/vmm/pdmcritsect.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/iem.h>
#include <VBox/vmm/rem.h>
#include <VBox/vmm/tm.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/patm.h>
#include <VBox/vmm/csam.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/ssm.h>
#include <VBox/vmm/ftm.h>
#include <VBox/vmm/hwaccm.h>
#include "VMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/uvm.h>

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
#include <iprt/uuid.h>


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
static int                  vmR3CreateUVM(uint32_t cCpus, PCVMM2USERMETHODS pVmm2UserMethods, PUVM *ppUVM);
static int                  vmR3CreateU(PUVM pUVM, uint32_t cCpus, PFNCFGMCONSTRUCTOR pfnCFGMConstructor, void *pvUserCFGM);
static int                  vmR3InitRing3(PVM pVM, PUVM pUVM);
static int                  vmR3InitRing0(PVM pVM);
static int                  vmR3InitGC(PVM pVM);
static int                  vmR3InitDoCompleted(PVM pVM, VMINITCOMPLETED enmWhat);
static DECLCALLBACK(size_t) vmR3LogPrefixCallback(PRTLOGGER pLogger, char *pchBuf, size_t cchBuf, void *pvUser);
static void                 vmR3DestroyUVM(PUVM pUVM, uint32_t cMilliesEMTWait);
static void                 vmR3AtDtor(PVM pVM);
static bool                 vmR3ValidateStateTransition(VMSTATE enmStateOld, VMSTATE enmStateNew);
static void                 vmR3DoAtState(PVM pVM, PUVM pUVM, VMSTATE enmStateNew, VMSTATE enmStateOld);
static int                  vmR3TrySetState(PVM pVM, const char *pszWho, unsigned cTransitions, ...);
static void                 vmR3SetStateLocked(PVM pVM, PUVM pUVM, VMSTATE enmStateNew, VMSTATE enmStateOld);
static void                 vmR3SetState(PVM pVM, VMSTATE enmStateNew, VMSTATE enmStateOld);
static int                  vmR3SetErrorU(PUVM pUVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...);


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
 * @param   pVmm2UserMethods    An optional method table that the VMM can use
 *                              to make the user perform various action, like
 *                              for instance state saving.
 * @param   pfnVMAtError        Pointer to callback function for setting VM
 *                              errors.  This was added as an implicit call to
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
VMMR3DECL(int)   VMR3Create(uint32_t cCpus, PCVMM2USERMETHODS pVmm2UserMethods,
                            PFNVMATERROR pfnVMAtError, void *pvUserVM,
                            PFNCFGMCONSTRUCTOR pfnCFGMConstructor, void *pvUserCFGM,
                            PVM *ppVM)
{
    LogFlow(("VMR3Create: cCpus=%RU32 pVmm2UserMethods=%p pfnVMAtError=%p pvUserVM=%p  pfnCFGMConstructor=%p pvUserCFGM=%p ppVM=%p\n",
             cCpus, pVmm2UserMethods, pfnVMAtError, pvUserVM, pfnCFGMConstructor, pvUserCFGM, ppVM));

    if (pVmm2UserMethods)
    {
        AssertPtrReturn(pVmm2UserMethods, VERR_INVALID_POINTER);
        AssertReturn(pVmm2UserMethods->u32Magic    == VMM2USERMETHODS_MAGIC,   VERR_INVALID_PARAMETER);
        AssertReturn(pVmm2UserMethods->u32Version  == VMM2USERMETHODS_VERSION, VERR_INVALID_PARAMETER);
        AssertPtrNullReturn(pVmm2UserMethods->pfnSaveState, VERR_INVALID_POINTER);
        AssertPtrNullReturn(pVmm2UserMethods->pfnNotifyEmtInit, VERR_INVALID_POINTER);
        AssertPtrNullReturn(pVmm2UserMethods->pfnNotifyEmtTerm, VERR_INVALID_POINTER);
        AssertPtrNullReturn(pVmm2UserMethods->pfnNotifyPdmtInit, VERR_INVALID_POINTER);
        AssertPtrNullReturn(pVmm2UserMethods->pfnNotifyPdmtTerm, VERR_INVALID_POINTER);
        AssertReturn(pVmm2UserMethods->u32EndMagic == VMM2USERMETHODS_MAGIC,   VERR_INVALID_PARAMETER);
    }
    AssertPtrNullReturn(pfnVMAtError, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pfnCFGMConstructor, VERR_INVALID_POINTER);
    AssertPtrReturn(ppVM, VERR_INVALID_POINTER);

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
     * and consolidate a bit of cleanup code.
     */
    PUVM pUVM = NULL;                   /* shuts up gcc */
    int rc = vmR3CreateUVM(cCpus, pVmm2UserMethods, &pUVM);
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
             * An error occurred during VM creation.  Set the error message directly
             * using the initial callback, as the callback list might not exist yet.
             */
            const char *pszError;
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

#ifndef RT_OS_DARWIN
                case VERR_HWACCM_CONFIG_MISMATCH:
                    pszError = N_("VT-x/AMD-V is either not available on your host or disabled. "
                                  "This hardware extension is required by the VM configuration");
                    break;
#endif

                case VERR_SVM_IN_USE:
#ifdef RT_OS_LINUX
                    pszError = N_("VirtualBox can't enable the AMD-V extension. "
                                  "Please disable the KVM kernel extension, recompile your kernel and reboot");
#else
                    pszError = N_("VirtualBox can't enable the AMD-V extension. Please close all other virtualization programs.");
#endif
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

                case VERR_PDM_DEVICE_NOT_FOUND:
                    pszError = N_("A virtual device is configured in the VM settings but the device "
                                  "implementation is missing.\n"
                                  "A possible reason for this error is a missing extension pack. Note "
                                  "that as of VirtualBox 4.0, certain features (for example USB 2.0 "
                                  "support and remote desktop) are only available from an 'extension "
                                  "pack' which must be downloaded and installed separately");
                    break;

                case VERR_PCI_PASSTHROUGH_NO_HWACCM:
                    pszError = N_("PCI passthrough requires VT-x/AMD-V");
                    break;

                case VERR_PCI_PASSTHROUGH_NO_NESTED_PAGING:
                    pszError = N_("PCI passthrough requires nested paging");
                    break;

                default:
                    if (VMR3GetErrorCountU(pUVM) == 0)
                        pszError = RTErrGetFull(rc);
                    else
                        pszError = NULL; /* already set. */
                    break;
            }
            if (pszError)
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
 * @param   cCpus               Number of virtual CPUs
 * @param   pVmm2UserMethods    Pointer to the optional VMM -> User method
 *                              table.
 * @param   ppUVM               Where to store the UVM pointer.
 */
static int vmR3CreateUVM(uint32_t cCpus, PCVMM2USERMETHODS pVmm2UserMethods, PUVM *ppUVM)
{
    uint32_t i;

    /*
     * Create and initialize the UVM.
     */
    PUVM pUVM = (PUVM)RTMemPageAllocZ(RT_OFFSETOF(UVM, aCpus[cCpus]));
    AssertReturn(pUVM, VERR_NO_MEMORY);
    pUVM->u32Magic          = UVM_MAGIC;
    pUVM->cCpus             = cCpus;
    pUVM->pVmm2UserMethods  = pVmm2UserMethods;

    AssertCompile(sizeof(pUVM->vm.s) <= sizeof(pUVM->vm.padding));

    pUVM->vm.s.cUvmRefs      = 1;
    pUVM->vm.s.ppAtStateNext = &pUVM->vm.s.pAtState;
    pUVM->vm.s.ppAtErrorNext = &pUVM->vm.s.pAtError;
    pUVM->vm.s.ppAtRuntimeErrorNext = &pUVM->vm.s.pAtRuntimeError;

    pUVM->vm.s.enmHaltMethod = VMHALTMETHOD_BOOTSTRAP;
    RTUuidClear(&pUVM->vm.s.Uuid);

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
    RTMemPageFree(pUVM, RT_OFFSETOF(UVM, aCpus[pUVM->cCpus]));
    return rc;
}


/**
 * Creates and initializes the VM.
 *
 * @thread EMT
 */
static int vmR3CreateU(PUVM pUVM, uint32_t cCpus, PFNCFGMCONSTRUCTOR pfnCFGMConstructor, void *pvUserCFGM)
{
    /*
     * Load the VMMR0.r0 module so that we can call GVMMR0CreateVM.
     */
    int rc = PDMR3LdrLoadVMMR0U(pUVM);
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
        AssertRelease(pVM->uCpuExecutionCap == 100);
        AssertRelease(pVM->offVMCPU == RT_UOFFSETOF(VM, aCpus));
        AssertCompileMemberAlignment(VM, cpum, 64);
        AssertCompileMemberAlignment(VM, tm, 64);
        AssertCompileMemberAlignment(VM, aCpus, PAGE_SIZE);

        Log(("VMR3Create: Created pUVM=%p pVM=%p pVMR0=%p hSelf=%#x cCpus=%RU32\n",
             pUVM, pVM, pVM->pVMR0, pVM->hSelf, pVM->cCpus));

        /*
         * Initialize the VM structure and our internal data (VMINT).
         */
        pVM->pUVM = pUVM;

        for (VMCPUID i = 0; i < pVM->cCpus; i++)
        {
            pVM->aCpus[i].pUVCpu            = &pUVM->aCpus[i];
            pVM->aCpus[i].idCpu             = i;
            pVM->aCpus[i].hNativeThread     = pUVM->aCpus[i].vm.s.NativeThreadEMT;
            Assert(pVM->aCpus[i].hNativeThread != NIL_RTNATIVETHREAD);
            /* hNativeThreadR0 is initialized on EMT registration. */
            pUVM->aCpus[i].pVCpu            = &pVM->aCpus[i];
            pUVM->aCpus[i].pVM              = pVM;
        }


        /*
         * Init the configuration.
         */
        rc = CFGMR3Init(pVM, pfnCFGMConstructor, pvUserCFGM);
        if (RT_SUCCESS(rc))
        {
            PCFGMNODE pRoot = CFGMR3GetRoot(pVM);
            rc = CFGMR3QueryBoolDef(pRoot, "HwVirtExtForced", &pVM->fHwVirtExtForced, false);
            if (RT_SUCCESS(rc) && pVM->fHwVirtExtForced)
                pVM->fHWACCMEnabled = true;

            /*
             * If executing in fake suplib mode disable RR3 and RR0 in the config.
             */
            const char *psz = RTEnvGet("VBOX_SUPLIB_FAKE");
            if (psz && !strcmp(psz, "fake"))
            {
                CFGMR3RemoveValue(pRoot, "RawR3Enabled");
                CFGMR3InsertInteger(pRoot, "RawR3Enabled", 0);
                CFGMR3RemoveValue(pRoot, "RawR0Enabled");
                CFGMR3InsertInteger(pRoot, "RawR0Enabled", 0);
            }

            /*
             * Make sure the CPU count in the config data matches.
             */
            if (RT_SUCCESS(rc))
            {
                uint32_t cCPUsCfg;
                rc = CFGMR3QueryU32Def(pRoot, "NumCPUs", &cCPUsCfg, 1);
                AssertLogRelMsgRC(rc, ("Configuration error: Querying \"NumCPUs\" as integer failed, rc=%Rrc\n", rc));
                if (RT_SUCCESS(rc) && cCPUsCfg != cCpus)
                {
                    AssertLogRelMsgFailed(("Configuration error: \"NumCPUs\"=%RU32 and VMR3CreateVM::cCpus=%RU32 does not match!\n",
                                           cCPUsCfg, cCpus));
                    rc = VERR_INVALID_PARAMETER;
                }
            }

            /*
             * Get the CPU execution cap.
             */
            if (RT_SUCCESS(rc))
            {
                rc = CFGMR3QueryU32Def(pRoot, "CpuExecutionCap", &pVM->uCpuExecutionCap, 100);
                AssertLogRelMsgRC(rc, ("Configuration error: Querying \"CpuExecutionCap\" as integer failed, rc=%Rrc\n", rc));
            }

            /*
             * Get the VM name and UUID.
             */
            if (RT_SUCCESS(rc))
            {
                rc = CFGMR3QueryStringAllocDef(pRoot, "Name", &pUVM->vm.s.pszName, "<unknown>");
                AssertLogRelMsg(RT_SUCCESS(rc), ("Configuration error: Querying \"Name\" failed, rc=%Rrc\n", rc));
            }

            if (RT_SUCCESS(rc))
            {
                rc = CFGMR3QueryBytes(pRoot, "UUID", &pUVM->vm.s.Uuid, sizeof(pUVM->vm.s.Uuid));
                if (rc == VERR_CFGM_VALUE_NOT_FOUND)
                    rc = VINF_SUCCESS;
                AssertLogRelMsg(RT_SUCCESS(rc), ("Configuration error: Querying \"UUID\" failed, rc=%Rrc\n", rc));
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
         * Do automatic cleanups while the VM structure is still alive and all
         * references to it are still working.
         */
        PDMR3CritSectTerm(pVM);

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
        rc = VMR3ReqCallWait(pVM, idCpu, (PFNRT)vmR3RegisterEMT, 2, pVM, idCpu);
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
            rc = STAMR3RegisterF(pVM, &pUVM->aCpus[idCpu].vm.s.StatHaltYield,           STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_NS_PER_CALL, "Profiling halted state yielding.",  "/PROF/VM/CPU%d/Halt/Yield", idCpu);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pUVM->aCpus[idCpu].vm.s.StatHaltBlock,           STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_NS_PER_CALL, "Profiling halted state blocking.",  "/PROF/VM/CPU%d/Halt/Block", idCpu);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pUVM->aCpus[idCpu].vm.s.StatHaltBlockOverslept,  STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_NS_PER_CALL, "Time wasted by blocking too long.", "/PROF/VM/CPU%d/Halt/BlockOverslept", idCpu);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pUVM->aCpus[idCpu].vm.s.StatHaltBlockInsomnia,   STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_NS_PER_CALL, "Time slept when returning to early.","/PROF/VM/CPU%d/Halt/BlockInsomnia", idCpu);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pUVM->aCpus[idCpu].vm.s.StatHaltBlockOnTime,     STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_NS_PER_CALL, "Time slept on time.",                "/PROF/VM/CPU%d/Halt/BlockOnTime", idCpu);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pUVM->aCpus[idCpu].vm.s.StatHaltTimers,          STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_NS_PER_CALL, "Profiling halted state timer tasks.", "/PROF/VM/CPU%d/Halt/Timers", idCpu);
            AssertRC(rc);
        }

        STAM_REG(pVM, &pUVM->vm.s.StatReqAllocNew,   STAMTYPE_COUNTER,     "/VM/Req/AllocNew",       STAMUNIT_OCCURENCES,        "Number of VMR3ReqAlloc returning a new packet.");
        STAM_REG(pVM, &pUVM->vm.s.StatReqAllocRaces, STAMTYPE_COUNTER,     "/VM/Req/AllocRaces",     STAMUNIT_OCCURENCES,        "Number of VMR3ReqAlloc causing races.");
        STAM_REG(pVM, &pUVM->vm.s.StatReqAllocRecycled, STAMTYPE_COUNTER,  "/VM/Req/AllocRecycled",  STAMUNIT_OCCURENCES,        "Number of VMR3ReqAlloc returning a recycled packet.");
        STAM_REG(pVM, &pUVM->vm.s.StatReqFree,       STAMTYPE_COUNTER,     "/VM/Req/Free",           STAMUNIT_OCCURENCES,        "Number of VMR3ReqFree calls.");
        STAM_REG(pVM, &pUVM->vm.s.StatReqFreeOverflow, STAMTYPE_COUNTER,   "/VM/Req/FreeOverflow",   STAMUNIT_OCCURENCES,        "Number of times the request was actually freed.");
        STAM_REG(pVM, &pUVM->vm.s.StatReqProcessed,  STAMTYPE_COUNTER,     "/VM/Req/Processed",      STAMUNIT_OCCURENCES,        "Number of processed requests (any queue).");
        STAM_REG(pVM, &pUVM->vm.s.StatReqMoreThan1,  STAMTYPE_COUNTER,     "/VM/Req/MoreThan1",      STAMUNIT_OCCURENCES,        "Number of times there are more than one request on the queue when processing it.");
        STAM_REG(pVM, &pUVM->vm.s.StatReqPushBackRaces, STAMTYPE_COUNTER,  "/VM/Req/PushBackRaces",  STAMUNIT_OCCURENCES,        "Number of push back races.");

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
                            rc = FTMR3Init(pVM);
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
                                                    rc = IOMR3Init(pVM);
                                                    if (RT_SUCCESS(rc))
                                                    {
                                                        rc = EMR3Init(pVM);
                                                        if (RT_SUCCESS(rc))
                                                        {
                                                            rc = IEMR3Init(pVM);
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
                                                                int rc2 = IEMR3Term(pVM);
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
                                int rc2 = FTMR3Term(pVM);
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
    if (RT_SUCCESS(rc))
        rc = vmR3InitDoCompleted(pVM, VMINITCOMPLETED_HWACCM);

    /** @todo Move this to the VMINITCOMPLETED_HWACCM notification handler. */
    if (RT_SUCCESS(rc))
        CPUMR3SetHWVirtEx(pVM, HWACCMIsEnabled(pVM));

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
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   enmWhat     What's completed.
 */
static int vmR3InitDoCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
    int rc = VMMR3InitCompleted(pVM, enmWhat);
    if (RT_SUCCESS(rc))
        rc = HWACCMR3InitCompleted(pVM, enmWhat);
    if (RT_SUCCESS(rc))
        rc = PGMR3InitCompleted(pVM, enmWhat);
    return rc;
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
    IEMR3Relocate(pVM);
    DBGFR3Relocate(pVM, offDelta);
    PDMR3Relocate(pVM, offDelta);
}


/**
 * EMT rendezvous worker for VMR3PowerOn.
 *
 * @returns VERR_VM_INVALID_VM_STATE or VINF_SUCCESS. (This is a strict return
 *          code, see FNVMMEMTRENDEZVOUS.)
 *
 * @param   pVM             The VM handle.
 * @param   pVCpu           The VMCPU handle of the EMT.
 * @param   pvUser          Ignored.
 */
static DECLCALLBACK(VBOXSTRICTRC) vmR3PowerOn(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    LogFlow(("vmR3PowerOn: pVM=%p pVCpu=%p/#%u\n", pVM, pVCpu, pVCpu->idCpu));
    Assert(!pvUser); NOREF(pvUser);

    /*
     * The first thread thru here tries to change the state.  We shouldn't be
     * called again if this fails.
     */
    if (pVCpu->idCpu == pVM->cCpus - 1)
    {
        int rc = vmR3TrySetState(pVM, "VMR3PowerOn", 1, VMSTATE_POWERING_ON, VMSTATE_CREATED);
        if (RT_FAILURE(rc))
            return rc;
    }

    VMSTATE enmVMState = VMR3GetState(pVM);
    AssertMsgReturn(enmVMState == VMSTATE_POWERING_ON,
                    ("%s\n", VMR3GetStateName(enmVMState)),
                    VERR_INTERNAL_ERROR_4);

    /*
     * All EMTs changes their state to started.
     */
    VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED);

    /*
     * EMT(0) is last thru here and it will make the notification calls
     * and advance the state.
     */
    if (pVCpu->idCpu == 0)
    {
        PDMR3PowerOn(pVM);
        vmR3SetState(pVM, VMSTATE_RUNNING, VMSTATE_POWERING_ON);
    }

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
 * @vmstateto   PoweringOn+Running
 */
VMMR3DECL(int) VMR3PowerOn(PVM pVM)
{
    LogFlow(("VMR3PowerOn: pVM=%p\n", pVM));
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);

    /*
     * Gather all the EMTs to reduce the init TSC drift and keep
     * the state changing APIs a bit uniform.
     */
    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_DESCENDING | VMMEMTRENDEZVOUS_FLAGS_STOP_ON_ERROR,
                                vmR3PowerOn, NULL);
    LogFlow(("VMR3PowerOn: returns %Rrc\n", rc));
    return rc;
}


/**
 * Does the suspend notifications.
 *
 * @param  pVM      The VM handle.
 * @thread  EMT(0)
 */
static void vmR3SuspendDoWork(PVM pVM)
{
    PDMR3Suspend(pVM);
}


/**
 * EMT rendezvous worker for VMR3Suspend.
 *
 * @returns VERR_VM_INVALID_VM_STATE or VINF_EM_SUSPEND. (This is a strict
 *          return code, see FNVMMEMTRENDEZVOUS.)
 *
 * @param   pVM             The VM handle.
 * @param   pVCpu           The VMCPU handle of the EMT.
 * @param   pvUser          Ignored.
 */
static DECLCALLBACK(VBOXSTRICTRC) vmR3Suspend(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    LogFlow(("vmR3Suspend: pVM=%p pVCpu=%p/#%u\n", pVM, pVCpu, pVCpu->idCpu));
    Assert(!pvUser); NOREF(pvUser);

    /*
     * The first EMT switches the state to suspending.  If this fails because
     * something was racing us in one way or the other, there will be no more
     * calls and thus the state assertion below is not going to annoy anyone.
     */
    if (pVCpu->idCpu == pVM->cCpus - 1)
    {
        int rc = vmR3TrySetState(pVM, "VMR3Suspend", 2,
                                 VMSTATE_SUSPENDING,        VMSTATE_RUNNING,
                                 VMSTATE_SUSPENDING_EXT_LS, VMSTATE_RUNNING_LS);
        if (RT_FAILURE(rc))
            return rc;
    }

    VMSTATE enmVMState = VMR3GetState(pVM);
    AssertMsgReturn(    enmVMState == VMSTATE_SUSPENDING
                    ||  enmVMState == VMSTATE_SUSPENDING_EXT_LS,
                    ("%s\n", VMR3GetStateName(enmVMState)),
                    VERR_INTERNAL_ERROR_4);

    /*
     * EMT(0) does the actually suspending *after* all the other CPUs have
     * been thru here.
     */
    if (pVCpu->idCpu == 0)
    {
        vmR3SuspendDoWork(pVM);

        int rc = vmR3TrySetState(pVM, "VMR3Suspend", 2,
                                 VMSTATE_SUSPENDED,        VMSTATE_SUSPENDING,
                                 VMSTATE_SUSPENDED_EXT_LS, VMSTATE_SUSPENDING_EXT_LS);
        if (RT_FAILURE(rc))
            return VERR_INTERNAL_ERROR_3;
    }

    return VINF_EM_SUSPEND;
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
 * @vmstateto   Suspending + Suspended or SuspendingExtLS + SuspendedExtLS
 */
VMMR3DECL(int) VMR3Suspend(PVM pVM)
{
    LogFlow(("VMR3Suspend: pVM=%p\n", pVM));
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);

    /*
     * Gather all the EMTs to make sure there are no races before
     * changing the VM state.
     */
    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_DESCENDING | VMMEMTRENDEZVOUS_FLAGS_STOP_ON_ERROR,
                                vmR3Suspend, NULL);
    LogFlow(("VMR3Suspend: returns %Rrc\n", rc));
    return rc;
}


/**
 * EMT rendezvous worker for VMR3Resume.
 *
 * @returns VERR_VM_INVALID_VM_STATE or VINF_EM_RESUME. (This is a strict
 *          return code, see FNVMMEMTRENDEZVOUS.)
 *
 * @param   pVM             The VM handle.
 * @param   pVCpu           The VMCPU handle of the EMT.
 * @param   pvUser          Ignored.
 */
static DECLCALLBACK(VBOXSTRICTRC) vmR3Resume(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    LogFlow(("vmR3Resume: pVM=%p pVCpu=%p/#%u\n", pVM, pVCpu, pVCpu->idCpu));
    Assert(!pvUser); NOREF(pvUser);

    /*
     * The first thread thru here tries to change the state.  We shouldn't be
     * called again if this fails.
     */
    if (pVCpu->idCpu == pVM->cCpus - 1)
    {
        int rc = vmR3TrySetState(pVM, "VMR3Resume", 1, VMSTATE_RESUMING, VMSTATE_SUSPENDED);
        if (RT_FAILURE(rc))
            return rc;
    }

    VMSTATE enmVMState = VMR3GetState(pVM);
    AssertMsgReturn(enmVMState == VMSTATE_RESUMING,
                    ("%s\n", VMR3GetStateName(enmVMState)),
                    VERR_INTERNAL_ERROR_4);

#if 0
    /*
     * All EMTs changes their state to started.
     */
    VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED);
#endif

    /*
     * EMT(0) is last thru here and it will make the notification calls
     * and advance the state.
     */
    if (pVCpu->idCpu == 0)
    {
        PDMR3Resume(pVM);
        vmR3SetState(pVM, VMSTATE_RUNNING, VMSTATE_RESUMING);
        pVM->vm.s.fTeleportedAndNotFullyResumedYet = false;
    }

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
     * Gather all the EMTs to make sure there are no races before
     * changing the VM state.
     */
    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_DESCENDING | VMMEMTRENDEZVOUS_FLAGS_STOP_ON_ERROR,
                                vmR3Resume, NULL);
    LogFlow(("VMR3Resume: returns %Rrc\n", rc));
    return rc;
}


/**
 * EMT rendezvous worker for VMR3Save and VMR3Teleport that suspends the VM
 * after the live step has been completed.
 *
 * @returns VERR_VM_INVALID_VM_STATE or VINF_EM_RESUME. (This is a strict
 *          return code, see FNVMMEMTRENDEZVOUS.)
 *
 * @param   pVM             The VM handle.
 * @param   pVCpu           The VMCPU handle of the EMT.
 * @param   pvUser          The pfSuspended argument of vmR3SaveTeleport.
 */
static DECLCALLBACK(VBOXSTRICTRC) vmR3LiveDoSuspend(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    LogFlow(("vmR3LiveDoSuspend: pVM=%p pVCpu=%p/#%u\n", pVM, pVCpu, pVCpu->idCpu));
    bool *pfSuspended = (bool *)pvUser;

    /*
     * The first thread thru here tries to change the state.  We shouldn't be
     * called again if this fails.
     */
    if (pVCpu->idCpu == pVM->cCpus - 1U)
    {
        PUVM     pUVM = pVM->pUVM;
        int      rc;

        RTCritSectEnter(&pUVM->vm.s.AtStateCritSect);
        VMSTATE enmVMState = pVM->enmVMState;
        switch (enmVMState)
        {
            case VMSTATE_RUNNING_LS:
                vmR3SetStateLocked(pVM, pUVM, VMSTATE_SUSPENDING_LS, VMSTATE_RUNNING_LS);
                rc = VINF_SUCCESS;
                break;

            case VMSTATE_SUSPENDED_EXT_LS:
            case VMSTATE_SUSPENDED_LS:          /* (via reset) */
                rc = VINF_SUCCESS;
                break;

            case VMSTATE_DEBUGGING_LS:
                rc = VERR_TRY_AGAIN;
                break;

            case VMSTATE_OFF_LS:
                vmR3SetStateLocked(pVM, pUVM, VMSTATE_OFF, VMSTATE_OFF_LS);
                rc = VERR_SSM_LIVE_POWERED_OFF;
                break;

            case VMSTATE_FATAL_ERROR_LS:
                vmR3SetStateLocked(pVM, pUVM, VMSTATE_FATAL_ERROR, VMSTATE_FATAL_ERROR_LS);
                rc = VERR_SSM_LIVE_FATAL_ERROR;
                break;

            case VMSTATE_GURU_MEDITATION_LS:
                vmR3SetStateLocked(pVM, pUVM, VMSTATE_GURU_MEDITATION, VMSTATE_GURU_MEDITATION_LS);
                rc = VERR_SSM_LIVE_GURU_MEDITATION;
                break;

            case VMSTATE_POWERING_OFF_LS:
            case VMSTATE_SUSPENDING_EXT_LS:
            case VMSTATE_RESETTING_LS:
            default:
                AssertMsgFailed(("%s\n", VMR3GetStateName(enmVMState)));
                rc = VERR_INTERNAL_ERROR_3;
                break;
        }
        RTCritSectLeave(&pUVM->vm.s.AtStateCritSect);
        if (RT_FAILURE(rc))
        {
            LogFlow(("vmR3LiveDoSuspend: returns %Rrc (state was %s)\n", rc, VMR3GetStateName(enmVMState)));
            return rc;
        }
    }

    VMSTATE enmVMState = VMR3GetState(pVM);
    AssertMsgReturn(enmVMState == VMSTATE_SUSPENDING_LS,
                    ("%s\n", VMR3GetStateName(enmVMState)),
                    VERR_INTERNAL_ERROR_4);

    /*
     * Only EMT(0) have work to do since it's last thru here.
     */
    if (pVCpu->idCpu == 0)
    {
        vmR3SuspendDoWork(pVM);
        int rc = vmR3TrySetState(pVM, "VMR3Suspend", 1,
                                 VMSTATE_SUSPENDED_LS, VMSTATE_SUSPENDING_LS);
        if (RT_FAILURE(rc))
            return VERR_INTERNAL_ERROR_3;

        *pfSuspended = true;
    }

    return VINF_EM_SUSPEND;
}


/**
 * EMT rendezvous worker that VMR3Save and VMR3Teleport uses to clean up a
 * SSMR3LiveDoStep1 failure.
 *
 * Doing this as a rendezvous operation avoids all annoying transition
 * states.
 *
 * @returns VERR_VM_INVALID_VM_STATE, VINF_SUCCESS or some specific VERR_SSM_*
 *          status code. (This is a strict return code, see FNVMMEMTRENDEZVOUS.)
 *
 * @param   pVM             The VM handle.
 * @param   pVCpu           The VMCPU handle of the EMT.
 * @param   pvUser          The pfSuspended argument of vmR3SaveTeleport.
 */
static DECLCALLBACK(VBOXSTRICTRC) vmR3LiveDoStep1Cleanup(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    LogFlow(("vmR3LiveDoStep1Cleanup: pVM=%p pVCpu=%p/#%u\n", pVM, pVCpu, pVCpu->idCpu));
    bool *pfSuspended = (bool *)pvUser;
    NOREF(pVCpu);

    int rc = vmR3TrySetState(pVM, "vmR3LiveDoStep1Cleanup", 8,
                             VMSTATE_OFF,               VMSTATE_OFF_LS,                     /* 1 */
                             VMSTATE_FATAL_ERROR,       VMSTATE_FATAL_ERROR_LS,             /* 2 */
                             VMSTATE_GURU_MEDITATION,   VMSTATE_GURU_MEDITATION_LS,         /* 3 */
                             VMSTATE_SUSPENDED,         VMSTATE_SUSPENDED_LS,               /* 4 */
                             VMSTATE_SUSPENDED,         VMSTATE_SAVING,
                             VMSTATE_SUSPENDED,         VMSTATE_SUSPENDED_EXT_LS,
                             VMSTATE_RUNNING,           VMSTATE_RUNNING_LS,
                             VMSTATE_DEBUGGING,         VMSTATE_DEBUGGING_LS);
    if (rc == 1)
        rc = VERR_SSM_LIVE_POWERED_OFF;
    else if (rc == 2)
        rc = VERR_SSM_LIVE_FATAL_ERROR;
    else if (rc == 3)
        rc = VERR_SSM_LIVE_GURU_MEDITATION;
    else if (rc == 4)
    {
        *pfSuspended = true;
        rc = VINF_SUCCESS;
    }
    else if (rc > 0)
        rc = VINF_SUCCESS;
    return rc;
}


/**
 * EMT(0) worker for VMR3Save and VMR3Teleport that completes the live save.
 *
 * @returns VBox status code.
 * @retval  VINF_SSM_LIVE_SUSPENDED if VMR3Suspend was called.
 *
 * @param   pVM             The VM handle.
 * @param   pSSM            The handle of saved state operation.
 *
 * @thread  EMT(0)
 */
static DECLCALLBACK(int) vmR3LiveDoStep2(PVM pVM, PSSMHANDLE pSSM)
{
    LogFlow(("vmR3LiveDoStep2: pVM=%p pSSM=%p\n", pVM, pSSM));
    VM_ASSERT_EMT0(pVM);

    /*
     * Advance the state and mark if VMR3Suspend was called.
     */
    int rc = VINF_SUCCESS;
    VMSTATE enmVMState = VMR3GetState(pVM);
    if (enmVMState == VMSTATE_SUSPENDED_LS)
        vmR3SetState(pVM, VMSTATE_SAVING, VMSTATE_SUSPENDED_LS);
    else
    {
        if (enmVMState != VMSTATE_SAVING)
            vmR3SetState(pVM, VMSTATE_SAVING, VMSTATE_SUSPENDED_EXT_LS);
        rc = VINF_SSM_LIVE_SUSPENDED;
    }

    /*
     * Finish up and release the handle. Careful with the status codes.
     */
    int rc2 = SSMR3LiveDoStep2(pSSM);
    if (rc == VINF_SUCCESS || (RT_FAILURE(rc2) && RT_SUCCESS(rc)))
        rc = rc2;

    rc2 = SSMR3LiveDone(pSSM);
    if (rc == VINF_SUCCESS || (RT_FAILURE(rc2) && RT_SUCCESS(rc)))
        rc = rc2;

    /*
     * Advance to the final state and return.
     */
    vmR3SetState(pVM, VMSTATE_SUSPENDED, VMSTATE_SAVING);
    Assert(rc > VINF_EM_LAST || rc < VINF_EM_FIRST);
    return rc;
}


/**
 * Worker for vmR3SaveTeleport that validates the state and calls SSMR3Save or
 * SSMR3LiveSave.
 *
 * @returns VBox status code.
 *
 * @param   pVM                 The VM handle.
 * @param   cMsMaxDowntime      The maximum downtime given as milliseconds.
 * @param   pszFilename         The name of the file.  NULL if pStreamOps is used.
 * @param   pStreamOps          The stream methods.  NULL if pszFilename is used.
 * @param   pvStreamOpsUser     The user argument to the stream methods.
 * @param   enmAfter            What to do afterwards.
 * @param   pfnProgress         Progress callback. Optional.
 * @param   pvProgressUser      User argument for the progress callback.
 * @param   ppSSM               Where to return the saved state handle in case of a
 *                              live snapshot scenario.
 * @param   fSkipStateChanges   Set if we're supposed to skip state changes (FTM delta case)
 *
 * @thread  EMT
 */
static DECLCALLBACK(int) vmR3Save(PVM pVM, uint32_t cMsMaxDowntime, const char *pszFilename, PCSSMSTRMOPS pStreamOps, void *pvStreamOpsUser,
                                  SSMAFTER enmAfter, PFNVMPROGRESS pfnProgress, void *pvProgressUser, PSSMHANDLE *ppSSM,
                                  bool fSkipStateChanges)
{
    int rc = VINF_SUCCESS;

    LogFlow(("vmR3Save: pVM=%p cMsMaxDowntime=%u pszFilename=%p:{%s} pStreamOps=%p pvStreamOpsUser=%p enmAfter=%d pfnProgress=%p pvProgressUser=%p ppSSM=%p\n",
             pVM, cMsMaxDowntime, pszFilename, pszFilename, pStreamOps, pvStreamOpsUser, enmAfter, pfnProgress, pvProgressUser, ppSSM));

    /*
     * Validate input.
     */
    AssertPtrNull(pszFilename);
    AssertPtrNull(pStreamOps);
    AssertPtr(pVM);
    Assert(   enmAfter == SSMAFTER_DESTROY
           || enmAfter == SSMAFTER_CONTINUE
           || enmAfter == SSMAFTER_TELEPORT);
    AssertPtr(ppSSM);
    *ppSSM = NULL;

    /*
     * Change the state and perform/start the saving.
     */
    if (!fSkipStateChanges)
    {
        rc = vmR3TrySetState(pVM, "VMR3Save", 2,
                             VMSTATE_SAVING,     VMSTATE_SUSPENDED,
                             VMSTATE_RUNNING_LS, VMSTATE_RUNNING);
    }
    else
    {
        Assert(enmAfter != SSMAFTER_TELEPORT);
        rc = 1;
    }

    if (rc == 1 && enmAfter != SSMAFTER_TELEPORT)
    {
        rc = SSMR3Save(pVM, pszFilename, pStreamOps, pvStreamOpsUser, enmAfter, pfnProgress, pvProgressUser);
        if (!fSkipStateChanges)
            vmR3SetState(pVM, VMSTATE_SUSPENDED, VMSTATE_SAVING);
    }
    else if (rc == 2 || enmAfter == SSMAFTER_TELEPORT)
    {
        Assert(!fSkipStateChanges);
        if (enmAfter == SSMAFTER_TELEPORT)
            pVM->vm.s.fTeleportedAndNotFullyResumedYet = true;
        rc = SSMR3LiveSave(pVM, cMsMaxDowntime, pszFilename, pStreamOps, pvStreamOpsUser,
                           enmAfter, pfnProgress, pvProgressUser, ppSSM);
        /* (We're not subject to cancellation just yet.) */
    }
    else
        Assert(RT_FAILURE(rc));
    return rc;
}


/**
 * Common worker for VMR3Save and VMR3Teleport.
 *
 * @returns VBox status code.
 *
 * @param   pVM                 The VM handle.
 * @param   cMsMaxDowntime      The maximum downtime given as milliseconds.
 * @param   pszFilename         The name of the file.  NULL if pStreamOps is used.
 * @param   pStreamOps          The stream methods.  NULL if pszFilename is used.
 * @param   pvStreamOpsUser     The user argument to the stream methods.
 * @param   enmAfter            What to do afterwards.
 * @param   pfnProgress         Progress callback. Optional.
 * @param   pvProgressUser      User argument for the progress callback.
 * @param   pfSuspended         Set if we suspended the VM.
 * @param   fSkipStateChanges   Set if we're supposed to skip state changes (FTM delta case)
 *
 * @thread  Non-EMT
 */
static int vmR3SaveTeleport(PVM pVM, uint32_t cMsMaxDowntime,
                            const char *pszFilename, PCSSMSTRMOPS pStreamOps, void *pvStreamOpsUser,
                            SSMAFTER enmAfter, PFNVMPROGRESS pfnProgress, void *pvProgressUser, bool *pfSuspended,
                            bool fSkipStateChanges)
{
    /*
     * Request the operation in EMT(0).
     */
    PSSMHANDLE pSSM;
    int rc = VMR3ReqCallWait(pVM, 0 /*idDstCpu*/,
                             (PFNRT)vmR3Save, 10, pVM, cMsMaxDowntime, pszFilename, pStreamOps, pvStreamOpsUser,
                             enmAfter, pfnProgress, pvProgressUser, &pSSM, fSkipStateChanges);
    if (    RT_SUCCESS(rc)
        &&  pSSM)
    {
        Assert(!fSkipStateChanges);

        /*
         * Live snapshot.
         *
         * The state handling here is kind of tricky, doing it on EMT(0) helps
         * a bit. See the VMSTATE diagram for details.
         */
        rc = SSMR3LiveDoStep1(pSSM);
        if (RT_SUCCESS(rc))
        {
            if (VMR3GetState(pVM) != VMSTATE_SAVING)
                for (;;)
                {
                    /* Try suspend the VM. */
                    rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_DESCENDING | VMMEMTRENDEZVOUS_FLAGS_STOP_ON_ERROR,
                                            vmR3LiveDoSuspend, pfSuspended);
                    if (rc != VERR_TRY_AGAIN)
                        break;

                    /* Wait for the state to change. */
                    RTThreadSleep(250); /** @todo Live Migration: fix this polling wait by some smart use of multiple release event  semaphores.. */
                }
            if (RT_SUCCESS(rc))
                rc = VMR3ReqCallWait(pVM, 0 /*idDstCpu*/, (PFNRT)vmR3LiveDoStep2, 2, pVM, pSSM);
            else
            {
                int rc2 = VMR3ReqCallWait(pVM, 0 /*idDstCpu*/, (PFNRT)SSMR3LiveDone, 1, pSSM);
                AssertMsg(rc2 == rc, ("%Rrc != %Rrc\n", rc2, rc));
            }
        }
        else
        {
            int rc2 = VMR3ReqCallWait(pVM, 0 /*idDstCpu*/, (PFNRT)SSMR3LiveDone, 1, pSSM);
            AssertMsg(rc2 == rc, ("%Rrc != %Rrc\n", rc2, rc));

            rc2 = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONCE, vmR3LiveDoStep1Cleanup, pfSuspended);
            if (RT_FAILURE(rc2) && rc == VERR_SSM_CANCELLED)
                rc = rc2;
        }
    }

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
 * @param   pStreamOps          The stream methods.
 * @param   pvStreamOpsUser     The user argument to the stream methods.
 * @param   fContinueAfterwards Whether continue execution afterwards or not.
 *                              When in doubt, set this to true.
 * @param   pfnProgress         Progress callback. Optional.
 * @param   pvUser              User argument for the progress callback.
 * @param   pfSuspended         Set if we suspended the VM.
 *
 * @thread      Non-EMT.
 * @vmstate     Suspended or Running
 * @vmstateto   Saving+Suspended or
 *              RunningLS+SuspendingLS+SuspendedLS+Saving+Suspended.
 */
VMMR3DECL(int) VMR3Save(PVM pVM, const char *pszFilename, bool fContinueAfterwards, PFNVMPROGRESS pfnProgress, void *pvUser, bool *pfSuspended)
{
    LogFlow(("VMR3Save: pVM=%p pszFilename=%p:{%s} fContinueAfterwards=%RTbool pfnProgress=%p pvUser=%p pfSuspended=%p\n",
             pVM, pszFilename, pszFilename, fContinueAfterwards, pfnProgress, pvUser, pfSuspended));

    /*
     * Validate input.
     */
    AssertPtr(pfSuspended);
    *pfSuspended = false;
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    VM_ASSERT_OTHER_THREAD(pVM);
    AssertReturn(VALID_PTR(pszFilename), VERR_INVALID_POINTER);
    AssertReturn(*pszFilename, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pfnProgress, VERR_INVALID_POINTER);

    /*
     * Join paths with VMR3Teleport.
     */
    SSMAFTER enmAfter = fContinueAfterwards ? SSMAFTER_CONTINUE : SSMAFTER_DESTROY;
    int rc = vmR3SaveTeleport(pVM, 250 /*cMsMaxDowntime*/,
                              pszFilename, NULL /* pStreamOps */, NULL /* pvStreamOpsUser */,
                              enmAfter, pfnProgress, pvUser, pfSuspended,
                              false /* fSkipStateChanges */);
    LogFlow(("VMR3Save: returns %Rrc (*pfSuspended=%RTbool)\n", rc, *pfSuspended));
    return rc;
}

/**
 * Save current VM state (used by FTM)
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
 * @param   pStreamOps          The stream methods.
 * @param   pvStreamOpsUser     The user argument to the stream methods.
 * @param   pfSuspended         Set if we suspended the VM.
 * @param   fSkipStateChanges   Set if we're supposed to skip state changes (FTM delta case)
 *
 * @thread      Any
 * @vmstate     Suspended or Running
 * @vmstateto   Saving+Suspended or
 *              RunningLS+SuspendingLS+SuspendedLS+Saving+Suspended.
 */
VMMR3DECL(int) VMR3SaveFT(PVM pVM, PCSSMSTRMOPS pStreamOps, void *pvStreamOpsUser, bool *pfSuspended,
                          bool fSkipStateChanges)
{
    LogFlow(("VMR3SaveFT: pVM=%p pStreamOps=%p pvSteamOpsUser=%p pfSuspended=%p\n",
             pVM, pStreamOps, pvStreamOpsUser, pfSuspended));

    /*
     * Validate input.
     */
    AssertPtr(pfSuspended);
    *pfSuspended = false;
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(pStreamOps, VERR_INVALID_PARAMETER);

    /*
     * Join paths with VMR3Teleport.
     */
    int rc = vmR3SaveTeleport(pVM, 250 /*cMsMaxDowntime*/,
                              NULL, pStreamOps, pvStreamOpsUser,
                              SSMAFTER_CONTINUE, NULL, NULL, pfSuspended,
                              fSkipStateChanges);
    LogFlow(("VMR3SaveFT: returns %Rrc (*pfSuspended=%RTbool)\n", rc, *pfSuspended));
    return rc;
}


/**
 * Teleport the VM (aka live migration).
 *
 * @returns VBox status code.
 *
 * @param   pVM                 The VM which state should be saved.
 * @param   cMsMaxDowntime      The maximum downtime given as milliseconds.
 * @param   pStreamOps          The stream methods.
 * @param   pvStreamOpsUser     The user argument to the stream methods.
 * @param   pfnProgress         Progress callback. Optional.
 * @param   pvProgressUser      User argument for the progress callback.
 * @param   pfSuspended         Set if we suspended the VM.
 *
 * @thread      Non-EMT.
 * @vmstate     Suspended or Running
 * @vmstateto   Saving+Suspended or
 *              RunningLS+SuspendingLS+SuspendedLS+Saving+Suspended.
 */
VMMR3DECL(int) VMR3Teleport(PVM pVM, uint32_t cMsMaxDowntime, PCSSMSTRMOPS pStreamOps, void *pvStreamOpsUser,
                            PFNVMPROGRESS pfnProgress, void *pvProgressUser, bool *pfSuspended)
{
    LogFlow(("VMR3Teleport: pVM=%p cMsMaxDowntime=%u pStreamOps=%p pvStreamOps=%p pfnProgress=%p pvProgressUser=%p\n",
             pVM, cMsMaxDowntime, pStreamOps, pvStreamOpsUser, pfnProgress, pvProgressUser));

    /*
     * Validate input.
     */
    AssertPtr(pfSuspended);
    *pfSuspended = false;
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    VM_ASSERT_OTHER_THREAD(pVM);
    AssertPtrReturn(pStreamOps, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pfnProgress, VERR_INVALID_POINTER);

    /*
     * Join paths with VMR3Save.
     */
    int rc = vmR3SaveTeleport(pVM, cMsMaxDowntime,
                              NULL /*pszFilename*/, pStreamOps, pvStreamOpsUser,
                              SSMAFTER_TELEPORT, pfnProgress, pvProgressUser, pfSuspended,
                              false /* fSkipStateChanges */);
    LogFlow(("VMR3Teleport: returns %Rrc (*pfSuspended=%RTbool)\n", rc, *pfSuspended));
    return rc;
}



/**
 * EMT(0) worker for VMR3LoadFromFile and VMR3LoadFromStream.
 *
 * @returns VBox status code.
 *
 * @param   pVM                 The VM handle.
 * @param   pszFilename         The name of the file.  NULL if pStreamOps is used.
 * @param   pStreamOps          The stream methods.  NULL if pszFilename is used.
 * @param   pvStreamOpsUser     The user argument to the stream methods.
 * @param   pfnProgress         Progress callback. Optional.
 * @param   pvUser              User argument for the progress callback.
 * @param   fTeleporting        Indicates whether we're teleporting or not.
 * @param   fSkipStateChanges   Set if we're supposed to skip state changes (FTM delta case)
 *
 * @thread  EMT.
 */
static DECLCALLBACK(int) vmR3Load(PVM pVM, const char *pszFilename, PCSSMSTRMOPS pStreamOps, void *pvStreamOpsUser,
                                  PFNVMPROGRESS pfnProgress, void *pvProgressUser, bool fTeleporting,
                                  bool fSkipStateChanges)
{
    int rc = VINF_SUCCESS;

    LogFlow(("vmR3Load: pVM=%p pszFilename=%p:{%s} pStreamOps=%p pvStreamOpsUser=%p pfnProgress=%p pvProgressUser=%p fTeleporting=%RTbool\n",
             pVM, pszFilename, pszFilename, pStreamOps, pvStreamOpsUser, pfnProgress, pvProgressUser, fTeleporting));

    /*
     * Validate input (paranoia).
     */
    AssertPtr(pVM);
    AssertPtrNull(pszFilename);
    AssertPtrNull(pStreamOps);
    AssertPtrNull(pfnProgress);

    if (!fSkipStateChanges)
    {
        /*
         * Change the state and perform the load.
         *
         * Always perform a relocation round afterwards to make sure hypervisor
         * selectors and such are correct.
         */
        rc = vmR3TrySetState(pVM, "VMR3Load", 2,
                                 VMSTATE_LOADING, VMSTATE_CREATED,
                                 VMSTATE_LOADING, VMSTATE_SUSPENDED);
        if (RT_FAILURE(rc))
            return rc;
    }
    pVM->vm.s.fTeleportedAndNotFullyResumedYet = fTeleporting;

    uint32_t cErrorsPriorToSave = VMR3GetErrorCount(pVM);
    rc = SSMR3Load(pVM, pszFilename, pStreamOps, pvStreamOpsUser, SSMAFTER_RESUME, pfnProgress, pvProgressUser);
    if (RT_SUCCESS(rc))
    {
        VMR3Relocate(pVM, 0 /*offDelta*/);
        if (!fSkipStateChanges)
            vmR3SetState(pVM, VMSTATE_SUSPENDED, VMSTATE_LOADING);
    }
    else
    {
        pVM->vm.s.fTeleportedAndNotFullyResumedYet = false;
        if (!fSkipStateChanges)
            vmR3SetState(pVM, VMSTATE_LOAD_FAILURE, VMSTATE_LOADING);

        if (cErrorsPriorToSave == VMR3GetErrorCount(pVM))
            rc = VMSetError(pVM, rc, RT_SRC_POS,
                            N_("Unable to restore the virtual machine's saved state from '%s'. "
                               "It may be damaged or from an older version of VirtualBox.  "
                               "Please discard the saved state before starting the virtual machine"),
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
VMMR3DECL(int) VMR3LoadFromFile(PVM pVM, const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser)
{
    LogFlow(("VMR3LoadFromFile: pVM=%p pszFilename=%p:{%s} pfnProgress=%p pvUser=%p\n",
             pVM, pszFilename, pszFilename, pfnProgress, pvUser));

    /*
     * Validate input.
     */
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);

    /*
     * Forward the request to EMT(0).  No need to setup a rendezvous here
     * since there is no execution taking place when this call is allowed.
     */
    int rc = VMR3ReqCallWait(pVM, 0 /*idDstCpu*/, (PFNRT)vmR3Load, 8,
                             pVM, pszFilename, (uintptr_t)NULL /*pStreamOps*/, (uintptr_t)NULL /*pvStreamOpsUser*/, pfnProgress, pvUser,
                             false /*fTeleporting*/, false /* fSkipStateChanges */);
    LogFlow(("VMR3LoadFromFile: returns %Rrc\n", rc));
    return rc;
}


/**
 * VMR3LoadFromFile for arbitrary file streams.
 *
 * @returns VBox status code.
 *
 * @param   pVM             The VM handle.
 * @param   pStreamOps      The stream methods.
 * @param   pvStreamOpsUser The user argument to the stream methods.
 * @param   pfnProgress     Progress callback. Optional.
 * @param   pvProgressUser  User argument for the progress callback.
 *
 * @thread      Any thread.
 * @vmstate     Created, Suspended
 * @vmstateto   Loading+Suspended
 */
VMMR3DECL(int) VMR3LoadFromStream(PVM pVM, PCSSMSTRMOPS pStreamOps, void *pvStreamOpsUser,
                                  PFNVMPROGRESS pfnProgress, void *pvProgressUser)
{
    LogFlow(("VMR3LoadFromStream: pVM=%p pStreamOps=%p pvStreamOpsUser=%p pfnProgress=%p pvProgressUser=%p\n",
             pVM, pStreamOps, pvStreamOpsUser, pfnProgress, pvProgressUser));

    /*
     * Validate input.
     */
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertPtrReturn(pStreamOps, VERR_INVALID_POINTER);

    /*
     * Forward the request to EMT(0).  No need to setup a rendezvous here
     * since there is no execution taking place when this call is allowed.
     */
    int rc = VMR3ReqCallWait(pVM, 0 /*idDstCpu*/, (PFNRT)vmR3Load, 8,
                             pVM, (uintptr_t)NULL /*pszFilename*/, pStreamOps, pvStreamOpsUser, pfnProgress, pvProgressUser,
                             true /*fTeleporting*/, false /* fSkipStateChanges */);
    LogFlow(("VMR3LoadFromStream: returns %Rrc\n", rc));
    return rc;
}


/**
 * VMR3LoadFromFileFT for arbitrary file streams.
 *
 * @returns VBox status code.
 *
 * @param   pVM             The VM handle.
 * @param   pStreamOps      The stream methods.
 * @param   pvStreamOpsUser The user argument to the stream methods.
 * @param   pfnProgress     Progress callback. Optional.
 * @param   pvProgressUser  User argument for the progress callback.
 *
 * @thread      Any thread.
 * @vmstate     Created, Suspended
 * @vmstateto   Loading+Suspended
 */
VMMR3DECL(int) VMR3LoadFromStreamFT(PVM pVM, PCSSMSTRMOPS pStreamOps, void *pvStreamOpsUser)
{
    LogFlow(("VMR3LoadFromStreamFT: pVM=%p pStreamOps=%p pvStreamOpsUser=%p\n",
             pVM, pStreamOps, pvStreamOpsUser));

    /*
     * Validate input.
     */
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertPtrReturn(pStreamOps, VERR_INVALID_POINTER);

    /*
     * Forward the request to EMT(0).  No need to setup a rendezvous here
     * since there is no execution taking place when this call is allowed.
     */
    int rc = VMR3ReqCallWait(pVM, 0 /*idDstCpu*/, (PFNRT)vmR3Load, 8,
                             pVM, (uintptr_t)NULL /*pszFilename*/, pStreamOps, pvStreamOpsUser, NULL, NULL,
                             true /*fTeleporting*/, true /* fSkipStateChanges */);
    LogFlow(("VMR3LoadFromStream: returns %Rrc\n", rc));
    return rc;
}

/**
 * EMT rendezvous worker for VMR3PowerOff.
 *
 * @returns VERR_VM_INVALID_VM_STATE or VINF_EM_OFF. (This is a strict
 *          return code, see FNVMMEMTRENDEZVOUS.)
 *
 * @param   pVM             The VM handle.
 * @param   pVCpu           The VMCPU handle of the EMT.
 * @param   pvUser          Ignored.
 */
static DECLCALLBACK(VBOXSTRICTRC) vmR3PowerOff(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    LogFlow(("vmR3PowerOff: pVM=%p pVCpu=%p/#%u\n", pVM, pVCpu, pVCpu->idCpu));
    Assert(!pvUser); NOREF(pvUser);

    /*
     * The first EMT thru here will change the state to PoweringOff.
     */
    if (pVCpu->idCpu == pVM->cCpus - 1)
    {
        int rc = vmR3TrySetState(pVM, "VMR3PowerOff", 11,
                                 VMSTATE_POWERING_OFF,    VMSTATE_RUNNING,           /* 1 */
                                 VMSTATE_POWERING_OFF,    VMSTATE_SUSPENDED,         /* 2 */
                                 VMSTATE_POWERING_OFF,    VMSTATE_DEBUGGING,         /* 3 */
                                 VMSTATE_POWERING_OFF,    VMSTATE_LOAD_FAILURE,      /* 4 */
                                 VMSTATE_POWERING_OFF,    VMSTATE_GURU_MEDITATION,   /* 5 */
                                 VMSTATE_POWERING_OFF,    VMSTATE_FATAL_ERROR,       /* 6 */
                                 VMSTATE_POWERING_OFF,    VMSTATE_CREATED,           /* 7 */   /** @todo update the diagram! */
                                 VMSTATE_POWERING_OFF_LS, VMSTATE_RUNNING_LS,        /* 8 */
                                 VMSTATE_POWERING_OFF_LS, VMSTATE_DEBUGGING_LS,      /* 9 */
                                 VMSTATE_POWERING_OFF_LS, VMSTATE_GURU_MEDITATION_LS,/* 10 */
                                 VMSTATE_POWERING_OFF_LS, VMSTATE_FATAL_ERROR_LS);   /* 11 */
        if (RT_FAILURE(rc))
            return rc;
        if (rc >= 7)
            SSMR3Cancel(pVM);
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
            /** @todo make the state dumping at VMR3PowerOff optional. */
            bool fOldBuffered = RTLogRelSetBuffering(true /*fBuffered*/);
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
            RTLogRelSetBuffering(fOldBuffered);
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
     * Gather all the EMTs to make sure there are no races before
     * changing the VM state.
     */
    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_DESCENDING | VMMEMTRENDEZVOUS_FLAGS_STOP_ON_ERROR,
                                vmR3PowerOff, NULL);
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
 * @thread      Any none emulation thread.
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
        return VERR_INVALID_VM_HANDLE;
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertLogRelReturn(!VM_IS_EMT(pVM), VERR_VM_THREAD_IS_EMT);

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
     * Call vmR3Destroy on each of the EMTs ending with EMT(0) doing the bulk
     * of the cleanup.
     */
    /* vmR3Destroy on all EMTs, ending with EMT(0). */
    rc = VMR3ReqCallWait(pVM, VMCPUID_ALL_REVERSE, (PFNRT)vmR3Destroy, 1, pVM);
    AssertLogRelRC(rc);

    /* Wait for EMTs and destroy the UVM. */
    vmR3DestroyUVM(pUVM, 30000);

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
     * Only VCPU 0 does the full cleanup (last).
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
        rc = FTMR3Term(pVM);
        AssertRC(rc);
        rc = DBGFR3Term(pVM);
        AssertRC(rc);
        rc = PDMR3Term(pVM);
        AssertRC(rc);
        rc = IEMR3Term(pVM);
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
         * We're done, tell the other EMTs to quit.
         */
        ASMAtomicUoWriteBool(&pUVM->vm.s.fTerminateEMT, true);
        ASMAtomicWriteU32(&pVM->fGlobalForcedActions, VM_FF_CHECK_VM_STATE); /* Can't hurt... */
        LogFlow(("vmR3Destroy: returning %Rrc\n", VINF_EM_TERMINATE));
    }
    return VINF_EM_TERMINATE;
}


/**
 * Destroys the UVM portion.
 *
 * This is called as the final step in the VM destruction or as the cleanup
 * in case of a creation failure.
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
    if (pUVM->pVM)
        VM_FF_SET(pUVM->pVM, VM_FF_CHECK_VM_STATE); /* Can't hurt... */
    for (VMCPUID i = 0; i < pUVM->cCpus; i++)
    {
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
        PVMREQ pReqHead = ASMAtomicXchgPtrT(&pUVM->vm.s.pPriorityReqs, NULL, PVMREQ);
        if (!pReqHead)
        {
            pReqHead = ASMAtomicXchgPtrT(&pUVM->vm.s.pNormalReqs, NULL, PVMREQ);
            if (!pReqHead)
                break;
        }
        AssertLogRelMsgFailed(("Requests pending! VMR3Destroy caller has to serialize this.\n"));

        for (PVMREQ pReq = pReqHead; pReq; pReq = pReq->pNext)
        {
            ASMAtomicUoWriteS32(&pReq->iStatus, VERR_INTERNAL_ERROR);
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
    for (VMCPUID idCpu = 0; idCpu < pUVM->cCpus; idCpu++)
    {
        PUVMCPU pUVCpu = &pUVM->aCpus[idCpu];

        for (unsigned i = 0; i < 10; i++)
        {
            PVMREQ pReqHead = ASMAtomicXchgPtrT(&pUVCpu->vm.s.pPriorityReqs, NULL, PVMREQ);
            if (!pReqHead)
            {
                pReqHead = ASMAtomicXchgPtrT(&pUVCpu->vm.s.pNormalReqs, NULL, PVMREQ);
                if (!pReqHead)
                    break;
            }
            AssertLogRelMsgFailed(("Requests pending! VMR3Destroy caller has to serialize this.\n"));

            for (PVMREQ pReq = pReqHead; pReq; pReq = pReq->pNext)
            {
                ASMAtomicUoWriteS32(&pReq->iStatus, VERR_INTERNAL_ERROR);
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
     * Release the UVM structure reference.
     */
    VMR3ReleaseUVM(pUVM);

    /*
     * Clean up and flush logs.
     */
#ifdef LOG_ENABLED
    RTLogSetCustomPrefixCallback(NULL, NULL, NULL);
#endif
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
VMMR3DECL(PVM) VMR3EnumVMs(PVM pVMPrev)
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
VMMR3DECL(int) VMR3AtDtorRegister(PFNVMATDTOR pfnAtDtor, void *pvUser)
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
VMMR3DECL(int) VMR3AtDtorDeregister(PFNVMATDTOR pfnAtDtor)
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
 * EMT rendezvous worker for VMR3Reset.
 *
 * This is called by the emulation threads as a response to the reset request
 * issued by VMR3Reset().
 *
 * @returns VERR_VM_INVALID_VM_STATE, VINF_EM_RESET or VINF_EM_SUSPEND. (This
 *          is a strict return code, see FNVMMEMTRENDEZVOUS.)
 *
 * @param   pVM             The VM handle.
 * @param   pVCpu           The VMCPU handle of the EMT.
 * @param   pvUser          Ignored.
 */
static DECLCALLBACK(VBOXSTRICTRC) vmR3Reset(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    Assert(!pvUser); NOREF(pvUser);

    /*
     * The first EMT will try change the state to resetting.  If this fails,
     * we won't get called for the other EMTs.
     */
    if (pVCpu->idCpu == pVM->cCpus - 1)
    {
        int rc = vmR3TrySetState(pVM, "VMR3Reset", 3,
                                 VMSTATE_RESETTING,     VMSTATE_RUNNING,
                                 VMSTATE_RESETTING,     VMSTATE_SUSPENDED,
                                 VMSTATE_RESETTING_LS,  VMSTATE_RUNNING_LS);
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
                          VERR_INTERNAL_ERROR_4);

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
/** @todo PGMR3Reset should be called after PDMR3Reset really, because we'll trash OS <-> hardware
 * communication structures residing in RAM when done in the other order.  I.e. the device must be
 * quiesced first, then we clear the memory and plan tables. Probably have to make these things
 * explicit in some way, some memory setup pass or something.
 * (Example: DevAHCI may assert if memory is zeroed before it has read the FIS.)
 *
 * @bugref{4467}
 */
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
         * When a live save is active, we will move on to SuspendingLS but
         * leave it for VMR3Reset to do the actual suspending due to deadlock risks.
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
            vmR3SetStateLocked(pVM, pUVM, VMSTATE_SUSPENDING_LS, VMSTATE_RESETTING_LS);
        RTCritSectLeave(&pUVM->vm.s.AtStateCritSect);

        vmR3CheckIntegrity(pVM);

        /*
         * Do the suspend bit as well.
         * It only requires some EMT(0) work at present.
         */
        if (enmVMState != VMSTATE_RESETTING)
        {
            vmR3SuspendDoWork(pVM);
            vmR3SetState(pVM, VMSTATE_SUSPENDED_LS, VMSTATE_SUSPENDING_LS);
        }
    }

    return enmVMState == VMSTATE_RESETTING
         ? VINF_EM_RESET
         : VINF_EM_SUSPEND; /** @todo VINF_EM_SUSPEND has lower priority than VINF_EM_RESET, so fix races. Perhaps add a new code for this combined case. */
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
     * Gather all the EMTs to make sure there are no races before
     * changing the VM state.
     */
    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_DESCENDING | VMMEMTRENDEZVOUS_FLAGS_STOP_ON_ERROR,
                                vmR3Reset, NULL);
    LogFlow(("VMR3Reset: returns %Rrc\n", rc));
    return rc;
}


/**
 * Gets the user mode VM structure pointer given the VM handle.
 *
 * @returns Pointer to the user mode VM structure on success. NULL if @a pVM is
 *          invalid (asserted).
 * @param   pVM                 The VM handle.
 * @sa      VMR3GetVM, VMR3RetainUVM
 */
VMMR3DECL(PUVM) VMR3GetUVM(PVM pVM)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, NULL);
    return pVM->pUVM;
}


/**
 * Gets the shared VM structure pointer given the pointer to the user mode VM
 * structure.
 *
 * @returns Pointer to the shared VM structure.
 *          NULL if @a pUVM is invalid (asserted) or if no shared VM structure
 *          is currently associated with it.
 * @param   pUVM                The user mode VM handle.
 * @sa      VMR3GetUVM
 */
VMMR3DECL(PVM) VMR3GetVM(PUVM pUVM)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, NULL);
    return pUVM->pVM;
}


/**
 * Retain the user mode VM handle.
 *
 * @returns Reference count.
 *          UINT32_MAX if @a pUVM is invalid.
 *
 * @param   pUVM                The user mode VM handle.
 * @sa      VMR3ReleaseUVM
 */
VMMR3DECL(uint32_t) VMR3RetainUVM(PUVM pUVM)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, UINT32_MAX);
    uint32_t cRefs = ASMAtomicIncU32(&pUVM->vm.s.cUvmRefs);
    AssertMsg(cRefs > 0 && cRefs < _64K, ("%u\n", cRefs));
    return cRefs;
}


/**
 * Does the final release of the UVM structure.
 *
 * @param   pUVM                The user mode VM handle.
 */
static void vmR3DoReleaseUVM(PUVM pUVM)
{
    /*
     * Free the UVM.
     */
    Assert(!pUVM->pVM);

    MMR3TermUVM(pUVM);
    STAMR3TermUVM(pUVM);

    ASMAtomicUoWriteU32(&pUVM->u32Magic, UINT32_MAX);
    RTTlsFree(pUVM->vm.s.idxTLS);
    RTMemPageFree(pUVM, RT_OFFSETOF(UVM, aCpus[pUVM->cCpus]));
}


/**
 * Releases a refernece to the mode VM handle.
 *
 * @returns The new reference count, 0 if destroyed.
 *          UINT32_MAX if @a pUVM is invalid.
 *
 * @param   pUVM                The user mode VM handle.
 * @sa      VMR3RetainUVM
 */
VMMR3DECL(uint32_t) VMR3ReleaseUVM(PUVM pUVM)
{
    if (!pUVM)
        return 0;
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, UINT32_MAX);
    uint32_t cRefs = ASMAtomicDecU32(&pUVM->vm.s.cUvmRefs);
    if (!cRefs)
        vmR3DoReleaseUVM(pUVM);
    else
        AssertMsg(cRefs < _64K, ("%u\n", cRefs));
    return cRefs;
}


/**
 * Gets the VM name.
 *
 * @returns Pointer to a read-only string containing the name. NULL if called
 *          too early.
 * @param   pUVM                The user mode VM handle.
 */
VMMR3DECL(const char *) VMR3GetName(PUVM pUVM)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, NULL);
    return pUVM->vm.s.pszName;
}


/**
 * Gets the VM UUID.
 *
 * @returns pUuid on success, NULL on failure.
 * @param   pUVM                The user mode VM handle.
 * @param   pUuid               Where to store the UUID.
 */
VMMR3DECL(PRTUUID) VMR3GetUuid(PUVM pUVM, PRTUUID pUuid)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, NULL);
    AssertPtrReturn(pUuid, NULL);

    *pUuid = pUVM->vm.s.Uuid;
    return pUuid;
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
    VM_ASSERT_VALID_EXT_RETURN(pVM, VMSTATE_TERMINATED);
    return pVM->enmVMState;
}


/**
 * Gets the current VM state.
 *
 * @returns The current VM state.
 * @param   pUVM            The user-mode VM handle.
 * @thread  Any
 */
VMMR3DECL(VMSTATE) VMR3GetStateU(PUVM pUVM)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VMSTATE_TERMINATED);
    if (RT_UNLIKELY(!pUVM->pVM))
        return VMSTATE_TERMINATED;
    return pUVM->pVM->enmVMState;
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
        case VMSTATE_RUNNING_FT:        return "RUNNING_FT";
        case VMSTATE_RESETTING:         return "RESETTING";
        case VMSTATE_RESETTING_LS:      return "RESETTING_LS";
        case VMSTATE_SUSPENDED:         return "SUSPENDED";
        case VMSTATE_SUSPENDED_LS:      return "SUSPENDED_LS";
        case VMSTATE_SUSPENDED_EXT_LS:  return "SUSPENDED_EXT_LS";
        case VMSTATE_SUSPENDING:        return "SUSPENDING";
        case VMSTATE_SUSPENDING_LS:     return "SUSPENDING_LS";
        case VMSTATE_SUSPENDING_EXT_LS: return "SUSPENDING_EXT_LS";
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
        case VMSTATE_OFF_LS:            return "OFF_LS";
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
                            /*|| enmStateNew == VMSTATE_FATAL_ERROR ?*/
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_RESUMING:
            AssertMsgReturn(   enmStateNew == VMSTATE_RUNNING
                            /*|| enmStateNew == VMSTATE_FATAL_ERROR ?*/
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_RUNNING:
            AssertMsgReturn(   enmStateNew == VMSTATE_POWERING_OFF
                            || enmStateNew == VMSTATE_SUSPENDING
                            || enmStateNew == VMSTATE_RESETTING
                            || enmStateNew == VMSTATE_RUNNING_LS
                            || enmStateNew == VMSTATE_RUNNING_FT
                            || enmStateNew == VMSTATE_DEBUGGING
                            || enmStateNew == VMSTATE_FATAL_ERROR
                            || enmStateNew == VMSTATE_GURU_MEDITATION
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_RUNNING_LS:
            AssertMsgReturn(   enmStateNew == VMSTATE_POWERING_OFF_LS
                            || enmStateNew == VMSTATE_SUSPENDING_LS
                            || enmStateNew == VMSTATE_SUSPENDING_EXT_LS
                            || enmStateNew == VMSTATE_RESETTING_LS
                            || enmStateNew == VMSTATE_RUNNING
                            || enmStateNew == VMSTATE_DEBUGGING_LS
                            || enmStateNew == VMSTATE_FATAL_ERROR_LS
                            || enmStateNew == VMSTATE_GURU_MEDITATION_LS
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_RUNNING_FT:
            AssertMsgReturn(   enmStateNew == VMSTATE_POWERING_OFF
                            || enmStateNew == VMSTATE_FATAL_ERROR
                            || enmStateNew == VMSTATE_GURU_MEDITATION
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_RESETTING:
            AssertMsgReturn(enmStateNew == VMSTATE_RUNNING, ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_RESETTING_LS:
            AssertMsgReturn(   enmStateNew == VMSTATE_SUSPENDING_LS
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_SUSPENDING:
            AssertMsgReturn(enmStateNew == VMSTATE_SUSPENDED, ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_SUSPENDING_LS:
            AssertMsgReturn(   enmStateNew == VMSTATE_SUSPENDING
                            || enmStateNew == VMSTATE_SUSPENDED_LS
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_SUSPENDING_EXT_LS:
            AssertMsgReturn(   enmStateNew == VMSTATE_SUSPENDING
                            || enmStateNew == VMSTATE_SUSPENDED_EXT_LS
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
            AssertMsgReturn(   enmStateNew == VMSTATE_SUSPENDED
                            || enmStateNew == VMSTATE_SAVING
                            , ("%s -> %s\n", VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew)), false);
            break;

        case VMSTATE_SUSPENDED_EXT_LS:
            AssertMsgReturn(   enmStateNew == VMSTATE_SUSPENDED
                            || enmStateNew == VMSTATE_SAVING
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
                            || enmStateNew == VMSTATE_DEBUGGING_LS
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
    VM_FF_CLEAR(pVM, VM_FF_CHECK_VM_STATE);

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
     * Grab the lock and see if any of the proposed transitions works out.
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
        {
            LogRel(("%s: %s -> %s failed, because the VM state is actually %s\n",
                    pszWho, VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew), VMR3GetStateName(enmStateCur)));
            VMSetError(pVM, VERR_VM_INVALID_VM_STATE, RT_SRC_POS,
                       N_("%s failed because the VM state is %s instead of %s"),
                       pszWho, VMR3GetStateName(enmStateCur), VMR3GetStateName(enmStateOld));
            AssertMsgFailed(("%s: %s -> %s failed, because the VM state is actually %s\n",
                             pszWho, VMR3GetStateName(enmStateOld), VMR3GetStateName(enmStateNew), VMR3GetStateName(enmStateCur)));
        }
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
            VMSetError(pVM, VERR_VM_INVALID_VM_STATE, RT_SRC_POS,
                       N_("%s failed because the current VM state, %s, was not found in the state transition table"),
                       pszWho, VMR3GetStateName(enmStateCur), VMR3GetStateName(enmStateOld));
            AssertMsgFailed(("%s - state=%s, see release log for full details. Check the cTransitions passed us.\n",
                             pszWho, VMR3GetStateName(enmStateCur)));
        }
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
 * Called by vmR3EmulationThreadWithId just before the VM structure is freed.
 *
 * @param   pVM             The VM handle.
 */
void vmR3SetTerminated(PVM pVM)
{
    vmR3SetState(pVM, VMSTATE_TERMINATED, VMSTATE_DESTROYING);
}


/**
 * Checks if the VM was teleported and hasn't been fully resumed yet.
 *
 * This applies to both sides of the teleportation since we may leave a working
 * clone behind and the user is allowed to resume this...
 *
 * @returns true / false.
 * @param   pVM                 The VM handle.
 * @thread  Any thread.
 */
VMMR3DECL(bool) VMR3TeleportedAndNotFullyResumedYet(PVM pVM)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, false);
    return pVM->vm.s.fTeleportedAndNotFullyResumedYet;
}


/**
 * Registers a VM state change callback.
 *
 * You are not allowed to call any function which changes the VM state from a
 * state callback.
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
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
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
    AssertReleaseMsgFailed(("And we have a winner! You get to implement Ring-0 and GC VMSetErrorV! Contracts!\n"));

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
    ASMAtomicIncU32(&pUVM->vm.s.cRuntimeErrors);
    for (PVMATERROR pCur = pUVM->vm.s.pAtError; pCur; pCur = pCur->pNext)
        vmR3SetErrorWorkerDoCall(pVM, pCur, rc, RT_SRC_POS_ARGS, "%s", pszMessage);
    RTCritSectLeave(&pUVM->vm.s.AtErrorCritSect);
}


/**
 * Gets the number of errors raised via VMSetError.
 *
 * This can be used avoid double error messages.
 *
 * @returns The error count.
 * @param   pVM             The VM handle.
 */
VMMR3DECL(uint32_t) VMR3GetErrorCount(PVM pVM)
{
    AssertPtrReturn(pVM, 0);
    return VMR3GetErrorCountU(pVM->pUVM);
}


/**
 * Gets the number of errors raised via VMSetError.
 *
 * This can be used avoid double error messages.
 *
 * @returns The error count.
 * @param   pVM             The VM handle.
 */
VMMR3DECL(uint32_t) VMR3GetErrorCountU(PUVM pUVM)
{
    AssertPtrReturn(pUVM, 0);
    AssertReturn(pUVM->u32Magic == UVM_MAGIC, 0);
    return pUVM->vm.s.cErrors;
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
    /*
     * Log the error.
     */
    va_list va3;
    va_copy(va3, *pArgs);
    RTLogRelPrintf("VMSetError: %s(%d) %s; rc=%Rrc\n"
                   "VMSetError: %N\n",
                   pszFile, iLine, pszFunction, rc,
                   pszFormat, &va3);
    va_end(va3);

#ifdef LOG_ENABLED
    va_copy(va3, *pArgs);
    RTLogPrintf("VMSetError: %s(%d) %s; rc=%Rrc\n"
                "%N\n",
                pszFile, iLine, pszFunction, rc,
                pszFormat, &va3);
    va_end(va3);
#endif

    /*
     * Make a copy of the message.
     */
    if (pUVM->pVM)
        vmSetErrorCopy(pUVM->pVM, rc, RT_SRC_POS_ARGS, pszFormat, *pArgs);

    /*
     * Call the at error callbacks.
     */
    bool fCalledSomeone = false;
    RTCritSectEnter(&pUVM->vm.s.AtErrorCritSect);
    ASMAtomicIncU32(&pUVM->vm.s.cErrors);
    for (PVMATERROR pCur = pUVM->vm.s.pAtError; pCur; pCur = pCur->pNext)
    {
        va_list va2;
        va_copy(va2, *pArgs);
        pCur->pfnAtError(pUVM->pVM, pCur->pvUser, rc, RT_SRC_POS_ARGS, pszFormat, va2);
        va_end(va2);
        fCalledSomeone = true;
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
 * EMT rendezvous worker that vmR3SetRuntimeErrorCommon uses to safely change
 * the state to FatalError(LS).
 *
 * @returns VERR_VM_INVALID_VM_STATE or VINF_EM_SUSPEND.  (This is a strict
 *          return code, see FNVMMEMTRENDEZVOUS.)
 *
 * @param   pVM             The VM handle.
 * @param   pVCpu           The VMCPU handle of the EMT.
 * @param   pvUser          Ignored.
 */
static DECLCALLBACK(VBOXSTRICTRC) vmR3SetRuntimeErrorChangeState(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    NOREF(pVCpu);
    Assert(!pvUser); NOREF(pvUser);

    /*
     * The first EMT thru here changes the state.
     */
    if (pVCpu->idCpu == pVM->cCpus - 1)
    {
        int rc = vmR3TrySetState(pVM, "VMSetRuntimeError", 2,
                                 VMSTATE_FATAL_ERROR,    VMSTATE_RUNNING,
                                 VMSTATE_FATAL_ERROR_LS, VMSTATE_RUNNING_LS);
        if (RT_FAILURE(rc))
            return rc;
        if (rc == 2)
            SSMR3Cancel(pVM);

        VM_FF_SET(pVM, VM_FF_CHECK_VM_STATE);
    }

    /* This'll make sure we get out of whereever we are (e.g. REM). */
    return VINF_EM_SUSPEND;
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
    int rc;
    if (fFlags & VMSETRTERR_FLAGS_FATAL)
        rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_DESCENDING | VMMEMTRENDEZVOUS_FLAGS_STOP_ON_ERROR,
                                vmR3SetRuntimeErrorChangeState, NULL);
    else if (fFlags & VMSETRTERR_FLAGS_SUSPEND)
        rc = VMR3Suspend(pVM);
    else
        rc = VINF_SUCCESS;

    /*
     * Do the callback round.
     */
    PUVM pUVM = pVM->pUVM;
    RTCritSectEnter(&pUVM->vm.s.AtErrorCritSect);
    ASMAtomicIncU32(&pUVM->vm.s.cRuntimeErrors);
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
 * Gets the number of runtime errors raised via VMR3SetRuntimeError.
 *
 * This can be used avoid double error messages.
 *
 * @returns The runtime error count.
 * @param   pVM             The VM handle.
 */
VMMR3DECL(uint32_t) VMR3GetRuntimeErrorCount(PVM pVM)
{
    return pVM->pUVM->vm.s.cRuntimeErrors;
}


/**
 * Gets the ID virtual of the virtual CPU associated with the calling thread.
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


/**
 * Return the package and core id of a CPU.
 *
 * @returns VBOX status code.
 * @param   pVM              The VM to operate on.
 * @param   idCpu            Virtual CPU to get the ID from.
 * @param   pidCpuCore       Where to store the core ID of the virtual CPU.
 * @param   pidCpuPackage    Where to store the package ID of the virtual CPU.
 *
 */
VMMR3DECL(int) VMR3GetCpuCoreAndPackageIdFromCpuId(PVM pVM, VMCPUID idCpu, uint32_t *pidCpuCore, uint32_t *pidCpuPackage)
{
    /*
     * Validate input.
     */
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertPtrReturn(pidCpuCore, VERR_INVALID_POINTER);
    AssertPtrReturn(pidCpuPackage, VERR_INVALID_POINTER);
    if (idCpu >= pVM->cCpus)
        return VERR_INVALID_CPU_ID;

    /*
     * Set return values.
     */
#ifdef VBOX_WITH_MULTI_CORE
    *pidCpuCore    = idCpu;
    *pidCpuPackage = 0;
#else
    *pidCpuCore    = 0;
    *pidCpuPackage = idCpu;
#endif

    return VINF_SUCCESS;
}


/**
 * Worker for VMR3HotUnplugCpu.
 *
 * @returns VINF_EM_WAIT_SPIP (strict status code).
 * @param   pVM                 The VM handle.
 * @param   idCpu               The current CPU.
 */
static DECLCALLBACK(int) vmR3HotUnplugCpu(PVM pVM, VMCPUID idCpu)
{
    PVMCPU pVCpu = VMMGetCpuById(pVM, idCpu);
    VMCPU_ASSERT_EMT(pVCpu);

    /*
     * Reset per CPU resources.
     *
     * Actually only needed for VT-x because the CPU seems to be still in some
     * paged mode and startup fails after a new hot plug event. SVM works fine
     * even without this.
     */
    Log(("vmR3HotUnplugCpu for VCPU %u\n", idCpu));
    PGMR3ResetUnpluggedCpu(pVM, pVCpu);
    PDMR3ResetCpu(pVCpu);
    TRPMR3ResetCpu(pVCpu);
    CPUMR3ResetCpu(pVCpu);
    EMR3ResetCpu(pVCpu);
    HWACCMR3ResetCpu(pVCpu);
    return VINF_EM_WAIT_SIPI;
}


/**
 * Hot-unplugs a CPU from the guest.
 *
 * @returns VBox status code.
 * @param   pVM     The VM to operate on.
 * @param   idCpu   Virtual CPU to perform the hot unplugging operation on.
 */
VMMR3DECL(int) VMR3HotUnplugCpu(PVM pVM, VMCPUID idCpu)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(idCpu < pVM->cCpus, VERR_INVALID_CPU_ID);

    /** @todo r=bird: Don't destroy the EMT, it'll break VMMR3EmtRendezvous and
     *        broadcast requests.  Just note down somewhere that the CPU is
     *        offline and send it to SPIP wait.  Maybe modify VMCPUSTATE and push
     *        it out of the EM loops when offline. */
    return VMR3ReqCallNoWait(pVM, idCpu, (PFNRT)vmR3HotUnplugCpu, 2, pVM, idCpu);
}


/**
 * Hot-plugs a CPU on the guest.
 *
 * @returns VBox status code.
 * @param   pVM     The VM to operate on.
 * @param   idCpu   Virtual CPU to perform the hot plugging operation on.
 */
VMMR3DECL(int) VMR3HotPlugCpu(PVM pVM, VMCPUID idCpu)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(idCpu < pVM->cCpus, VERR_INVALID_CPU_ID);

    /** @todo r-bird: Just mark it online and make sure it waits on SPIP. */
    return VINF_SUCCESS;
}


/**
 * Changes the VMM execution cap.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM to operate on.
 * @param   uCpuExecutionCap    New CPU execution cap in precent, 1-100. Where
 *                              100 is max performance (default).
 */
VMMR3DECL(int) VMR3SetCpuExecutionCap(PVM pVM, uint32_t uCpuExecutionCap)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(uCpuExecutionCap > 0 && uCpuExecutionCap <= 100, VERR_INVALID_PARAMETER);

    Log(("VMR3SetCpuExecutionCap: new priority = %d\n", uCpuExecutionCap));
    /* Note: not called from EMT. */
    pVM->uCpuExecutionCap = uCpuExecutionCap;
    return VINF_SUCCESS;
}

