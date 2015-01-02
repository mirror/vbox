/* $Id$ */
/** @file
 * VBoxDTraceTypes.h - Fake a bunch of Solaris types.
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

#ifndef ___VBoxDTraceTypes_h___
#define ___VBoxDTraceTypes_h___

#include <iprt/types.h>
#include <iprt/stdarg.h>

RT_C_DECLS_BEGIN

typedef unsigned char           uchar_t;
typedef unsigned int            uint_t;
typedef uintptr_t               ulong_t;
typedef uintptr_t               pc_t;
typedef uint32_t                zoneid_t;
typedef char                   *caddr_t;
typedef uint64_t                hrtime_t;
typedef RTCPUID                 processorid_t;
typedef RTCCUINTREG             greg_t;
typedef unsigned int            model_t;

typedef struct VBoxDtCred
{
    RTUID                   cr_uid;
    RTUID                   cr_ruid;
    RTUID                   cr_suid;
    RTGID                   cr_gid;
    RTGID                   cr_rgid;
    RTGID                   cr_sgid;
    zoneid_t                cr_zone;
} cred_t;

typedef struct VBoxDtVMem       vmem_t;
typedef struct VBoxDtCyclicId  *cyclic_id_t;

typedef struct VBoxDtThread
{
    hrtime_t                t_dtrace_vtime;
    hrtime_t                t_dtrace_start;
    uint8_t                 t_dtrace_stop;
    uintptr_t               t_dtrace_scrpc;
    uintptr_t               t_dtrace_astpc;
    uint32_t                t_predcache;
} kthread_t;
extern kthread_t *VBoxDtGetCurrentThread(void);
#define curthread               (VBoxDtGetCurrentThread())


typedef struct VBoxDtProcess
{
    uint32_t                p_flag;
} proc_t;

#define SNOCD                   RT_BIT(0)


typedef struct VBoxDtDevInfo    dev_info_t;
typedef struct VBoxDtTaskQueue  taskq_t;
typedef struct VBoxDtMemCache   kmem_cache_t;

typedef struct VBoxDtMutex
{
    RTSPINLOCK hSpinlock;
} kmutex_t;

typedef struct VBoxDtCpuCore
{
    RTCPUID             cpu_id;
    uintptr_t           cpuc_dtrace_illval;
    uint16_t volatile   cpuc_dtrace_flags;

} cpucore_t;

#define CPU_DTRACE_BADADDR      RT_BIT(0)
#define CPU_DTRACE_BADALIGN     RT_BIT(1)
#define CPU_DTRACE_BADSTACK     RT_BIT(2)
#define CPU_DTRACE_KPRIV        RT_BIT(3)
#define CPU_DTRACE_DIVZERO      RT_BIT(4)
#define CPU_DTRACE_ILLOP        RT_BIT(5)
#define CPU_DTRACE_NOSCRATCH    RT_BIT(6)
#define CPU_DTRACE_UPRIV        RT_BIT(7)
#define CPU_DTRACE_TUPOFLOW     RT_BIT(8)
#define CPU_DTRACE_ENTRY        RT_BIT(9)
#define CPU_DTRACE_FAULT        UINT16_C(0x03ff)
#define CPU_DTRACE_DROP         RT_BIT(12)
#define CPU_DTRACE_ERROR        UINT16_C(0x13ff)
#define CPU_DTRACE_NOFAULT      RT_BIT(15)

extern cpucore_t                g_aVBoxDtCpuCores[RTCPUSET_MAX_CPUS];
#define cpu_core                (g_aVBoxDtCpuCores)

cred_t *VBoxDtGetCurrentCreds(void);
#define CRED()                  VBoxDtGetCurrentCreds()

proc_t *VBoxDtThreadToProc(kthread_t *);


#define ASSERT(a_Expr)          Assert(a_Expr)

#if 1 /* */

typedef RTUID                   uid_t;
typedef RTPROCESS               pid_t;
typedef RTDEV                   dev_t;
#endif

#define NANOSEC                 RT_NS_1SEC
#define MILLISEC                RT_MS_1SEC
#define NBBY                    8
#define NCPU                    RTCPUSET_MAX_CPUS
#define P2ROUNDUP(uWhat, uAlign) ( ((uWhat) + (uAlign) - 1) & ~(uAlign - 1) )
#define CPU_ON_INTR(a_pCpu)     (false)


#if defined(RT_ARCH_X86)
# ifndef __i386
#  define __i386            1
# endif

#elif defined(RT_ARCH_AMD64)
# ifndef __x86_64
#  define __x86_64          1
# endif

#else
# error "unsupported arch!"
#endif

/** Mark a cast added when porting the code to VBox.
 * Avoids lots of \#ifdef VBOX otherwise needed to mark up the changes. */
#define VBDTCAST(a_Type)        (a_Type)

RT_C_DECLS_END
#endif

