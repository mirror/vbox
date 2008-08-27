/* $Id$ */

/** @file
 *
 * VBox Darwin-specific Performance Classes implementation.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <iprt/stdint.h>
#include <mach/mach_error.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/mach_time.h>
#include <mach/task.h>
#include <mach/task_info.h>
#include <mach/vm_statistics.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <iprt/err.h>
#include <iprt/log.h>
#include <iprt/param.h>
#include "Performance.h"

namespace pm {

class CollectorDarwin : public CollectorHAL
{
public:
    virtual int getRawHostCpuLoad(uint64_t *user, uint64_t *kernel, uint64_t *idle);
    virtual int getHostMemoryUsage(ULONG *total, ULONG *used, ULONG *available);
    virtual int getRawProcessCpuLoad(RTPROCESS process, uint64_t *user, uint64_t *kernel, uint64_t *total);
    virtual int getProcessMemoryUsage(RTPROCESS process, ULONG *used);
};

MetricFactoryDarwin::MetricFactoryDarwin()
{
    mHAL = new CollectorDarwin();
    Assert(mHAL);
}

int CollectorDarwin::getRawHostCpuLoad(uint64_t *user, uint64_t *kernel, uint64_t *idle)
{
    kern_return_t krc;
    mach_msg_type_number_t count;
    host_cpu_load_info_data_t info;
    
    count = HOST_CPU_LOAD_INFO_COUNT;
    
    krc = host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&info, &count);
    if (krc != KERN_SUCCESS)
    {
        Log(("host_statistics() -> %s", mach_error_string(krc)));
        return RTErrConvertFromDarwinKern(krc);
    }
        
    *user = (uint64_t)info.cpu_ticks[CPU_STATE_USER]
                    + info.cpu_ticks[CPU_STATE_NICE];
    *kernel = (uint64_t)info.cpu_ticks[CPU_STATE_SYSTEM];
    *idle = (uint64_t)info.cpu_ticks[CPU_STATE_IDLE];
    return VINF_SUCCESS;
}

int CollectorDarwin::getHostMemoryUsage(ULONG *total, ULONG *used, ULONG *available)
{
    kern_return_t krc;
    mach_msg_type_number_t count;
    vm_statistics_data_t info;
    
    count = HOST_VM_INFO_COUNT;
    
    krc = host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&info, &count);
    if (krc != KERN_SUCCESS)
    {
        Log(("host_statistics() -> %s", mach_error_string(krc)));
        return RTErrConvertFromDarwinKern(krc);
    }

    uint64_t hostMemory;
    int mib[2];
    size_t size;

    mib[0] = CTL_HW;
    mib[1] = HW_MEMSIZE;

    size = sizeof(hostMemory);
    if (sysctl(mib, 2, &hostMemory, &size, NULL, 0) == -1) {
        int error = errno;
        Log(("sysctl() -> %s", strerror(error)));
        return RTErrConvertFromErrno(error);
    }
    *total = (ULONG)(hostMemory / 1024);
    *available = info.free_count * (PAGE_SIZE / 1024);
    *used = *total - *available;
    return VINF_SUCCESS;
}

int CollectorDarwin::getRawProcessCpuLoad(RTPROCESS process, uint64_t *user, uint64_t *kernel, uint64_t *total)
{
    kern_return_t krc;
    task_absolutetime_info_data_t tinfo;
    mach_port_name_t vmTask;
    mach_msg_type_number_t count;

    krc = task_for_pid(task_self_trap(), process, &vmTask);
    if (krc != KERN_SUCCESS)
    {
        Log(("task_for_pid() -> %s", mach_error_string(krc)));
        return RTErrConvertFromDarwinKern(krc);
    }

    count = TASK_ABSOLUTETIME_INFO_COUNT;
    krc = task_info(vmTask, TASK_ABSOLUTETIME_INFO, (task_info_t)&tinfo, &count);
    if (krc != KERN_SUCCESS) {
        Log(("task_info() -> %s", mach_error_string(krc)));
        return RTErrConvertFromDarwinKern(krc);
    }

    *user = tinfo.total_user;
    *kernel = tinfo.total_system;
    *total = mach_absolute_time();
    return VINF_SUCCESS;
}

int CollectorDarwin::getProcessMemoryUsage(RTPROCESS process, ULONG *used)
{
    kern_return_t krc;
    task_basic_info_64_data_t tinfo;
    mach_port_name_t vmTask;
    mach_msg_type_number_t count;

    krc = task_for_pid(task_self_trap(), process, &vmTask);
    if (krc != KERN_SUCCESS)
    {
        Log(("task_for_pid() -> %s", mach_error_string(krc)));
        return RTErrConvertFromDarwinKern(krc);
    }
    count = TASK_BASIC_INFO_64_COUNT;
    krc = task_info(task_self_trap(), TASK_BASIC_INFO_64, (task_info_t)&tinfo, &count);
    if (krc != KERN_SUCCESS) {
        Log(("host_statistics() -> %s", mach_error_string(krc)));
        return RTErrConvertFromDarwinKern(krc);
    }

    *used = tinfo.resident_size / 1024;
    return VINF_SUCCESS;
}

}
