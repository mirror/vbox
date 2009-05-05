/** @file
 * VM - The Virtual Machine, API.
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

#ifndef ___VBox_vmapi_h
#define ___VBox_vmapi_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/stam.h>
#include <VBox/cfgm.h>

#include <iprt/stdarg.h>

__BEGIN_DECLS

/** @defgroup grp_vmm_apis  VM All Contexts API
 * @ingroup grp_vm
 * @{ */

/** @def VM_RC_ADDR
 * Converts a current context address of data within the VM structure to the equivalent
 * raw-mode address.
 *
 * @returns raw-mode virtual address.
 * @param   pVM     Pointer to the VM.
 * @param   pvInVM  CC Pointer within the VM.
 */
#ifdef IN_RING3
# define VM_RC_ADDR(pVM, pvInVM)        ( (RTRCPTR)((RTRCUINTPTR)pVM->pVMRC + (uint32_t)((uintptr_t)(pvInVM) - (uintptr_t)pVM->pVMR3)) )
#elif defined(IN_RING0)
# define VM_RC_ADDR(pVM, pvInVM)        ( (RTRCPTR)((RTRCUINTPTR)pVM->pVMRC + (uint32_t)((uintptr_t)(pvInVM) - (uintptr_t)pVM->pVMR0)) )
#else
# define VM_RC_ADDR(pVM, pvInVM)        ( (RTRCPTR)(pvInVM) )
#endif

/** @def VM_R3_ADDR
 * Converts a current context address of data within the VM structure to the equivalent
 * ring-3 host address.
 *
 * @returns host virtual address.
 * @param   pVM     Pointer to the VM.
 * @param   pvInVM  CC pointer within the VM.
 */
#ifdef IN_RC
# define VM_R3_ADDR(pVM, pvInVM)       ( (RTR3PTR)((RTR3UINTPTR)pVM->pVMR3 + (uint32_t)((uintptr_t)(pvInVM) - (uintptr_t)pVM->pVMRC)) )
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
#ifdef IN_RC
# define VM_R0_ADDR(pVM, pvInVM)       ( (RTR0PTR)((RTR0UINTPTR)pVM->pVMR0 + (uint32_t)((uintptr_t)(pvInVM) - (uintptr_t)pVM->pVMRC)) )
#elif defined(IN_RING3)
# define VM_R0_ADDR(pVM, pvInVM)       ( (RTR0PTR)((RTR0UINTPTR)pVM->pVMR0 + (uint32_t)((uintptr_t)(pvInVM) - (uintptr_t)pVM->pVMR3)) )
#else
# define VM_R0_ADDR(pVM, pvInVM)       ( (RTR0PTR)(pvInVM) )
#endif



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

VMMDECL(int) VMSetError(PVM pVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...);
VMMDECL(int) VMSetErrorV(PVM pVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list args);

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
 *
 * See VMSetRuntimeError for the detailed description of parameters.
 *
 * @param   pVM             The VM handle.
 * @param   pvUser          The user argument.
 * @param   fFlags          The error flags.
 * @param   pszErrorId      Error ID string.
 * @param   pszFormat       Error message format string.
 * @param   va              Error message arguments.
 */
typedef DECLCALLBACK(void) FNVMATRUNTIMEERROR(PVM pVM, void *pvUser, uint32_t fFlags, const char *pszErrorId,
                                              const char *pszFormat, va_list va);
/** Pointer to a VM runtime error callback. */
typedef FNVMATRUNTIMEERROR *PFNVMATRUNTIMEERROR;

VMMDECL(int) VMSetRuntimeError(PVM pVM, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, ...);
VMMDECL(int) VMSetRuntimeErrorV(PVM pVM, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, va_list args);

/** @name VMSetRuntimeError fFlags
 * When no flags are given the VM will continue running and it's up to the front
 * end to take action on the error condition.
 *
 * @{ */
/** The error is fatal.
 * The VM is not in a state where it can be saved and will enter a state
 * where it can no longer execute code. The caller <b>must</b> propagate status
 * codes. */
#define VMSETRTERR_FLAGS_FATAL      RT_BIT_32(0)
/** Suspend the VM after, or if possible before, raising the error on EMT. The
 * caller <b>must</b> propagate status codes. */
#define VMSETRTERR_FLAGS_SUSPEND    RT_BIT_32(1)
/** Don't wait for the EMT to handle the request.
 * Only valid when on a worker thread and there is a high risk of a dead
 * lock. Be careful not to flood the user with errors. */
#define VMSETRTERR_FLAGS_NO_WAIT    RT_BIT_32(2)
/** @} */


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

VMMDECL(const char *) VMGetStateName(VMSTATE enmState);


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
    VMREQFLAGS_NO_WAIT      = 2,
    /** Poke the destination EMT(s) if executing guest code. Use with care. */
    VMREQFLAGS_POKE         = 4
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
    /** Pointer to ring-3 VM structure which this request belongs to. */
    PUVM                    pUVM;
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
    /** Request destination. */
    VMCPUID                 idDstCpu;
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


#ifndef IN_RC
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


VMMR3DECL(int)  VMR3Create(uint32_t cCPUs, PFNVMATERROR pfnVMAtError, void *pvUserVM, PFNCFGMCONSTRUCTOR pfnCFGMConstructor, void *pvUserCFGM, PVM *ppVM);
VMMR3DECL(int)  VMR3PowerOn(PVM pVM);
VMMR3DECL(int)  VMR3Suspend(PVM pVM);
VMMR3DECL(int)  VMR3SuspendNoSave(PVM pVM);
VMMR3DECL(int)  VMR3Resume(PVM pVM);
VMMR3DECL(int)  VMR3Reset(PVM pVM);

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

VMMR3DECL(int)  VMR3Save(PVM pVM, const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser);
VMMR3DECL(int)  VMR3Load(PVM pVM, const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser);
VMMR3DECL(int)  VMR3PowerOff(PVM pVM);
VMMR3DECL(int)  VMR3Destroy(PVM pVM);
VMMR3DECL(void) VMR3Relocate(PVM pVM, RTGCINTPTR offDelta);
VMMR3DECL(PVM)  VMR3EnumVMs(PVM pVMPrev);
VMMR3DECL(int)  VMR3WaitForResume(PVM pVM);

/**
 * VM destruction callback.
 * @param   pVM     The VM which is about to be destroyed.
 * @param   pvUser  The user parameter specified at registration.
 */
typedef DECLCALLBACK(void) FNVMATDTOR(PVM pVM, void *pvUser);
/** Pointer to a VM destruction callback. */
typedef FNVMATDTOR *PFNVMATDTOR;

VMMR3DECL(int)  VMR3AtDtorRegister(PFNVMATDTOR pfnAtDtor, void *pvUser);
VMMR3DECL(int)  VMR3AtDtorDeregister(PFNVMATDTOR pfnAtDtor);
VMMR3DECL(int)  VMR3AtResetRegister(PVM pVM, PPDMDEVINS pDevInst, PFNVMATRESET pfnCallback, void *pvUser, const char *pszDesc);
VMMR3DECL(int)  VMR3AtResetRegisterInternal(PVM pVM, PFNVMATRESETINT pfnCallback, void *pvUser, const char *pszDesc);
VMMR3DECL(int)  VMR3AtResetRegisterExternal(PVM pVM, PFNVMATRESETEXT pfnCallback, void *pvUser, const char *pszDesc);
VMMR3DECL(int)  VMR3AtResetDeregister(PVM pVM, PPDMDEVINS pDevInst, PFNVMATRESET pfnCallback);
VMMR3DECL(int)  VMR3AtResetDeregisterInternal(PVM pVM, PFNVMATRESETINT pfnCallback);
VMMR3DECL(int)  VMR3AtResetDeregisterExternal(PVM pVM, PFNVMATRESETEXT pfnCallback);
VMMR3DECL(int)  VMR3AtStateRegister(PVM pVM, PFNVMATSTATE pfnAtState, void *pvUser);
VMMR3DECL(int)  VMR3AtStateDeregister(PVM pVM, PFNVMATSTATE pfnAtState, void *pvUser);
VMMR3DECL(VMSTATE) VMR3GetState(PVM pVM);
VMMR3DECL(const char *) VMR3GetStateName(VMSTATE enmState);
VMMR3DECL(int)  VMR3AtErrorRegister(PVM pVM, PFNVMATERROR pfnAtError, void *pvUser);
VMMR3DECL(int)  VMR3AtErrorRegisterU(PUVM pVM, PFNVMATERROR pfnAtError, void *pvUser);
VMMR3DECL(int)  VMR3AtErrorDeregister(PVM pVM, PFNVMATERROR pfnAtError, void *pvUser);
VMMR3DECL(void) VMR3SetErrorWorker(PVM pVM);
VMMR3DECL(int)  VMR3AtRuntimeErrorRegister(PVM pVM, PFNVMATRUNTIMEERROR pfnAtRuntimeError, void *pvUser);
VMMR3DECL(int)  VMR3AtRuntimeErrorDeregister(PVM pVM, PFNVMATRUNTIMEERROR pfnAtRuntimeError, void *pvUser);
VMMR3DECL(int)  VMR3SetRuntimeErrorWorker(PVM pVM);
VMMR3DECL(int)  VMR3ReqCall(PVM pVM, VMCPUID idDstCpu, PVMREQ *ppReq, unsigned cMillies, PFNRT pfnFunction, unsigned cArgs, ...);
VMMR3DECL(int)  VMR3ReqCallVoidU(PUVM pUVM, VMCPUID idDstCpu, PVMREQ *ppReq, unsigned cMillies, PFNRT pfnFunction, unsigned cArgs, ...);
VMMR3DECL(int)  VMR3ReqCallVoid(PVM pVM, VMCPUID idDstCpu, PVMREQ *ppReq, unsigned cMillies, PFNRT pfnFunction, unsigned cArgs, ...);
VMMR3DECL(int)  VMR3ReqCallEx(PVM pVM, VMCPUID idDstCpu, PVMREQ *ppReq, unsigned cMillies, unsigned fFlags, PFNRT pfnFunction, unsigned cArgs, ...);
VMMR3DECL(int)  VMR3ReqCallU(PUVM pUVM, VMCPUID idDstCpu, PVMREQ *ppReq, unsigned cMillies, unsigned fFlags, PFNRT pfnFunction, unsigned cArgs, ...);
VMMR3DECL(int)  VMR3ReqCallVU(PUVM pUVM, VMCPUID idDstCpu, PVMREQ *ppReq, unsigned cMillies, unsigned fFlags, PFNRT pfnFunction, unsigned cArgs, va_list Args);
VMMR3DECL(int)  VMR3ReqAlloc(PVM pVM, PVMREQ *ppReq, VMREQTYPE enmType, VMCPUID idDstCpu);
VMMR3DECL(int)  VMR3ReqAllocU(PUVM pUVM, PVMREQ *ppReq, VMREQTYPE enmType, VMCPUID idDstCpu);
VMMR3DECL(int)  VMR3ReqFree(PVMREQ pReq);
VMMR3DECL(int)  VMR3ReqQueue(PVMREQ pReq, unsigned cMillies);
VMMR3DECL(int)  VMR3ReqWait(PVMREQ pReq, unsigned cMillies);
VMMR3DECL(int)  VMR3ReqProcessU(PUVM pUVM, VMCPUID idDstCpu);
VMMR3DECL(void) VMR3NotifyGlobalFFU(PUVM pUVM, uint32_t fFlags);
VMMR3DECL(void) VMR3NotifyCpuFFU(PUVMCPU pUVMCpu, uint32_t fFlags);
/** @name Flags for VMR3NotifyCpuFFU and VMR3NotifyGlobalFFU.
 * @{ */
/** Whether we've done REM or not. */
#define VMNOTIFYFF_FLAGS_DONE_REM   RT_BIT_32(0)
/** Whether we should poke the CPU if it's executing guest code. */
#define VMNOTIFYFF_FLAGS_POKE       RT_BIT_32(1)
/** @} */

VMMR3DECL(int)  VMR3WaitHalted(PVM pVM, PVMCPU pVCpu, bool fIgnoreInterrupts);
VMMR3DECL(int)  VMR3WaitU(PUVMCPU pUVMCpu);
VMMR3DECL(RTCPUID)          VMR3GetVMCPUId(PVM pVM);
VMMR3DECL(RTTHREAD)         VMR3GetVMCPUThread(PVM pVM);
VMMR3DECL(RTTHREAD)         VMR3GetVMCPUThreadU(PUVM pUVM);
VMMR3DECL(RTNATIVETHREAD)   VMR3GetVMCPUNativeThread(PVM pVM);
VMMR3DECL(RTNATIVETHREAD)   VMR3GetVMCPUNativeThreadU(PUVM pUVM);

/** @} */
#endif /* IN_RING3 */


#ifdef IN_RC
/** @defgroup grp_vmm_apis_gc  VM Guest Context APIs
 * @ingroup grp_vm
 * @{ */

/** @} */
#endif

__END_DECLS

/** @} */

#endif


