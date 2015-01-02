/* $Id$ */
/** @file
 * VBoxDTraceR0.
 */

/*
 * Copyright (c) 2012 bird
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/sup.h>
#include <VBox/log.h>

#include <iprt/asm-amd64-x86.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/mp.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/time.h>

#include <sys/dtrace_impl.h>

#include <VBox/VBoxTpG.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#if !defined(RT_OS_WINDOWS) && !defined(RT_OS_OS2)
# define HAVE_RTMEMALLOCEX_FEATURES
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/** Caller indicator. */
typedef enum VBOXDTCALLER
{
    kVBoxDtCaller_Invalid = 0,
    kVBoxDtCaller_Generic,
    kVBoxDtCaller_ProbeFireUser,
    kVBoxDtCaller_ProbeFireKernel
} VBOXDTCALLER;

/**
 * Stack data used for thread structure and such.
 *
 * This is planted in every external entry point and used to emulate solaris
 * curthread, CRED, curproc and similar.  It is also used to get at the
 * uncached probe arguments.
 */
typedef struct VBoxDtStackData
{
    /** Eyecatcher no. 1 (VBDT_STACK_DATA_MAGIC2). */
    uint32_t                u32Magic1;
    /** Eyecatcher no. 2 (VBDT_STACK_DATA_MAGIC2). */
    uint32_t                u32Magic2;
    /** The format of the caller specific data. */
    VBOXDTCALLER            enmCaller;
    /** Caller specific data.  */
    union
    {
        /** kVBoxDtCaller_ProbeFireKernel. */
        struct
        {
            /** The caller. */
            uintptr_t               uCaller;
            /** Pointer to the stack arguments of a probe function call. */
            uintptr_t              *pauStackArgs;
        } ProbeFireKernel;
        /** kVBoxDtCaller_ProbeFireUser. */
        struct
        {
            /** The user context.  */
            PCSUPDRVTRACERUSRCTX    pCtx;
        } ProbeFireUser;
    } u;
    /** Credentials allocated by VBoxDtGetCurrentCreds. */
    struct VBoxDtCred      *pCred;
    /** Thread structure currently being held by this thread. */
    struct VBoxDtThread    *pThread;
    /** Pointer to this structure.
     * This is the final bit of integrity checking. */
    struct VBoxDtStackData *pSelf;
} VBDTSTACKDATA;
/** Pointer to the on-stack thread specific data. */
typedef VBDTSTACKDATA *PVBDTSTACKDATA;

/** The first magic value. */
#define VBDT_STACK_DATA_MAGIC1      RT_MAKE_U32_FROM_U8('V', 'B', 'o', 'x')
/** The second magic value. */
#define VBDT_STACK_DATA_MAGIC2      RT_MAKE_U32_FROM_U8('D', 'T', 'r', 'c')

/** The alignment of the stack data.
 * The data doesn't require more than sizeof(uintptr_t) alignment, but the
 * greater alignment the quicker lookup. */
#define VBDT_STACK_DATA_ALIGN       32

/** Plants the stack data. */
#define VBDT_SETUP_STACK_DATA(a_enmCaller) \
    uint8_t abBlob[sizeof(VBoxDtStackData) + VBDT_STACK_DATA_ALIGN - 1]; \
    PVBDTSTACKDATA pStackData = (PVBDTSTACKDATA)(   (uintptr_t)&abBlob[VBDT_STACK_DATA_ALIGN - 1] \
                                                 &        ~(uintptr_t)(VBDT_STACK_DATA_ALIGN - 1)); \
    pStackData->u32Magic1   = VBDT_STACK_DATA_MAGIC1; \
    pStackData->u32Magic2   = VBDT_STACK_DATA_MAGIC2; \
    pStackData->enmCaller   = a_enmCaller; \
    pStackData->pCred       = NULL; \
    pStackData->pThread     = NULL; \
    pStackData->pSelf       = pStackData

/** Passifies the stack data and frees up resource held within it. */
#define VBDT_CLEAR_STACK_DATA() \
    do \
    { \
        pStackData->u32Magic1   = 0; \
        pStackData->u32Magic2   = 0; \
        pStackData->pSelf       = NULL; \
        if (pStackData->pCred) \
            crfree(pStackData->pCred); \
        if (pStackData->pThread) \
            VBoxDtReleaseThread(pStackData->pThread); \
    } while (0)


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Per CPU information */
cpucore_t                       g_aVBoxDtCpuCores[RTCPUSET_MAX_CPUS];
/** Dummy mutex. */
struct VBoxDtMutex              g_DummyMtx;
/** Pointer to the tracer helpers provided by VBoxDrv. */
static PCSUPDRVTRACERHLP        g_pVBoxDTraceHlp;

dtrace_cacheid_t dtrace_predcache_id = DTRACE_CACHEIDNONE + 1;

#if 0
void           (*dtrace_cpu_init)(processorid_t);
void           (*dtrace_modload)(struct modctl *);
void           (*dtrace_modunload)(struct modctl *);
void           (*dtrace_helpers_cleanup)(void);
void           (*dtrace_helpers_fork)(proc_t *, proc_t *);
void           (*dtrace_cpustart_init)(void);
void           (*dtrace_cpustart_fini)(void);
void           (*dtrace_cpc_fire)(uint64_t);
void           (*dtrace_debugger_init)(void);
void           (*dtrace_debugger_fini)(void);
#endif


/**
 * Gets the stack data.
 *
 * @returns Pointer to the stack data.  Never NULL.
 */
static PVBDTSTACKDATA vboxDtGetStackData(void)
{
    int volatile    iDummy = 1; /* use this to get the stack address. */
    PVBDTSTACKDATA  pData = (PVBDTSTACKDATA)(  ((uintptr_t)&iDummy + VBDT_STACK_DATA_ALIGN - 1)
                                             & ~(uintptr_t)(VBDT_STACK_DATA_ALIGN - 1));
    for (;;)
    {
        if (   pData->u32Magic1 == VBDT_STACK_DATA_MAGIC1
            && pData->u32Magic2 == VBDT_STACK_DATA_MAGIC2
            && pData->pSelf     == pData)
            return pData;
        pData = (PVBDTSTACKDATA)((uintptr_t)pData + VBDT_STACK_DATA_ALIGN);
    }
}


void dtrace_toxic_ranges(void (*pfnAddOne)(uintptr_t uBase, uintptr_t cbRange))
{
    /** @todo ? */
}



/**
 * Dummy callback used by dtrace_sync.
 */
static DECLCALLBACK(void) vboxDtSyncCallback(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    NOREF(idCpu); NOREF(pvUser1); NOREF(pvUser2);
}


/**
 * Synchronzie across all CPUs (expensive).
 */
void    dtrace_sync(void)
{
    int rc = RTMpOnAll(vboxDtSyncCallback, NULL, NULL);
    AssertRC(rc);
}


/**
 * Fetch a 8-bit "word" from userland.
 *
 * @return  The byte value.
 * @param   pvUserAddr      The userland address.
 */
uint8_t  dtrace_fuword8( void *pvUserAddr)
{
    uint8_t u8;
    int rc = RTR0MemUserCopyFrom(&u8, (uintptr_t)pvUserAddr, sizeof(u8));
    if (RT_FAILURE(rc))
    {
        RTCPUID iCpu = VBDT_GET_CPUID();
        cpu_core[iCpu].cpuc_dtrace_flags |= CPU_DTRACE_BADADDR;
        cpu_core[iCpu].cpuc_dtrace_illval = (uintptr_t)pvUserAddr;
        u8 = 0;
    }
    return u8;
}


/**
 * Fetch a 16-bit word from userland.
 *
 * @return  The word value.
 * @param   pvUserAddr      The userland address.
 */
uint16_t dtrace_fuword16(void *pvUserAddr)
{
    uint16_t u16;
    int rc = RTR0MemUserCopyFrom(&u16, (uintptr_t)pvUserAddr, sizeof(u16));
    if (RT_FAILURE(rc))
    {
        RTCPUID iCpu = VBDT_GET_CPUID();
        cpu_core[iCpu].cpuc_dtrace_flags |= CPU_DTRACE_BADADDR;
        cpu_core[iCpu].cpuc_dtrace_illval = (uintptr_t)pvUserAddr;
        u16 = 0;
    }
    return u16;
}


/**
 * Fetch a 32-bit word from userland.
 *
 * @return  The dword value.
 * @param   pvUserAddr      The userland address.
 */
uint32_t dtrace_fuword32(void *pvUserAddr)
{
    uint32_t u32;
    int rc = RTR0MemUserCopyFrom(&u32, (uintptr_t)pvUserAddr, sizeof(u32));
    if (RT_FAILURE(rc))
    {
        RTCPUID iCpu = VBDT_GET_CPUID();
        cpu_core[iCpu].cpuc_dtrace_flags |= CPU_DTRACE_BADADDR;
        cpu_core[iCpu].cpuc_dtrace_illval = (uintptr_t)pvUserAddr;
        u32 = 0;
    }
    return u32;
}


/**
 * Fetch a 64-bit word from userland.
 *
 * @return  The qword value.
 * @param   pvUserAddr      The userland address.
 */
uint64_t dtrace_fuword64(void *pvUserAddr)
{
    uint64_t u64;
    int rc = RTR0MemUserCopyFrom(&u64, (uintptr_t)pvUserAddr, sizeof(u64));
    if (RT_FAILURE(rc))
    {
        RTCPUID iCpu = VBDT_GET_CPUID();
        cpu_core[iCpu].cpuc_dtrace_flags |= CPU_DTRACE_BADADDR;
        cpu_core[iCpu].cpuc_dtrace_illval = (uintptr_t)pvUserAddr;
        u64 = 0;
    }
    return u64;
}


/** copyin implementation */
int  VBoxDtCopyIn(void const *pvUser, void *pvDst, size_t cb)
{
    int rc = RTR0MemUserCopyFrom(pvDst, (uintptr_t)pvUser, cb);
    return RT_SUCCESS(rc) ? 0 : -1;
}


/** copyout implementation */
int  VBoxDtCopyOut(void const *pvSrc, void *pvUser, size_t cb)
{
    int rc = RTR0MemUserCopyTo((uintptr_t)pvUser, pvSrc, cb);
    return RT_SUCCESS(rc) ? 0 : -1;
}


/**
 * Copy data from userland into the kernel.
 *
 * @param   uUserAddr           The userland address.
 * @param   uKrnlAddr           The kernel buffer address.
 * @param   cb                  The number of bytes to copy.
 * @param   pfFlags             Pointer to the relevant cpuc_dtrace_flags.
 */
void dtrace_copyin(    uintptr_t uUserAddr, uintptr_t uKrnlAddr, size_t cb, volatile uint16_t *pfFlags)
{
    int rc = RTR0MemUserCopyFrom((void *)uKrnlAddr, uUserAddr, cb);
    if (RT_FAILURE(rc))
    {
        *pfFlags |= CPU_DTRACE_BADADDR;
        cpu_core[VBDT_GET_CPUID()].cpuc_dtrace_illval = uUserAddr;
    }
}


/**
 * Copy data from the kernel into userlad.
 *
 * @param   uKrnlAddr           The kernel buffer address.
 * @param   uUserAddr           The userland address.
 * @param   cb                  The number of bytes to copy.
 * @param   pfFlags             Pointer to the relevant cpuc_dtrace_flags.
 */
void dtrace_copyout(   uintptr_t uKrnlAddr, uintptr_t uUserAddr, size_t cb, volatile uint16_t *pfFlags)
{
    int rc = RTR0MemUserCopyTo(uUserAddr, (void const *)uKrnlAddr, cb);
    if (RT_FAILURE(rc))
    {
        *pfFlags |= CPU_DTRACE_BADADDR;
        cpu_core[VBDT_GET_CPUID()].cpuc_dtrace_illval = uUserAddr;
    }
}


/**
 * Copy a string from userland into the kernel.
 *
 * @param   uUserAddr           The userland address.
 * @param   uKrnlAddr           The kernel buffer address.
 * @param   cbMax               The maximum number of bytes to copy. May stop
 *                              earlier if zero byte is encountered.
 * @param   pfFlags             Pointer to the relevant cpuc_dtrace_flags.
 */
void dtrace_copyinstr( uintptr_t uUserAddr, uintptr_t uKrnlAddr, size_t cbMax, volatile uint16_t *pfFlags)
{
    if (!cbMax)
        return;

    char   *pszDst = (char *)uKrnlAddr;
    int     rc = RTR0MemUserCopyFrom(pszDst, uUserAddr, cbMax);
    if (RT_FAILURE(rc))
    {
        /* Byte by byte - lazy bird! */
        size_t off = 0;
        while (off < cbMax)
        {
            rc = RTR0MemUserCopyFrom(&pszDst[off], uUserAddr + off, 1);
            if (RT_FAILURE(rc))
            {
                *pfFlags |= CPU_DTRACE_BADADDR;
                cpu_core[VBDT_GET_CPUID()].cpuc_dtrace_illval = uUserAddr;
                pszDst[off] = '\0';
                return;
            }
            if (!pszDst[off])
                return;
            off++;
        }
    }

    pszDst[cbMax - 1] = '\0';
}


/**
 * Copy a string from the kernel and into user land.
 *
 * @param   uKrnlAddr           The kernel string address.
 * @param   uUserAddr           The userland address.
 * @param   cbMax               The maximum number of bytes to copy.  Will stop
 *                              earlier if zero byte is encountered.
 * @param   pfFlags             Pointer to the relevant cpuc_dtrace_flags.
 */
void dtrace_copyoutstr(uintptr_t uKrnlAddr, uintptr_t uUserAddr, size_t cbMax, volatile uint16_t *pfFlags)
{
    const char *pszSrc   = (const char *)uKrnlAddr;
    size_t      cbActual = RTStrNLen(pszSrc, cbMax);
    cbActual += cbActual < cbMax;
    dtrace_copyout(uKrnlAddr,uUserAddr, cbActual, pfFlags);
}


/**
 * Get the caller @a cCallFrames call frames up the stack.
 *
 * @returns The caller's return address or ~(uintptr_t)0.
 * @param   cCallFrames         The number of frames.
 */
uintptr_t dtrace_caller(int cCallFrames)
{
    PVBDTSTACKDATA pData = vboxDtGetStackData();
    if (pData->enmCaller == kVBoxDtCaller_ProbeFireKernel)
        return pData->u.ProbeFireKernel.uCaller;
    return ~(uintptr_t)0;
}


/**
 * Get argument number @a iArg @a cCallFrames call frames up the stack.
 *
 * @returns The caller's return address or ~(uintptr_t)0.
 * @param   iArg                The argument to get.
 * @param   cCallFrames         The number of frames.
 */
uint64_t dtrace_getarg(int iArg, int cCallFrames)
{
    PVBDTSTACKDATA pData = vboxDtGetStackData();
    AssertReturn(iArg >= 5, UINT64_MAX);

    if (pData->enmCaller == kVBoxDtCaller_ProbeFireKernel)
        return pData->u.ProbeFireKernel.pauStackArgs[iArg - 5];
    return UINT64_MAX;
}


/**
 * Produce a traceback of the kernel stack.
 *
 * @param   paPcStack           Where to return the program counters.
 * @param   cMaxFrames          The maximum number of PCs to return.
 * @param   cSkipFrames         The number of artificial callstack frames to
 *                              skip at the top.
 * @param   pIntr               Not sure what this is...
 */
void dtrace_getpcstack(pc_t *paPcStack, int cMaxFrames, int cSkipFrames, uint32_t *pIntr)
{
    int iFrame = 0;
    while (iFrame < cMaxFrames)
    {
        paPcStack[iFrame] = NULL;
        iFrame++;
    }
}


/**
 * Get the number of call frames on the stack.
 *
 * @returns The stack depth.
 * @param   cSkipFrames         The number of artificial callstack frames to
 *                              skip at the top.
 */
int dtrace_getstackdepth(int cSkipFrames)
{
    return 1;
}


/**
 * Produce a traceback of the userland stack.
 *
 * @param   paPcStack           Where to return the program counters.
 * @param   paFpStack           Where to return the frame pointers.
 * @param   cMaxFrames          The maximum number of frames to return.
 */
void dtrace_getufpstack(uint64_t *paPcStack, uint64_t *paFpStack, int cMaxFrames)
{
    int iFrame = 0;
    while (iFrame < cMaxFrames)
    {
        paPcStack[iFrame] = 0;
        paFpStack[iFrame] = 0;
        iFrame++;
    }
}


/**
 * Produce a traceback of the userland stack.
 *
 * @param   paPcStack           Where to return the program counters.
 * @param   cMaxFrames          The maximum number of frames to return.
 */
void dtrace_getupcstack(uint64_t *paPcStack, int cMaxFrames)
{
    int iFrame = 0;
    while (iFrame < cMaxFrames)
    {
        paPcStack[iFrame] = 0;
        iFrame++;
    }
}


/**
 * Computes the depth of the userland stack.
 */
int dtrace_getustackdepth(void)
{
    return 0;
}


/**
 * Get the current IPL/IRQL.
 *
 * @returns Current level.
 */
int dtrace_getipl(void)
{
#ifdef RT_ARCH_AMD64
    /* CR8 is normally the same as IRQL / IPL on AMD64. */
    return ASMGetCR8();
#else
    /* Just fake it on x86. */
    return !ASMIntAreEnabled();
#endif
}


/**
 * Get current monotonic timestamp.
 *
 * @returns Timestamp, nano seconds.
 */
hrtime_t dtrace_gethrtime(void)
{
    return RTTimeNanoTS();
}


/**
 * Get current walltime.
 *
 * @returns Timestamp, nano seconds.
 */
hrtime_t dtrace_gethrestime(void)
{
    /** @todo try get better resolution here somehow ... */
    RTTIMESPEC Now;
    return RTTimeSpecGetNano(RTTimeNow(&Now));
}


/**
 * DTrace panic routine.
 *
 * @param   pszFormat           Panic message.
 * @param   va                  Arguments to the panic message.
 */
void dtrace_vpanic(const char *pszFormat, va_list va)
{
    RTAssertMsg1(NULL, __LINE__, __FILE__, __FUNCTION__);
    RTAssertMsg2WeakV(pszFormat, va);
    RTR0AssertPanicSystem();
    for (;;)
    {
        ASMBreakpoint();
        volatile char *pchCrash = (volatile char *)~(uintptr_t)0;
        *pchCrash = '\0';
    }
}


/**
 * DTrace panic routine.
 *
 * @param   pszFormat           Panic message.
 * @param   ...                 Arguments to the panic message.
 */
void VBoxDtPanic(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    dtrace_vpanic(pszFormat, va);
    va_end(va);
}


/**
 * DTrace kernel message routine.
 *
 * @param   pszFormat           Kernel message.
 * @param   ...                 Arguments to the panic message.
 */
void VBoxDtCmnErr(int iLevel, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    SUPR0Printf("%N", pszFormat, va);
    va_end(va);
}


/** uprintf implementation */
void VBoxDtUPrintf(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    VBoxDtUPrintfV(pszFormat, va);
    va_end(va);
}


/** vuprintf implementation */
void VBoxDtUPrintfV(const char *pszFormat, va_list va)
{
    SUPR0Printf("%N", pszFormat, va);
}


/* CRED implementation. */
cred_t *VBoxDtGetCurrentCreds(void)
{
    PVBDTSTACKDATA pData = vboxDtGetStackData();
    if (!pData->pCred)
    {
        struct VBoxDtCred *pCred;
#ifdef HAVE_RTMEMALLOCEX_FEATURES
        int rc = RTMemAllocEx(sizeof(*pCred), 0, RTMEMALLOCEX_FLAGS_ANY_CTX, (void **)&pCred);
#else
        int rc = RTMemAllocEx(sizeof(*pCred), 0, 0, (void **)&pCred);
#endif
        AssertFatalRC(rc);
        pCred->cr_refs  = 1;
        /** @todo get the right creds on unix systems. */
        pCred->cr_uid   = 0;
        pCred->cr_ruid  = 0;
        pCred->cr_suid  = 0;
        pCred->cr_gid   = 0;
        pCred->cr_rgid  = 0;
        pCred->cr_sgid  = 0;
        pCred->cr_zone  = 0;
        pData->pCred = pCred;
    }

    return pData->pCred;
}


/* crhold implementation */
void VBoxDtCredHold(struct VBoxDtCred *pCred)
{
    int32_t cRefs = ASMAtomicIncS32(&pCred->cr_refs);
    Assert(cRefs > 1);
}


/* crfree implementation */
void VBoxDtCredFree(struct VBoxDtCred *pCred)
{
    int32_t cRefs = ASMAtomicDecS32(&pCred->cr_refs);
    Assert(cRefs >= 0);
    if (!cRefs)
        RTMemFree(pCred);
}

/** Spinlock protecting the thread structures. */
static RTSPINLOCK           g_hThreadSpinlock = NIL_RTSPINLOCK;
/** List of threads by usage age. */
static RTLISTANCHOR         g_ThreadAgeList;
/** Hash table for looking up thread structures.  */
static struct VBoxDtThread *g_apThreadsHash[16384];
/** Fake kthread_t structures.
 * The size of this array is making horrible ASSUMPTIONS about the number of
 * thread in the system that will be subjected to DTracing. */
static struct VBoxDtThread  g_aThreads[8192];


static int vboxDtInitThreadDb(void)
{
    int rc = RTSpinlockCreate(&g_hThreadSpinlock, RTSPINLOCK_FLAGS_INTERRUPT_SAFE, "VBoxDtThreadDb");
    if (RT_FAILURE(rc))
        return rc;

    RTListInit(&g_ThreadAgeList);
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aThreads); i++)
    {
        g_aThreads[i].hNative = NIL_RTNATIVETHREAD;
        g_aThreads[i].uPid    = NIL_RTPROCESS;
        RTListPrepend(&g_ThreadAgeList, &g_aThreads[i].AgeEntry);
    }

    return VINF_SUCCESS;
}


static void vboxDtTermThreadDb(void)
{
    RTSpinlockDestroy(g_hThreadSpinlock);
    g_hThreadSpinlock = NIL_RTSPINLOCK;
    RTListInit(&g_ThreadAgeList);
}


/* curthread implementation, providing a fake kthread_t. */
struct VBoxDtThread *VBoxDtGetCurrentThread(void)
{
    /*
     * Once we've retrieved a thread, we hold on to it until the thread exits
     * the VBoxDTrace module.
     */
    PVBDTSTACKDATA  pData       = vboxDtGetStackData();
    if (pData->pThread)
    {
        AssertPtr(pData->pThread);
        Assert(pData->pThread->hNative   == RTThreadNativeSelf());
        Assert(pData->pThread->uPid      == RTProcSelf());
        Assert(RTListIsEmpty(&pData->pThread->AgeEntry));
        return pData->pThread;
    }

    /*
     * Lookup the thread in the hash table.
     */
    RTNATIVETHREAD  hNativeSelf = RTThreadNativeSelf();
    RTPROCESS       uPid        = RTProcSelf();
    uintptr_t       iHash       = (hNativeSelf * 2654435761) % RT_ELEMENTS(g_apThreadsHash);

    RTSpinlockAcquire(g_hThreadSpinlock);

    struct VBoxDtThread *pThread = g_apThreadsHash[iHash];
    while (pThread)
    {
        if (pThread->hNative == hNativeSelf)
        {
            if (pThread->uPid != uPid)
            {
                /* Re-initialize the reused thread. */
                pThread->uPid           = uPid;
                pThread->t_dtrace_vtime = 0;
                pThread->t_dtrace_start = 0;
                pThread->t_dtrace_stop  = 0;
                pThread->t_dtrace_scrpc = 0;
                pThread->t_dtrace_astpc = 0;
                pThread->t_predcache    = 0;
            }

            /* Hold the thread in the on-stack data, making sure it does not
               get reused till the thread leaves VBoxDTrace. */
            RTListNodeRemove(&pThread->AgeEntry);
            pData->pThread = pThread;

            RTSpinlockReleaseNoInts(g_hThreadSpinlock);
            return pThread;
        }

        pThread = pThread->pNext;
    }

    /*
     * Unknown thread.  Allocate a new entry, recycling unused or old ones.
     */
    pThread = RTListGetLast(&g_ThreadAgeList, struct VBoxDtThread, AgeEntry);
    AssertFatal(pThread);
    RTListNodeRemove(&pThread->AgeEntry);
    if (pThread->hNative != NIL_RTNATIVETHREAD)
    {
        uintptr_t   iHash2 = (pThread->hNative * 2654435761) % RT_ELEMENTS(g_apThreadsHash);
        if (g_apThreadsHash[iHash2] == pThread)
            g_apThreadsHash[iHash2] = pThread->pNext;
        else
        {
            for (struct VBoxDtThread *pPrev = g_apThreadsHash[iHash2]; ; pPrev = pPrev->pNext)
            {
                AssertPtr(pPrev);
                if (pPrev->pNext == pThread)
                {
                    pPrev->pNext = pThread->pNext;
                    break;
                }
            }
        }
    }

    /*
     * Initialize the data.
     */
    pThread->t_dtrace_vtime = 0;
    pThread->t_dtrace_start = 0;
    pThread->t_dtrace_stop  = 0;
    pThread->t_dtrace_scrpc = 0;
    pThread->t_dtrace_astpc = 0;
    pThread->t_predcache    = 0;
    pThread->hNative        = hNativeSelf;
    pThread->uPid           = uPid;

    /*
     * Add it to the hash as well as the on-stack data.
     */
    pThread->pNext = g_apThreadsHash[iHash];
    g_apThreadsHash[iHash] = pThread->pNext;

    pData->pThread = pThread;

    RTSpinlockReleaseNoInts(g_hThreadSpinlock);
    return pThread;
}


/**
 * Called by the stack data destructor.
 *
 * @param   pThread         The thread to release.
 *
 */
static void VBoxDtReleaseThread(struct VBoxDtThread *pThread)
{
    RTSpinlockAcquire(g_hThreadSpinlock);

    RTListAppend(&g_ThreadAgeList, &pThread->AgeEntry);

    RTSpinlockReleaseNoInts(g_hThreadSpinlock);
}




/*
 *
 * Virtual Memory / Resource Allocator.
 * Virtual Memory / Resource Allocator.
 * Virtual Memory / Resource Allocator.
 *
 */


/** The number of bits per chunk.
 * @remarks The 32 bytes are for heap headers and such like.  */
#define VBOXDTVMEMCHUNK_BITS    ( ((_64K - 32 - sizeof(uint32_t) * 2) / sizeof(uint32_t)) * 32)

/**
 * Resource allocator chunk.
 */
typedef struct  VBoxDtVMemChunk
{
    /** The ordinal (unbased) of the first item. */
    uint32_t            iFirst;
    /** The current number of free items in this chunk. */
    uint32_t            cCurFree;
    /** The allocation bitmap. */
    uint32_t            bm[VBOXDTVMEMCHUNK_BITS / 32];
} VBOXDTVMEMCHUNK;
/** Pointer to a resource allocator chunk. */
typedef VBOXDTVMEMCHUNK *PVBOXDTVMEMCHUNK;



/**
 * Resource allocator instance.
 */
typedef struct VBoxDtVMem
{
    /** Spinlock protecting the data. */
    RTSPINLOCK          hSpinlock;
    /** Magic value. */
    uint32_t            u32Magic;
    /** The current number of free items in the chunks. */
    uint32_t            cCurFree;
    /** The current number of chunks that we have allocated. */
    uint32_t            cCurChunks;
    /** The configured resource base. */
    uint32_t            uBase;
    /** The configured max number of items. */
    uint32_t            cMaxItems;
    /** The size of the apChunks array. */
    uint32_t            cMaxChunks;
    /** Array of chunk pointers.
     * (The size is determined at creation.) */
    PVBOXDTVMEMCHUNK    apChunks[1];
} VBOXDTVMEM;
/** Pointer to a resource allocator instance. */
typedef VBOXDTVMEM *PVBOXDTVMEM;

/** Magic value for the VBOXDTVMEM structure. */
#define VBOXDTVMEM_MAGIC        RT_MAKE_U32_FROM_U8('V', 'M',  'e',  'm')


/* vmem_create implementation */
struct VBoxDtVMem *VBoxDtVMemCreate(const char *pszName, void *pvBase, size_t cb, size_t cbUnit,
                                    PFNRT pfnAlloc, PFNRT pfnFree, struct VBoxDtVMem *pSrc,
                                    size_t cbQCacheMax, uint32_t fFlags)
{
    /*
     * Assert preconditions of this implementation.
     */
    AssertMsgReturn((uintptr_t)pvBase <= UINT32_MAX, ("%p\n", pvBase), NULL);
    AssertMsgReturn(cb <= UINT32_MAX, ("%zu\n", cb), NULL);
    AssertMsgReturn((uintptr_t)pvBase + cb - 1 <= UINT32_MAX, ("%p %zu\n", pvBase, cb), NULL);
    AssertMsgReturn(cbUnit == 1, ("%zu\n", cbUnit), NULL);
    AssertReturn(!pfnAlloc, NULL);
    AssertReturn(!pfnFree, NULL);
    AssertReturn(!pSrc, NULL);
    AssertReturn(!cbQCacheMax, NULL);
    AssertReturn(fFlags & VM_SLEEP, NULL);
    AssertReturn(fFlags & VMC_IDENTIFIER, NULL);

    /*
     * Allocate the instance.
     */
    uint32_t cChunks = (uint32_t)cb / VBOXDTVMEMCHUNK_BITS;
    if (cb % VBOXDTVMEMCHUNK_BITS)
        cChunks++;
    PVBOXDTVMEM pThis = (PVBOXDTVMEM)RTMemAllocZ(RT_OFFSETOF(VBOXDTVMEM, apChunks[cChunks]));
    if (!pThis)
        return NULL;
    int rc = RTSpinlockCreate(&pThis->hSpinlock, RTSPINLOCK_FLAGS_INTERRUPT_SAFE, "VBoxDtVMem");
    if (RT_FAILURE(rc))
    {
        RTMemFree(pThis);
        return NULL;
    }
    pThis->u32Magic     = VBOXDTVMEM_MAGIC;
    pThis->cCurFree     = 0;
    pThis->cCurChunks   = 0;
    pThis->uBase        = (uint32_t)(uintptr_t)pvBase;
    pThis->cMaxItems    = (uint32_t)cb;
    pThis->cMaxChunks   = cChunks;

    return pThis;
}


/* vmem_destroy implementation */
void  VBoxDtVMemDestroy(struct VBoxDtVMem *pThis)
{
    if (!pThis)
        return;
    AssertPtrReturnVoid(pThis);
    AssertReturnVoid(pThis->u32Magic == VBOXDTVMEM_MAGIC);

    /*
     * Invalidate the instance.
     */
    RTSpinlockAcquire(pThis->hSpinlock); /* paranoia */
    pThis->u32Magic = 0;
    RTSpinlockRelease(pThis->hSpinlock);
    RTSpinlockDestroy(pThis->hSpinlock);

    /*
     * Free the chunks, then the instance.
     */
    uint32_t iChunk = pThis->cCurChunks;
    while (iChunk-- > 0)
    {
        RTMemFree(pThis->apChunks[iChunk]);
        pThis->apChunks[iChunk] = NULL;
    }
    RTMemFree(pThis);
}


/* vmem_alloc implementation */
void *VBoxDtVMemAlloc(struct VBoxDtVMem *pThis, size_t cbMem, uint32_t fFlags)
{
    /*
     * Validate input.
     */
    AssertReturn(fFlags & VM_BESTFIT, NULL);
    AssertReturn(fFlags & VM_SLEEP, NULL);
    AssertReturn(cbMem == 1, NULL);
    AssertPtrReturn(pThis, NULL);
    AssertReturn(pThis->u32Magic == VBOXDTVMEM_MAGIC, NULL);

    /*
     * Allocation loop.
     */
    RTSpinlockAcquire(pThis->hSpinlock);
    for (;;)
    {
        PVBOXDTVMEMCHUNK pChunk;
        uint32_t const   cChunks = pThis->cCurChunks;

        if (RT_LIKELY(pThis->cCurFree > 0))
        {
            for (uint32_t iChunk = 0; iChunk < cChunks; iChunk++)
            {
                pChunk = pThis->apChunks[iChunk];
                if (pChunk->cCurFree > 0)
                {
                    int iBit = ASMBitFirstClear(pChunk->bm, VBOXDTVMEMCHUNK_BITS);
                    AssertMsgReturnStmt(iBit >= 0 && (unsigned)iBit < VBOXDTVMEMCHUNK_BITS, ("%d\n", iBit),
                                        RTSpinlockRelease(pThis->hSpinlock),
                                        NULL);

                    ASMBitSet(pChunk->bm, iBit);
                    pChunk->cCurFree--;
                    pThis->cCurFree--;

                    uint32_t iRet = (uint32_t)iBit + pChunk->iFirst + pThis->uBase;
                    RTSpinlockReleaseNoInts(pThis->hSpinlock);
                    return (void *)(uintptr_t)iRet;
                }
            }
            AssertFailedBreak();
        }

        /* Out of resources? */
        if (cChunks >= pThis->cMaxChunks)
            break;

        /*
         * Allocate another chunk.
         */
        uint32_t const  iFirstBit = cChunks > 0 ? pThis->apChunks[cChunks - 1]->iFirst + VBOXDTVMEMCHUNK_BITS : 0;
        uint32_t const  cFreeBits = cChunks + 1 == pThis->cMaxChunks
                                  ? pThis->cMaxItems - (iFirstBit - pThis->uBase)
                                  : VBOXDTVMEMCHUNK_BITS;
        Assert(cFreeBits <= VBOXDTVMEMCHUNK_BITS);

        RTSpinlockRelease(pThis->hSpinlock);

        pChunk = (PVBOXDTVMEMCHUNK)RTMemAllocZ(sizeof(*pChunk));
        if (!pChunk)
            return NULL;

        pChunk->iFirst   = iFirstBit;
        pChunk->cCurFree = cFreeBits;
        if (cFreeBits != VBOXDTVMEMCHUNK_BITS)
        {
            /* lazy bird. */
            uint32_t iBit = cFreeBits;
            while (iBit < VBOXDTVMEMCHUNK_BITS)
            {
                ASMBitSet(pChunk->bm, iBit);
                iBit++;
            }
        }

        RTSpinlockAcquire(pThis->hSpinlock);

        /*
         * Insert the new chunk.  If someone raced us here, we'll drop it to
         * avoid wasting resources.
         */
        if (pThis->cCurChunks == cChunks)
        {
            pThis->apChunks[cChunks] = pChunk;
            pThis->cCurFree   += pChunk->cCurFree;
            pThis->cCurChunks += 1;
        }
        else
        {
            RTSpinlockRelease(pThis->hSpinlock);
            RTMemFree(pChunk);
            RTSpinlockAcquire(pThis->hSpinlock);
        }
    }
    RTSpinlockReleaseNoInts(pThis->hSpinlock);

    return NULL;
}

/* vmem_free implementation */
void VBoxDtVMemFree(struct VBoxDtVMem *pThis, void *pvMem, size_t cbMem)
{
    /*
     * Validate input.
     */
    AssertReturnVoid(cbMem == 1);
    AssertPtrReturnVoid(pThis);
    AssertReturnVoid(pThis->u32Magic == VBOXDTVMEM_MAGIC);

    AssertReturnVoid((uintptr_t)pvMem < UINT32_MAX);
    uint32_t uMem = (uint32_t)(uintptr_t)pvMem;
    AssertReturnVoid(uMem >= pThis->uBase);
    uMem -= pThis->uBase;
    AssertReturnVoid(uMem < pThis->cMaxItems);


    /*
     * Free it.
     */
    RTSpinlockAcquire(pThis->hSpinlock);
    uint32_t const iChunk = uMem / VBOXDTVMEMCHUNK_BITS;
    if (iChunk < pThis->cCurChunks)
    {
        PVBOXDTVMEMCHUNK pChunk = pThis->apChunks[iChunk];
        uint32_t iBit = uMem - pChunk->iFirst;
        AssertReturnVoidStmt(iBit < VBOXDTVMEMCHUNK_BITS, RTSpinlockRelease(pThis->hSpinlock));
        AssertReturnVoidStmt(ASMBitTestAndClear(pChunk->bm, iBit), RTSpinlockRelease(pThis->hSpinlock));

        pChunk->cCurFree++;
        pThis->cCurFree++;
    }

    RTSpinlockRelease(pThis->hSpinlock);
}


/*
 *
 * Memory Allocators.
 * Memory Allocators.
 * Memory Allocators.
 *
 */


/* kmem_alloc implementation */
void *VBoxDtKMemAlloc(size_t cbMem, uint32_t fFlags)
{
    void    *pvMem;
#ifdef HAVE_RTMEMALLOCEX_FEATURES
    uint32_t fMemAllocFlags = fFlags & KM_NOSLEEP ? RTMEMALLOCEX_FLAGS_ANY_CTX : 0;
#else
    uint32_t fMemAllocFlags = 0;
#endif
    int rc = RTMemAllocEx(cbMem, 0, fMemAllocFlags, &pvMem);
    AssertRCReturn(rc, NULL);
    AssertPtr(pvMem);
    return pvMem;
}


/* kmem_zalloc implementation */
void *VBoxDtKMemAllocZ(size_t cbMem, uint32_t fFlags)
{
    void    *pvMem;
#ifdef HAVE_RTMEMALLOCEX_FEATURES
    uint32_t fMemAllocFlags = (fFlags & KM_NOSLEEP ? RTMEMALLOCEX_FLAGS_ANY_CTX : 0) | RTMEMALLOCEX_FLAGS_ZEROED;
#else
    uint32_t fMemAllocFlags = RTMEMALLOCEX_FLAGS_ZEROED;
#endif
    int rc = RTMemAllocEx(cbMem, 0, fMemAllocFlags, &pvMem);
    AssertRCReturn(rc, NULL);
    AssertPtr(pvMem);
    return pvMem;
}


/* kmem_free implementation */
void  VBoxDtKMemFree(void *pvMem, size_t cbMem)
{
    RTMemFreeEx(pvMem, cbMem);
}


/**
 * Memory cache mockup structure.
 * No slab allocator here!
 */
struct VBoxDtMemCache
{
    uint32_t u32Magic;
    size_t cbBuf;
    size_t cbAlign;
};


/* Limited kmem_cache_create implementation. */
struct VBoxDtMemCache *VBoxDtKMemCacheCreate(const char *pszName, size_t cbBuf, size_t cbAlign,
                                             PFNRT pfnCtor, PFNRT pfnDtor, PFNRT pfnReclaim,
                                             void *pvUser, void *pvVM, uint32_t fFlags)
{
    /*
     * Check the input.
     */
    AssertReturn(cbBuf > 0 && cbBuf < _1G, NULL);
    AssertReturn(RT_IS_POWER_OF_TWO(cbAlign), NULL);
    AssertReturn(!pfnCtor, NULL);
    AssertReturn(!pfnDtor, NULL);
    AssertReturn(!pfnReclaim, NULL);
    AssertReturn(!pvUser, NULL);
    AssertReturn(!pvVM, NULL);
    AssertReturn(!fFlags, NULL);

    /*
     * Create a parameter container. Don't bother with anything fancy here yet,
     * just get something working.
     */
    struct VBoxDtMemCache *pThis = (struct VBoxDtMemCache *)RTMemAlloc(sizeof(*pThis));
    if (!pThis)
        return NULL;

    pThis->cbAlign = cbAlign;
    pThis->cbBuf   = cbBuf;
    return pThis;
}


/* Limited kmem_cache_destroy implementation. */
void  VBoxDtKMemCacheDestroy(struct VBoxDtMemCache *pThis)
{
    RTMemFree(pThis);
}


/* kmem_cache_alloc implementation. */
void *VBoxDtKMemCacheAlloc(struct VBoxDtMemCache *pThis, uint32_t fFlags)
{
    void    *pvMem;
#ifdef HAVE_RTMEMALLOCEX_FEATURES
    uint32_t fMemAllocFlags = (fFlags & KM_NOSLEEP ? RTMEMALLOCEX_FLAGS_ANY_CTX : 0) | RTMEMALLOCEX_FLAGS_ZEROED;
#else
    uint32_t fMemAllocFlags = RTMEMALLOCEX_FLAGS_ZEROED;
#endif
    int rc = RTMemAllocEx(pThis->cbBuf, pThis->cbAlign, fMemAllocFlags, &pvMem);
    AssertRCReturn(rc, NULL);
    AssertPtr(pvMem);
    return pvMem;
}


/* kmem_cache_free implementation. */
void  VBoxDtKMemCacheFree(struct VBoxDtMemCache *pThis, void *pvMem)
{
    RTMemFreeEx(pvMem, pThis->cbBuf);
}


/*
 *
 * Mutex Sempahore Wrappers.
 *
 */


/** Initializes a mutex. */
int VBoxDtMutexInit(struct VBoxDtMutex *pMtx)
{
    AssertReturn(pMtx != &g_DummyMtx, -1);
    AssertPtr(pMtx);

    pMtx->hOwner = NIL_RTNATIVETHREAD;
    pMtx->hMtx   = NIL_RTSEMMUTEX;
    int rc = RTSemMutexCreate(&pMtx->hMtx);
    if (RT_SUCCESS(rc))
        return 0;
    return -1;
}


/** Deletes a mutex. */
void VBoxDtMutexDelete(struct VBoxDtMutex *pMtx)
{
    AssertReturnVoid(pMtx != &g_DummyMtx);
    AssertPtr(pMtx);
    if (pMtx->hMtx == NIL_RTSEMMUTEX || pMtx->hMtx == NULL)
        return;

    Assert(pMtx->hOwner == NIL_RTNATIVETHREAD);
    int rc = RTSemMutexDestroy(pMtx->hMtx); AssertRC(rc);
    pMtx->hMtx = NIL_RTSEMMUTEX;
}


/* mutex_enter implementation */
void VBoxDtMutexEnter(struct VBoxDtMutex *pMtx)
{
    AssertPtr(pMtx);
    if (pMtx == &g_DummyMtx)
        return;

    RTNATIVETHREAD hSelf = RTThreadNativeSelf();

    int rc = RTSemMutexRequest(pMtx->hMtx, RT_INDEFINITE_WAIT);
    AssertFatalRC(rc);

    Assert(pMtx->hOwner == NIL_RTNATIVETHREAD);
    pMtx->hOwner = hSelf;
}


/* mutex_exit implementation */
void VBoxDtMutexExit(struct VBoxDtMutex *pMtx)
{
    AssertPtr(pMtx);
    if (pMtx == &g_DummyMtx)
        return;

    Assert(pMtx->hOwner == RTThreadNativeSelf());

    pMtx->hOwner = NIL_RTNATIVETHREAD;
    int rc = RTSemMutexRelease(pMtx->hMtx);
    AssertFatalRC(rc);
}


/* MUTEX_HELD implementation */
bool VBoxDtMutexIsOwner(struct VBoxDtMutex *pMtx)
{
    AssertPtrReturn(pMtx, false);
    if (pMtx == &g_DummyMtx)
        return true;
    return pMtx->hOwner == RTThreadNativeSelf();
}



/*
 *
 * Helpers for handling VTG structures.
 * Helpers for handling VTG structures.
 * Helpers for handling VTG structures.
 *
 */



/**
 * Converts an attribute from VTG description speak to DTrace.
 *
 * @param   pDtAttr             The DTrace attribute (dst).
 * @param   pVtgAttr            The VTG attribute descriptor (src).
 */
static void vboxDtVtgConvAttr(dtrace_attribute_t *pDtAttr, PCVTGDESCATTR pVtgAttr)
{
    pDtAttr->dtat_name  = pVtgAttr->u8Code - 1;
    pDtAttr->dtat_data  = pVtgAttr->u8Data - 1;
    pDtAttr->dtat_class = pVtgAttr->u8DataDep - 1;
}

/**
 * Gets a string from the string table.
 *
 * @returns Pointer to the string.
 * @param   pVtgHdr             The VTG object header.
 * @param   offStrTab           The string table offset.
 */
static const char *vboxDtVtgGetString(PVTGOBJHDR pVtgHdr,  uint32_t offStrTab)
{
    Assert(offStrTab < pVtgHdr->cbStrTab);
    return &pVtgHdr->pachStrTab[offStrTab];
}



/*
 *
 * DTrace Provider Interface.
 * DTrace Provider Interface.
 * DTrace Provider Interface.
 *
 */


/**
 * @callback_method_impl{dtrace_pops_t,dtps_provide}
 */
static void     vboxDtPOps_Provide(void *pvProv, const dtrace_probedesc_t *pDtProbeDesc)
{
    PSUPDRVVDTPROVIDERCORE  pProv        = (PSUPDRVVDTPROVIDERCORE)pvProv;
    PVTGPROBELOC            pProbeLoc    = pProv->pHdr->paProbLocs;
    PVTGPROBELOC            pProbeLocEnd = pProv->pHdr->paProbLocsEnd;
    dtrace_provider_id_t    idProvider   = pProv->TracerData.DTrace.idProvider;
    size_t const            cbFnNmBuf    = _4K + _1K;
    char                   *pszFnNmBuf;
    uint16_t                idxProv;

    if (pDtProbeDesc)
        return;  /* We don't generate probes, so never mind these requests. */

    if (pProv->TracerData.DTrace.fZombie)
        return;

    if (pProv->TracerData.DTrace.cProvidedProbes >= pProbeLocEnd - pProbeLoc)
        return;

     /* Need a buffer for extracting the function names and mangling them in
        case of collision. */
     pszFnNmBuf = (char *)RTMemAlloc(cbFnNmBuf);
     if (!pszFnNmBuf)
         return;

     /*
      * Itereate the probe location list and register all probes related to
      * this provider.
      */
     idxProv      = (uint16_t)(&pProv->pHdr->paProviders[0] - pProv->pDesc);
     while ((uintptr_t)pProbeLoc < (uintptr_t)pProbeLocEnd)
     {
         PVTGDESCPROBE pProbeDesc = (PVTGDESCPROBE)pProbeLoc->pbProbe;
         if (   pProbeDesc->idxProvider == idxProv
             && pProbeLoc->idProbe      == UINT32_MAX)
         {
             /* The function name normally needs to be stripped since we're
                using C++ compilers for most of the code.  ASSUMES nobody are
                brave/stupid enough to use function pointer returns without
                typedef'ing properly them. */
             const char *pszPrbName = vboxDtVtgGetString(pProv->pHdr, pProbeDesc->offName);
             const char *pszFunc    = pProbeLoc->pszFunction;
             const char *psz        = strchr(pProbeLoc->pszFunction, '(');
             size_t      cch;
             if (psz)
             {
                 /* skip blanks preceeding the parameter parenthesis. */
                 while (   (uintptr_t)psz > (uintptr_t)pProbeLoc->pszFunction
                        && RT_C_IS_BLANK(psz[-1]))
                     psz--;

                 /* Find the start of the function name. */
                 pszFunc = psz - 1;
                 while ((uintptr_t)pszFunc > (uintptr_t)pProbeLoc->pszFunction)
                 {
                     char ch = pszFunc[-1];
                     if (!RT_C_IS_ALNUM(ch) && ch != '_' && ch != ':')
                         break;
                     pszFunc--;
                 }
                 cch = psz - pszFunc;
             }
             else
                 cch = strlen(pszFunc);
             RTStrCopyEx(pszFnNmBuf, cbFnNmBuf, pszFunc, cch);

             /* Look up the probe, if we have one in the same function, mangle
                the function name a little to avoid having to deal with having
                multiple location entries with the same probe ID. (lazy bird) */
             Assert(pProbeLoc->idProbe == UINT32_MAX);
             if (dtrace_probe_lookup(idProvider, pProv->pszModName, pszFnNmBuf, pszPrbName) != DTRACE_IDNONE)
             {
                 RTStrPrintf(pszFnNmBuf+cch, cbFnNmBuf - cch, "-%u",  pProbeLoc->uLine);
                 if (dtrace_probe_lookup(idProvider, pProv->pszModName, pszFnNmBuf, pszPrbName) != DTRACE_IDNONE)
                 {
                     unsigned iOrd = 2;
                     while (iOrd < 128)
                     {
                         RTStrPrintf(pszFnNmBuf+cch, cbFnNmBuf - cch, "-%u-%u",  pProbeLoc->uLine, iOrd);
                         if (dtrace_probe_lookup(idProvider, pProv->pszModName, pszFnNmBuf, pszPrbName) == DTRACE_IDNONE)
                             break;
                         iOrd++;
                     }
                     if (iOrd >= 128)
                     {
                         LogRel(("VBoxDrv: More than 128 duplicate probe location instances in file %s at line %u, function %s [%s], probe %s\n",
                                 pProbeLoc->pszFile, pProbeLoc->uLine, pProbeLoc->pszFunction, pszFnNmBuf, pszPrbName));
                         continue;
                     }
                 }
             }

             /* Create the probe. */
             AssertCompile(sizeof(pProbeLoc->idProbe) == sizeof(dtrace_id_t));
             pProbeLoc->idProbe = dtrace_probe_create(idProvider, pProv->pszModName, pszFnNmBuf, pszPrbName,
                                                      1 /*aframes*/, pProbeLoc);
             pProv->TracerData.DTrace.cProvidedProbes++;
         }

         pProbeLoc++;
     }

     RTMemFree(pszFnNmBuf);
}


/**
 * @callback_method_impl{dtrace_pops_t,dtps_enable}
 */
static int      vboxDtPOps_Enable(void *pvProv, dtrace_id_t idProbe, void *pvProbe)
{
    PSUPDRVVDTPROVIDERCORE  pProv  = (PSUPDRVVDTPROVIDERCORE)pvProv;
    if (!pProv->TracerData.DTrace.fZombie)
    {
        PVTGPROBELOC    pProbeLoc  = (PVTGPROBELOC)pvProbe;
        PVTGDESCPROBE   pProbeDesc = (PVTGDESCPROBE)pProbeLoc->pbProbe;

        if (!pProbeLoc->fEnabled)
        {
            pProbeLoc->fEnabled = 1;
            if (ASMAtomicIncU32(&pProbeDesc->u32User) == 1)
                pProv->pHdr->pafProbeEnabled[pProbeDesc->idxEnabled] = 1;
        }
    }

    return 0;
}


/**
 * @callback_method_impl{dtrace_pops_t,dtps_disable}
 */
static void     vboxDtPOps_Disable(void *pvProv, dtrace_id_t idProbe, void *pvProbe)
{
    PSUPDRVVDTPROVIDERCORE  pProv  = (PSUPDRVVDTPROVIDERCORE)pvProv;
    if (!pProv->TracerData.DTrace.fZombie)
    {
        PVTGPROBELOC    pProbeLoc  = (PVTGPROBELOC)pvProbe;
        PVTGDESCPROBE   pProbeDesc = (PVTGDESCPROBE)pProbeLoc->pbProbe;

        if (pProbeLoc->fEnabled)
        {
            pProbeLoc->fEnabled = 0;
            if (ASMAtomicDecU32(&pProbeDesc->u32User) == 0)
                pProv->pHdr->pafProbeEnabled[pProbeDesc->idxEnabled] = 1;
        }
    }
}


/**
 * @callback_method_impl{dtrace_pops_t,dtps_getargdesc}
 */
static void     vboxDtPOps_GetArgDesc(void *pvProv, dtrace_id_t idProbe, void *pvProbe,
                                            dtrace_argdesc_t *pArgDesc)
{
    PSUPDRVVDTPROVIDERCORE  pProv  = (PSUPDRVVDTPROVIDERCORE)pvProv;
    unsigned                uArg   = pArgDesc->dtargd_ndx;

    if (!pProv->TracerData.DTrace.fZombie)
    {
        PVTGPROBELOC    pProbeLoc  = (PVTGPROBELOC)pvProbe;
        PVTGDESCPROBE   pProbeDesc = (PVTGDESCPROBE)pProbeLoc->pbProbe;
        PVTGDESCARGLIST pArgList   = (PVTGDESCARGLIST)((uintptr_t)pProv->pHdr->paArgLists + pProbeDesc->offArgList);

        Assert(pProbeDesc->offArgList < pProv->pHdr->cbArgLists);
        if (pArgList->cArgs > uArg)
        {
            const char *pszType = vboxDtVtgGetString(pProv->pHdr, pArgList->aArgs[uArg].offType);
            size_t      cchType = strlen(pszType);
            if (cchType < sizeof(pArgDesc->dtargd_native))
            {
                memcpy(pArgDesc->dtargd_native, pszType, cchType + 1);
                /** @todo mapping */
                return;
            }
        }
    }

    pArgDesc->dtargd_ndx = DTRACE_ARGNONE;
}


/**
 * @callback_method_impl{dtrace_pops_t,dtps_getargval}
 */
static uint64_t vboxDtPOps_GetArgVal(void *pvProv, dtrace_id_t idProbe, void *pvProbe,
                                     int iArg, int cFrames)
{
    PVBDTSTACKDATA pData = vboxDtGetStackData();
    AssertReturn(iArg >= 5, UINT64_MAX);

    if (pData->enmCaller == kVBoxDtCaller_ProbeFireKernel)
        return pData->u.ProbeFireKernel.pauStackArgs[iArg - 5];

    if (pData->enmCaller == kVBoxDtCaller_ProbeFireUser)
    {
        PCSUPDRVTRACERUSRCTX pCtx = pData->u.ProbeFireUser.pCtx;
        if (pCtx->cBits == 32)
        {
            if ((unsigned)iArg < RT_ELEMENTS(pCtx->u.X86.aArgs))
                return pCtx->u.X86.aArgs[iArg];
        }
        else if (pCtx->cBits == 64)
        {
            if ((unsigned)iArg < RT_ELEMENTS(pCtx->u.Amd64.aArgs))
                return pCtx->u.Amd64.aArgs[iArg];
        }
        else
            AssertFailed();
    }

    return UINT64_MAX;
}


/**
 * @callback_method_impl{dtrace_pops_t,dtps_destroy}
 */
static void    vboxDtPOps_Destroy(void *pvProv, dtrace_id_t idProbe, void *pvProbe)
{
    PSUPDRVVDTPROVIDERCORE  pProv  = (PSUPDRVVDTPROVIDERCORE)pvProv;
    if (!pProv->TracerData.DTrace.fZombie)
    {
        PVTGPROBELOC    pProbeLoc  = (PVTGPROBELOC)pvProbe;
        Assert(!pProbeLoc->fEnabled);
        Assert(pProbeLoc->idProbe == idProbe); NOREF(idProbe);
        pProbeLoc->idProbe = UINT32_MAX;
    }
    pProv->TracerData.DTrace.cProvidedProbes--;
}



/**
 * DTrace provider method table.
 */
static const dtrace_pops_t g_vboxDtVtgProvOps =
{
    /* .dtps_provide         = */ vboxDtPOps_Provide,
    /* .dtps_provide_module  = */ NULL,
    /* .dtps_enable          = */ vboxDtPOps_Enable,
    /* .dtps_disable         = */ vboxDtPOps_Disable,
    /* .dtps_suspend         = */ NULL,
    /* .dtps_resume          = */ NULL,
    /* .dtps_getargdesc      = */ vboxDtPOps_GetArgDesc,
    /* .dtps_getargval       = */ vboxDtPOps_GetArgVal,
    /* .dtps_usermode        = */ NULL,
    /* .dtps_destroy         = */ vboxDtPOps_Destroy
};




/*
 *
 * Support Driver Tracer Interface.
 * Support Driver Tracer Interface.
 * Support Driver Tracer Interface.
 *
 */



/**
 * interface_method_impl{SUPDRVTRACERREG,pfnProbeFireUser}
 */
static DECLCALLBACK(void) vbdt_ProbeFireKernel(struct VTGPROBELOC *pVtgProbeLoc, uintptr_t uArg0, uintptr_t uArg1, uintptr_t uArg2,
                                               uintptr_t uArg3, uintptr_t uArg4)
{
    VBDT_SETUP_STACK_DATA(kVBoxDtCaller_ProbeFireKernel);

    pStackData->u.ProbeFireKernel.uCaller      = (uintptr_t)ASMReturnAddress();
    pStackData->u.ProbeFireKernel.pauStackArgs = &uArg4 + 1;
    dtrace_probe(pVtgProbeLoc->idProbe, uArg0, uArg1, uArg2, uArg3, uArg4);

    VBDT_CLEAR_STACK_DATA();
    return ;
}


/**
 * interface_method_impl{SUPDRVTRACERREG,pfnProbeFireUser}
 */
static DECLCALLBACK(void) vbdt_ProbeFireUser(PCSUPDRVTRACERREG pThis, PSUPDRVSESSION pSession, PCSUPDRVTRACERUSRCTX pCtx)
{
    VBDT_SETUP_STACK_DATA(kVBoxDtCaller_ProbeFireUser);

    pStackData->u.ProbeFireUser.pCtx = pCtx;
    if (pCtx->cBits == 32)
        dtrace_probe(pCtx->idProbe,
                     pCtx->u.X86.aArgs[0],
                     pCtx->u.X86.aArgs[1],
                     pCtx->u.X86.aArgs[2],
                     pCtx->u.X86.aArgs[3],
                     pCtx->u.X86.aArgs[4]);
    else if (pCtx->cBits == 64)
        dtrace_probe(pCtx->idProbe,
                     pCtx->u.Amd64.aArgs[0],
                     pCtx->u.Amd64.aArgs[1],
                     pCtx->u.Amd64.aArgs[2],
                     pCtx->u.Amd64.aArgs[3],
                     pCtx->u.Amd64.aArgs[4]);
    else
        AssertFailed();

    VBDT_CLEAR_STACK_DATA();
}


/**
 * interface_method_impl{SUPDRVTRACERREG,pfnTracerOpen}
 */
static DECLCALLBACK(int) vbdt_TracerOpen(PCSUPDRVTRACERREG pThis, PSUPDRVSESSION pSession, uint32_t uCookie, uintptr_t uArg,
                                         uintptr_t *puSessionData)
{
    if (uCookie != RT_MAKE_U32_FROM_U8('V', 'B', 'D', 'T'))
        return VERR_INVALID_MAGIC;
    if (uArg)
        return VERR_INVALID_PARAMETER;

    VBDT_SETUP_STACK_DATA(kVBoxDtCaller_Generic);

    int rc = dtrace_open((dtrace_state_t **)puSessionData, VBoxDtGetCurrentCreds());

    VBDT_CLEAR_STACK_DATA();
    return RTErrConvertFromErrno(rc);
}


/**
 * interface_method_impl{SUPDRVTRACERREG,pfnTracerClose}
 */
static DECLCALLBACK(int) vbdt_TracerIoCtl(PCSUPDRVTRACERREG pThis, PSUPDRVSESSION pSession, uintptr_t uSessionData,
                                          uintptr_t uCmd, uintptr_t uArg, int32_t *piRetVal)
{
    AssertPtrReturn(uSessionData, VERR_INVALID_POINTER);
    VBDT_SETUP_STACK_DATA(kVBoxDtCaller_Generic);

    int rc = dtrace_ioctl((dtrace_state_t *)uSessionData, (intptr_t)uCmd, (intptr_t)uArg, piRetVal);

    VBDT_CLEAR_STACK_DATA();
    return VINF_SUCCESS;
}


/**
 * interface_method_impl{SUPDRVTRACERREG,pfnTracerClose}
 */
static DECLCALLBACK(void) vbdt_TracerClose(PCSUPDRVTRACERREG pThis, PSUPDRVSESSION pSession, uintptr_t uSessionData)
{
    AssertPtrReturnVoid(uSessionData);
    VBDT_SETUP_STACK_DATA(kVBoxDtCaller_Generic);

    dtrace_close((dtrace_state_t *)uSessionData);

    VBDT_CLEAR_STACK_DATA();
}


/**
 * interface_method_impl{SUPDRVTRACERREG,pfnProviderRegister}
 */
static DECLCALLBACK(int) vbdt_ProviderRegister(PCSUPDRVTRACERREG pThis, PSUPDRVVDTPROVIDERCORE pCore)
{
    AssertReturn(pCore->TracerData.DTrace.idProvider == UINT32_MAX || pCore->TracerData.DTrace.idProvider == 0,
                 VERR_INTERNAL_ERROR_3);
    VBDT_SETUP_STACK_DATA(kVBoxDtCaller_Generic);

    PVTGDESCPROVIDER    pDesc = pCore->pDesc;
    dtrace_pattr_t      DtAttrs;
    vboxDtVtgConvAttr(&DtAttrs.dtpa_provider, &pDesc->AttrSelf);
    vboxDtVtgConvAttr(&DtAttrs.dtpa_mod,      &pDesc->AttrModules);
    vboxDtVtgConvAttr(&DtAttrs.dtpa_func,     &pDesc->AttrFunctions);
    vboxDtVtgConvAttr(&DtAttrs.dtpa_name,     &pDesc->AttrNames);
    vboxDtVtgConvAttr(&DtAttrs.dtpa_args,     &pDesc->AttrArguments);

    dtrace_provider_id_t idProvider;
    int rc = dtrace_register(pCore->pszName,
                             &DtAttrs,
                             DTRACE_PRIV_KERNEL,
                             NULL /* cred */,
                             &g_vboxDtVtgProvOps,
                             pCore,
                             &idProvider);
    if (!rc)
    {
        Assert(idProvider != UINT32_MAX && idProvider != 0);
        pCore->TracerData.DTrace.idProvider = idProvider;
        Assert(pCore->TracerData.DTrace.idProvider == idProvider);
        rc = VINF_SUCCESS;
    }
    else
        rc = RTErrConvertFromErrno(rc);

    VBDT_CLEAR_STACK_DATA();
    return rc;
}


/**
 * interface_method_impl{SUPDRVTRACERREG,pfnProviderDeregister}
 */
static DECLCALLBACK(int) vbdt_ProviderDeregister(PCSUPDRVTRACERREG pThis, PSUPDRVVDTPROVIDERCORE pCore)
{
    uintptr_t idProvider = pCore->TracerData.DTrace.idProvider;
    AssertReturn(idProvider != UINT32_MAX && idProvider != 0, VERR_INTERNAL_ERROR_4);
    VBDT_SETUP_STACK_DATA(kVBoxDtCaller_Generic);

    dtrace_invalidate(idProvider);
    int rc = dtrace_unregister(idProvider);
    if (!rc)
    {
        pCore->TracerData.DTrace.idProvider = UINT32_MAX;
        rc = VINF_SUCCESS;
    }
    else
    {
        AssertMsg(rc == EBUSY, ("%d\n", rc));
        rc = VERR_TRY_AGAIN;
    }

    VBDT_CLEAR_STACK_DATA();
    return rc;
}


/**
 * interface_method_impl{SUPDRVTRACERREG,pfnProviderDeregisterZombie}
 */
static DECLCALLBACK(int) vbdt_ProviderDeregisterZombie(PCSUPDRVTRACERREG pThis, PSUPDRVVDTPROVIDERCORE pCore)
{
    uintptr_t idProvider = pCore->TracerData.DTrace.idProvider;
    AssertReturn(idProvider != UINT32_MAX && idProvider != 0, VERR_INTERNAL_ERROR_4);
    VBDT_SETUP_STACK_DATA(kVBoxDtCaller_Generic);

    int rc = dtrace_unregister(idProvider);
    if (!rc)
    {
        pCore->TracerData.DTrace.idProvider = UINT32_MAX;
        rc = VINF_SUCCESS;
    }
    else
    {
        AssertMsg(rc == EBUSY, ("%d\n", rc));
        rc = VERR_TRY_AGAIN;
    }

    VBDT_CLEAR_STACK_DATA();
    return rc;
}



/**
 * The tracer registration record of the VBox DTrace implementation
 */
static SUPDRVTRACERREG g_VBoxDTraceReg =
{
    SUPDRVTRACERREG_MAGIC,
    SUPDRVTRACERREG_VERSION,
    vbdt_ProbeFireKernel,
    vbdt_ProbeFireUser,
    vbdt_TracerOpen,
    vbdt_TracerIoCtl,
    vbdt_TracerClose,
    vbdt_ProviderRegister,
    vbdt_ProviderDeregister,
    vbdt_ProviderDeregisterZombie,
    SUPDRVTRACERREG_MAGIC
};



/**
 * Module termination code.
 *
 * @param   hMod            Opque module handle.
 */
DECLEXPORT(void) ModuleTerm(void *hMod)
{
    SUPR0TracerDeregisterImpl(hMod, NULL);
    dtrace_detach();
}


/**
 * Module initialization code.
 *
 * @param   hMod            Opque module handle.
 */
DECLEXPORT(int)  ModuleInit(void *hMod)
{
    int rc = dtrace_attach();
    if (rc == DDI_SUCCESS)
    {
        rc = SUPR0TracerRegisterImpl(hMod, NULL, &g_VBoxDTraceReg, &g_pVBoxDTraceHlp);
        if (RT_SUCCESS(rc))
        {
            return rc;
        }

        dtrace_detach();
    }
    else
    {
        SUPR0Printf("dtrace_attach -> %d\n", rc);
        rc = VERR_INTERNAL_ERROR_5;
    }

    return rc;
}

