/* $Id$ */

/** @file
 *
 * VBox OS/2-specific Performance Classes implementation.
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

class CollectorOS2 : public CollectorHAL
{
public:
    virtual int getHostCpuLoad(unsigned long *user, unsigned long *kernel, unsigned long *idle);
    virtual int getHostCpuMHz(unsigned long *mhz);
    virtual int getHostMemoryUsage(unsigned long *total, unsigned long *used, unsigned long *available);
    virtual int getProcessCpuLoad(RTPROCESS process, unsigned long *user, unsigned long *kernel);
    virtual int getProcessMemoryUsage(RTPROCESS process, unsigned long *used);
};


MetricFactoryOS2::MetricFactoryOS2()
{
    mHAL = new CollectorOS2();
    Assert(mHAL);
}

int CollectorOS2::getHostCpuLoad(unsigned long *user, unsigned long *kernel, unsigned long *idle)
{
    return E_NOTIMPL;
}

int CollectorOS2::getHostCpuMHz(unsigned long *mhz)
{
    return E_NOTIMPL;
}

int CollectorOS2::getHostMemoryUsage(unsigned long *total, unsigned long *used, unsigned long *available)
{
    return E_NOTIMPL;
}

int CollectorOS2::getProcessCpuLoad(RTPROCESS process, unsigned long *user, unsigned long *kernel)
{
    return E_NOTIMPL;
}

int CollectorOS2::getProcessMemoryUsage(RTPROCESS process, unsigned long *used)
{
    return E_NOTIMPL;
}

}
