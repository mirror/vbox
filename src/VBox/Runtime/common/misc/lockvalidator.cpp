/* $Id$ */
/** @file
 * IPRT - Lock Validator.
 */

/*
 * Copyright (C) 2009-2010 Sun Microsystems, Inc.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/lockvalidator.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/once.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/thread.h>

#include "internal/lockvalidator.h"
#include "internal/magics.h"
#include "internal/thread.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Deadlock detection stack entry.
 */
typedef struct RTLOCKVALDDENTRY
{
    /** The current record. */
    PRTLOCKVALRECUNION      pRec;
    /** The current entry number if pRec is a shared one. */
    uint32_t                iEntry;
    /** The thread state of the thread we followed to get to pFirstSibling.
     * This is only used for validating a deadlock stack.  */
    RTTHREADSTATE           enmState;
    /** The thread we followed to get to pFirstSibling.
     * This is only used for validating a deadlock stack. */
    PRTTHREADINT            pThread;
    /** What pThread is waiting on, i.e. where we entered the circular list of
     * siblings.  This is used for validating a deadlock stack as well as
     * terminating the sibling walk. */
    PRTLOCKVALRECUNION      pFirstSibling;
} RTLOCKVALDDENTRY;


/**
 * Deadlock detection stack.
 */
typedef struct RTLOCKVALDDSTACK
{
    /** The number stack entries. */
    uint32_t                c;
    /** The stack entries. */
    RTLOCKVALDDENTRY        a[32];
} RTLOCKVALDDSTACK;
/** Pointer to a deadlock detction stack. */
typedef RTLOCKVALDDSTACK *PRTLOCKVALDDSTACK;


/**
 * Reference to another class.
 */
typedef struct RTLOCKVALCLASSREF
{
    /** The class. */
    RTLOCKVALCLASS          hClass;
    /** The number of lookups of this class. */
    uint32_t volatile       cLookups;
    /** Indicates whether the entry was added automatically during order checking
     *  (true) or manually via the API (false). */
    bool                    fAutodidacticism;
    /** Reserved / explicit alignment padding. */
    bool                    afReserved[3];
} RTLOCKVALCLASSREF;
/** Pointer to a class reference. */
typedef RTLOCKVALCLASSREF *PRTLOCKVALCLASSREF;


/** Pointer to a chunk of class references. */
typedef struct RTLOCKVALCLASSREFCHUNK *PRTLOCKVALCLASSREFCHUNK;
/**
 * Chunk of class references.
 */
typedef struct RTLOCKVALCLASSREFCHUNK
{
    /** Array of refs. */
#if 0 /** @todo for testing alloction of new chunks. */
    RTLOCKVALCLASSREF       aRefs[ARCH_BITS == 32 ? 10 : 8];
#else
    RTLOCKVALCLASSREF       aRefs[2];
#endif
    /** Pointer to the next chunk. */
    PRTLOCKVALCLASSREFCHUNK volatile pNext;
} RTLOCKVALCLASSREFCHUNK;


/**
 * Lock class.
 */
typedef struct RTLOCKVALCLASSINT
{
    /** AVL node core. */
    AVLLU32NODECORE         Core;
    /** Magic value (RTLOCKVALCLASS_MAGIC). */
    uint32_t volatile       u32Magic;
    /** Reference counter.  See RTLOCKVALCLASS_MAX_REFS. */
    uint32_t volatile       cRefs;
    /** Whether the class is allowed to teach it self new locking order rules. */
    bool                    fAutodidact;
    /** Whether this class is in the tree. */
    bool                    fInTree;
    bool                    afReserved[2]; /**< Explicit padding */
    /** The minimum wait interval for which we do deadlock detection
     *  (milliseconds). */
    RTMSINTERVAL            cMsMinDeadlock;
    /** The minimum wait interval for which we do order checks (milliseconds). */
    RTMSINTERVAL            cMsMinOrder;
    /** More padding. */
    uint32_t                au32Reserved[ARCH_BITS == 32 ? 6 : 3];
    /** Classes that may be taken prior to this one.
     * This is a linked list where each node contains a chunk of locks so that we
     * reduce the number of allocations as well as localize the data. */
    RTLOCKVALCLASSREFCHUNK  PriorLocks;
    /** Hash table containing frequently encountered prior locks. */
    PRTLOCKVALCLASSREF      apPriorLocksHash[11];
#define RTLOCKVALCLASS_HASH_STATS
#ifdef RTLOCKVALCLASS_HASH_STATS
    /** Hash hits. */
    uint32_t volatile       cHashHits;
    /** Hash misses. */
    uint32_t volatile       cHashMisses;
#endif
    /** Where this class was created.
     *  This is mainly used for finding automatically created lock classes.
     *  @remarks The strings are stored after this structure so we won't crash
     *           if the class lives longer than the module (dll/so/dylib) that
     *           spawned it. */
    RTLOCKVALSRCPOS         CreatePos;
} RTLOCKVALCLASSINT;
AssertCompileSize(AVLLU32NODECORE, ARCH_BITS == 32 ? 20 : 32);
AssertCompileMemberOffset(RTLOCKVALCLASSINT, PriorLocks, 64);


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Macro that asserts that a pointer is aligned correctly.
 * Only used when fighting bugs. */
#if 1
# define RTLOCKVAL_ASSERT_PTR_ALIGN(p) \
    AssertMsg(!((uintptr_t)(p) & (sizeof(uintptr_t) - 1)), ("%p\n", (p)));
#else
# define RTLOCKVAL_ASSERT_PTR_ALIGN(p)   do { } while (0)
#endif

/** Hashes the class handle (pointer) into an apPriorLocksHash index. */
#define RTLOCKVALCLASS_HASH(hClass) \
    (   (uintptr_t)(hClass) \
      % (  RT_SIZEOFMEMB(RTLOCKVALCLASSINT, apPriorLocksHash) \
         / sizeof(PRTLOCKVALCLASSREF)) )

/** The max value for RTLOCKVALCLASSINT::cRefs. */
#define RTLOCKVALCLASS_MAX_REFS             UINT32_C(0xffff0000)
/** The max value for RTLOCKVALCLASSREF::cLookups. */
#define RTLOCKVALCLASSREF_MAX_LOOKUPS       UINT32_C(0xfffe0000)
/** The absolute max value for RTLOCKVALCLASSREF::cLookups at which it will
 *  be set back to RTLOCKVALCLASSREF_MAX_LOOKUPS. */
#define RTLOCKVALCLASSREF_MAX_LOOKUPS_FIX   UINT32_C(0xffff0000)


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Serializing object destruction and deadlock detection.
 *
 * This makes sure that none of the memory examined by the deadlock detection
 * code will become invalid (reused for other purposes or made not present)
 * while the detection is in progress.
 *
 * NS: RTLOCKVALREC*, RTTHREADINT and RTLOCKVALDRECSHRD::papOwners destruction.
 * EW: Deadlock detection and some related activities.
 */
static RTSEMXROADS      g_hLockValidatorXRoads   = NIL_RTSEMXROADS;
/** Whether the lock validator is enabled or disabled.
 * Only applies to new locks.  */
static bool volatile    g_fLockValidatorEnabled  = true;
/** Set if the lock validator is quiet. */
#ifdef RT_STRICT
static bool volatile    g_fLockValidatorQuiet    = false;
#else
static bool volatile    g_fLockValidatorQuiet    = true;
#endif
/** Set if the lock validator may panic. */
#ifdef RT_STRICT
static bool volatile    g_fLockValidatorMayPanic = true;
#else
static bool volatile    g_fLockValidatorMayPanic = false;
#endif
/** Serializing class tree insert and lookups. */
static RTSEMRW          g_hLockValClassTreeRWLock = NIL_RTSEMRW;
/** Class tree. */
static PAVLLU32NODECORE g_LockValClassTree = NULL;
/** Critical section serializing the teaching new rules to the classes. */
static RTCRITSECT       g_LockValClassTeachCS;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void rtLockValidatorClassDestroy(RTLOCKVALCLASSINT *pClass);


/**
 * Lazy initialization of the lock validator globals.
 */
static void rtLockValidatorLazyInit(void)
{
    static uint32_t volatile s_fInitializing = false;
    if (ASMAtomicCmpXchgU32(&s_fInitializing, true, false))
    {
        if (!RTCritSectIsInitialized(&g_LockValClassTeachCS))
            RTCritSectInit(&g_LockValClassTeachCS);

        if (g_hLockValClassTreeRWLock == NIL_RTSEMRW)
        {
            RTSEMRW hSemRW;
            int rc = RTSemRWCreate(&hSemRW);
            if (RT_SUCCESS(rc))
                ASMAtomicWriteHandle(&g_hLockValClassTreeRWLock, hSemRW);
        }

        if (g_hLockValidatorXRoads == NIL_RTSEMXROADS)
        {
            RTSEMXROADS hXRoads;
            int rc = RTSemXRoadsCreate(&hXRoads);
            if (RT_SUCCESS(rc))
                ASMAtomicWriteHandle(&g_hLockValidatorXRoads, hXRoads);
        }

        /** @todo register some cleanup callback if we care. */

        ASMAtomicWriteU32(&s_fInitializing, false);
    }
    else
        RTThreadYield();
}



/** Wrapper around ASMAtomicReadPtr. */
DECL_FORCE_INLINE(PRTLOCKVALRECUNION) rtLockValidatorReadRecUnionPtr(PRTLOCKVALRECUNION volatile *ppRec)
{
    PRTLOCKVALRECUNION p = (PRTLOCKVALRECUNION)ASMAtomicReadPtr((void * volatile *)ppRec);
    RTLOCKVAL_ASSERT_PTR_ALIGN(p);
    return p;
}


/** Wrapper around ASMAtomicWritePtr. */
DECL_FORCE_INLINE(void) rtLockValidatorWriteRecUnionPtr(PRTLOCKVALRECUNION volatile *ppRec, PRTLOCKVALRECUNION pRecNew)
{
    RTLOCKVAL_ASSERT_PTR_ALIGN(pRecNew);
    ASMAtomicWritePtr((void * volatile *)ppRec, pRecNew);
}


/** Wrapper around ASMAtomicReadPtr. */
DECL_FORCE_INLINE(PRTTHREADINT) rtLockValidatorReadThreadHandle(RTTHREAD volatile *phThread)
{
    PRTTHREADINT p = (PRTTHREADINT)ASMAtomicReadPtr((void * volatile *)phThread);
    RTLOCKVAL_ASSERT_PTR_ALIGN(p);
    return p;
}


/** Wrapper around ASMAtomicUoReadPtr. */
DECL_FORCE_INLINE(PRTLOCKVALRECSHRDOWN) rtLockValidatorUoReadSharedOwner(PRTLOCKVALRECSHRDOWN volatile *ppOwner)
{
    PRTLOCKVALRECSHRDOWN p = (PRTLOCKVALRECSHRDOWN)ASMAtomicUoReadPtr((void * volatile *)ppOwner);
    RTLOCKVAL_ASSERT_PTR_ALIGN(p);
    return p;
}


/**
 * Reads a volatile thread handle field and returns the thread name.
 *
 * @returns Thread name (read only).
 * @param   phThread            The thread handle field.
 */
static const char *rtLockValidatorNameThreadHandle(RTTHREAD volatile *phThread)
{
    PRTTHREADINT pThread = rtLockValidatorReadThreadHandle(phThread);
    if (!pThread)
        return "<NIL>";
    if (!VALID_PTR(pThread))
        return "<INVALID>";
    if (pThread->u32Magic != RTTHREADINT_MAGIC)
        return "<BAD-THREAD-MAGIC>";
    return pThread->szName;
}


/**
 * Launch a simple assertion like complaint w/ panic.
 *
 * @param   pszFile             Where from - file.
 * @param   iLine               Where from - line.
 * @param   pszFunction         Where from - function.
 * @param   pszWhat             What we're complaining about.
 * @param   ...                 Format arguments.
 */
static void rtLockValidatorComplain(RT_SRC_POS_DECL, const char *pszWhat, ...)
{
    if (!ASMAtomicUoReadBool(&g_fLockValidatorQuiet))
    {
        RTAssertMsg1Weak("RTLockValidator", iLine, pszFile, pszFunction);
        va_list va;
        va_start(va, pszWhat);
        RTAssertMsg2WeakV(pszWhat, va);
        va_end(va);
    }
    if (!ASMAtomicUoReadBool(&g_fLockValidatorQuiet))
        RTAssertPanic();
}


/**
 * Describes the lock.
 *
 * @param   pszPrefix           Message prefix.
 * @param   pRec                The lock record we're working on.
 * @param   pszSuffix           Message suffix.
 */
static void rtLockValidatorComplainAboutLock(const char *pszPrefix, PRTLOCKVALRECUNION pRec, const char *pszSuffix)
{
    if (    VALID_PTR(pRec)
        &&  !ASMAtomicUoReadBool(&g_fLockValidatorQuiet))
    {
        switch (pRec->Core.u32Magic)
        {
            case RTLOCKVALRECEXCL_MAGIC:
                RTAssertMsg2AddWeak("%s%p %s xrec=%p own=%s nest=%u pos={%Rbn(%u) %Rfn %p}%s", pszPrefix,
                                    pRec->Excl.hLock, pRec->Excl.pszName, pRec,
                                    rtLockValidatorNameThreadHandle(&pRec->Excl.hThread), pRec->Excl.cRecursion,
                                    pRec->Excl.SrcPos.pszFile, pRec->Excl.SrcPos.uLine, pRec->Excl.SrcPos.pszFunction, pRec->Excl.SrcPos.uId,
                                    pszSuffix);
                break;

            case RTLOCKVALRECSHRD_MAGIC:
                RTAssertMsg2AddWeak("%s%p %s srec=%p%s", pszPrefix,
                                    pRec->Shared.hLock, pRec->Shared.pszName, pRec,
                                    pszSuffix);
                break;

            case RTLOCKVALRECSHRDOWN_MAGIC:
            {
                PRTLOCKVALRECSHRD pShared = pRec->ShrdOwner.pSharedRec;
                if (    VALID_PTR(pShared)
                    &&  pShared->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC)
                    RTAssertMsg2AddWeak("%s%p %s srec=%p trec=%p thr=%s nest=%u pos={%Rbn(%u) %Rfn %p}%s", pszPrefix,
                                        pShared->hLock, pShared->pszName, pShared,
                                        pRec, rtLockValidatorNameThreadHandle(&pRec->ShrdOwner.hThread), pRec->ShrdOwner.cRecursion,
                                        pRec->ShrdOwner.SrcPos.pszFile, pRec->ShrdOwner.SrcPos.uLine, pRec->ShrdOwner.SrcPos.pszFunction, pRec->ShrdOwner.SrcPos.uId,
                                        pszSuffix);
                else
                    RTAssertMsg2AddWeak("%sbad srec=%p trec=%p thr=%s nest=%u pos={%Rbn(%u) %Rfn %p}%s", pszPrefix,
                                        pShared,
                                        pRec, rtLockValidatorNameThreadHandle(&pRec->ShrdOwner.hThread), pRec->ShrdOwner.cRecursion,
                                        pRec->ShrdOwner.SrcPos.pszFile, pRec->ShrdOwner.SrcPos.uLine, pRec->ShrdOwner.SrcPos.pszFunction, pRec->ShrdOwner.SrcPos.uId,
                                        pszSuffix);
                break;
            }

            default:
                RTAssertMsg2AddWeak("%spRec=%p u32Magic=%#x (bad)%s", pszPrefix, pRec, pRec->Core.u32Magic, pszSuffix);
                break;
        }
    }
}


/**
 * Launch the initial complaint.
 *
 * @param   pszWhat             What we're complaining about.
 * @param   pSrcPos             Where we are complaining from, as it were.
 * @param   pThreadSelf         The calling thread.
 * @param   pRec                The main lock involved. Can be NULL.
 */
static void rtLockValidatorComplainFirst(const char *pszWhat, PCRTLOCKVALSRCPOS pSrcPos, PRTTHREADINT pThreadSelf, PRTLOCKVALRECUNION pRec)
{
    if (!ASMAtomicUoReadBool(&g_fLockValidatorQuiet))
    {
        ASMCompilerBarrier(); /* paranoia */
        RTAssertMsg1Weak("RTLockValidator", pSrcPos ? pSrcPos->uLine : 0, pSrcPos ? pSrcPos->pszFile : NULL, pSrcPos ? pSrcPos->pszFunction : NULL);
        if (pSrcPos && pSrcPos->uId)
            RTAssertMsg2Weak("%s  [uId=%p  thrd=%s]\n", pszWhat, pSrcPos->uId, VALID_PTR(pThreadSelf) ? pThreadSelf->szName : "<NIL>");
        else
            RTAssertMsg2Weak("%s  [thrd=%s]\n", pszWhat, VALID_PTR(pThreadSelf) ? pThreadSelf->szName : "<NIL>");
        rtLockValidatorComplainAboutLock("Lock: ", pRec, "\n");
    }
}


/**
 * Continue bitching.
 *
 * @param   pszFormat           Format string.
 * @param   ...                 Format arguments.
 */
static void rtLockValidatorComplainMore(const char *pszFormat, ...)
{
    if (!ASMAtomicUoReadBool(&g_fLockValidatorQuiet))
    {
        va_list va;
        va_start(va, pszFormat);
        RTAssertMsg2AddWeakV(pszFormat, va);
        va_end(va);
    }
}


/**
 * Raise a panic if enabled.
 */
static void rtLockValidatorComplainPanic(void)
{
    if (ASMAtomicUoReadBool(&g_fLockValidatorMayPanic))
        RTAssertPanic();
}


/**
 * Copy a source position record.
 *
 * @param   pDst                The destination.
 * @param   pSrc                The source.  Can be NULL.
 */
DECL_FORCE_INLINE(void) rtLockValidatorSrcPosCopy(PRTLOCKVALSRCPOS pDst, PCRTLOCKVALSRCPOS pSrc)
{
    if (pSrc)
    {
        ASMAtomicUoWriteU32(&pDst->uLine,                           pSrc->uLine);
        ASMAtomicUoWritePtr((void * volatile *)&pDst->pszFile,      pSrc->pszFile);
        ASMAtomicUoWritePtr((void * volatile *)&pDst->pszFunction,  pSrc->pszFunction);
        ASMAtomicUoWritePtr((void * volatile *)&pDst->uId,          (void *)pSrc->uId);
    }
    else
    {
        ASMAtomicUoWriteU32(&pDst->uLine,                           0);
        ASMAtomicUoWritePtr((void * volatile *)&pDst->pszFile,      NULL);
        ASMAtomicUoWritePtr((void * volatile *)&pDst->pszFunction,  NULL);
        ASMAtomicUoWritePtr((void * volatile *)&pDst->uId,          0);
    }
}


/**
 * Init a source position record.
 *
 * @param   pSrcPos             The source position record.
 */
DECL_FORCE_INLINE(void) rtLockValidatorSrcPosInit(PRTLOCKVALSRCPOS pSrcPos)
{
    pSrcPos->pszFile        = NULL;
    pSrcPos->pszFunction    = NULL;
    pSrcPos->uId            = 0;
    pSrcPos->uLine          = 0;
#if HC_ARCH_BITS == 64
    pSrcPos->u32Padding     = 0;
#endif
}


/* sdbm:
   This algorithm was created for sdbm (a public-domain reimplementation of
   ndbm) database library. it was found to do well in scrambling bits,
   causing better distribution of the keys and fewer splits. it also happens
   to be a good general hashing function with good distribution. the actual
   function is hash(i) = hash(i - 1) * 65599 + str[i]; what is included below
   is the faster version used in gawk. [there is even a faster, duff-device
   version] the magic constant 65599 was picked out of thin air while
   experimenting with different constants, and turns out to be a prime.
   this is one of the algorithms used in berkeley db (see sleepycat) and
   elsewhere. */
DECL_FORCE_INLINE(uint32_t) sdbm(const char *str, uint32_t hash)
{
    uint8_t *pu8 = (uint8_t *)str;
    int c;

    while ((c = *pu8++))
        hash = c + (hash << 6) + (hash << 16) - hash;

    return hash;
}


/**
 * Hashes the specified source position.
 *
 * @returns Hash.
 * @param   pSrcPos             The source position record.
 */
static uint32_t rtLockValidatorSrcPosHash(PCRTLOCKVALSRCPOS pSrcPos)
{
    uint32_t uHash;
    if (   (   pSrcPos->pszFile
            || pSrcPos->pszFunction)
        && pSrcPos->uLine != 0)
    {
        uHash = 0;
        if (pSrcPos->pszFile)
            uHash = sdbm(pSrcPos->pszFile, uHash);
        if (pSrcPos->pszFunction)
            uHash = sdbm(pSrcPos->pszFunction, uHash);
        uHash += pSrcPos->uLine;
    }
    else
    {
        Assert(pSrcPos->uId);
        uHash = (uint32_t)pSrcPos->uId;
    }

    return uHash;
}


/**
 * Compares two source positions.
 *
 * @returns 0 if equal, < 0 if pSrcPos1 is smaller than pSrcPos2, > 0 if
 *          otherwise.
 * @param   pSrcPos1            The first source position.
 * @param   pSrcPos2            The second source position.
 */
static int rtLockValidatorSrcPosCompare(PCRTLOCKVALSRCPOS pSrcPos1, PCRTLOCKVALSRCPOS pSrcPos2)
{
    if (pSrcPos1->uLine != pSrcPos2->uLine)
        return pSrcPos1->uLine < pSrcPos2->uLine ? -1 : 1;

    int iDiff = RTStrCmp(pSrcPos1->pszFile, pSrcPos2->pszFile);
    if (iDiff != 0)
        return iDiff;

    iDiff = RTStrCmp(pSrcPos1->pszFunction, pSrcPos2->pszFunction);
    if (iDiff != 0)
        return iDiff;

    if (pSrcPos1->uId != pSrcPos2->uId)
        return pSrcPos1->uId < pSrcPos2->uId ? -1 : 1;
    return 0;
}



/**
 * Serializes destruction of RTLOCKVALREC* and RTTHREADINT structures.
 */
DECLHIDDEN(void) rtLockValidatorSerializeDestructEnter(void)
{
    RTSEMXROADS hXRoads = g_hLockValidatorXRoads;
    if (hXRoads != NIL_RTSEMXROADS)
        RTSemXRoadsNSEnter(hXRoads);
}


/**
 * Call after rtLockValidatorSerializeDestructEnter.
 */
DECLHIDDEN(void) rtLockValidatorSerializeDestructLeave(void)
{
    RTSEMXROADS hXRoads = g_hLockValidatorXRoads;
    if (hXRoads != NIL_RTSEMXROADS)
        RTSemXRoadsNSLeave(hXRoads);
}


/**
 * Serializes deadlock detection against destruction of the objects being
 * inspected.
 */
DECLINLINE(void) rtLockValidatorSerializeDetectionEnter(void)
{
    RTSEMXROADS hXRoads = g_hLockValidatorXRoads;
    if (hXRoads != NIL_RTSEMXROADS)
        RTSemXRoadsEWEnter(hXRoads);
}


/**
 * Call after rtLockValidatorSerializeDetectionEnter.
 */
DECLHIDDEN(void) rtLockValidatorSerializeDetectionLeave(void)
{
    RTSEMXROADS hXRoads = g_hLockValidatorXRoads;
    if (hXRoads != NIL_RTSEMXROADS)
        RTSemXRoadsEWLeave(hXRoads);
}


/**
 * Initializes the per thread lock validator data.
 *
 * @param   pPerThread      The data.
 */
DECLHIDDEN(void) rtLockValidatorInitPerThread(RTLOCKVALPERTHREAD *pPerThread)
{
    pPerThread->bmFreeShrdOwners = UINT32_MAX;

    /* ASSUMES the rest has already been zeroed. */
    Assert(pPerThread->pRec == NULL);
    Assert(pPerThread->cWriteLocks == 0);
    Assert(pPerThread->cReadLocks == 0);
    Assert(pPerThread->fInValidator == false);
}


RTDECL(int) RTLockValidatorClassCreateEx(PRTLOCKVALCLASS phClass, PCRTLOCKVALSRCPOS pSrcPos,
                                         bool fAutodidact, RTMSINTERVAL cMsMinDeadlock, RTMSINTERVAL cMsMinOrder)
{
    Assert(cMsMinDeadlock >= 1);
    Assert(cMsMinOrder    >= 1);
    AssertPtr(pSrcPos);

    size_t const       cbFile   = pSrcPos->pszFile ? strlen(pSrcPos->pszFile) + 1 : 0;
    size_t const     cbFunction = pSrcPos->pszFile ? strlen(pSrcPos->pszFunction) + 1 : 0;
    RTLOCKVALCLASSINT *pThis    = (RTLOCKVALCLASSINT *)RTMemAlloc(sizeof(*pThis) + cbFile + cbFunction);
    if (!pThis)
        return VERR_NO_MEMORY;

    pThis->Core.Key             = rtLockValidatorSrcPosHash(pSrcPos);
    pThis->Core.uchHeight       = 0;
    pThis->Core.pLeft           = NULL;
    pThis->Core.pRight          = NULL;
    pThis->Core.pList           = NULL;
    pThis->u32Magic             = RTLOCKVALCLASS_MAGIC;
    pThis->cRefs                = 1;
    pThis->fAutodidact          = fAutodidact;
    pThis->fInTree              = false;
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->afReserved); i++)
        pThis->afReserved[i]    = false;
    pThis->cMsMinDeadlock       = cMsMinDeadlock;
    pThis->cMsMinOrder          = cMsMinOrder;
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->au32Reserved); i++)
        pThis->au32Reserved[i]  = 0;
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->au32Reserved); i++)
    {
        pThis->PriorLocks.aRefs[i].hClass           = NIL_RTLOCKVALCLASS;
        pThis->PriorLocks.aRefs[i].cLookups         = 0;
        pThis->PriorLocks.aRefs[i].fAutodidacticism = false;
        pThis->PriorLocks.aRefs[i].afReserved[0]    = false;
        pThis->PriorLocks.aRefs[i].afReserved[1]    = false;
        pThis->PriorLocks.aRefs[i].afReserved[2]    = false;
    }
    pThis->PriorLocks.pNext     = NULL;
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->apPriorLocksHash); i++)
        pThis->apPriorLocksHash[i] = NULL;
    rtLockValidatorSrcPosCopy(&pThis->CreatePos, pSrcPos);
    char *pszDst = (char *)(pThis + 1);
    pThis->CreatePos.pszFile    = pSrcPos->pszFile     ? (char *)memcpy(pszDst, pSrcPos->pszFile,     cbFile)     : NULL;
    pszDst += cbFile;
    pThis->CreatePos.pszFunction= pSrcPos->pszFunction ? (char *)memcpy(pszDst, pSrcPos->pszFunction, cbFunction) : NULL;
    Assert(rtLockValidatorSrcPosHash(&pThis->CreatePos) == pThis->Core.Key);

    *phClass = pThis;
    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorClassCreate(PRTLOCKVALCLASS phClass, bool fAutodidact, RT_SRC_POS_DECL)
{
    RTLOCKVALSRCPOS SrcPos = RTLOCKVALSRCPOS_INIT_POS_NO_ID();
    return RTLockValidatorClassCreateEx(phClass, &SrcPos, fAutodidact,
                                        1 /*cMsMinDeadlock*/, 1 /*cMsMinOrder*/);
}


/**
 * Internal class retainer.
 * @returns The new reference count.
 * @param   pClass              The class.
 */
DECL_FORCE_INLINE(uint32_t) rtLockValidatorClassRetain(RTLOCKVALCLASSINT *pClass)
{
    uint32_t cRefs = ASMAtomicIncU32(&pClass->cRefs);
    if (cRefs > RTLOCKVALCLASS_MAX_REFS)
        ASMAtomicWriteU32(&pClass->cRefs, RTLOCKVALCLASS_MAX_REFS);
    return cRefs;
}


/**
 * Validates and retains a lock validator class.
 *
 * @returns @a hClass on success, NIL_RTLOCKVALCLASS on failure.
 * @param   hClass              The class handle.  NIL_RTLOCKVALCLASS is ok.
 */
DECL_FORCE_INLINE(RTLOCKVALCLASS) rtLockValidatorClassValidateAndRetain(RTLOCKVALCLASS hClass)
{
    if (hClass == NIL_RTLOCKVALCLASS)
        return hClass;
    AssertPtrReturn(hClass, NIL_RTLOCKVALCLASS);
    AssertReturn(hClass->u32Magic == RTLOCKVALCLASS_MAGIC, NIL_RTLOCKVALCLASS);
    rtLockValidatorClassRetain(hClass);
    return hClass;
}


/**
 * Internal class releaser.
 * @returns The new reference count.
 * @param   pClass              The class.
 */
DECL_FORCE_INLINE(uint32_t) rtLockValidatorClassRelease(RTLOCKVALCLASSINT *pClass)
{
    uint32_t cRefs = ASMAtomicDecU32(&pClass->cRefs);
    if (cRefs + 1 == RTLOCKVALCLASS_MAX_REFS)
        ASMAtomicWriteU32(&pClass->cRefs, RTLOCKVALCLASS_MAX_REFS);
    else if (!cRefs)
        rtLockValidatorClassDestroy(pClass);
    return cRefs;
}


/**
 * Destroys a class once there are not more references to it.
 *
 * @param   Class               The class.
 */
static void rtLockValidatorClassDestroy(RTLOCKVALCLASSINT *pClass)
{
    AssertReturnVoid(pClass->fInTree);
    ASMAtomicWriteU32(&pClass->u32Magic, RTLOCKVALCLASS_MAGIC_DEAD);

    PRTLOCKVALCLASSREFCHUNK pChunk = &pClass->PriorLocks;
    while (pChunk)
    {
        for (uint32_t i = 0; i < RT_ELEMENTS(pChunk->aRefs); i++)
        {
            RTLOCKVALCLASSINT *pClass2 = pChunk->aRefs[i].hClass;
            if (pClass2 != NIL_RTLOCKVALCLASS)
            {
                pChunk->aRefs[i].hClass = NIL_RTLOCKVALCLASS;
                rtLockValidatorClassRelease(pClass2);
            }
        }

        PRTLOCKVALCLASSREFCHUNK pNext = pChunk->pNext;
        pChunk->pNext = NULL;
        if (pChunk != &pClass->PriorLocks)
            RTMemFree(pChunk);
        pNext = pChunk;
    }

    RTMemFree(pClass);
}


RTDECL(RTLOCKVALCLASS) RTLockValidatorClassFindForSrcPos(PRTLOCKVALSRCPOS pSrcPos)
{
    if (g_hLockValClassTreeRWLock == NIL_RTSEMRW)
        rtLockValidatorLazyInit();
    int rcLock = RTSemRWRequestRead(g_hLockValClassTreeRWLock, RT_INDEFINITE_WAIT);

    uint32_t uSrcPosHash = rtLockValidatorSrcPosHash(pSrcPos);
    RTLOCKVALCLASSINT *pClass = (RTLOCKVALCLASSINT *)RTAvllU32Get(&g_LockValClassTree, uSrcPosHash);
    while (pClass)
    {
        if (rtLockValidatorSrcPosCompare(&pClass->CreatePos, pSrcPos) == 0)
        {
            rtLockValidatorClassRetain(pClass);
            break;
        }
        pClass = (RTLOCKVALCLASSINT *)pClass->Core.pList;
    }

    if (RT_SUCCESS(rcLock))
        RTSemRWReleaseRead(g_hLockValClassTreeRWLock);
    return pClass;
}


RTDECL(RTLOCKVALCLASS) RTLockValidatorClassForSrcPos(RT_SRC_POS_DECL)
{
    RTLOCKVALSRCPOS SrcPos = RTLOCKVALSRCPOS_INIT_POS_NO_ID();
    RTLOCKVALCLASS  hClass = RTLockValidatorClassFindForSrcPos(&SrcPos);
    if (hClass == NIL_RTLOCKVALCLASS)
    {
        /*
         * Create a new class and insert it into the tree.
         */
        int rc = RTLockValidatorClassCreateEx(&hClass, &SrcPos, true /*fAutodidact*/,
                                              1 /*cMsMinDeadlock*/, 1 /*cMsMinOrder*/);
        if (RT_SUCCESS(rc))
        {
            if (g_hLockValClassTreeRWLock == NIL_RTSEMRW)
                rtLockValidatorLazyInit();
            int rcLock = RTSemRWRequestWrite(g_hLockValClassTreeRWLock, RT_INDEFINITE_WAIT);

            Assert(!hClass->fInTree);
            hClass->fInTree = RTAvllU32Insert(&g_LockValClassTree, &hClass->Core);
            Assert(hClass->fInTree);

            if (RT_SUCCESS(rcLock))
                RTSemRWReleaseWrite(g_hLockValClassTreeRWLock);
            return hClass;
        }
    }
    return hClass;
}


RTDECL(uint32_t) RTLockValidatorClassRetain(RTLOCKVALCLASS hClass)
{
    RTLOCKVALCLASSINT *pClass = hClass;
    AssertPtrReturn(pClass, UINT32_MAX);
    AssertReturn(pClass->u32Magic == RTLOCKVALCLASS_MAGIC, UINT32_MAX);
    return rtLockValidatorClassRetain(pClass);
}


RTDECL(uint32_t) RTLockValidatorClassRelease(RTLOCKVALCLASS hClass)
{
    RTLOCKVALCLASSINT *pClass = hClass;
    AssertPtrReturn(pClass, UINT32_MAX);
    AssertReturn(pClass->u32Magic == RTLOCKVALCLASS_MAGIC, UINT32_MAX);
    return rtLockValidatorClassRelease(pClass);
}


/**
 * Worker for rtLockValidatorClassIsPriorClass that does a linear search thru
 * all the chunks for @a pPriorClass.
 *
 * @returns true / false.
 * @param   pClass              The class to search.
 * @param   pPriorClass         The class to search for.
 */
static bool rtLockValidatorClassIsPriorClassByLinearSearch(RTLOCKVALCLASSINT *pClass, RTLOCKVALCLASSINT *pPriorClass)
{
    for (PRTLOCKVALCLASSREFCHUNK pChunk = &pClass->PriorLocks; pChunk; pChunk = pChunk->pNext)
        for (uint32_t i = 0; i < RT_ELEMENTS(pChunk->aRefs); i++)
        {
            if (pChunk->aRefs[i].hClass == pPriorClass)
            {
                uint32_t cLookups = ASMAtomicIncU32(&pChunk->aRefs[i].cLookups);
                if (RT_UNLIKELY(cLookups >= RTLOCKVALCLASSREF_MAX_LOOKUPS_FIX))
                {
                    ASMAtomicWriteU32(&pChunk->aRefs[i].cLookups, RTLOCKVALCLASSREF_MAX_LOOKUPS);
                    cLookups = RTLOCKVALCLASSREF_MAX_LOOKUPS;
                }

                /* update the hash table entry. */
                PRTLOCKVALCLASSREF *ppHashEntry = &pClass->apPriorLocksHash[RTLOCKVALCLASS_HASH(pPriorClass)];
                if (    !(*ppHashEntry)
                    ||  (*ppHashEntry)->cLookups + 128 < cLookups)
                    ASMAtomicWritePtr((void * volatile *)ppHashEntry, &pChunk->aRefs[i]);

#ifdef RTLOCKVALCLASS_HASH_STATS
                ASMAtomicIncU32(&pClass->cHashMisses);
#endif
                return true;
            }
        }

    return false;
}


/**
 * Checks if @a pPriorClass is a known prior class.
 *
 * @returns true / false.
 * @param   pClass              The class to search.
 * @param   pPriorClass         The class to search for.
 */
DECL_FORCE_INLINE(bool) rtLockValidatorClassIsPriorClass(RTLOCKVALCLASSINT *pClass, RTLOCKVALCLASSINT *pPriorClass)
{
    /*
     * Hash lookup here.
     */
    PRTLOCKVALCLASSREF pRef = pClass->apPriorLocksHash[RTLOCKVALCLASS_HASH(pPriorClass)];
    if (    pRef
        &&  pRef->hClass == pPriorClass)
    {
        uint32_t cLookups = ASMAtomicIncU32(&pRef->cLookups);
        if (RT_UNLIKELY(cLookups >= RTLOCKVALCLASSREF_MAX_LOOKUPS_FIX))
            ASMAtomicWriteU32(&pRef->cLookups, RTLOCKVALCLASSREF_MAX_LOOKUPS);
#ifdef RTLOCKVALCLASS_HASH_STATS
        ASMAtomicIncU32(&pClass->cHashHits);
#endif
        return true;
    }

    return rtLockValidatorClassIsPriorClassByLinearSearch(pClass, pPriorClass);
}


/**
 * Adds a class to the prior list.
 *
 * @returns VINF_SUCCESS, VERR_NO_MEMORY or VERR_SEM_LV_WRONG_ORDER.
 * @param   pClass              The class to work on.
 * @param   pPriorClass         The class to add.
 * @param   fAutodidacticism    Whether we're teaching ourselfs (true) or
 *                              somebody is teaching us via the API (false).
 */
static int rtLockValidatorClassAddPriorClass(RTLOCKVALCLASSINT *pClass, RTLOCKVALCLASSINT *pPriorClass, bool fAutodidacticism)
{
    if (!RTCritSectIsInitialized(&g_LockValClassTeachCS))
        rtLockValidatorLazyInit();
    int rcLock = RTCritSectEnter(&g_LockValClassTeachCS);

    /*
     * Check that there are no conflict (no assert since we might race each other).
     */
    int rc = VERR_INTERNAL_ERROR_5;
    if (!rtLockValidatorClassIsPriorClass(pPriorClass, pClass))
    {
        if (!rtLockValidatorClassIsPriorClass(pClass, pPriorClass))
        {
            /*
             * Scan the table for a free entry, allocating a new chunk if necessary.
             */
            for (PRTLOCKVALCLASSREFCHUNK pChunk = &pClass->PriorLocks; ; pChunk = pChunk->pNext)
            {
                bool fDone = false;
                for (uint32_t i = 0; i < RT_ELEMENTS(pChunk->aRefs); i++)
                {
                    ASMAtomicCmpXchgHandle(&pChunk->aRefs[i].hClass, pPriorClass, NIL_RTLOCKVALCLASS, fDone);
                    if (fDone)
                    {
                        pChunk->aRefs[i].fAutodidacticism = fAutodidacticism;
                        rc = VINF_SUCCESS;
                        break;
                    }
                }
                if (fDone)
                    break;

                /* If no more chunks, allocate a new one and insert the class before linking it. */
                if (!pChunk->pNext)
                {
                    PRTLOCKVALCLASSREFCHUNK pNew = (PRTLOCKVALCLASSREFCHUNK)RTMemAlloc(sizeof(*pNew));
                    if (!pNew)
                    {
                        rc = VERR_NO_MEMORY;
                        break;
                    }
                    pNew->pNext = NULL;
                    for (uint32_t i = 0; i < RT_ELEMENTS(pNew->aRefs); i++)
                    {
                        pNew->aRefs[i].hClass           = NIL_RTLOCKVALCLASS;
                        pNew->aRefs[i].cLookups         = 0;
                        pNew->aRefs[i].fAutodidacticism = false;
                        pNew->aRefs[i].afReserved[0]    = false;
                        pNew->aRefs[i].afReserved[1]    = false;
                        pNew->aRefs[i].afReserved[2]    = false;
                    }

                    pNew->aRefs[0].hClass           = pPriorClass;
                    pNew->aRefs[0].fAutodidacticism = fAutodidacticism;

                    ASMAtomicWritePtr((void * volatile *)&pChunk->pNext, pNew);
                    rc = VINF_SUCCESS;
                    break;
                }
            } /* chunk loop */
        }
        else
            rc = VINF_SUCCESS;
    }
    else
        rc = VERR_SEM_LV_WRONG_ORDER;

    if (RT_SUCCESS(rcLock))
        RTCritSectLeave(&g_LockValClassTeachCS);
    return rc;
}


RTDECL(int) RTLockValidatorClassAddPriorClass(RTLOCKVALCLASS hClass, RTLOCKVALCLASS hPriorClass)
{
    RTLOCKVALCLASSINT *pClass = hClass;
    AssertPtrReturn(pClass, VERR_INVALID_HANDLE);
    AssertReturn(pClass->u32Magic == RTLOCKVALCLASS_MAGIC, VERR_INVALID_HANDLE);

    RTLOCKVALCLASSINT *pPriorClass = hPriorClass;
    AssertPtrReturn(pPriorClass, VERR_INVALID_HANDLE);
    AssertReturn(pPriorClass->u32Magic == RTLOCKVALCLASS_MAGIC, VERR_INVALID_HANDLE);

    return rtLockValidatorClassAddPriorClass(pClass, pPriorClass, false /*fAutodidacticism*/);
}


/**
 * Checks if all owners are blocked - shared record operated in signaller mode.
 *
 * @returns true / false accordingly.
 * @param   pRec                The record.
 * @param   pThreadSelf         The current thread.
 */
DECL_FORCE_INLINE(bool) rtLockValidatorDdAreAllThreadsBlocked(PRTLOCKVALRECSHRD pRec, PRTTHREADINT pThreadSelf)
{
    PRTLOCKVALRECSHRDOWN volatile  *papOwners  = pRec->papOwners;
    uint32_t                        cAllocated = pRec->cAllocated;
    uint32_t                        cEntries   = ASMAtomicUoReadU32(&pRec->cEntries);
    if (cEntries == 0)
        return false;

    for (uint32_t i = 0; i < cAllocated; i++)
    {
        PRTLOCKVALRECSHRDOWN pEntry = rtLockValidatorUoReadSharedOwner(&papOwners[i]);
        if (   pEntry
            && pEntry->Core.u32Magic == RTLOCKVALRECSHRDOWN_MAGIC)
        {
            PRTTHREADINT pCurThread = rtLockValidatorReadThreadHandle(&pEntry->hThread);
            if (!pCurThread)
                return false;
            if (pCurThread->u32Magic != RTTHREADINT_MAGIC)
                return false;
            if (   !RTTHREAD_IS_SLEEPING(rtThreadGetState(pCurThread))
                && pCurThread != pThreadSelf)
                return false;
            if (--cEntries == 0)
                break;
        }
        else
            Assert(!pEntry || pEntry->Core.u32Magic == RTLOCKVALRECSHRDOWN_MAGIC_DEAD);
    }

    return true;
}


/**
 * Verifies the deadlock stack before calling it a deadlock.
 *
 * @retval  VERR_SEM_LV_DEADLOCK if it's a deadlock.
 * @retval  VERR_SEM_LV_ILLEGAL_UPGRADE if it's a deadlock on the same lock.
 * @retval  VERR_TRY_AGAIN if something changed.
 *
 * @param   pStack              The deadlock detection stack.
 * @param   pThreadSelf         The current thread.
 */
static int rtLockValidatorDdVerifyDeadlock(PRTLOCKVALDDSTACK pStack, PRTTHREADINT pThreadSelf)
{
    uint32_t const c = pStack->c;
    for (uint32_t iPass = 0; iPass < 3; iPass++)
    {
        for (uint32_t i = 1; i < c; i++)
        {
            PRTTHREADINT pThread = pStack->a[i].pThread;
            if (pThread->u32Magic != RTTHREADINT_MAGIC)
                return VERR_TRY_AGAIN;
            if (rtThreadGetState(pThread) != pStack->a[i].enmState)
                return VERR_TRY_AGAIN;
            if (rtLockValidatorReadRecUnionPtr(&pThread->LockValidator.pRec) != pStack->a[i].pFirstSibling)
                return VERR_TRY_AGAIN;
            /* ASSUMES the signaller records won't have siblings! */
            PRTLOCKVALRECUNION pRec = pStack->a[i].pRec;
            if (   pRec->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC
                && pRec->Shared.fSignaller
                && !rtLockValidatorDdAreAllThreadsBlocked(&pRec->Shared, pThreadSelf))
                return VERR_TRY_AGAIN;
        }
        RTThreadYield();
    }

    if (c == 1)
        return VERR_SEM_LV_ILLEGAL_UPGRADE;
    return VERR_SEM_LV_DEADLOCK;
}


/**
 * Checks for stack cycles caused by another deadlock before returning.
 *
 * @retval  VINF_SUCCESS if the stack is simply too small.
 * @retval  VERR_SEM_LV_EXISTING_DEADLOCK if a cycle was detected.
 *
 * @param   pStack              The deadlock detection stack.
 */
static int rtLockValidatorDdHandleStackOverflow(PRTLOCKVALDDSTACK pStack)
{
    for (size_t i = 0; i < RT_ELEMENTS(pStack->a) - 1; i++)
    {
        PRTTHREADINT pThread = pStack->a[i].pThread;
        for (size_t j = i + 1; j < RT_ELEMENTS(pStack->a); j++)
            if (pStack->a[j].pThread == pThread)
                return VERR_SEM_LV_EXISTING_DEADLOCK;
    }
    static bool volatile s_fComplained = false;
    if (!s_fComplained)
    {
        s_fComplained = true;
        rtLockValidatorComplain(RT_SRC_POS, "lock validator stack is too small! (%zu entries)\n", RT_ELEMENTS(pStack->a));
    }
    return VINF_SUCCESS;
}


/**
 * Worker for rtLockValidatorDeadlockDetection that does the actual deadlock
 * detection.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_SEM_LV_DEADLOCK
 * @retval  VERR_SEM_LV_EXISTING_DEADLOCK
 * @retval  VERR_SEM_LV_ILLEGAL_UPGRADE
 * @retval  VERR_TRY_AGAIN
 *
 * @param   pStack          The stack to use.
 * @param   pOriginalRec    The original record.
 * @param   pThreadSelf     The calling thread.
 */
static int rtLockValidatorDdDoDetection(PRTLOCKVALDDSTACK pStack, PRTLOCKVALRECUNION const pOriginalRec,
                                        PRTTHREADINT const pThreadSelf)
{
    pStack->c = 0;

    /* We could use a single RTLOCKVALDDENTRY variable here, but the
       compiler may make a better job of it when using individual variables. */
    PRTLOCKVALRECUNION  pRec            = pOriginalRec;
    PRTLOCKVALRECUNION  pFirstSibling   = pOriginalRec;
    uint32_t            iEntry          = UINT32_MAX;
    PRTTHREADINT        pThread         = NIL_RTTHREAD;
    RTTHREADSTATE       enmState        = RTTHREADSTATE_RUNNING;
    for (uint32_t iLoop = 0; ; iLoop++)
    {
        /*
         * Process the current record.
         */
        RTLOCKVAL_ASSERT_PTR_ALIGN(pRec);

        /* Find the next relevant owner thread and record. */
        PRTLOCKVALRECUNION  pNextRec     = NULL;
        RTTHREADSTATE       enmNextState = RTTHREADSTATE_RUNNING;
        PRTTHREADINT        pNextThread  = NIL_RTTHREAD;
        switch (pRec->Core.u32Magic)
        {
            case RTLOCKVALRECEXCL_MAGIC:
                Assert(iEntry == UINT32_MAX);
                for (;;)
                {
                    pNextThread = rtLockValidatorReadThreadHandle(&pRec->Excl.hThread);
                    if (   !pNextThread
                        || pNextThread->u32Magic != RTTHREADINT_MAGIC)
                        break;
                    enmNextState = rtThreadGetState(pNextThread);
                    if (    !RTTHREAD_IS_SLEEPING(enmNextState)
                        &&  pNextThread != pThreadSelf)
                        break;
                    pNextRec = rtLockValidatorReadRecUnionPtr(&pNextThread->LockValidator.pRec);
                    if (RT_LIKELY(   !pNextRec
                                  || enmNextState == rtThreadGetState(pNextThread)))
                        break;
                    pNextRec = NULL;
                }
                if (!pNextRec)
                {
                    pRec = pRec->Excl.pSibling;
                    if (    pRec
                        &&  pRec != pFirstSibling)
                        continue;
                    pNextThread = NIL_RTTHREAD;
                }
                break;

            case RTLOCKVALRECSHRD_MAGIC:
                if (!pRec->Shared.fSignaller)
                {
                    /* Skip to the next sibling if same side.  ASSUMES reader priority. */
                    /** @todo The read side of a read-write lock is problematic if
                     * the implementation prioritizes writers over readers because
                     * that means we should could deadlock against current readers
                     * if a writer showed up.  If the RW sem implementation is
                     * wrapping some native API, it's not so easy to detect when we
                     * should do this and when we shouldn't.  Checking when we
                     * shouldn't is subject to wakeup scheduling and cannot easily
                     * be made reliable.
                     *
                     * At the moment we circumvent all this mess by declaring that
                     * readers has priority.  This is TRUE on linux, but probably
                     * isn't on Solaris and FreeBSD. */
                    if (   pRec == pFirstSibling
                        && pRec->Shared.pSibling != NULL
                        && pRec->Shared.pSibling != pFirstSibling)
                    {
                        pRec = pRec->Shared.pSibling;
                        Assert(iEntry == UINT32_MAX);
                        continue;
                    }
                }

                /* Scan the owner table for blocked owners. */
                if (    ASMAtomicUoReadU32(&pRec->Shared.cEntries) > 0
                    &&  (   !pRec->Shared.fSignaller
                         || iEntry != UINT32_MAX
                         || rtLockValidatorDdAreAllThreadsBlocked(&pRec->Shared, pThreadSelf)
                        )
                    )
                {
                    uint32_t                        cAllocated = pRec->Shared.cAllocated;
                    PRTLOCKVALRECSHRDOWN volatile  *papOwners  = pRec->Shared.papOwners;
                    while (++iEntry < cAllocated)
                    {
                        PRTLOCKVALRECSHRDOWN pEntry = rtLockValidatorUoReadSharedOwner(&papOwners[iEntry]);
                        if (pEntry)
                        {
                            for (;;)
                            {
                                if (pEntry->Core.u32Magic != RTLOCKVALRECSHRDOWN_MAGIC)
                                    break;
                                pNextThread = rtLockValidatorReadThreadHandle(&pEntry->hThread);
                                if (   !pNextThread
                                    || pNextThread->u32Magic != RTTHREADINT_MAGIC)
                                    break;
                                enmNextState = rtThreadGetState(pNextThread);
                                if (    !RTTHREAD_IS_SLEEPING(enmNextState)
                                    &&  pNextThread != pThreadSelf)
                                    break;
                                pNextRec = rtLockValidatorReadRecUnionPtr(&pNextThread->LockValidator.pRec);
                                if (RT_LIKELY(   !pNextRec
                                              || enmNextState == rtThreadGetState(pNextThread)))
                                    break;
                                pNextRec = NULL;
                            }
                            if (pNextRec)
                                break;
                        }
                        else
                            Assert(!pEntry || pEntry->Core.u32Magic == RTLOCKVALRECSHRDOWN_MAGIC_DEAD);
                    }
                    if (pNextRec)
                        break;
                    pNextThread = NIL_RTTHREAD;
                }

                /* Advance to the next sibling, if any. */
                pRec = pRec->Shared.pSibling;
                if (   pRec != NULL
                    && pRec != pFirstSibling)
                {
                    iEntry = UINT32_MAX;
                    continue;
                }
                break;

            case RTLOCKVALRECEXCL_MAGIC_DEAD:
            case RTLOCKVALRECSHRD_MAGIC_DEAD:
                break;

            case RTLOCKVALRECSHRDOWN_MAGIC:
            case RTLOCKVALRECSHRDOWN_MAGIC_DEAD:
            default:
                AssertMsgFailed(("%p: %#x\n", pRec, pRec->Core));
                break;
        }

        if (pNextRec)
        {
            /*
             * Recurse and check for deadlock.
             */
            uint32_t i = pStack->c;
            if (RT_UNLIKELY(i >= RT_ELEMENTS(pStack->a)))
                return rtLockValidatorDdHandleStackOverflow(pStack);

            pStack->c++;
            pStack->a[i].pRec           = pRec;
            pStack->a[i].iEntry         = iEntry;
            pStack->a[i].enmState       = enmState;
            pStack->a[i].pThread        = pThread;
            pStack->a[i].pFirstSibling  = pFirstSibling;

            if (RT_UNLIKELY(   pNextThread == pThreadSelf
                            && (   i != 0
                                || pRec->Core.u32Magic != RTLOCKVALRECSHRD_MAGIC
                                || !pRec->Shared.fSignaller) /* ASSUMES signaller records have no siblings. */
                            )
                )
                return rtLockValidatorDdVerifyDeadlock(pStack, pThreadSelf);

            pRec            = pNextRec;
            pFirstSibling   = pNextRec;
            iEntry          = UINT32_MAX;
            enmState        = enmNextState;
            pThread         = pNextThread;
        }
        else
        {
            /*
             * No deadlock here, unwind the stack and deal with any unfinished
             * business there.
             */
            uint32_t i = pStack->c;
            for (;;)
            {
                /* pop */
                if (i == 0)
                    return VINF_SUCCESS;
                i--;
                pRec    = pStack->a[i].pRec;
                iEntry  = pStack->a[i].iEntry;

                /* Examine it. */
                uint32_t u32Magic = pRec->Core.u32Magic;
                if (u32Magic == RTLOCKVALRECEXCL_MAGIC)
                    pRec = pRec->Excl.pSibling;
                else if (u32Magic == RTLOCKVALRECSHRD_MAGIC)
                {
                    if (iEntry + 1 < pRec->Shared.cAllocated)
                        break;  /* continue processing this record. */
                    pRec = pRec->Shared.pSibling;
                }
                else
                {
                    Assert(   u32Magic == RTLOCKVALRECEXCL_MAGIC_DEAD
                           || u32Magic == RTLOCKVALRECSHRD_MAGIC_DEAD);
                    continue;
                }

                /* Any next record to advance to? */
                if (   !pRec
                    || pRec == pStack->a[i].pFirstSibling)
                    continue;
                iEntry = UINT32_MAX;
                break;
            }

            /* Restore the rest of the state and update the stack. */
            pFirstSibling   = pStack->a[i].pFirstSibling;
            enmState        = pStack->a[i].enmState;
            pThread         = pStack->a[i].pThread;
            pStack->c       = i;
        }

        Assert(iLoop != 1000000);
    }
}


/**
 * Check for the simple no-deadlock case.
 *
 * @returns true if no deadlock, false if further investigation is required.
 *
 * @param   pOriginalRec    The original record.
 */
DECLINLINE(int) rtLockValidatorIsSimpleNoDeadlockCase(PRTLOCKVALRECUNION pOriginalRec)
{
    if (    pOriginalRec->Excl.Core.u32Magic == RTLOCKVALRECEXCL_MAGIC
        &&  !pOriginalRec->Excl.pSibling)
    {
        PRTTHREADINT pThread = rtLockValidatorReadThreadHandle(&pOriginalRec->Excl.hThread);
        if (   !pThread
            || pThread->u32Magic != RTTHREADINT_MAGIC)
            return true;
        RTTHREADSTATE enmState = rtThreadGetState(pThread);
        if (!RTTHREAD_IS_SLEEPING(enmState))
            return true;
    }
    return false;
}


/**
 * Worker for rtLockValidatorDeadlockDetection that bitches about a deadlock.
 *
 * @param   pStack          The chain of locks causing the deadlock.
 * @param   pRec            The record relating to the current thread's lock
 *                          operation.
 * @param   pThreadSelf     This thread.
 * @param   pSrcPos         Where we are going to deadlock.
 * @param   rc              The return code.
 */
static void rcLockValidatorDoDeadlockComplaining(PRTLOCKVALDDSTACK pStack, PRTLOCKVALRECUNION pRec,
                                                 PRTTHREADINT pThreadSelf, PCRTLOCKVALSRCPOS pSrcPos, int rc)
{
    if (!ASMAtomicUoReadBool(&g_fLockValidatorQuiet))
    {
        const char *pszWhat;
        switch (rc)
        {
            case VERR_SEM_LV_DEADLOCK:          pszWhat = "Detected deadlock!"; break;
            case VERR_SEM_LV_EXISTING_DEADLOCK: pszWhat = "Found existing deadlock!"; break;
            case VERR_SEM_LV_ILLEGAL_UPGRADE:   pszWhat = "Illegal lock upgrade!"; break;
            default:            AssertFailed(); pszWhat = "!unexpected rc!"; break;
        }
        rtLockValidatorComplainFirst(pszWhat, pSrcPos, pThreadSelf, pStack->a[0].pRec != pRec ? pRec : NULL);
        rtLockValidatorComplainMore("---- start of deadlock chain - %u entries ----\n", pStack->c);
        for (uint32_t i = 0; i < pStack->c; i++)
        {
            char szPrefix[24];
            RTStrPrintf(szPrefix, sizeof(szPrefix), "#%02u: ", i);
            PRTLOCKVALRECSHRDOWN pShrdOwner = NULL;
            if (pStack->a[i].pRec->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC)
                pShrdOwner = pStack->a[i].pRec->Shared.papOwners[pStack->a[i].iEntry];
            if (VALID_PTR(pShrdOwner) && pShrdOwner->Core.u32Magic == RTLOCKVALRECSHRDOWN_MAGIC)
                rtLockValidatorComplainAboutLock(szPrefix, (PRTLOCKVALRECUNION)pShrdOwner, "\n");
            else
                rtLockValidatorComplainAboutLock(szPrefix, pStack->a[i].pRec, "\n");
        }
        rtLockValidatorComplainMore("---- end of deadlock chain ----\n");
    }

    rtLockValidatorComplainPanic();
}


/**
 * Perform deadlock detection.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_SEM_LV_DEADLOCK
 * @retval  VERR_SEM_LV_EXISTING_DEADLOCK
 * @retval  VERR_SEM_LV_ILLEGAL_UPGRADE
 *
 * @param   pRec            The record relating to the current thread's lock
 *                          operation.
 * @param   pThreadSelf     The current thread.
 * @param   pSrcPos         The position of the current lock operation.
 */
static int rtLockValidatorDeadlockDetection(PRTLOCKVALRECUNION pRec, PRTTHREADINT pThreadSelf, PCRTLOCKVALSRCPOS pSrcPos)
{
#ifdef DEBUG_bird
    RTLOCKVALDDSTACK Stack;
    int rc = rtLockValidatorDdDoDetection(&Stack, pRec, pThreadSelf);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    if (rc == VERR_TRY_AGAIN)
    {
        for (uint32_t iLoop = 0; ; iLoop++)
        {
            rc = rtLockValidatorDdDoDetection(&Stack, pRec, pThreadSelf);
            if (RT_SUCCESS_NP(rc))
                return VINF_SUCCESS;
            if (rc != VERR_TRY_AGAIN)
                break;
            RTThreadYield();
            if (iLoop >= 3)
                return VINF_SUCCESS;
        }
    }

    rcLockValidatorDoDeadlockComplaining(&Stack, pRec, pThreadSelf, pSrcPos, rc);
    return rc;
#else
    return VINF_SUCCESS;
#endif
}


/**
 * Unlinks all siblings.
 *
 * This is used during record deletion and assumes no races.
 *
 * @param   pCore               One of the siblings.
 */
static void rtLockValidatorUnlinkAllSiblings(PRTLOCKVALRECCORE pCore)
{
    /* ASSUMES sibling destruction doesn't involve any races and that all
       related records are to be disposed off now.  */
    PRTLOCKVALRECUNION pSibling = (PRTLOCKVALRECUNION)pCore;
    while (pSibling)
    {
        PRTLOCKVALRECUNION volatile *ppCoreNext;
        switch (pSibling->Core.u32Magic)
        {
            case RTLOCKVALRECEXCL_MAGIC:
            case RTLOCKVALRECEXCL_MAGIC_DEAD:
                ppCoreNext = &pSibling->Excl.pSibling;
                break;

            case RTLOCKVALRECSHRD_MAGIC:
            case RTLOCKVALRECSHRD_MAGIC_DEAD:
                ppCoreNext = &pSibling->Shared.pSibling;
                break;

            default:
                AssertFailed();
                ppCoreNext = NULL;
                break;
        }
        if (RT_UNLIKELY(ppCoreNext))
            break;
        pSibling = (PRTLOCKVALRECUNION)ASMAtomicXchgPtr((void * volatile *)ppCoreNext, NULL);
    }
}


RTDECL(int) RTLockValidatorRecMakeSiblings(PRTLOCKVALRECCORE pRec1, PRTLOCKVALRECCORE pRec2)
{
    /*
     * Validate input.
     */
    PRTLOCKVALRECUNION p1 = (PRTLOCKVALRECUNION)pRec1;
    PRTLOCKVALRECUNION p2 = (PRTLOCKVALRECUNION)pRec2;

    AssertPtrReturn(p1, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(   p1->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC
                 || p1->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC
                 , VERR_SEM_LV_INVALID_PARAMETER);

    AssertPtrReturn(p2, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(   p2->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC
                 || p2->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC
                 , VERR_SEM_LV_INVALID_PARAMETER);

    /*
     * Link them (circular list).
     */
    if (    p1->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC
        &&  p2->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC)
    {
        p1->Excl.pSibling   = p2;
        p2->Shared.pSibling = p1;
    }
    else if (   p1->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC
             && p2->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC)
    {
        p1->Shared.pSibling = p2;
        p2->Excl.pSibling   = p1;
    }
    else
        AssertFailedReturn(VERR_SEM_LV_INVALID_PARAMETER); /* unsupported mix */

    return VINF_SUCCESS;
}


RTDECL(void) RTLockValidatorRecExclInit(PRTLOCKVALRECEXCL pRec, RTLOCKVALCLASS hClass,
                                        uint32_t uSubClass, const char *pszName, void *hLock)
{
    RTLOCKVAL_ASSERT_PTR_ALIGN(pRec);
    RTLOCKVAL_ASSERT_PTR_ALIGN(hLock);

    pRec->Core.u32Magic = RTLOCKVALRECEXCL_MAGIC;
    pRec->fEnabled      = RTLockValidatorIsEnabled();
    pRec->afReserved[0] = 0;
    pRec->afReserved[1] = 0;
    pRec->afReserved[2] = 0;
    rtLockValidatorSrcPosInit(&pRec->SrcPos);
    pRec->hThread       = NIL_RTTHREAD;
    pRec->pDown         = NULL;
    pRec->hClass        = rtLockValidatorClassValidateAndRetain(hClass);
    pRec->uSubClass     = uSubClass;
    pRec->cRecursion    = 0;
    pRec->hLock         = hLock;
    pRec->pszName       = pszName;
    pRec->pSibling      = NULL;

    /* Lazy initialization. */
    if (RT_UNLIKELY(g_hLockValidatorXRoads == NIL_RTSEMXROADS))
        rtLockValidatorLazyInit();
}


RTDECL(int)  RTLockValidatorRecExclCreate(PRTLOCKVALRECEXCL *ppRec, RTLOCKVALCLASS hClass,
                                          uint32_t uSubClass, const char *pszName, void *pvLock)
{
    PRTLOCKVALRECEXCL pRec;
    *ppRec = pRec = (PRTLOCKVALRECEXCL)RTMemAlloc(sizeof(*pRec));
    if (!pRec)
        return VERR_NO_MEMORY;

    RTLockValidatorRecExclInit(pRec, hClass, uSubClass, pszName, pvLock);

    return VINF_SUCCESS;
}


RTDECL(void) RTLockValidatorRecExclDelete(PRTLOCKVALRECEXCL pRec)
{
    Assert(pRec->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC);

    rtLockValidatorSerializeDestructEnter();

    ASMAtomicWriteU32(&pRec->Core.u32Magic, RTLOCKVALRECEXCL_MAGIC_DEAD);
    ASMAtomicWriteHandle(&pRec->hThread, NIL_RTTHREAD);
    ASMAtomicWriteHandle(&pRec->hClass, NIL_RTLOCKVALCLASS);
    if (pRec->pSibling)
        rtLockValidatorUnlinkAllSiblings(&pRec->Core);
    rtLockValidatorSerializeDestructLeave();
}


RTDECL(void) RTLockValidatorRecExclDestroy(PRTLOCKVALRECEXCL *ppRec)
{
    PRTLOCKVALRECEXCL pRec = *ppRec;
    *ppRec = NULL;
    if (pRec)
    {
        RTLockValidatorRecExclDelete(pRec);
        RTMemFree(pRec);
    }
}


RTDECL(void) RTLockValidatorRecExclSetOwner(PRTLOCKVALRECEXCL pRec, RTTHREAD hThreadSelf,
                                            PCRTLOCKVALSRCPOS pSrcPos, bool fFirstRecursion)
{
    AssertReturnVoid(pRec->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC);
    if (!pRec->fEnabled)
        return;
    if (hThreadSelf == NIL_RTTHREAD)
    {
        hThreadSelf = RTThreadSelfAutoAdopt();
        AssertReturnVoid(hThreadSelf != NIL_RTTHREAD);
    }
    Assert(hThreadSelf == RTThreadSelf());

    ASMAtomicIncS32(&hThreadSelf->LockValidator.cWriteLocks);

    if (pRec->hThread == hThreadSelf)
    {
        Assert(!fFirstRecursion);
        pRec->cRecursion++;
    }
    else
    {
        Assert(pRec->hThread == NIL_RTTHREAD);

        /*
         * Update the record.
         */
        rtLockValidatorSrcPosCopy(&pRec->SrcPos, pSrcPos);
        ASMAtomicUoWriteU32(&pRec->cRecursion, 1);
        ASMAtomicWriteHandle(&pRec->hThread, hThreadSelf);

        /*
         * Push the lock onto the lock stack.
         */
        /** @todo push it onto the per-thread lock stack. */
    }
}


RTDECL(int)  RTLockValidatorRecExclReleaseOwner(PRTLOCKVALRECEXCL pRec, bool fFinalRecursion)
{
    AssertReturn(pRec->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;
    AssertReturn(pRec->hThread != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);

    RTLockValidatorRecExclReleaseOwnerUnchecked(pRec);
    return VINF_SUCCESS;
}


RTDECL(void) RTLockValidatorRecExclReleaseOwnerUnchecked(PRTLOCKVALRECEXCL pRec)
{
    AssertReturnVoid(pRec->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC);
    if (!pRec->fEnabled)
        return;
    RTTHREADINT *pThread = pRec->hThread;
    AssertReturnVoid(pThread != NIL_RTTHREAD);
    Assert(pThread == RTThreadSelf());

    ASMAtomicDecS32(&pThread->LockValidator.cWriteLocks);

    if (ASMAtomicDecU32(&pRec->cRecursion) == 0)
    {
        /*
         * Pop (remove) the lock.
         */
        /** @todo remove it from the per-thread stack/whatever. */

        /*
         * Update the record.
         */
        ASMAtomicWriteHandle(&pRec->hThread, NIL_RTTHREAD);
    }
}


RTDECL(int) RTLockValidatorRecExclRecursion(PRTLOCKVALRECEXCL pRec, PCRTLOCKVALSRCPOS pSrcPos)
{
    AssertReturn(pRec->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;
    AssertReturn(pRec->hThread != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);

    Assert(pRec->cRecursion < _1M);
    pRec->cRecursion++;

    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorRecExclUnwind(PRTLOCKVALRECEXCL pRec)
{
    AssertReturn(pRec->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;
    AssertReturn(pRec->hThread != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRec->cRecursion > 0, VERR_SEM_LV_INVALID_PARAMETER);

    Assert(pRec->cRecursion);
    pRec->cRecursion--;
    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorRecExclRecursionMixed(PRTLOCKVALRECEXCL pRec, PRTLOCKVALRECCORE pRecMixed, PCRTLOCKVALSRCPOS pSrcPos)
{
    AssertReturn(pRec->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    PRTLOCKVALRECUNION pRecMixedU = (PRTLOCKVALRECUNION)pRecMixed;
    AssertReturn(   pRecMixedU->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC
                 || pRecMixedU->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC
                 , VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;
    AssertReturn(pRec->hThread != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRec->cRecursion > 0, VERR_SEM_LV_INVALID_PARAMETER);

    Assert(pRec->cRecursion < _1M);
    pRec->cRecursion++;

    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorRecExclUnwindMixed(PRTLOCKVALRECEXCL pRec, PRTLOCKVALRECCORE pRecMixed)
{
    AssertReturn(pRec->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    PRTLOCKVALRECUNION pRecMixedU = (PRTLOCKVALRECUNION)pRecMixed;
    AssertReturn(   pRecMixedU->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC
                 || pRecMixedU->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC
                 , VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;
    AssertReturn(pRec->hThread != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRec->cRecursion > 0, VERR_SEM_LV_INVALID_PARAMETER);

    Assert(pRec->cRecursion);
    pRec->cRecursion--;
    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorRecExclCheckOrder(PRTLOCKVALRECEXCL pRec, RTTHREAD hThreadSelf, PCRTLOCKVALSRCPOS pSrcPos)
{
    AssertReturn(pRec->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;

    /*
     * Check it locks we're currently holding.
     */
    /** @todo later */

    /*
     * If missing order rules, add them.
     */

    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorRecExclCheckBlocking(PRTLOCKVALRECEXCL pRec, RTTHREAD hThreadSelf,
                                                PCRTLOCKVALSRCPOS pSrcPos, bool fRecursiveOk,
                                                RTTHREADSTATE enmSleepState, bool fReallySleeping)
{
    /*
     * Fend off wild life.
     */
    PRTLOCKVALRECUNION pRecU = (PRTLOCKVALRECUNION)pRec;
    AssertPtrReturn(pRecU, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRecU->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRecU->Excl.fEnabled)
        return VINF_SUCCESS;

    PRTTHREADINT pThreadSelf = hThreadSelf;
    AssertPtrReturn(pThreadSelf, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pThreadSelf->u32Magic == RTTHREADINT_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    Assert(pThreadSelf == RTThreadSelf());

    AssertReturn(RTTHREAD_IS_SLEEPING(enmSleepState), VERR_SEM_LV_INVALID_PARAMETER);

    RTTHREADSTATE enmThreadState = rtThreadGetState(pThreadSelf);
    if (RT_UNLIKELY(enmThreadState != RTTHREADSTATE_RUNNING))
    {
        AssertReturn(   enmThreadState == RTTHREADSTATE_TERMINATED   /* rtThreadRemove uses locks too */
                     || enmThreadState == RTTHREADSTATE_INITIALIZING /* rtThreadInsert uses locks too */
                     , VERR_SEM_LV_INVALID_PARAMETER);
        enmSleepState = enmThreadState;
    }

    /*
     * Record the location.
     */
    rtLockValidatorWriteRecUnionPtr(&pThreadSelf->LockValidator.pRec, pRecU);
    rtLockValidatorSrcPosCopy(&pThreadSelf->LockValidator.SrcPos, pSrcPos);
    ASMAtomicWriteBool(&pThreadSelf->LockValidator.fInValidator, true);
    pThreadSelf->LockValidator.enmRecState = enmSleepState;
    rtThreadSetState(pThreadSelf, enmSleepState);

    /*
     * Don't do deadlock detection if we're recursing.
     *
     * On some hosts we don't do recursion accounting our selves and there
     * isn't any other place to check for this.
     */
    int rc = VINF_SUCCESS;
    if (rtLockValidatorReadThreadHandle(&pRecU->Excl.hThread) == pThreadSelf)
    {
        if (!fRecursiveOk)
        {
            rtLockValidatorComplainFirst("Recursion not allowed", pSrcPos, pThreadSelf, pRecU);
            rtLockValidatorComplainPanic();
            rc = VERR_SEM_LV_NESTED;
        }
    }
    /*
     * Perform deadlock detection.
     */
    else if (!rtLockValidatorIsSimpleNoDeadlockCase(pRecU))
        rc = rtLockValidatorDeadlockDetection(pRecU, pThreadSelf, pSrcPos);

    if (RT_SUCCESS(rc))
        ASMAtomicWriteBool(&pThreadSelf->fReallySleeping, fReallySleeping);
    else
    {
        rtThreadSetState(pThreadSelf, enmThreadState);
        rtLockValidatorWriteRecUnionPtr(&pThreadSelf->LockValidator.pRec, NULL);
    }
    ASMAtomicWriteBool(&pThreadSelf->LockValidator.fInValidator, false);
    return rc;
}
RT_EXPORT_SYMBOL(RTLockValidatorRecExclCheckBlocking);


RTDECL(int) RTLockValidatorRecExclCheckOrderAndBlocking(PRTLOCKVALRECEXCL pRec, RTTHREAD hThreadSelf,
                                                        PCRTLOCKVALSRCPOS pSrcPos, bool fRecursiveOk,
                                                        RTTHREADSTATE enmSleepState, bool fReallySleeping)
{
    int rc = RTLockValidatorRecExclCheckOrder(pRec, hThreadSelf, pSrcPos);
    if (RT_SUCCESS(rc))
        rc = RTLockValidatorRecExclCheckBlocking(pRec, hThreadSelf, pSrcPos, fRecursiveOk, enmSleepState, fReallySleeping);
    return rc;
}
RT_EXPORT_SYMBOL(RTLockValidatorRecExclCheckOrderAndBlocking);


RTDECL(void) RTLockValidatorRecSharedInit(PRTLOCKVALRECSHRD pRec, RTLOCKVALCLASS hClass, uint32_t uSubClass,
                                          const char *pszName, void *hLock, bool fSignaller)
{
    RTLOCKVAL_ASSERT_PTR_ALIGN(pRec);
    RTLOCKVAL_ASSERT_PTR_ALIGN(hLock);

    pRec->Core.u32Magic = RTLOCKVALRECSHRD_MAGIC;
    pRec->uSubClass     = uSubClass;
    pRec->hClass        = rtLockValidatorClassValidateAndRetain(hClass);
    pRec->hLock         = hLock;
    pRec->pszName       = pszName;
    pRec->fEnabled      = RTLockValidatorIsEnabled();
    pRec->fSignaller    = fSignaller;
    pRec->pSibling      = NULL;

    /* the table */
    pRec->cEntries      = 0;
    pRec->iLastEntry    = 0;
    pRec->cAllocated    = 0;
    pRec->fReallocating = false;
    pRec->fPadding      = false;
    pRec->papOwners     = NULL;
#if HC_ARCH_BITS == 32
    pRec->u32Alignment  = UINT32_MAX;
#endif
}


RTDECL(void) RTLockValidatorRecSharedDelete(PRTLOCKVALRECSHRD pRec)
{
    Assert(pRec->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC);

    /*
     * Flip it into table realloc mode and take the destruction lock.
     */
    rtLockValidatorSerializeDestructEnter();
    while (!ASMAtomicCmpXchgBool(&pRec->fReallocating, true, false))
    {
        rtLockValidatorSerializeDestructLeave();

        rtLockValidatorSerializeDetectionEnter();
        rtLockValidatorSerializeDetectionLeave();

        rtLockValidatorSerializeDestructEnter();
    }

    ASMAtomicWriteU32(&pRec->Core.u32Magic, RTLOCKVALRECSHRD_MAGIC_DEAD);
    ASMAtomicUoWriteHandle(&pRec->hClass, NIL_RTLOCKVALCLASS);
    if (pRec->papOwners)
    {
        PRTLOCKVALRECSHRDOWN volatile *papOwners = pRec->papOwners;
        ASMAtomicUoWritePtr((void * volatile *)&pRec->papOwners, NULL);
        ASMAtomicUoWriteU32(&pRec->cAllocated, 0);

        RTMemFree((void *)pRec->papOwners);
    }
    if (pRec->pSibling)
        rtLockValidatorUnlinkAllSiblings(&pRec->Core);
    ASMAtomicWriteBool(&pRec->fReallocating, false);

    rtLockValidatorSerializeDestructLeave();
}


/**
 * Locates an owner (thread) in a shared lock record.
 *
 * @returns Pointer to the owner entry on success, NULL on failure..
 * @param   pShared             The shared lock record.
 * @param   hThread             The thread (owner) to find.
 * @param   piEntry             Where to optionally return the table in index.
 *                              Optional.
 */
DECLINLINE(PRTLOCKVALRECSHRDOWN)
rtLockValidatorRecSharedFindOwner(PRTLOCKVALRECSHRD pShared, RTTHREAD hThread, uint32_t *piEntry)
{
    rtLockValidatorSerializeDetectionEnter();

    PRTLOCKVALRECSHRDOWN volatile *papOwners = pShared->papOwners;
    if (papOwners)
    {
        uint32_t const cMax = pShared->cAllocated;
        for (uint32_t iEntry = 0; iEntry < cMax; iEntry++)
        {
            PRTLOCKVALRECSHRDOWN pEntry = rtLockValidatorUoReadSharedOwner(&papOwners[iEntry]);
            if (pEntry && pEntry->hThread == hThread)
            {
                rtLockValidatorSerializeDetectionLeave();
                if (piEntry)
                    *piEntry = iEntry;
                return pEntry;
            }
        }
    }

    rtLockValidatorSerializeDetectionLeave();
    return NULL;
}


RTDECL(int) RTLockValidatorRecSharedCheckOrder(PRTLOCKVALRECSHRD pRec, RTTHREAD hThreadSelf, PCRTLOCKVALSRCPOS pSrcPos)
{
    AssertReturn(pRec->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;
    Assert(hThreadSelf == NIL_RTTHREAD || hThreadSelf == RTThreadSelf());

    /*
     * Check it locks we're currently holding.
     */
    /** @todo later */

    /*
     * If missing order rules, add them.
     */

    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorRecSharedCheckBlocking(PRTLOCKVALRECSHRD pRec, RTTHREAD hThreadSelf,
                                                  PCRTLOCKVALSRCPOS pSrcPos, bool fRecursiveOk,
                                                  RTTHREADSTATE enmSleepState, bool fReallySleeping)
{
    /*
     * Fend off wild life.
     */
    PRTLOCKVALRECUNION pRecU = (PRTLOCKVALRECUNION)pRec;
    AssertPtrReturn(pRecU, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRecU->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRecU->Shared.fEnabled)
        return VINF_SUCCESS;

    PRTTHREADINT pThreadSelf = hThreadSelf;
    AssertPtrReturn(pThreadSelf, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pThreadSelf->u32Magic == RTTHREADINT_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    Assert(pThreadSelf == RTThreadSelf());

    AssertReturn(RTTHREAD_IS_SLEEPING(enmSleepState), VERR_SEM_LV_INVALID_PARAMETER);

    RTTHREADSTATE enmThreadState = rtThreadGetState(pThreadSelf);
    if (RT_UNLIKELY(enmThreadState != RTTHREADSTATE_RUNNING))
    {
        AssertReturn(   enmThreadState == RTTHREADSTATE_TERMINATED   /* rtThreadRemove uses locks too */
                     || enmThreadState == RTTHREADSTATE_INITIALIZING /* rtThreadInsert uses locks too */
                     , VERR_SEM_LV_INVALID_PARAMETER);
        enmSleepState = enmThreadState;
    }

    /*
     * Record the location.
     */
    rtLockValidatorWriteRecUnionPtr(&pThreadSelf->LockValidator.pRec, pRecU);
    rtLockValidatorSrcPosCopy(&pThreadSelf->LockValidator.SrcPos, pSrcPos);
    ASMAtomicWriteBool(&pThreadSelf->LockValidator.fInValidator, true);
    pThreadSelf->LockValidator.enmRecState = enmSleepState;
    rtThreadSetState(pThreadSelf, enmSleepState);

    /*
     * Don't do deadlock detection if we're recursing.
     */
    int rc = VINF_SUCCESS;
    PRTLOCKVALRECSHRDOWN pEntry = !pRecU->Shared.fSignaller
                                ? rtLockValidatorRecSharedFindOwner(&pRecU->Shared, pThreadSelf, NULL)
                                : NULL;
    if (pEntry)
    {
        if (!fRecursiveOk)
        {
            rtLockValidatorComplainFirst("Recursion not allowed", pSrcPos, pThreadSelf, pRecU);
            rtLockValidatorComplainPanic();
            rc =  VERR_SEM_LV_NESTED;
        }
    }
    /*
     * Perform deadlock detection.
     */
    else if (!rtLockValidatorIsSimpleNoDeadlockCase(pRecU))
        rc = rtLockValidatorDeadlockDetection(pRecU, pThreadSelf, pSrcPos);

    if (RT_SUCCESS(rc))
        ASMAtomicWriteBool(&pThreadSelf->fReallySleeping, fReallySleeping);
    else
    {
        rtThreadSetState(pThreadSelf, enmThreadState);
        rtLockValidatorWriteRecUnionPtr(&pThreadSelf->LockValidator.pRec, NULL);
    }
    ASMAtomicWriteBool(&pThreadSelf->LockValidator.fInValidator, false);
    return rc;
}
RT_EXPORT_SYMBOL(RTLockValidatorRecSharedCheckBlocking);


RTDECL(int) RTLockValidatorRecSharedCheckOrderAndBlocking(PRTLOCKVALRECSHRD pRec, RTTHREAD hThreadSelf,
                                                          PCRTLOCKVALSRCPOS pSrcPos, bool fRecursiveOk,
                                                          RTTHREADSTATE enmSleepState, bool fReallySleeping)
{
    int rc = RTLockValidatorRecSharedCheckOrder(pRec, hThreadSelf, pSrcPos);
    if (RT_SUCCESS(rc))
        rc = RTLockValidatorRecSharedCheckBlocking(pRec, hThreadSelf, pSrcPos, fRecursiveOk, enmSleepState, fReallySleeping);
    return rc;
}
RT_EXPORT_SYMBOL(RTLockValidatorRecSharedCheckOrderAndBlocking);


/**
 * Allocates and initializes an owner entry for the shared lock record.
 *
 * @returns The new owner entry.
 * @param   pRec                The shared lock record.
 * @param   pThreadSelf         The calling thread and owner.  Used for record
 *                              initialization and allocation.
 * @param   pSrcPos             The source position.
 */
DECLINLINE(PRTLOCKVALRECSHRDOWN)
rtLockValidatorRecSharedAllocOwner(PRTLOCKVALRECSHRD pRec, PRTTHREADINT pThreadSelf, PCRTLOCKVALSRCPOS pSrcPos)
{
    PRTLOCKVALRECSHRDOWN pEntry;

    /*
     * Check if the thread has any statically allocated records we can easily
     * make use of.
     */
    unsigned iEntry = ASMBitFirstSetU32(ASMAtomicUoReadU32(&pThreadSelf->LockValidator.bmFreeShrdOwners));
    if (   iEntry > 0
        && ASMAtomicBitTestAndClear(&pThreadSelf->LockValidator.bmFreeShrdOwners, iEntry - 1))
    {
        pEntry = &pThreadSelf->LockValidator.aShrdOwners[iEntry - 1];
        Assert(!pEntry->fReserved);
        pEntry->fStaticAlloc = true;
        rtThreadGet(pThreadSelf);
    }
    else
    {
        pEntry = (PRTLOCKVALRECSHRDOWN)RTMemAlloc(sizeof(RTLOCKVALRECSHRDOWN));
        if (RT_UNLIKELY(!pEntry))
            return NULL;
        pEntry->fStaticAlloc = false;
    }

    pEntry->Core.u32Magic   = RTLOCKVALRECSHRDOWN_MAGIC;
    pEntry->cRecursion      = 1;
    pEntry->fReserved       = true;
    pEntry->hThread         = pThreadSelf;
    pEntry->pDown           = NULL;
    pEntry->pSharedRec      = pRec;
#if HC_ARCH_BITS == 32
    pEntry->pvReserved      = NULL;
#endif
    if (pSrcPos)
        pEntry->SrcPos      = *pSrcPos;
    else
        rtLockValidatorSrcPosInit(&pEntry->SrcPos);
    return pEntry;
}


/**
 * Frees an owner entry allocated by rtLockValidatorRecSharedAllocOwner.
 *
 * @param   pEntry              The owner entry.
 */
DECLINLINE(void) rtLockValidatorRecSharedFreeOwner(PRTLOCKVALRECSHRDOWN pEntry)
{
    if (pEntry)
    {
        Assert(pEntry->Core.u32Magic == RTLOCKVALRECSHRDOWN_MAGIC);
        ASMAtomicWriteU32(&pEntry->Core.u32Magic, RTLOCKVALRECSHRDOWN_MAGIC_DEAD);

        PRTTHREADINT pThread;
        ASMAtomicXchgHandle(&pEntry->hThread, NIL_RTTHREAD, &pThread);

        Assert(pEntry->fReserved);
        pEntry->fReserved = false;

        if (pEntry->fStaticAlloc)
        {
            AssertPtrReturnVoid(pThread);
            AssertReturnVoid(pThread->u32Magic == RTTHREADINT_MAGIC);

            uintptr_t iEntry = pEntry - &pThread->LockValidator.aShrdOwners[0];
            AssertReleaseReturnVoid(iEntry < RT_ELEMENTS(pThread->LockValidator.aShrdOwners));

            Assert(!ASMBitTest(&pThread->LockValidator.bmFreeShrdOwners, iEntry));
            ASMAtomicBitSet(&pThread->LockValidator.bmFreeShrdOwners, iEntry);

            rtThreadRelease(pThread);
        }
        else
        {
            rtLockValidatorSerializeDestructEnter();
            rtLockValidatorSerializeDestructLeave();

            RTMemFree(pEntry);
        }
    }
}


/**
 * Make more room in the table.
 *
 * @retval  true on success
 * @retval  false if we're out of memory or running into a bad race condition
 *          (probably a bug somewhere).  No longer holding the lock.
 *
 * @param   pShared             The shared lock record.
 */
static bool rtLockValidatorRecSharedMakeRoom(PRTLOCKVALRECSHRD pShared)
{
    for (unsigned i = 0; i < 1000; i++)
    {
        /*
         * Switch to the other data access direction.
         */
        rtLockValidatorSerializeDetectionLeave();
        if (i >= 10)
        {
            Assert(i != 10 && i != 100);
            RTThreadSleep(i >= 100);
        }
        rtLockValidatorSerializeDestructEnter();

        /*
         * Try grab the privilege to reallocating the table.
         */
        if (    pShared->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC
            &&  ASMAtomicCmpXchgBool(&pShared->fReallocating, true, false))
        {
            uint32_t cAllocated = pShared->cAllocated;
            if (cAllocated < pShared->cEntries)
            {
                /*
                 * Ok, still not enough space.  Reallocate the table.
                 */
#if 0  /** @todo enable this after making sure growing works flawlessly. */
                uint32_t                cInc = RT_ALIGN_32(pShared->cEntries - cAllocated, 16);
#else
                uint32_t                cInc = RT_ALIGN_32(pShared->cEntries - cAllocated, 1);
#endif
                PRTLOCKVALRECSHRDOWN   *papOwners;
                papOwners = (PRTLOCKVALRECSHRDOWN *)RTMemRealloc((void *)pShared->papOwners,
                                                                 (cAllocated + cInc) * sizeof(void *));
                if (!papOwners)
                {
                    ASMAtomicWriteBool(&pShared->fReallocating, false);
                    rtLockValidatorSerializeDestructLeave();
                    /* RTMemRealloc will assert */
                    return false;
                }

                while (cInc-- > 0)
                {
                    papOwners[cAllocated] = NULL;
                    cAllocated++;
                }

                ASMAtomicWritePtr((void * volatile *)&pShared->papOwners, papOwners);
                ASMAtomicWriteU32(&pShared->cAllocated, cAllocated);
            }
            ASMAtomicWriteBool(&pShared->fReallocating, false);
        }
        rtLockValidatorSerializeDestructLeave();

        rtLockValidatorSerializeDetectionEnter();
        if (RT_UNLIKELY(pShared->Core.u32Magic != RTLOCKVALRECSHRD_MAGIC))
            break;

        if (pShared->cAllocated >= pShared->cEntries)
            return true;
    }

    rtLockValidatorSerializeDetectionLeave();
    AssertFailed(); /* too many iterations or destroyed while racing. */
    return false;
}


/**
 * Adds an owner entry to a shared lock record.
 *
 * @returns true on success, false on serious race or we're if out of memory.
 * @param   pShared             The shared lock record.
 * @param   pEntry              The owner entry.
 */
DECLINLINE(bool) rtLockValidatorRecSharedAddOwner(PRTLOCKVALRECSHRD pShared, PRTLOCKVALRECSHRDOWN pEntry)
{
    rtLockValidatorSerializeDetectionEnter();
    if (RT_LIKELY(pShared->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC)) /* paranoia */
    {
        if (   ASMAtomicIncU32(&pShared->cEntries) > pShared->cAllocated /** @todo add fudge */
            && !rtLockValidatorRecSharedMakeRoom(pShared))
            return false; /* the worker leave the lock */

        PRTLOCKVALRECSHRDOWN volatile  *papOwners = pShared->papOwners;
        uint32_t const                  cMax      = pShared->cAllocated;
        for (unsigned i = 0; i < 100; i++)
        {
            for (uint32_t iEntry = 0; iEntry < cMax; iEntry++)
            {
                if (ASMAtomicCmpXchgPtr((void * volatile *)&papOwners[iEntry], pEntry, NULL))
                {
                    rtLockValidatorSerializeDetectionLeave();
                    return true;
                }
            }
            Assert(i != 25);
        }
        AssertFailed();
    }
    rtLockValidatorSerializeDetectionLeave();
    return false;
}


/**
 * Remove an owner entry from a shared lock record and free it.
 *
 * @param   pShared             The shared lock record.
 * @param   pEntry              The owner entry to remove.
 * @param   iEntry              The last known index.
 */
DECLINLINE(void) rtLockValidatorRecSharedRemoveAndFreeOwner(PRTLOCKVALRECSHRD pShared, PRTLOCKVALRECSHRDOWN pEntry,
                                                            uint32_t iEntry)
{
    /*
     * Remove it from the table.
     */
    rtLockValidatorSerializeDetectionEnter();
    AssertReturnVoidStmt(pShared->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC, rtLockValidatorSerializeDetectionLeave());
    if (RT_UNLIKELY(   iEntry >= pShared->cAllocated
                    || !ASMAtomicCmpXchgPtr((void * volatile *)&pShared->papOwners[iEntry], NULL, pEntry)))
    {
        /* this shouldn't happen yet... */
        AssertFailed();
        PRTLOCKVALRECSHRDOWN volatile  *papOwners = pShared->papOwners;
        uint32_t const                  cMax      = pShared->cAllocated;
        for (iEntry = 0; iEntry < cMax; iEntry++)
            if (ASMAtomicCmpXchgPtr((void * volatile *)&papOwners[iEntry], NULL, pEntry))
               break;
        AssertReturnVoidStmt(iEntry < cMax, rtLockValidatorSerializeDetectionLeave());
    }
    uint32_t cNow = ASMAtomicDecU32(&pShared->cEntries);
    Assert(!(cNow & RT_BIT_32(31))); NOREF(cNow);
    rtLockValidatorSerializeDetectionLeave();

    /*
     * Successfully removed, now free it.
     */
    rtLockValidatorRecSharedFreeOwner(pEntry);
}


RTDECL(void) RTLockValidatorRecSharedResetOwner(PRTLOCKVALRECSHRD pRec, RTTHREAD hThread, PCRTLOCKVALSRCPOS pSrcPos)
{
    AssertReturnVoid(pRec->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC);
    if (!pRec->fEnabled)
        return;
    AssertReturnVoid(hThread == NIL_RTTHREAD || hThread->u32Magic == RTTHREADINT_MAGIC);

    /*
     * Free all current owners.
     */
    rtLockValidatorSerializeDetectionEnter();
    while (ASMAtomicUoReadU32(&pRec->cEntries) > 0)
    {
        AssertReturnVoidStmt(pRec->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC, rtLockValidatorSerializeDetectionLeave());
        uint32_t                        iEntry     = 0;
        uint32_t                        cEntries   = pRec->cAllocated;
        PRTLOCKVALRECSHRDOWN volatile  *papEntries = pRec->papOwners;
        while (iEntry < cEntries)
        {
            PRTLOCKVALRECSHRDOWN pEntry = (PRTLOCKVALRECSHRDOWN)ASMAtomicXchgPtr((void * volatile *)&papEntries[iEntry], NULL);
            if (pEntry)
            {
                ASMAtomicDecU32(&pRec->cEntries);
                rtLockValidatorSerializeDetectionLeave();

                rtLockValidatorRecSharedFreeOwner(pEntry);

                rtLockValidatorSerializeDetectionEnter();
                if (ASMAtomicUoReadU32(&pRec->cEntries) == 0)
                    break;
                cEntries   = pRec->cAllocated;
                papEntries = pRec->papOwners;
            }
            iEntry++;
        }
    }
    rtLockValidatorSerializeDetectionLeave();

    if (hThread != NIL_RTTHREAD)
    {
        /*
         * Allocate a new owner entry and insert it into the table.
         */
        PRTLOCKVALRECSHRDOWN pEntry = rtLockValidatorRecSharedAllocOwner(pRec, hThread, pSrcPos);
        if (    pEntry
            &&  !rtLockValidatorRecSharedAddOwner(pRec, pEntry))
            rtLockValidatorRecSharedFreeOwner(pEntry);
    }
}
RT_EXPORT_SYMBOL(RTLockValidatorRecSharedResetOwner);


RTDECL(void) RTLockValidatorRecSharedAddOwner(PRTLOCKVALRECSHRD pRec, RTTHREAD hThread, PCRTLOCKVALSRCPOS pSrcPos)
{
    AssertReturnVoid(pRec->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC);
    if (!pRec->fEnabled)
        return;
    if (hThread == NIL_RTTHREAD)
    {
        hThread = RTThreadSelfAutoAdopt();
        AssertReturnVoid(hThread != NIL_RTTHREAD);
    }
    AssertReturnVoid(hThread != NIL_RTTHREAD);
    AssertReturnVoid(hThread->u32Magic == RTTHREADINT_MAGIC);

    /*
     * Recursive?
     *
     * Note! This code can be optimized to try avoid scanning the table on
     *       insert.  However, that's annoying work that makes the code big,
     *       so it can wait til later sometime.
     */
    PRTLOCKVALRECSHRDOWN pEntry = rtLockValidatorRecSharedFindOwner(pRec, hThread, NULL);
    if (pEntry)
    {
        Assert(hThread == RTThreadSelf());
        pEntry->cRecursion++;
        return;
    }

    /*
     * Allocate a new owner entry and insert it into the table.
     */
    pEntry = rtLockValidatorRecSharedAllocOwner(pRec, hThread, pSrcPos);
    if (    pEntry
        &&  !rtLockValidatorRecSharedAddOwner(pRec, pEntry))
        rtLockValidatorRecSharedFreeOwner(pEntry);
}
RT_EXPORT_SYMBOL(RTLockValidatorRecSharedAddOwner);


RTDECL(void) RTLockValidatorRecSharedRemoveOwner(PRTLOCKVALRECSHRD pRec, RTTHREAD hThread)
{
    AssertReturnVoid(pRec->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC);
    if (!pRec->fEnabled)
        return;
    AssertReturnVoid(hThread != NIL_RTTHREAD);
    AssertReturnVoid(hThread->u32Magic == RTTHREADINT_MAGIC);

    /*
     * Find the entry hope it's a recursive one.
     */
    uint32_t iEntry = UINT32_MAX; /* shuts up gcc */
    PRTLOCKVALRECSHRDOWN pEntry = rtLockValidatorRecSharedFindOwner(pRec, hThread, &iEntry);
    AssertReturnVoid(pEntry);
    if (pEntry->cRecursion > 1)
    {
        Assert(hThread == RTThreadSelf());
        pEntry->cRecursion--;
    }
    else
        rtLockValidatorRecSharedRemoveAndFreeOwner(pRec, pEntry, iEntry);
}
RT_EXPORT_SYMBOL(RTLockValidatorRecSharedRemoveOwner);


RTDECL(int) RTLockValidatorRecSharedCheckAndRelease(PRTLOCKVALRECSHRD pRec, RTTHREAD hThreadSelf)
{
    AssertReturn(pRec->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;
    if (hThreadSelf == NIL_RTTHREAD)
    {
        hThreadSelf = RTThreadSelfAutoAdopt();
        AssertReturn(hThreadSelf != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);
    }
    Assert(hThreadSelf == RTThreadSelf());

    /*
     * Locate the entry for this thread in the table.
     */
    uint32_t                iEntry = 0;
    PRTLOCKVALRECSHRDOWN    pEntry = rtLockValidatorRecSharedFindOwner(pRec, hThreadSelf, &iEntry);
    if (RT_UNLIKELY(!pEntry))
    {
        rtLockValidatorComplainFirst("Not owner (shared)", NULL, hThreadSelf, (PRTLOCKVALRECUNION)pRec);
        rtLockValidatorComplainPanic();
        return VERR_SEM_LV_NOT_OWNER;
    }

    /*
     * Check the release order.
     */
    if (pRec->hClass != NIL_RTLOCKVALCLASS)
    {
        /** @todo order validation */
    }

    /*
     * Release the ownership or unwind a level of recursion.
     */
    Assert(pEntry->cRecursion > 0);
    if (pEntry->cRecursion > 1)
        pEntry->cRecursion--;
    else
        rtLockValidatorRecSharedRemoveAndFreeOwner(pRec, pEntry, iEntry);

    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorRecSharedCheckSignaller(PRTLOCKVALRECSHRD pRec, RTTHREAD hThreadSelf)
{
    AssertReturn(pRec->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;
    if (hThreadSelf == NIL_RTTHREAD)
    {
        hThreadSelf = RTThreadSelfAutoAdopt();
        AssertReturn(hThreadSelf != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);
    }
    Assert(hThreadSelf == RTThreadSelf());

    /*
     * Locate the entry for this thread in the table.
     */
    uint32_t                iEntry = 0;
    PRTLOCKVALRECSHRDOWN    pEntry = rtLockValidatorRecSharedFindOwner(pRec, hThreadSelf, &iEntry);
    if (RT_UNLIKELY(!pEntry))
    {
        rtLockValidatorComplainFirst("Invalid signaller", NULL, hThreadSelf, (PRTLOCKVALRECUNION)pRec);
        rtLockValidatorComplainPanic();
        return VERR_SEM_LV_NOT_SIGNALLER;
    }
    return VINF_SUCCESS;
}


RTDECL(int32_t) RTLockValidatorWriteLockGetCount(RTTHREAD Thread)
{
    if (Thread == NIL_RTTHREAD)
        return 0;

    PRTTHREADINT pThread = rtThreadGet(Thread);
    if (!pThread)
        return VERR_INVALID_HANDLE;
    int32_t cWriteLocks = ASMAtomicReadS32(&pThread->LockValidator.cWriteLocks);
    rtThreadRelease(pThread);
    return cWriteLocks;
}
RT_EXPORT_SYMBOL(RTLockValidatorWriteLockGetCount);


RTDECL(void) RTLockValidatorWriteLockInc(RTTHREAD Thread)
{
    PRTTHREADINT pThread = rtThreadGet(Thread);
    AssertReturnVoid(pThread);
    ASMAtomicIncS32(&pThread->LockValidator.cWriteLocks);
    rtThreadRelease(pThread);
}
RT_EXPORT_SYMBOL(RTLockValidatorWriteLockInc);


RTDECL(void) RTLockValidatorWriteLockDec(RTTHREAD Thread)
{
    PRTTHREADINT pThread = rtThreadGet(Thread);
    AssertReturnVoid(pThread);
    ASMAtomicDecS32(&pThread->LockValidator.cWriteLocks);
    rtThreadRelease(pThread);
}
RT_EXPORT_SYMBOL(RTLockValidatorWriteLockDec);


RTDECL(int32_t) RTLockValidatorReadLockGetCount(RTTHREAD Thread)
{
    if (Thread == NIL_RTTHREAD)
        return 0;

    PRTTHREADINT pThread = rtThreadGet(Thread);
    if (!pThread)
        return VERR_INVALID_HANDLE;
    int32_t cReadLocks = ASMAtomicReadS32(&pThread->LockValidator.cReadLocks);
    rtThreadRelease(pThread);
    return cReadLocks;
}
RT_EXPORT_SYMBOL(RTLockValidatorReadLockGetCount);


RTDECL(void) RTLockValidatorReadLockInc(RTTHREAD Thread)
{
    PRTTHREADINT pThread = rtThreadGet(Thread);
    Assert(pThread);
    ASMAtomicIncS32(&pThread->LockValidator.cReadLocks);
    rtThreadRelease(pThread);
}
RT_EXPORT_SYMBOL(RTLockValidatorReadLockInc);


RTDECL(void) RTLockValidatorReadLockDec(RTTHREAD Thread)
{
    PRTTHREADINT pThread = rtThreadGet(Thread);
    Assert(pThread);
    ASMAtomicDecS32(&pThread->LockValidator.cReadLocks);
    rtThreadRelease(pThread);
}
RT_EXPORT_SYMBOL(RTLockValidatorReadLockDec);


RTDECL(void *) RTLockValidatorQueryBlocking(RTTHREAD hThread)
{
    void           *pvLock  = NULL;
    PRTTHREADINT    pThread = rtThreadGet(hThread);
    if (pThread)
    {
        RTTHREADSTATE enmState = rtThreadGetState(pThread);
        if (RTTHREAD_IS_SLEEPING(enmState))
        {
            rtLockValidatorSerializeDetectionEnter();

            enmState = rtThreadGetState(pThread);
            if (RTTHREAD_IS_SLEEPING(enmState))
            {
                PRTLOCKVALRECUNION pRec = rtLockValidatorReadRecUnionPtr(&pThread->LockValidator.pRec);
                if (pRec)
                {
                    switch (pRec->Core.u32Magic)
                    {
                        case RTLOCKVALRECEXCL_MAGIC:
                            pvLock = pRec->Excl.hLock;
                            break;

                        case RTLOCKVALRECSHRDOWN_MAGIC:
                            pRec = (PRTLOCKVALRECUNION)pRec->ShrdOwner.pSharedRec;
                            if (!pRec || pRec->Core.u32Magic != RTLOCKVALRECSHRD_MAGIC)
                                break;
                        case RTLOCKVALRECSHRD_MAGIC:
                            pvLock = pRec->Shared.hLock;
                            break;
                    }
                    if (RTThreadGetState(pThread) != enmState)
                        pvLock = NULL;
                }
            }

            rtLockValidatorSerializeDetectionLeave();
        }
        rtThreadRelease(pThread);
    }
    return pvLock;
}
RT_EXPORT_SYMBOL(RTLockValidatorQueryBlocking);


RTDECL(bool) RTLockValidatorIsBlockedThreadInValidator(RTTHREAD hThread)
{
    bool            fRet    = false;
    PRTTHREADINT    pThread = rtThreadGet(hThread);
    if (pThread)
    {
        fRet = ASMAtomicReadBool(&pThread->LockValidator.fInValidator);
        rtThreadRelease(pThread);
    }
    return fRet;
}
RT_EXPORT_SYMBOL(RTLockValidatorIsBlockedThreadInValidator);


RTDECL(bool) RTLockValidatorSetEnabled(bool fEnabled)
{
    return ASMAtomicXchgBool(&g_fLockValidatorEnabled, fEnabled);
}
RT_EXPORT_SYMBOL(RTLockValidatorSetEnabled);


RTDECL(bool) RTLockValidatorIsEnabled(void)
{
    return ASMAtomicUoReadBool(&g_fLockValidatorEnabled);
}
RT_EXPORT_SYMBOL(RTLockValidatorIsEnabled);


RTDECL(bool) RTLockValidatorSetQuiet(bool fQuiet)
{
    return ASMAtomicXchgBool(&g_fLockValidatorQuiet, fQuiet);
}
RT_EXPORT_SYMBOL(RTLockValidatorSetQuiet);


RTDECL(bool) RTLockValidatorAreQuiet(void)
{
    return ASMAtomicUoReadBool(&g_fLockValidatorQuiet);
}
RT_EXPORT_SYMBOL(RTLockValidatorAreQuiet);


RTDECL(bool) RTLockValidatorSetMayPanic(bool fMayPanic)
{
    return ASMAtomicXchgBool(&g_fLockValidatorMayPanic, fMayPanic);
}
RT_EXPORT_SYMBOL(RTLockValidatorSetMayPanic);


RTDECL(bool) RTLockValidatorMayPanic(void)
{
    return ASMAtomicUoReadBool(&g_fLockValidatorMayPanic);
}
RT_EXPORT_SYMBOL(RTLockValidatorMayPanic);

