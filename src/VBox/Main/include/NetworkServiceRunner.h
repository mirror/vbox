/* $Id$ */
/** @file
 * VirtualBox Main - interface for VBox DHCP server
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
    NETCFG_NAME = 1,
    NETCFG_NETNAME,
    NETCFG_TRUNKTYPE,
    NETCFG_TRUNKNAME,
    NETCFG_MACADDRESS,
    NETCFG_IPADDRESS,
    NETCFG_NETMASK,
    NETCFG_VERBOSE,
    NETCFG_NOTOPT_MAXVAL
}NETCFG;

#define TRUNKTYPE_WHATEVER "whatever"
#define TRUNKTYPE_NETFLT   "netflt"
#define TRUNKTYPE_NETADP   "netadp"
#define TRUNKTYPE_SRVNAT   "srvnat"

class NetworkServiceRunner
{
public:
    NetworkServiceRunner(const char *aProcName):
      mProcName(aProcName),
      mProcess(NIL_RTPROCESS)
    {
        memset(mOptionEnabled, 0, sizeof(mOptionEnabled));
    };
    ~NetworkServiceRunner() { stop(); /* don't leave abandoned servers */}

    int setOption(NETCFG opt, const char *val, bool enabled)
    {
        if (opt == 0 || opt >= NETCFG_NOTOPT_MAXVAL)
            return VERR_INVALID_PARAMETER;
        if (isRunning())
            return VERR_INVALID_STATE;

        mOptions[opt] = val;
        mOptionEnabled[opt] = enabled;
        return VINF_SUCCESS;
    }

    int setOption(NETCFG opt, const com::Utf8Str &val, bool enabled)
    {
        return setOption(opt, val.c_str(), enabled);
    }

    int start();
    int stop();
    bool isRunning();

    void detachFromServer();
private:
    com::Utf8Str mOptions[NETCFG_NOTOPT_MAXVAL];
    bool mOptionEnabled[NETCFG_NOTOPT_MAXVAL];
    const char *mProcName;
    RTPROCESS mProcess;
};
