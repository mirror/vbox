/** @file
 * IPRT - Lock Validator.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#ifndef ___iprt_lockvalidator_h
#define ___iprt_lockvalidator_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/assert.h>
#include <iprt/thread.h>


/** @defgroup grp_ldr       RTLockValidator - Lock Validator
 * @ingroup grp_rt
 * @{
 */

RT_C_DECLS_BEGIN

/**
 * Record recording the ownership of a lock.
 *
 * This is typically part of the per-lock data structure when compiling with
 * the lock validator.
 */
typedef struct RTLOCKVALIDATORREC
{
    /** Magic value (RTLOCKVALIDATORREC_MAGIC). */
    uint32_t                            u32Magic;
    /** The line number in the file. */
    uint32_t volatile                   uLine;
    /** The file where the lock was taken. */
    R3R0PTRTYPE(const char * volatile)  pszFile;
    /** The function where the lock was taken. */
    R3R0PTRTYPE(const char * volatile)  pszFunction;
    /** Some ID indicating where the lock was taken, typically an address. */
    RTHCUINTPTR volatile                uId;
    /** The current owner thread. */
    RTTHREAD volatile                   hThread;
    /** Pointer to the lock record below us. Only accessed by the owner. */
    R3R0PTRTYPE(PRTLOCKVALIDATORREC)    pDown;
    /** Recursion count */
    uint32_t                            cRecursion;
    /** The lock sub-class. */
    uint32_t volatile                   uSubClass;
    /** The lock class. */
    RTLOCKVALIDATORCLASS                hClass;
    /** Pointer to the lock. */
    RTHCPTR                             hLock;
    /** The lock name. */
    R3R0PTRTYPE(const char *)           pszName;
} RTLOCKVALIDATORREC;
AssertCompileSize(RTLOCKVALIDATORREC, HC_ARCH_BITS == 32 ? 48 : 80);
/* The pointer is defined in iprt/types.h */


/** @name   Special sub-class values.
 * The range 16..UINT32_MAX is available to the user, the range 0..15 is
 * reserved for the lock validator.
 * @{ */
/** Not allowed to be taken with any other locks in the same class.
  * This is the recommended value.  */
#define RTLOCKVALIDATOR_SUB_CLASS_NONE  UINT32_C(0)
/** Any order is allowed within the class. */
#define RTLOCKVALIDATOR_SUB_CLASS_ANY   UINT32_C(1)
/** The first user value. */
#define RTLOCKVALIDATOR_SUB_CLASS_USER  UINT32_C(16)
/** @} */

/**
 * Initialize a lock validator record.
 *
 * Use RTLockValidatorDelete to deinitialize it.
 *
 * @param   pRec                The record.
 * @param   hClass              The class. If NIL, the no lock order
 *                              validation will be performed on this lock.
 * @param   uSubClass           The sub-class.  This is used to define lock
 *                              order inside the same class.  If you don't know,
 *                              then pass RTLOCKVALIDATOR_SUB_CLASS_NONE.
 * @param   pszName             The lock name (optional).
 * @param   hLock               The lock handle.
 */
RTDECL(void) RTLockValidatorInit(PRTLOCKVALIDATORREC pRec, RTLOCKVALIDATORCLASS hClass,
                                 uint32_t uSubClass, const char *pszName, void *hLock);
/**
 * Uninitialize a lock validator record previously initialized by
 * RTLockValidatorInit.
 *
 * @param   pRec                The record.  Must be valid.
 */
RTDECL(void) RTLockValidatorDelete(PRTLOCKVALIDATORREC pRec);

/**
 * Create and initialize a lock validator record.
 *
 * Use RTLockValidatorDestroy to deinitialize and destroy the returned record.
 *
 * @return VINF_SUCCESS or VERR_NO_MEMORY.
 * @param   ppRec               Where to return the record pointer.
 * @param   hClass              The class. If NIL, the no lock order
 *                              validation will be performed on this lock.
 * @param   uSubClass           The sub-class.  This is used to define lock
 *                              order inside the same class.  If you don't know,
 *                              then pass RTLOCKVALIDATOR_SUB_CLASS_NONE.
 * @param   pszName             The lock name (optional).
 * @param   hLock               The lock handle.
 */
RTDECL(int)  RTLockValidatorCreate(PRTLOCKVALIDATORREC *ppRec, RTLOCKVALIDATORCLASS hClass,
                                   uint32_t uSubClass, const char *pszName, void *hLock);

/**
 * Deinitialize and destroy a record created by RTLockValidatorCreate.
 *
 * @param   ppRec               Pointer to the record pointer.  Will be set to
 *                              NULL.
 */
RTDECL(void) RTLockValidatorDestroy(PRTLOCKVALIDATORREC *ppRec);

/**
 * Check the locking order.
 *
 * This is called by routines implementing lock acquisition.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_SEM_LV_WRONG_ORDER if the order is wrong.  Will have done all
 *          necessary whining and breakpointing before returning.
 * @retval  VERR_SEM_LV_INVALID_PARAMETER if the input is invalid.
 *
 * @param   pRec                The validator record.
 * @param   hThread             The handle of the calling thread.  If not known,
 *                              pass NIL_RTTHREAD and this method will figure it
 *                              out.
 * @param   uId                 Some kind of locking location ID.  Typically a
 *                              return address up the stack.  Optional (0).
 * @param   pszFile             The file where the lock is being acquired from.
 *                              Optional.
 * @param   iLine               The line number in that file.  Optional (0).
 * @param   pszFunction         The functionn where the lock is being acquired
 *                              from.  Optional.
 */
RTDECL(int)  RTLockValidatorCheckOrder(PRTLOCKVALIDATORREC pRec, RTTHREAD hThread, RTHCUINTPTR uId, RT_SRC_POS_DECL);

/**
 * Change the thread state to blocking and do deadlock detection.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_SEM_LV_DEADLOCK if blocking would deadlock.  Gone thru the
 *          motions.
 * @retval  VERR_SEM_LV_NESTED if the semaphore isn't recursive and hThread is
 *          already the owner.  Gone thru the motions.
 * @retval  VERR_SEM_LV_INVALID_PARAMETER if the input is invalid.
 *
 * @param   pRec                The validator record we're blocing on.
 * @param   hThread             The current thread.  Shall not be NIL_RTTHREAD!
 * @param   enmState            The sleep state.
 * @param   pvBlock             Pointer to a RTLOCKVALIDATORREC structure.
 * @param   fRecursiveOk        Whether it's ok to recurse.
 * @param   uId                 Where we are blocking.
 * @param   RT_SRC_POS_DECL     Where we are blocking.
 */
RTDECL(int) RTLockValidatorCheckBlocking(PRTLOCKVALIDATORREC pRec, RTTHREAD hThread,
                                         RTTHREADSTATE enmState, bool fRecursiveOk,
                                         RTHCUINTPTR uId, RT_SRC_POS_DECL);

/**
 * Check the exit order.
 *
 * This is called by routines implementing releasing the lock.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_SEM_LV_WRONG_RELEASE_ORDER if the order is wrong.  Will have
 *          done all necessary whining and breakpointing before returning.
 * @retval  VERR_SEM_LV_INVALID_PARAMETER if the input is invalid.
 *
 * @param   pRec                The validator record.
 */
RTDECL(int)  RTLockValidatorCheckReleaseOrder(PRTLOCKVALIDATORREC pRec);

/**
 * Checks and records a lock recursion.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_SEM_LV_NESTED if the semaphore class forbids recursion.  Gone
 *          thru the motions.
 * @retval  VERR_SEM_LV_INVALID_PARAMETER if the input is invalid.
 *
 * @param   pRec                The validator record.
 * @param   uId                 Some kind of locking location ID.  Typically a
 *                              return address up the stack.  Optional (0).
 * @param   pszFile             The file where the lock is being acquired from.
 *                              Optional.
 * @param   iLine               The line number in that file.  Optional (0).
 * @param   pszFunction         The functionn where the lock is being acquired
 *                              from.  Optional.
 */
RTDECL(int) RTLockValidatorRecordRecursion(PRTLOCKVALIDATORREC pRec, RTHCUINTPTR uId, RT_SRC_POS_DECL);

/**
 * Records a lock unwind (releasing one recursion).
 *
 * This should be coupled with called to RTLockValidatorRecordRecursion.
 *
 * @param   pRec                The validator record.
 */
RTDECL(void) RTLockValidatorRecordUnwind(PRTLOCKVALIDATORREC pRec);

/**
 * Record the specified thread as lock owner and increment the write lock count.
 *
 * This function is typically called after acquiring the lock.
 *
 * @returns hThread resolved.  Can return NIL_RTHREAD iff we fail to adopt the
 *          alien thread or if pRec is invalid.
 *
 * @param   pRec                The validator record.
 * @param   hThread             The handle of the calling thread.  If not known,
 *                              pass NIL_RTTHREAD and this method will figure it
 *                              out.
 * @param   uId                 Some kind of locking location ID.  Typically a
 *                              return address up the stack.  Optional (0).
 * @param   pszFile             The file where the lock is being acquired from.
 *                              Optional.
 * @param   iLine               The line number in that file.  Optional (0).
 * @param   pszFunction         The functionn where the lock is being acquired
 *                              from.  Optional.
 */
RTDECL(RTTHREAD) RTLockValidatorSetOwner(PRTLOCKVALIDATORREC pRec, RTTHREAD hThread, RTHCUINTPTR uId, RT_SRC_POS_DECL);


/**
 * Clear the lock ownership and decrement the write lock count.
 *
 * This is typically called before release the lock.
 *
 * @returns The thread handle of the previous owner.  NIL_RTTHREAD if the record
 *          is invalid or didn't have any owner.
 * @param   pRec                The validator record.
 */
RTDECL(RTTHREAD) RTLockValidatorUnsetOwner(PRTLOCKVALIDATORREC pRec);

/**
 * Gets the number of write locks and critical sections the specified
 * thread owns.
 *
 * This number does not include any nested lock/critect entries.
 *
 * Note that it probably will return 0 for non-strict builds since
 * release builds doesn't do unnecessary diagnostic counting like this.
 *
 * @returns Number of locks on success (0+) and VERR_INVALID_HANDLER on failure
 * @param   Thread          The thread we're inquiring about.
 * @remarks Will only work for strict builds.
 */
RTDECL(int32_t) RTLockValidatorWriteLockGetCount(RTTHREAD Thread);

/**
 * Works the THREADINT::cWriteLocks member, mostly internal.
 *
 * @param   Thread      The current thread.
 */
RTDECL(void) RTLockValidatorWriteLockInc(RTTHREAD Thread);

/**
 * Works the THREADINT::cWriteLocks member, mostly internal.
 *
 * @param   Thread      The current thread.
 */
RTDECL(void) RTLockValidatorWriteLockDec(RTTHREAD Thread);

/**
 * Gets the number of read locks the specified thread owns.
 *
 * Note that nesting read lock entry will be included in the
 * total sum. And that it probably will return 0 for non-strict
 * builds since release builds doesn't do unnecessary diagnostic
 * counting like this.
 *
 * @returns Number of read locks on success (0+) and VERR_INVALID_HANDLER on failure
 * @param   Thread          The thread we're inquiring about.
 */
RTDECL(int32_t) RTLockValidatorReadLockGetCount(RTTHREAD Thread);

/**
 * Works the THREADINT::cReadLocks member.
 *
 * @param   Thread      The current thread.
 */
RTDECL(void) RTLockValidatorReadLockInc(RTTHREAD Thread);

/**
 * Works the THREADINT::cReadLocks member.
 *
 * @param   Thread      The current thread.
 */
RTDECL(void) RTLockValidatorReadLockDec(RTTHREAD Thread);



/*RTDECL(int) RTLockValidatorClassCreate();*/


RT_C_DECLS_END

/** @} */

#endif


