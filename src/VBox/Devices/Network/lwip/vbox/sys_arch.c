/** $Id$ */
/** @file
 * System dependent parts of lwIP, implemented with IPRT.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */


#include <lwip/sys.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/critsect.h>
#include <iprt/thread.h>
#include <iprt/time.h>

/** @todo during my tests on Debian Lenny 64 bit I ran into trouble using
 * mutex semaphores (crash deep down in the pthreads lib). Using the write
 * case of rw semaphores also gives mutual exclusion, and didn't show those
 * crashes. Should be investigated, because this "fix" might be just covering
 * the symptoms of a bug elsewhere. */
#if HC_ARCH_BITS == 64 && defined RT_ARCH_LINUX
#define LWIPMUTEXTYPE RTSEMRW
#define LWIPMutexCreate RTSemRWCreate
#define LWIPMutexDestroy RTSemRWDestroy
#define LWIPMutexRequest RTSemRWRequestWrite
#define LWIPMutexRelease RTSemRWReleaseWrite
#else
#define LWIPMUTEXTYPE RTSEMMUTEX
#define LWIPMutexCreate RTSemMutexCreate
#define LWIPMutexDestroy RTSemMutexDestroy
#define LWIPMutexRequest RTSemMutexRequest
#define LWIPMutexRelease RTSemMutexRelease
#endif

/** Maximum number of threads lwIP is allowed to create. */
#define THREADS_MAX 5

/** Maximum number of mbox entries needed for reasonable performance. */
#define MBOX_ENTRIES_MAX 128

/** Data type for slots in TLS. */
typedef struct
{
    RTTHREAD tid;
    void (* thread)(void *arg);
    void *arg;
    struct sys_timeouts timeouts;
} THREADLOCALSTORAGE;

/** Actual declaration of the mbox type. */
struct sys_mbox
{
    LWIPMUTEXTYPE mutex;
    RTSEMEVENTMULTI nonempty, nonfull;
    void *apvEntries[MBOX_ENTRIES_MAX];
    u32_t head, tail;
};

#if SYS_LIGHTWEIGHT_PROT
/** Critical section variable for short term synchronization. */
static RTCRITSECT g_ProtCritSect;
#else
/** Synchronization for thread creation handling. */
static RTSEMEVENT g_ThreadSem;
#endif

/** Number of threads currently created by lwIP. */
static u32_t g_cThreads = 0;

/** The simulated thread local storage for lwIP things. */
static THREADLOCALSTORAGE g_aTLS[THREADS_MAX];

/**
 * Initialize the port to IPRT.
 */
void sys_init(void)
{
    int rc;
    unsigned i;
#if SYS_LIGHTWEIGHT_PROT
    rc = RTCritSectInit(&g_ProtCritSect);
    AssertRC(rc);
#else
    rc = RTSemEventCreate(&g_ThreadSem);
    AssertRC(rc);
    rc = RTSemEventSignal(g_ThreadSem);
    AssertRC(rc);
#endif
    for (i = 0; i < THREADS_MAX; i++)
        g_aTLS[i].tid = NIL_RTTHREAD;
}

/**
 * Create a new (binary) semaphore.
 */
sys_sem_t sys_sem_new(u8_t count)
{
    int rc;
    RTSEMEVENT sem;

    Assert(count <= 1);
    rc = RTSemEventCreate(&sem);
    AssertRC(rc);
    if (count == 1)
    {
        rc = RTSemEventSignal(sem);
        AssertRC(rc);
    }
    return sem;
}

/**
 * Destroy a (binary) semaphore.
 */
void sys_sem_free(sys_sem_t sem)
{
    int rc;
    rc = RTSemEventDestroy(sem);
    AssertRC(rc);
}

/**
 * Signal a (binary) semaphore.
 */
void sys_sem_signal(sys_sem_t sem)
{
    int rc;
    rc = RTSemEventSignal(sem);
    AssertRC(rc);
}

/**
 * Wait for a (binary) semaphore.
 */
u32_t sys_arch_sem_wait(sys_sem_t sem, u32_t timeout)
{
    int rc;
    unsigned cMillies;
    uint64_t tsStart, tsEnd;

    tsStart = RTTimeMilliTS();
    if (timeout == 0)
        cMillies = RT_INDEFINITE_WAIT;
    else
        cMillies = timeout;
    rc = RTSemEventWait(sem, cMillies);
    if (rc == VERR_TIMEOUT)
        return SYS_ARCH_TIMEOUT;
    AssertRC(rc);
    tsEnd = RTTimeMilliTS();
    return tsEnd - tsStart;
}

/**
 * Create new mbox.
 */
sys_mbox_t sys_mbox_new(void)
{
    int rc;
    struct sys_mbox *mbox;

    mbox = RTMemAllocZ(sizeof(*mbox));
    Assert(mbox != NULL);
    if (!mbox)
        return mbox;
    rc = LWIPMutexCreate(&mbox->mutex);
    AssertRC(rc);
    if (RT_FAILURE(rc))
    {
        RTMemFree(mbox);
        return NULL;
    }
    rc = RTSemEventMultiCreate(&mbox->nonempty);
    AssertRC(rc);
    if (RT_FAILURE(rc))
    {
        rc = LWIPMutexDestroy(mbox->mutex);
        AssertRC(rc);
        RTMemFree(mbox);
        return NULL;
    }
    rc = RTSemEventMultiCreate(&mbox->nonfull);
    AssertRC(rc);
    if (RT_FAILURE(rc))
    {
        rc = RTSemEventMultiDestroy(mbox->nonempty);
        AssertRC(rc);
        rc = LWIPMutexDestroy(mbox->mutex);
        AssertRC(rc);
        RTMemFree(mbox);
        return NULL;
    }
    return mbox;
}

/**
 * Free an mbox.
 */
void sys_mbox_free(sys_mbox_t mbox)
{
    Assert(mbox != NULL);
    LWIPMutexDestroy(mbox->mutex);
    RTSemEventMultiDestroy(mbox->nonempty);
    RTSemEventMultiDestroy(mbox->nonfull);
    RTMemFree(mbox);
}

/**
 * Place an entry in an mbox.
 */
void sys_mbox_post(sys_mbox_t mbox, void *msg)
{
    int rc;

    Assert(mbox != NULL);
    rc = LWIPMutexRequest(mbox->mutex, RT_INDEFINITE_WAIT);
    AssertRC(rc);
    while ((mbox->head + 1) % MBOX_ENTRIES_MAX == mbox->tail)
    {
        /* mbox is full, have to wait until a slot becomes available. */
        rc = LWIPMutexRelease(mbox->mutex);
        AssertRC(rc);
        rc = RTSemEventMultiWait(mbox->nonfull, RT_INDEFINITE_WAIT);
        AssertRC(rc);
        rc = LWIPMutexRequest(mbox->mutex, RT_INDEFINITE_WAIT);
        AssertRC(rc);
    }
    if (mbox->head == mbox->tail)
    {
        rc = RTSemEventMultiSignal(mbox->nonempty);
        AssertRC(rc);
    }
    mbox->apvEntries[mbox->head] = msg;
    mbox->head++;
    mbox->head %= MBOX_ENTRIES_MAX;
    if ((mbox->head + 1) % MBOX_ENTRIES_MAX == mbox->tail)
    {
        rc = RTSemEventMultiReset(mbox->nonfull);
        AssertRC(rc);
    }
    rc = LWIPMutexRelease(mbox->mutex);
    AssertRC(rc);
}

/**
 * Get an entry from an mbox.
 */
u32_t sys_arch_mbox_fetch(sys_mbox_t mbox, void **msg, u32_t timeout)
{
    int rc;
    unsigned cMillies;
    uint64_t tsStart, tsEnd;

    Assert(mbox != NULL);
    tsStart = RTTimeMilliTS();
    if (timeout == 0)
        cMillies = RT_INDEFINITE_WAIT;
    else
        cMillies = timeout;
    rc = LWIPMutexRequest(mbox->mutex, cMillies);
    AssertRC(rc);
    while (mbox->head == mbox->tail)
    {
        /* mbox is empty, have to wait until a slot is filled. */
        rc = LWIPMutexRelease(mbox->mutex);
        AssertRC(rc);
        if (timeout != 0)
        {
            tsEnd = RTTimeMilliTS();
            if (tsEnd - tsStart >= cMillies)
                return SYS_ARCH_TIMEOUT;
            cMillies -= tsEnd - tsStart;
        }
        rc = RTSemEventMultiWait(mbox->nonempty, cMillies);
        if (rc == VERR_TIMEOUT)
            return SYS_ARCH_TIMEOUT;
        AssertRC(rc);
        if (timeout != 0)
        {
            tsEnd = RTTimeMilliTS();
            if (tsEnd - tsStart >= cMillies)
                return SYS_ARCH_TIMEOUT;
            cMillies -= tsEnd - tsStart;
        }
        rc = LWIPMutexRequest(mbox->mutex, cMillies);
        if (rc == VERR_TIMEOUT)
            return SYS_ARCH_TIMEOUT;
        AssertRC(rc);
    }
    if ((mbox->head + 1) % MBOX_ENTRIES_MAX == mbox->tail)
    {
        rc = RTSemEventMultiSignal(mbox->nonfull);
        AssertRC(rc);
    }
    if (msg != NULL)
        *msg = mbox->apvEntries[mbox->tail];
    mbox->tail++;
    mbox->tail %= MBOX_ENTRIES_MAX;
    rc = RTSemEventMultiSignal(mbox->nonfull);
    if (mbox->head == mbox->tail)
    {
        rc = RTSemEventMultiReset(mbox->nonempty);
        AssertRC(rc);
    }
    rc = LWIPMutexRelease(mbox->mutex);
    AssertRC(rc);
    tsEnd = RTTimeMilliTS();
    return tsEnd - tsStart;
}

/**
 * Grab the pointer to this thread's timeouts from TLS.
 */
struct sys_timeouts *sys_arch_timeouts(void)
{
    unsigned i;
#if SYS_LIGHTWEIGHT_PROT
    SYS_ARCH_DECL_PROTECT(old_level);
#endif
    RTTHREAD myself;
    struct sys_timeouts *to = NULL;

    myself = RTThreadSelf();
#if SYS_LIGHTWEIGHT_PROT
    SYS_ARCH_PROTECT(old_level);
#else
    RTSemEventWait(g_ThreadSem, RT_INDEFINITE_WAIT);
#endif
    for (i = 0; i < g_cThreads; i++)
    {
        if (g_aTLS[i].tid == myself)
        {
            to = &g_aTLS[i].timeouts;
            break;
        }
    }
    /* Auto-adopt new threads which use lwIP as they pop up. */
    if (!to)
    {
        unsigned id;
        id = g_cThreads;
        g_cThreads++;
        Assert(g_cThreads <= THREADS_MAX);
        g_aTLS[id].tid = myself;
        to = &g_aTLS[id].timeouts;
    }
#if SYS_LIGHTWEIGHT_PROT
    SYS_ARCH_UNPROTECT(old_level);
#else
    RTSemEventSignal(g_ThreadSem);
#endif
    return to;
}

/**
 * Internal: thread main function adapter, dropping the first parameter. Needed
 * to make lwip thread main function compatible with IPRT thread main function.
 */
static int sys_thread_adapter(RTTHREAD ThreadSelf, void *pvUser)
{
    THREADLOCALSTORAGE *tls = (THREADLOCALSTORAGE *)pvUser;
    tls->thread(tls->arg);
    return 0;
}

/**
 * Create new thread.
 */
sys_thread_t sys_thread_new(void (* thread)(void *arg), void *arg, int prio)
{
    int rc;
    char *pszThread = NULL;
#if SYS_LIGHTWEIGHT_PROT
    SYS_ARCH_DECL_PROTECT(old_level);
#endif
    unsigned id;
    RTTHREAD tid;

#if SYS_LIGHTWEIGHT_PROT
    SYS_ARCH_PROTECT(old_level);
#else
    RTSemEventWait(g_ThreadSem, RT_INDEFINITE_WAIT);
#endif
    id = g_cThreads;
    g_cThreads++;
    Assert(g_cThreads <= THREADS_MAX);
    g_aTLS[id].thread = thread;
    g_aTLS[id].arg = arg;
    RTStrAPrintf(&pszThread, "lwIP%u", id);
    rc = RTThreadCreate(&tid, sys_thread_adapter, &g_aTLS[id], 0,
                        RTTHREADTYPE_IO, 0, pszThread);
    if (RT_FAILURE(rc))
    {
        g_cThreads--;
        tid = NIL_RTTHREAD;
    }
    else
        g_aTLS[id].tid = tid;
#if SYS_LIGHTWEIGHT_PROT
    SYS_ARCH_UNPROTECT(old_level);
#else
    RTSemEventSignal(g_ThreadSem);
#endif
    AssertRC(rc);
    return tid;
}

#if SYS_LIGHTWEIGHT_PROT
/**
 * Start a short critical section.
 */
sys_prot_t sys_arch_protect(void)
{
    int rc;
    rc = RTCritSectEnter(&g_ProtCritSect);
    AssertRC(rc);
    return NULL;
}
#endif

#if SYS_LIGHTWEIGHT_PROT
/**
 * End a short critical section.
 */
void sys_arch_unprotect(sys_prot_t pval)
{
    int rc;
    (void)pval;
    rc = RTCritSectLeave(&g_ProtCritSect);
    AssertRC(rc);
}
#endif
