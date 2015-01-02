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

#include <iprt/asm-amd64-x86.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/mp.h>
#include <iprt/string.h>
#include <iprt/time.h>

#include <sys/dtrace_impl.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Per CPU information */
cpucore_t               g_aVBoxDtCpuCores[RTCPUSET_MAX_CPUS];
/** Dummy mutex. */
struct VBoxDtMutex      g_DummyMtx;



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
    /** @todo Will fix dtrace_probe this so it won't be necessary. */
    return (uintptr_t)~0;
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
    /** @todo Will fix dtrace_probe this so that this will be easier to do on all
     *        platforms (MSC sucks, no frames on AMD64). */
    return 0xdeadbeef;
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

#if 0
dtrace_probe_error

VBoxDtGetCurrentCreds
VBoxDtGetCurrentProc
VBoxDtGetCurrentThread
VBoxDtGetKernelBase
VBoxDtKMemAlloc
VBoxDtKMemAllocZ
VBoxDtKMemFree
VBoxDtMutexEnter
VBoxDtMutexExit
VBoxDtMutexIsOwner
VBoxDtVMemAlloc
VBoxDtVMemFree
#endif
