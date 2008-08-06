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

#include "Performance.h"

namespace pm {

class CollectorDarwin : public CollectorHAL
{
public:
    virtual int getHostCpuLoad(ULONG *user, ULONG *kernel, ULONG *idle);
    virtual int getHostCpuMHz(ULONG *mhz);
    virtual int getHostMemoryUsage(ULONG *total, ULONG *used, ULONG *available);
    virtual int getProcessCpuLoad(RTPROCESS process, ULONG *user, ULONG *kernel);
    virtual int getProcessMemoryUsage(RTPROCESS process, ULONG *used);
};

MetricFactoryDarwin::MetricFactoryDarwin()
{
    mHAL = new CollectorDarwin();
    Assert(mHAL);
}

int CollectorDarwin::getHostCpuLoad(ULONG *user, ULONG *kernel, ULONG *idle)
{
    return E_NOTIMPL;
}

int CollectorDarwin::getHostCpuMHz(ULONG *mhz)
{
    return E_NOTIMPL;
}

int CollectorDarwin::getHostMemoryUsage(ULONG *total, ULONG *used, ULONG *available)
{
    return E_NOTIMPL;
}

int CollectorDarwin::getProcessCpuLoad(RTPROCESS process, ULONG *user, ULONG *kernel)
{
    return E_NOTIMPL;
}

int CollectorDarwin::getProcessMemoryUsage(RTPROCESS process, ULONG *used)
{
    return E_NOTIMPL;
}

}
