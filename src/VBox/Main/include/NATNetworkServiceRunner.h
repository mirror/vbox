/* $Id$ */
/** @file
 * VirtualBox Main - interface for VBox NAT Network service
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include <iprt/err.h>
#include <iprt/types.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <VBox/com/string.h>

typedef enum
{
    NATSCCFG_NAME = 1,
    NATSCCFG_TRUNKTYPE,
    NATSCCFG_MACADDRESS,
    NATSCCFG_IPADDRESS,
    NATSCCFG_NETMASK,
    NATSCCFG_PORTFORWARD4,
    NATSCCFG_PORTFORWARD6,
    NATSCCFG_NOTOPT_MAXVAL
}NATSCCFG;

#define TRUNKTYPE_WHATEVER "whatever"
#define TRUNKTYPE_SRVNAT   "srvnat"

class NATNetworkServiceRunner
{
public:
    NATNetworkServiceRunner();
    ~NATNetworkServiceRunner() { stop(); /* don't leave abandoned networks */}

    int setOption(NATSCCFG opt, const char *val, bool enabled)
    {
        if(opt == 0 || opt >= NATSCCFG_NOTOPT_MAXVAL)
            return VERR_INVALID_PARAMETER;
        if(isRunning())
            return VERR_INVALID_STATE;

        mOptions[opt] = val;
        mOptionEnabled[opt] = enabled;
        return VINF_SUCCESS;
    }

    int setOption(NATSCCFG opt, const com::Utf8Str &val, bool enabled)
    {
        return setOption(opt, val.c_str(), enabled);
    }

    int start();
    int stop();
    bool isRunning();

    void detachFromServer();
private:
    com::Utf8Str mOptions[NATSCCFG_NOTOPT_MAXVAL];
    bool mOptionEnabled[NATSCCFG_NOTOPT_MAXVAL];
    RTPROCESS mProcess;
};
