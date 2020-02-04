/* $Id$ */
/** @file
 * VirtualBox Main - interface for VBox DHCP server.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_NetworkServiceRunner_h
#define MAIN_INCLUDED_NetworkServiceRunner_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/com/string.h>


/** @name Internal networking trunk type option values (NetworkServiceRunner::kpszKeyTrunkType)
 *  @{ */
#define TRUNKTYPE_WHATEVER "whatever"
#define TRUNKTYPE_NETFLT   "netflt"
#define TRUNKTYPE_NETADP   "netadp"
#define TRUNKTYPE_SRVNAT   "srvnat"
/** @} */

/**
 * Network service runner.
 *
 * Build arguments, starts and stops network service processes.
 */
class NetworkServiceRunner
{
public:
    NetworkServiceRunner(const char *aProcName);
    virtual ~NetworkServiceRunner();

    /** @name Argument management
     * @{ */
    int  addArgument(const char *pszArgument);
    int  addArgPair(const char *pszOption, const char *pszValue);
    void resetArguments();
    /** @} */

    int  start(bool aKillProcessOnStop);
    int  stop();
    void detachFromServer();
    bool isRunning();

    RTPROCESS getPid() const;

    /** @name Common options
     * @{ */
    static const char * const kpszKeyNetwork;
    static const char * const kpszKeyTrunkType;
    static const char * const kpszTrunkName;
    static const char * const kpszMacAddress;
    static const char * const kpszIpAddress;
    static const char * const kpszIpNetmask;
    static const char * const kpszKeyNeedMain;
    /** @} */

private:
    struct Data;
    Data *m;
};

#endif /* !MAIN_INCLUDED_NetworkServiceRunner_h */

