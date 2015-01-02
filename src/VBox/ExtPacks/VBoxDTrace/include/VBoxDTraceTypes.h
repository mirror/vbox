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
typedef uintptr_t               caddr_t;
typedef uint64_t                hrtime_t;
typedef RTCPUID                 processorid_t;
typedef RTCCUINTREG             greg_t;

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
typedef struct VBoxDtThread     kthread_t;
typedef struct VBoxDtThread     proc_t;
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
    void               *cpuc_dtrace_illval;
    uint16_t volatile   cpuc_dtrace_flags;

} cpucore_t;

#define CPU_DTRACE_BADADDR  RT_BIT(0)
#define CPU_DTRACE_BADALIGN RT_BIT(1)
#define CPU_DTRACE_NOFAULT  RT_BIT(2)
#define CPU_DTRACE_FAULT    RT_BIT(3)
#define CPU_DTRACE_KPRIV    RT_BIT(4)
#define CPU_DTRACE_DROP     RT_BIT(5)

extern cpucore_t  g_aVBoxDtCpuCores[RTCPUSET_MAX_CPUS];
#define cpu_core (g_aVBoxDtCpuCores)

#if 1 /* */

typedef RTUID               uid_t;
typedef RTPROCESS           pid_t;
typedef RTDEV               dev_t;
#endif

#define NANOSEC             RT_NS_1SEC
#define MILLISEC            RT_MS_1SEC
#define NBBY                8


RT_C_DECLS_END
#endif

