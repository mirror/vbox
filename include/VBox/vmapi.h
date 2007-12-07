/** @file
 * VM - The Virtual Machine, API.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
 */

#ifndef ___VBox_vmapi_h
#define ___VBox_vmapi_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/cpum.h>
#include <VBox/stam.h>
#include <VBox/cfgm.h>

#include <iprt/stdarg.h>

__BEGIN_DECLS

/** @defgroup grp_vmm_apis  VM All Contexts API
 * @ingroup grp_vm
 * @{ */

/** @def VM_GUEST_ADDR
 * Converts a current context address of data within the VM structure to the equivalent
 * guest address.
 *
 * @returns guest virtual address.
 * @param   pVM     Pointer to the VM.
 * @param   pvInVM  CC Pointer within the VM.
 */
#ifdef IN_RING3
# define VM_GUEST_ADDR(pVM, pvInVM)     ( (RTGCPTR)((RTGCUINTPTR)pVM->pVMGC + (uint32_t)((uintptr_t)(pvInVM) - (uintptr_t)pVM->pVMR3)) )
#elif defined(IN_RING0)
# define VM_GUEST_ADDR(pVM, pvInVM)     ( (RTGCPTR)((RTGCUINTPTR)pVM->pVMGC + (uint32_t)((uintptr_t)(pvInVM) - (uintptr_t)pVM->pVMR0)) )
#else
# define VM_GUEST_ADDR(pVM, pvInVM)     ( (RTGCPTR)(pvInVM) )
#endif 

/** @def VM_R3_ADDR
 * Converts a current context address of data within the VM structure to the equivalent
 * ring-3 host address.
 *
 * @returns host virtual address.
 * @param   pVM     Pointer to the VM.
 * @param   pvInVM  CC pointer within the VM.
 */
#ifdef IN_GC
# define VM_R3_ADDR(pVM, pvInVM)       ( (RTR3PTR)((RTR3UINTPTR)pVM->pVMR3 + (uint32_t)((uintptr_t)(pvInVM) - (uintptr_t)pVM->pVMGC)) )
#elif defined(IN_RING0)
# define VM_R3_ADDR(pVM, pvInVM)       ( (RTR3PTR)((RTR3UINTPTR)pVM->pVMR3 + (uint32_t)((uintptr_t)(pvInVM) - (uintptr_t)pVM->pVMR0)) )
#else
# define VM_R3_ADDR(pVM, pvInVM)       ( (RTR3PTR)(pvInVM) )
#endif 


/** @def VM_R0_ADDR
 * Converts a current context address of data within the VM structure to the equivalent
 * ring-0 host address.
 *
 * @returns host virtual address.
 * @param   pVM     Pointer to the VM.
 * @param   pvInVM  CC pointer within the VM.
 */
#ifdef IN_GC
# define VM_R0_ADDR(pVM, pvInVM)       ( (RTR0PTR)((RTR0UINTPTR)pVM->pVMR0 + (uint32_t)((uintptr_t)(pvInVM) - (uintptr_t)pVM->pVMGC)) )
#elif defined(IN_RING3)
# define VM_R0_ADDR(pVM, pvInVM)       ( (RTR0PTR)((RTR0UINTPTR)pVM->pVMR0 + (uint32_t)((uintptr_t)(pvInVM) - (uintptr_t)pVM->pVMR3)) )
#else
# define VM_R0_ADDR(pVM, pvInVM)       ( (RTR0PTR)(pvInVM) )
#endif 

/** @def VM_HOST_ADDR
 * Converts guest address of data within the VM structure to the equivalent
 * host address.
 *
 * @returns host virtual address.
 * @param   pVM     Pointer to the VM.
 * @param   pvInVM  GC Pointer within the VM.
 * @deprecated
 */
#define VM_HOST_ADDR(pVM, pvInVM)     ( (RTHCPTR)((RTHCUINTPTR)pVM->pVMHC + (uint32_t)((uintptr_t)(pvInVM) - (uintptr_t)pVM->pVMGC)) )



/**
 * VM error callback function.
 *
 * @param   pVM             The VM handle. Can be NULL if an error occurred before
 *                          successfully creating a VM.
 * @param   pvUser          The user argument.
 * @param   rc              VBox status code.
 * @param   RT_SRC_POS_DECL The source position arguments. See RT_SRC_POS and RT_SRC_POS_ARGS.
 * @param   pszFormat       Error message format string.
 * @param   args            Error message arguments.
 */
typedef DECLCALLBACK(void) FNVMATERROR(PVM pVM, void *pvUser, int rc, RT_SRC_POS_DECL, const char *pszError, va_list args);
/** Pointer to a VM error callback. */
typedef FNVMATERROR *PFNVMATERROR;

/**
 * Sets the error message.
 *
 * @returns rc. Meaning you can do:
 *    @code
 *    return VM_SET_ERROR(pVM, VERR_OF_YOUR_CHOICE, "descriptive message");
 *    @endcode
 * @param   pVM             VM handle. Must be non-NULL.
 * @param   rc              VBox status code.
 * @param   RT_SRC_POS_DECL Use RT_SRC_POS.
 * @param   pszFormat       Error message format string.
 * @param   ...             Error message arguments.
 * @thread  Any
 */
VMDECL(int) VMSetError(PVM pVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...);

/**
 * Sets the error message.
 *
 * @returns rc. Meaning you can do:
 *    @code
 *    return VM_SET_ERROR(pVM, VERR_OF_YOUR_CHOICE, "descriptive message");
 *    @endcode
 * @param   pVM             VM handle. Must be non-NULL.
 * @param   rc              VBox status code.
 * @param   RT_SRC_POS_DECL Use RT_SRC_POS.
 * @param   pszFormat       Error message format string.
 * @param   args            Error message arguments.
 * @thread  Any
 */
VMDECL(int) VMSetErrorV(PVM pVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list args);

/** @def VM_SET_ERROR
 * Macro for setting a simple VM error message.
 * Don't use '%' in the message!
 *
 * @returns rc. Meaning you can do:
 *    @code
 *    return VM_SET_ERROR(pVM, VERR_OF_YOUR_CHOICE, "descriptive message");
 *    @endcode
 * @param   pVM             VM handle.
 * @param   rc              VBox status code.
 * @param   pszMessage      Error message string.
 * @thread  Any
 */
#define VM_SET_ERROR(pVM, rc, pszMessage)   (VMSetError(pVM, rc, RT_SRC_POS, pszMessage))


/**
 * VM runtime error callback function.
 * See VMSetRuntimeError for the detailed description of parameters.
 *
 * @param   pVM             The VM handle.
 * @param   pvUser          The user argument.
 * @param   fFatal          Whether it is a fatal error or not.
 * @param   pszErrorID      Error ID string.
 * @param   pszFormat       Error message format string.
 * @param   args            Error message arguments.
 */
typedef DECLCALLBACK(void) FNVMATRUNTIMEERROR(PVM pVM, void *pvUser, bool fFatal,
                                              const char *pszErrorID,
                                              const char *pszFormat, va_list args);
/** Pointer to a VM runtime error callback. */
typedef FNVMATRUNTIMEERROR *PFNVMATRUNTIMEERROR;

/**
 * Sets the runtime error message.
 * As opposed VMSetError(), this method is intended to inform the VM user about
 * errors and error-like conditions that happen at an arbitrary point during VM
 * execution (like "host memory low" or "out of host disk space").
 *
 * The @a fFatal parameter defines whether the error is fatal or not. If it is
 * true, then it is expected that the caller has already paused the VM execution
 * before calling this method. The VM user is supposed to power off the VM
 * immediately after it has received the runtime error notification via the
 * FNVMATRUNTIMEERROR callback.
 *
 * If @a fFatal is false, then the paused state of the VM defines the kind of
 * the error. If the VM is paused before calling this method, it means that
 * the VM user may try to fix the error condition (i.e. free more host memory)
 * and then resume the VM execution. If the VM is not paused before calling
 * this method, it means that the given error is a warning about an error
 * condition that may happen soon but that doesn't directly affect the
 * VM execution by the time of the call.
 *
 * The @a pszErrorID parameter defines an unique error identificator.
 * It is used by the front-ends to show a proper message to the end user
 * containig possible actions (for example, Retry/Ignore). For this reason,
 * an error ID assigned once to some particular error condition should not
 * change in the future. The format of this parameter is "SomeErrorCondition". 
 *
 * @param   pVM             VM handle. Must be non-NULL.
 * @param   fFatal          Whether it is a fatal error or not.
 * @param   pszErrorID      Error ID string.
 * @param   pszFormat       Error message format string.
 * @param   ...             Error message arguments.
 *
 * @return  VBox status code (whether the error has been successfully set
 *          and delivered to callbacks or not).
 *
 * @thread  Any
 */
VMDECL(int) VMSetRuntimeError(PVM pVM, bool fFatal, const char *pszErrorID,
                              const char *pszFormat, ...);

/**
 * va_list version of VMSetRuntimeError.
 *
 * @param   pVM             VM handle. Must be non-NULL.
 * @param   fFatal          Whether it is a fatal error or not.
 * @param   pszErrorID      Error ID string.
 * @param   pszFormat       Error message format string.
 * @param   args            Error message arguments.
 *
 * @return  VBox status code (whether the error has been successfully set
 *          and delivered to callbacks or not).
 *
 * @thread  Any
 */
VMDECL(int) VMSetRuntimeErrorV(PVM pVM, bool fFatal, const char *pszErrorID,
                               const char *pszFormat, va_list args);


/**
 * VM reset callback.
 *
 * @returns VBox status code.
 * @param   pDevInst    Device instance of the device which registered the callback.
 * @param   pvUser      User argument.
 */
typedef DECLCALLBACK(int) FNVMATRESET(PPDMDEVINS pDevInst, void *pvUser);
/** VM reset callback. */
typedef FNVMATRESET *PFNVMATRESET;

/**
 * VM reset internal callback.
 *
 * @returns VBox status code.
 * @param   pVM     The VM which is begin reset.
 * @param   pvUser  User argument.
 */
typedef DECLCALLBACK(int) FNVMATRESETINT(PVM pVM, void *pvUser);
/** VM reset internal callback. */
typedef FNVMATRESETINT *PFNVMATRESETINT;

/**
 * VM reset external callback.
 *
 * @param   pvUser  User argument.
 */
typedef DECLCALLBACK(void) FNVMATRESETEXT(void *pvUser);
/** VM reset external callback. */
typedef FNVMATRESETEXT *PFNVMATRESETEXT;


/**
 * VM state callback function.
 *
 * You are not allowed to call any function which changes the VM state from a
 * state callback, except VMR3Destroy().
 *
 * @param   pVM         The VM handle.
 * @param   enmState    The new state.
 * @param   enmOldState The old state.
 * @param   pvUser      The user argument.
 */
typedef DECLCALLBACK(void) FNVMATSTATE(PVM pVM, VMSTATE enmState, VMSTATE enmOldState, void *pvUser);
/** Pointer to a VM state callback. */
typedef FNVMATSTATE *PFNVMATSTATE;


/**
 * Request type.
 */
typedef enum VMREQTYPE
{
    /** Invalid request. */
    VMREQTYPE_INVALID = 0,
    /** VM: Internal. */
    VMREQTYPE_INTERNAL,
    /** Maximum request type (exclusive). Used for validation. */
    VMREQTYPE_MAX
} VMREQTYPE;

/**
 * Request state.
 */
typedef enum VMREQSTATE
{
    /** The state is invalid. */
    VMREQSTATE_INVALID = 0,
    /** The request have been allocated and is in the process of being filed. */
    VMREQSTATE_ALLOCATED,
    /** The request is queued by the requester. */
    VMREQSTATE_QUEUED,
    /** The request is begin processed. */
    VMREQSTATE_PROCESSING,
    /** The request is completed, the requester is begin notified. */
    VMREQSTATE_COMPLETED,
    /** The request packet is in the free chain. (The requester */
    VMREQSTATE_FREE
} VMREQSTATE;

/**
 * Request flags.
 */
typedef enum VMREQFLAGS
{
    /** The request returns a VBox status code. */
    VMREQFLAGS_VBOX_STATUS  = 0,
    /** The request is a void request and have no status code. */
    VMREQFLAGS_VOID         = 1,
    /** Return type mask. */
    VMREQFLAGS_RETURN_MASK  = 1,
    /** Caller does not wait on the packet, EMT will free it. */
    VMREQFLAGS_NO_WAIT      = 2
} VMREQFLAGS;

/**
 * VM Request packet.
 *
 * This is used to request an action in the EMT. Usually the requester is
 * another thread, but EMT can also end up being the requester in which case
 * it's carried out synchronously.
 */
typedef struct VMREQ
{
    /** Pointer to the next request in the chain. */
    struct VMREQ * volatile pNext;
    /** Pointer to the VM this packet belongs to. */
    PVM                     pVM;
    /** Request state. */
    volatile VMREQSTATE     enmState;
    /** VBox status code for the completed request. */
    volatile int            iStatus;
    /** Requester event sem.
     * The request can use this event semaphore to wait/poll for completion
     * of the request.
     */
    RTSEMEVENT              EventSem;
    /** Set if the event semaphore is clear. */
    volatile bool           fEventSemClear;
    /** Flags, VMR3REQ_FLAGS_*. */
    unsigned                fFlags;
    /** Request type. */
    VMREQTYPE               enmType;
    /** Request specific data. */
    union VMREQ_U
    {
        /** VMREQTYPE_INTERNAL. */
        struct
        {
            /** Pointer to the function to be called. */
            PFNRT               pfn;
            /** Number of arguments. */
            unsigned            cArgs;
            /** Array of arguments. */
            uintptr_t           aArgs[64];
        } Internal;
    } u;
} VMREQ;
/** Pointer to a VM request packet. */
typedef VMREQ *PVMREQ;

/** @} */


#ifndef IN_GC
/** @defgroup grp_vmm_apis_hc  VM Host Context API
 * @ingroup grp_vm
 * @{ */

/** @} */
#endif


#ifdef IN_RING3
/** @defgroup grp_vmm_apis_r3  VM Host Context Ring 3 API
 * This interface is a _draft_!
 * @ingroup grp_vm
 * @{ */

/**
 * Completion notification codes.
 */
typedef enum VMINITCOMPLETED
{
    /** The Ring3 init is completed. */
    VMINITCOMPLETED_RING3 = 1,
    /** The Ring0 init is completed. */
    VMINITCOMPLETED_RING0,
    /** The GC init is completed. */
    VMINITCOMPLETED_GC
} VMINITCOMPLETED;


/**
 * Creates a virtual machine by calling the supplied configuration constructor.
 *
 * On successful return the VM is powered off, i.e. VMR3PowerOn() should be
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
 * @thread      Any thread.
 * @vmstate     N/A
 * @vmstateto   Created
 */
VMR3DECL(int)   VMR3Create(PFNVMATERROR pfnVMAtError, void *pvUserVM, PFNCFGMCONSTRUCTOR pfnCFGMConstructor, void *pvUserCFGM, PVM *ppVM);

/**
 * Power on a virtual machine.
 *
 * @returns 0 on success.
 * @returns VBox error code on failure.
 * @param   pVM         VM to power on.
 * @thread      Any thread.
 * @vmstate     Created
 * @vmstateto   Running
 */
VMR3DECL(int)   VMR3PowerOn(PVM pVM);

/**
 * Suspend a running VM.
 *
 * @returns VBox status.
 * @param   pVM     VM to suspend.
 * @thread      Any thread.
 * @vmstate     Running
 * @vmstateto   Suspended
 */
VMR3DECL(int)   VMR3Suspend(PVM pVM);

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
VMR3DECL(int) VMR3SuspendNoSave(PVM pVM);

/**
 * Resume VM execution.
 *
 * @returns VBox status.
 * @param   pVM         The VM to resume.
 * @thread      Any thread.
 * @vmstate     Suspended
 * @vmstateto   Running
 */
VMR3DECL(int)   VMR3Resume(PVM pVM);

/**
 * Reset the current VM.
 *
 * @returns VBox status code.
 * @param   pVM     VM to reset.
 * @thread      Any thread.
 * @vmstate     Suspended, Running
 * @vmstateto   Unchanged state.
 */
VMR3DECL(int)   VMR3Reset(PVM pVM);

/**
 * Progress callback.
 * This will report the completion percentage of an operation.
 *
 * @returns VINF_SUCCESS.
 * @returns Error code to cancel the operation with.
 * @param   pVM         The VM handle.
 * @param   uPercent    Completetion precentage (0-100).
 * @param   pvUser      User specified argument.
 */
typedef DECLCALLBACK(int) FNVMPROGRESS(PVM pVM, unsigned uPercent, void *pvUser);
/** Pointer to a FNVMPROGRESS function. */
typedef FNVMPROGRESS *PFNVMPROGRESS;

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
VMR3DECL(int)   VMR3Save(PVM pVM, const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Load a new VM state.
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
VMR3DECL(int)   VMR3Load(PVM pVM, const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Power off a VM.
 *
 * @returns 0 on success.
 * @returns VBox error code on failure.
 * @param   pVM     VM which should be destroyed.
 * @thread      Any thread.
 * @vmstate     Suspended, Running, Guru Mediation, Load Failure
 * @vmstateto   Off
 */
VMR3DECL(int)   VMR3PowerOff(PVM pVM);

/**
 * Destroy a VM.
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
VMR3DECL(int)   VMR3Destroy(PVM pVM);

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
 * @thread  The emulation thread.
 */
VMR3DECL(void)   VMR3Relocate(PVM pVM, RTGCINTPTR offDelta);

/**
 * Enumerates the VMs in this process.
 *
 * @returns Pointer to the next VM.
 * @returns NULL when no more VMs.
 * @param   pVMPrev     The previous VM
 *                      Use NULL to start the enumeration.
 */
VMR3DECL(PVM)   VMR3EnumVMs(PVM pVMPrev);

/**
 * Wait for VM to be resumed. Handle events like vmR3EmulationThread does.
 * In case the VM is stopped, clean up and long jump to the main EMT loop.
 *
 * @returns VINF_SUCCESS or doesn't return
 * @param   pVM             VM handle.
 */
VMR3DECL(int) VMR3WaitForResume(PVM pVM);


/**
 * VM destruction callback.
 * @param   pVM     The VM which is about to be destroyed.
 * @param   pvUser  The user parameter specified at registration.
 */
typedef DECLCALLBACK(void) FNVMATDTOR(PVM pVM, void *pvUser);
/** Pointer to a VM destruction callback. */
typedef FNVMATDTOR *PFNVMATDTOR;

/**
 * Registers an at VM destruction callback.
 *
 * @returns VBox status code.
 * @param   pfnAtDtor       Pointer to callback.
 * @param   pvUser          User argument.
 */
VMR3DECL(int)   VMR3AtDtorRegister(PFNVMATDTOR pfnAtDtor, void *pvUser);

/**
 * Deregisters an at VM destruction callback.
 *
 * @returns VBox status code.
 * @param   pfnAtDtor       Pointer to callback.
 */
VMR3DECL(int)   VMR3AtDtorDeregister(PFNVMATDTOR pfnAtDtor);


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
VMR3DECL(int)   VMR3AtResetRegister(PVM pVM, PPDMDEVINS pDevInst, PFNVMATRESET pfnCallback, void *pvUser, const char *pszDesc);

/**
 * Registers an at VM reset internal callback.
 *
 * @returns VBox status code.
 * @param   pVM             The VM.
 * @param   pfnCallback     Callback function.
 * @param   pvUser          User argument.
 * @param   pszDesc         Description (optional).
 */
VMR3DECL(int)   VMR3AtResetRegisterInternal(PVM pVM, PFNVMATRESETINT pfnCallback, void *pvUser, const char *pszDesc);

/**
 * Registers an at VM reset external callback.
 *
 * @returns VBox status code.
 * @param   pVM             The VM.
 * @param   pfnCallback     Callback function.
 * @param   pvUser          User argument.
 * @param   pszDesc         Description (optional).
 */
VMR3DECL(int)   VMR3AtResetRegisterExternal(PVM pVM, PFNVMATRESETEXT pfnCallback, void *pvUser, const char *pszDesc);


/**
 * Deregisters an at VM reset callback.
 *
 * @returns VBox status code.
 * @param   pVM             The VM.
 * @param   pDevInst        Device instance.
 * @param   pfnCallback     Callback function.
 */
VMR3DECL(int)   VMR3AtResetDeregister(PVM pVM, PPDMDEVINS pDevInst, PFNVMATRESET pfnCallback);

/**
 * Deregisters an at VM reset internal callback.
 *
 * @returns VBox status code.
 * @param   pVM             The VM.
 * @param   pfnCallback     Callback function.
 */
VMR3DECL(int)   VMR3AtResetDeregisterInternal(PVM pVM, PFNVMATRESETINT pfnCallback);

/**
 * Deregisters an at VM reset external callback.
 *
 * @returns VBox status code.
 * @param   pVM             The VM.
 * @param   pfnCallback     Callback function.
 */
VMR3DECL(int)   VMR3AtResetDeregisterExternal(PVM pVM, PFNVMATRESETEXT pfnCallback);


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
VMR3DECL(int)   VMR3AtStateRegister(PVM pVM, PFNVMATSTATE pfnAtState, void *pvUser);

/**
 * Deregisters a VM state change callback.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pfnAtState      Pointer to callback.
 * @param   pvUser          User argument.
 * @thread  Any.
 */
VMR3DECL(int)   VMR3AtStateDeregister(PVM pVM, PFNVMATSTATE pfnAtState, void *pvUser);

/**
 * Gets the current VM state.
 *
 * @returns The current VM state.
 * @param   pVM             The VM handle.
 * @thread  Any
 */
VMR3DECL(VMSTATE) VMR3GetState(PVM pVM);

/**
 * Gets the state name string for a VM state.
 *
 * @returns Pointer to the state name. (readonly)
 * @param   enmState        The state.
 */
VMR3DECL(const char *) VMR3GetStateName(VMSTATE enmState);

/**
 * Registers a VM error callback.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pfnAtError      Pointer to callback.
 * @param   pvUser          User argument.
 * @thread  Any.
 */
VMR3DECL(int)   VMR3AtErrorRegister(PVM pVM, PFNVMATERROR pfnAtError, void *pvUser);

/**
 * Deregisters a VM error callback.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pfnAtError      Pointer to callback.
 * @param   pvUser          User argument.
 * @thread  Any.
 */
VMR3DECL(int)   VMR3AtErrorDeregister(PVM pVM, PFNVMATERROR pfnAtError, void *pvUser);

/**
 * This is a worker function for GC and Ring-0 calls to VMSetError and VMSetErrorV.
 * The message is found in VMINT.
 *
 * @param   pVM             The VM handle.
 * @thread  EMT.
 */
VMR3DECL(void) VMR3SetErrorWorker(PVM pVM);

/**
 * Registers a VM runtime error callback.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pfnAtRuntimeError   Pointer to callback.
 * @param   pvUser              User argument.
 * @thread  Any.
 */
VMR3DECL(int)   VMR3AtRuntimeErrorRegister(PVM pVM, PFNVMATRUNTIMEERROR pfnAtRuntimeError, void *pvUser);

/**
 * Deregisters a VM runtime error callback.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pfnAtRuntimeError   Pointer to callback.
 * @param   pvUser              User argument.
 * @thread  Any.
 */
VMR3DECL(int)   VMR3AtRuntimeErrorDeregister(PVM pVM, PFNVMATRUNTIMEERROR pfnAtRuntimeError, void *pvUser);

/**
 * This is a worker function for GC and Ring-0 calls to VMSetRuntimeError and VMSetRuntimeErrorV.
 * The message is found in VMINT.
 *
 * @param   pVM             The VM handle.
 * @thread  EMT.
 */
VMR3DECL(void) VMR3SetRuntimeErrorWorker(PVM pVM);


/**
 * Allocate and queue a call request.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use VMR3ReqWait() to check for completation. In the other case
 * use RT_INDEFINITE_WAIT.
 * The returned request packet must be freed using VMR3ReqFree().
 *
 * @returns VBox status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pVM             The VM handle.
 * @param   ppReq           Where to store the pointer to the request.
 *                          This will be NULL or a valid request pointer not matter what happends.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 * @param   pfnFunction     Pointer to the function to call.
 * @param   cArgs           Number of arguments following in the ellipsis.
 *                          Not possible to pass 64-bit arguments!
 * @param   ...             Function arguments.
 */
VMR3DECL(int) VMR3ReqCall(PVM pVM, PVMREQ *ppReq, unsigned cMillies, PFNRT pfnFunction, unsigned cArgs, ...);

/**
 * Allocate and queue a call request to a void function.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use VMR3ReqWait() to check for completation. In the other case
 * use RT_INDEFINITE_WAIT.
 * The returned request packet must be freed using VMR3ReqFree().
 *
 * @returns VBox status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pVM             The VM handle.
 * @param   ppReq           Where to store the pointer to the request.
 *                          This will be NULL or a valid request pointer not matter what happends.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 * @param   pfnFunction     Pointer to the function to call.
 * @param   cArgs           Number of arguments following in the ellipsis.
 *                          Not possible to pass 64-bit arguments!
 * @param   ...             Function arguments.
 */
VMR3DECL(int) VMR3ReqCallVoid(PVM pVM, PVMREQ *ppReq, unsigned cMillies, PFNRT pfnFunction, unsigned cArgs, ...);

/**
 * Allocate and queue a call request to a void function.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use VMR3ReqWait() to check for completation. In the other case
 * use RT_INDEFINITE_WAIT.
 * The returned request packet must be freed using VMR3ReqFree().
 *
 * @returns VBox status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pVM             The VM handle.
 * @param   ppReq           Where to store the pointer to the request.
 *                          This will be NULL or a valid request pointer not matter what happends, unless fFlags
 *                          contains VMREQFLAGS_NO_WAIT when it will be optional and always NULL.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 * @param   fFlags          A combination of the VMREQFLAGS values.
 * @param   pfnFunction     Pointer to the function to call.
 * @param   cArgs           Number of arguments following in the ellipsis.
 *                          Not possible to pass 64-bit arguments!
 * @param   ...             Function arguments.
 */
VMR3DECL(int) VMR3ReqCallEx(PVM pVM, PVMREQ *ppReq, unsigned cMillies, unsigned fFlags, PFNRT pfnFunction, unsigned cArgs, ...);

/**
 * Allocate and queue a call request.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use VMR3ReqWait() to check for completation. In the other case
 * use RT_INDEFINITE_WAIT.
 * The returned request packet must be freed using VMR3ReqFree().
 *
 * @returns VBox status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pVM             The VM handle.
 * @param   ppReq           Where to store the pointer to the request.
 *                          This will be NULL or a valid request pointer not matter what happends, unless fFlags
 *                          contains VMREQFLAGS_NO_WAIT when it will be optional and always NULL.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 * @param   fFlags          A combination of the VMREQFLAGS values.
 * @param   pfnFunction     Pointer to the function to call.
 * @param   cArgs           Number of arguments following in the ellipsis.
 *                          Not possible to pass 64-bit arguments!
 * @param   pvArgs          Pointer to function arguments.
 */
VMR3DECL(int) VMR3ReqCallV(PVM pVM, PVMREQ *ppReq, unsigned cMillies, unsigned fFlags, PFNRT pfnFunction, unsigned cArgs, va_list Args);

/**
 * Allocates a request packet.
 *
 * The caller allocates a request packet, fills in the request data
 * union and queues the request.
 *
 * @returns VBox status code.
 *
 * @param   pVM             VM handle.
 * @param   ppReq           Where to store the pointer to the allocated packet.
 * @param   enmType         Package type.
 */
VMR3DECL(int) VMR3ReqAlloc(PVM pVM, PVMREQ *ppReq, VMREQTYPE enmType);

/**
 * Free a request packet.
 *
 * @returns VBox status code.
 *
 * @param   pReq            Package to free.
 * @remark  The request packet must be in allocated or completed state!
 */
VMR3DECL(int) VMR3ReqFree(PVMREQ pReq);

/**
 * Queue a request.
 *
 * The quest must be allocated using VMR3ReqAlloc() and contain
 * all the required data.
 * If it's disired to poll on the completion of the request set cMillies
 * to 0 and use VMR3ReqWait() to check for completation. In the other case
 * use RT_INDEFINITE_WAIT.
 *
 * @returns VBox status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pReq            The request to queue.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 */
VMR3DECL(int) VMR3ReqQueue(PVMREQ pReq, unsigned cMillies);

/**
 * Wait for a request to be completed.
 *
 * @returns VBox status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pReq            The request to wait for.
 * @param   cMillies        Number of milliseconds to wait.
 *                          Use RT_INDEFINITE_WAIT to only wait till it's completed.
 */
VMR3DECL(int) VMR3ReqWait(PVMREQ pReq, unsigned cMillies);



/**
 * Process pending request(s).
 *
 * This function is called from a forced action handler in
 * the EMT.
 *
 * @returns VBox status code.
 *
 * @param   pVM         VM handle.
 */
VMR3DECL(int) VMR3ReqProcess(PVM pVM);


/**
 * Notify the emulation thread (EMT) about pending Forced Action (FF).
 *
 * This function is called by thread other than EMT to make
 * sure EMT wakes up and promptly service a FF request.
 *
 * @param   pVM             VM handle.
 * @param   fNotifiedREM    Set if REM have already been notified. If clear the
 *                          generic REMR3NotifyFF() method is called.
 */
VMR3DECL(void) VMR3NotifyFF(PVM pVM, bool fNotifiedREM);

/**
 * Halted VM Wait.
 * Any external event will unblock the thread.
 *
 * @returns VINF_SUCCESS unless a fatal error occured. In the latter
 *          case an appropriate status code is returned.
 * @param   pVM                 VM handle.
 * @param   fIgnoreInterrupts   If set the VM_FF_INTERRUPT flags is ignored.
 * @thread  The emulation thread.
 */
VMR3DECL(int) VMR3WaitHalted(PVM pVM, bool fIgnoreInterrupts);

/**
 * Suspended VM Wait.
 * Only a handful of forced actions will cause the function to
 * return to the caller.
 *
 * @returns VINF_SUCCESS unless a fatal error occured. In the latter
 *          case an appropriate status code is returned.
 * @param   pVM         VM handle.
 * @thread  The emulation thread.
 */
VMR3DECL(int) VMR3Wait(PVM pVM);

/** @} */
#endif /* IN_RING3 */


#ifdef IN_GC
/** @defgroup grp_vmm_apis_gc  VM Guest Context APIs
 * @ingroup grp_vm
 * @{ */

/** @} */
#endif

__END_DECLS

/** @} */

#endif


