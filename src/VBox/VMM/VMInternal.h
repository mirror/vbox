/** @file
 *
 * VM - Internal header file.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __VMInternal_h__
#define __VMInternal_h__

#include <VBox/cdefs.h>
#include <VBox/vmapi.h>


#if !defined(IN_VM_R3) && !defined(IN_VM_R0) && !defined(IN_VM_GC)
# error "Not in VM! This is an internal header!"
#endif


/** @defgroup grp_vm_int   Internals
 * @ingroup grp_vm
 * @internal
 * @{
 */


/**
 * At-reset callback type.
 */
typedef enum VMATRESETTYPE
{
    /** Device callback. */
    VMATRESETTYPE_DEV = 1,
    /** Internal callback . */
    VMATRESETTYPE_INTERNAL,
    /** External callback. */
    VMATRESETTYPE_EXTERNAL
} VMATRESETTYPE;


/** Pointer to at-reset callback. */
typedef struct VMATRESET *PVMATRESET;

/**
 * At reset callback.
 */
typedef struct VMATRESET
{
    /** Pointer to the next one in the list. */
    PVMATRESET      pNext;
    /** Callback type. */
    VMATRESETTYPE   enmType;
    /** User argument for the callback. */
    void           *pvUser;
    /** Description. */
    const char     *pszDesc;
    /** Type specific data. */
    union
    {
        /** VMATRESETTYPE_DEV. */
        struct
        {
            /** Callback. */
            PFNVMATRESET    pfnCallback;
            /** Device instance. */
            PPDMDEVINS      pDevIns;
        } Dev;

        /** VMATRESETTYPE_INTERNAL. */
        struct
        {
            /** Callback. */
            PFNVMATRESETINT pfnCallback;
        } Internal;

        /** VMATRESETTYPE_EXTERNAL. */
        struct
        {
            /** Callback. */
            PFNVMATRESETEXT pfnCallback;
        } External;
    } u;
} VMATRESET;


/**
 * VM state change callback.
 */
typedef struct VMATSTATE
{
    /** Pointer to the next one. */
    struct VMATSTATE   *pNext;
    /** Pointer to the callback. */
    PFNVMATSTATE        pfnAtState;
    /** The user argument. */
    void               *pvUser;
} VMATSTATE;
/** Pointer to a VM state change callback. */
typedef VMATSTATE *PVMATSTATE;


/**
 * VM error callback.
 */
typedef struct VMATERROR
{
    /** Pointer to the next one. */
    struct VMATERROR   *pNext;
    /** Pointer to the callback. */
    PFNVMATERROR        pfnAtError;
    /** The user argument. */
    void               *pvUser;
} VMATERROR;
/** Pointer to a VM error callback. */
typedef VMATERROR *PVMATERROR;


/**
 * Chunk of memory allocated off the hypervisor heap in which
 * we copy the error details.
 */
typedef struct VMERROR
{
    /** The size of the chunk. */
    uint32_t                cbAllocated;
    /** The current offset into the chunk.
     * We start by putting the filename and function immediatly
     * after the end of the buffer. */
    uint32_t                off;
    /** Offset from the start of this structure to the file name. */
    uint32_t                offFile;
    /** The line number. */
    uint32_t                iLine;
    /** Offset from the start of this structure to the function name. */
    uint32_t                offFunction;
    /** Offset from the start of this structure to the formatted message text. */
    uint32_t                offMessage;
    /** The VBox status code. */
    int32_t                 rc;
} VMERROR, *PVMERROR;


/**
 * Converts a VMM pointer into a VM pointer.
 * @returns Pointer to the VM structure the VMM is part of.
 * @param   pVMM   Pointer to VMM instance data.
 */
#define VMINT2VM(pVMM)  ( (PVM)((char*)pVMM - pVMM->offVM) )


/**
 * VM Internal Data (part of VM)
 */
typedef struct VMINT
{
    /** Offset to the VM structure.
     * See VMINT2VM(). */
    RTINT                           offVM;

    /** List of registered reset callbacks. */
    HCPTRTYPE(PVMATRESET)           pAtReset;
    /** List of registered reset callbacks. */
    HCPTRTYPE(PVMATRESET *)         ppAtResetNext;

    /** List of registered state change callbacks. */
    HCPTRTYPE(PVMATSTATE)           pAtState;
    /** List of registered state change callbacks. */
    HCPTRTYPE(PVMATSTATE *)         ppAtStateNext;

    /** List of registered error callbacks. */
    HCPTRTYPE(PVMATERROR)           pAtError;
    /** List of registered error callbacks. */
    HCPTRTYPE(PVMATERROR *)         ppAtErrorNext;

    /** Head of the request queue. Atomic. */
    volatile HCPTRTYPE(PVMREQ)      pReqs;
    /** The last index used during alloc/free. */
    volatile uint32_t               iReqFree;
    /** Array of pointers to lists of free request packets. Atomic. */
    volatile HCPTRTYPE(PVMREQ)      apReqFree[9];
    /** Number of free request packets. */
    volatile uint32_t               cReqFree;

    /** Wait/Idle indicator. */
    volatile uint32_t               fWait;
    /** Wait event semaphore. */
    HCPTRTYPE(RTSEMEVENT)           EventSemWait;

    /** VM Error Message. */
    R3PTRTYPE(PVMERROR)             pErrorR3;

    /** Pointer to the DBGC instance data. */
    HCPTRTYPE(void *)               pvDBGC;

    /** If set the EMT does the final VM cleanup when it exits.
     * If clear the VMR3Destroy() caller does so. */
    bool                            fEMTDoesTheCleanup;

    /** Number of VMR3ReqAlloc returning a new packet. */
    STAMCOUNTER                     StatReqAllocNew;
    /** Number of VMR3ReqAlloc causing races. */
    STAMCOUNTER                     StatReqAllocRaces;
    /** Number of VMR3ReqAlloc returning a recycled packet. */
    STAMCOUNTER                     StatReqAllocRecycled;
    /** Number of VMR3ReqFree calls. */
    STAMCOUNTER                     StatReqFree;
    /** Number of times the request was actually freed. */
    STAMCOUNTER                     StatReqFreeOverflow;

    /** Profiling the halted state; yielding vs blocking. */
    STAMPROFILEADV                  StatHaltYield;
    STAMPROFILEADV                  StatHaltBlock;
    STAMPROFILEADV                  StatHaltTimers;
    STAMPROFILEADV                  StatHaltPoll;
} VMINT, *PVMINT;


/**
 * Emulation thread arguments.
 */
typedef struct VMEMULATIONTHREADARGS
{
    /** Pointer to the VM structure. */
    PVM     pVM;
} VMEMULATIONTHREADARGS;
/** Pointer to the emulation thread arguments. */
typedef VMEMULATIONTHREADARGS *PVMEMULATIONTHREADARGS;

DECLCALLBACK(int) vmR3EmulationThread(RTTHREAD ThreadSelf, void *pvArg);
DECLCALLBACK(int) vmR3Destroy(PVM pVM);
DECLCALLBACK(void) vmR3SetErrorV(PVM pVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list *args);
void vmSetErrorCopy(PVM pVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list args);
void vmR3DestroyFinalBit(PVM pVM);


/** @} */

#endif

