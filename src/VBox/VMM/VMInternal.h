/* $Id$ */
/** @file
 * VM - Internal header file.
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

#ifndef ___VMInternal_h
#define ___VMInternal_h

#include <VBox/cdefs.h>
#include <VBox/vmapi.h>
#include <setjmp.h>

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
 * VM runtime error callback.
 */
typedef struct VMATRUNTIMEERROR
{
    /** Pointer to the next one. */
    struct VMATRUNTIMEERROR *pNext;
    /** Pointer to the callback. */
    PFNVMATRUNTIMEERROR      pfnAtRuntimeError;
    /** The user argument. */
    void                    *pvUser;
} VMATRUNTIMEERROR;
/** Pointer to a VM error callback. */
typedef VMATRUNTIMEERROR *PVMATRUNTIMEERROR;


/**
 * Chunk of memory allocated off the hypervisor heap in which
 * we copy the runtime error details.
 */
typedef struct VMRUNTIMEERROR
{
    /** The size of the chunk. */
    uint32_t                cbAllocated;
    /** The current offset into the chunk.
     * We start by putting the error ID immediatly
     * after the end of the buffer. */
    uint32_t                off;
    /** Offset from the start of this structure to the error ID. */
    uint32_t                offErrorID;
    /** Offset from the start of this structure to the formatted message text. */
    uint32_t                offMessage;
    /** Whether the error is fatal or not */
    bool                    fFatal;
} VMRUNTIMEERROR, *PVMRUNTIMEERROR;

/** The halt method. */
typedef enum
{
    /** The usual invalid value. */
    VMHALTMETHOD_INVALID = 0,
    /** Use the method used during bootstrapping. */
    VMHALTMETHOD_BOOTSTRAP,
    /** Use the default method. */
    VMHALTMETHOD_DEFAULT,
    /** The old spin/yield/block method. */
    VMHALTMETHOD_OLD,
    /** The first go at a block/spin method. */
    VMHALTMETHOD_1,
    /** The first go at a more global approach. */
    VMHALTMETHOD_GLOBAL_1,
    /** The end of valid methods. (not inclusive of course) */
    VMHALTMETHOD_END,
    /** The usual 32-bit max value. */
    VMHALTMETHOD_32BIT_HACK = 0x7fffffff
} VMHALTMETHOD;


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
    /** VM Error Message. */
    R3PTRTYPE(PVMERROR)             pErrorR3;
    /** VM Runtime Error Message. */
    R3PTRTYPE(PVMRUNTIMEERROR)      pRuntimeErrorR3;
    /** Set by VMR3SuspendNoSave; cleared by VMR3Resume; signals the VM is in an inconsistent state and saving is not allowed. */
    bool                            fPreventSaveState;
} VMINT, *PVMINT;


/**
 * VM internal data kept in the UVM.
 */
typedef struct VMINTUSERPERVM
{
    /** Head of the request queue. Atomic. */
    volatile PVMREQ                 pReqs;
    /** The last index used during alloc/free. */
    volatile uint32_t               iReqFree;
    /** Number of free request packets. */
    volatile uint32_t               cReqFree;
    /** Array of pointers to lists of free request packets. Atomic. */
    volatile PVMREQ                 apReqFree[9];

#ifdef VBOX_WITH_STATISTICS
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
#endif

    /** Pointer to the support library session.
     * Mainly for creation and destruction.. */
    PSUPDRVSESSION                  pSession;

    /** The handle to the EMT thread. */
    RTTHREAD                        ThreadEMT;
    /** The native of the EMT thread. */
    RTNATIVETHREAD                  NativeThreadEMT;
    /** Wait event semaphore. */
    RTSEMEVENT                      EventSemWait;
    /** Wait/Idle indicator. */
    bool volatile                   fWait;
    /** Force EMT to terminate. */
    bool volatile                   fTerminateEMT;
    /** If set the EMT does the final VM cleanup when it exits.
     * If clear the VMR3Destroy() caller does so. */
    bool                            fEMTDoesTheCleanup;

    /** @name Generic Halt data
     * @{
     */
    /** The current halt method.
     * Can be selected by CFGM option 'VM/HaltMethod'. */
    VMHALTMETHOD                    enmHaltMethod;
    /** The index into g_aHaltMethods of the current halt method. */
    uint32_t volatile               iHaltMethod;
    /** The average time (ns) between two halts in the last second. (updated once per second) */
    uint32_t                        HaltInterval;
    /** The average halt frequency for the last second. (updated once per second) */
    uint32_t                        HaltFrequency;
    /** The number of halts in the current period. */
    uint32_t                        cHalts;
    uint32_t                        padding; /**< alignment padding. */
    /** When we started counting halts in cHalts (RTTimeNanoTS). */
    uint64_t                        u64HaltsStartTS;
    /** @} */

    /** Union containing data and config for the different halt algorithms. */
    union
    {
       /**
        * Method 1 & 2 - Block whenever possible, and when lagging behind
        * switch to spinning with regular blocking every 5-200ms (defaults)
        * depending on the accumulated lag. The blocking interval is adjusted
        * with the average oversleeping of the last 64 times.
        *
        * The difference between 1 and 2 is that we use native absolute
        * time APIs for the blocking instead of the millisecond based IPRT
        * interface.
        */
        struct
        {
            /** How many times we've blocked while cBlockedNS and cBlockedTooLongNS has been accumulating. */
            uint32_t                cBlocks;
            /** Avg. time spend oversleeping when blocking. (Re-calculated every so often.) */
            uint64_t                cNSBlockedTooLongAvg;
            /** Total time spend oversleeping when blocking. */
            uint64_t                cNSBlockedTooLong;
            /** Total time spent blocking. */
            uint64_t                cNSBlocked;
            /** The timestamp (RTTimeNanoTS) of the last block. */
            uint64_t                u64LastBlockTS;

            /** When we started spinning relentlessly in order to catch up some of the oversleeping.
             * This is 0 when we're not spinning. */
            uint64_t                u64StartSpinTS;

            /** The max interval without blocking (when spinning). */
            uint32_t                u32MinBlockIntervalCfg;
            /** The minimum interval between blocking (when spinning). */
            uint32_t                u32MaxBlockIntervalCfg;
            /** The value to divide the current lag by to get the raw blocking interval (when spinning). */
            uint32_t                u32LagBlockIntervalDivisorCfg;
            /** When to start spinning (lag / nano secs). */
            uint32_t                u32StartSpinningCfg;
            /** When to stop spinning (lag / nano secs). */
            uint32_t                u32StopSpinningCfg;
        }                           Method12;

#if 0
       /**
        * Method 3 & 4 - Same as method 1 & 2 respectivly, except that we
        * sprinkle it with yields.
        */
       struct
       {
           /** How many times we've blocked while cBlockedNS and cBlockedTooLongNS has been accumulating. */
           uint32_t                 cBlocks;
           /** Avg. time spend oversleeping when blocking. (Re-calculated every so often.) */
           uint64_t                 cBlockedTooLongNSAvg;
           /** Total time spend oversleeping when blocking. */
           uint64_t                 cBlockedTooLongNS;
           /** Total time spent blocking. */
           uint64_t                 cBlockedNS;
           /** The timestamp (RTTimeNanoTS) of the last block. */
           uint64_t                 u64LastBlockTS;

           /** How many times we've yielded while cBlockedNS and cBlockedTooLongNS has been accumulating. */
           uint32_t                 cYields;
           /** Avg. time spend oversleeping when yielding. */
           uint32_t                 cYieldTooLongNSAvg;
           /** Total time spend oversleeping when yielding. */
           uint64_t                 cYieldTooLongNS;
           /** Total time spent yielding. */
           uint64_t                 cYieldedNS;
           /** The timestamp (RTTimeNanoTS) of the last block. */
           uint64_t                 u64LastYieldTS;

           /** When we started spinning relentlessly in order to catch up some of the oversleeping. */
           uint64_t                 u64StartSpinTS;
       }                            Method34;
#endif
    }                               Halt;

    /** Profiling the halted state; yielding vs blocking.
     * @{ */
    STAMPROFILE                     StatHaltYield;
    STAMPROFILE                     StatHaltBlock;
    STAMPROFILE                     StatHaltTimers;
    STAMPROFILE                     StatHaltPoll;
    /** @} */


    /** List of registered reset callbacks. */
    PVMATRESET                      pAtReset;
    /** List of registered reset callbacks. */
    PVMATRESET                     *ppAtResetNext;

    /** List of registered state change callbacks. */
    PVMATSTATE                      pAtState;
    /** List of registered state change callbacks. */
    PVMATSTATE                     *ppAtStateNext;

    /** List of registered error callbacks. */
    PVMATERROR                      pAtError;
    /** List of registered error callbacks. */
    PVMATERROR                     *ppAtErrorNext;

    /** List of registered error callbacks. */
    PVMATRUNTIMEERROR               pAtRuntimeError;
    /** List of registered error callbacks. */
    PVMATRUNTIMEERROR              *ppAtRuntimeErrorNext;

    /** Pointer to the DBGC instance data. */
    void                           *pvDBGC;


    /** vmR3EmulationThread longjmp buffer. Must be last in the structure. */
    jmp_buf                         emtJumpEnv;
} VMINTUSERPERVM;

/** Pointer to the VM internal data kept in the UVM. */
typedef VMINTUSERPERVM *PVMINTUSERPERVM;


DECLCALLBACK(int) vmR3EmulationThread(RTTHREAD ThreadSelf, void *pvArg);
int vmR3SetHaltMethodU(PUVM pUVM, VMHALTMETHOD enmHaltMethod);
DECLCALLBACK(int) vmR3Destroy(PVM pVM);
DECLCALLBACK(void) vmR3SetErrorUV(PUVM pUVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list *args);
void vmSetErrorCopy(PVM pVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list args);
DECLCALLBACK(void) vmR3SetRuntimeErrorV(PVM pVM, bool fFatal, const char *pszErrorID, const char *pszFormat, va_list *args);
void vmSetRuntimeErrorCopy(PVM pVM, bool fFatal, const char *pszErrorID, const char *pszFormat, va_list args);
void vmR3DestroyFinalBitFromEMT(PUVM pUVM);
void vmR3SetState(PVM pVM, VMSTATE enmStateNew);


/** @} */

#endif

