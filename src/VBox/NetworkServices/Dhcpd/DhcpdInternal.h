/* $Id$ */
/** @file
 * DHCP server - Internal header.
 */

/*
 * Copyright (C) 2017-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Dhcpd_Defs_h
#define VBOX_INCLUDED_SRC_Dhcpd_Defs_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#define LOG_GROUP LOG_GROUP_NET_DHCPD
#include <iprt/stdint.h>
#include <iprt/string.h>
#include <VBox/log.h>

#include <map>
#include <vector>

#if __cplusplus >= 199711
#include <memory>
using std::shared_ptr;
#else
# include <tr1/memory>
using std::tr1::shared_ptr;
#endif


/** Byte vector. */
typedef std::vector<uint8_t> octets_t;

/** Raw DHCP option map (keyed by option number, byte vector value). */
typedef std::map<uint8_t, octets_t> rawopts_t;

class DhcpOption;
/** DHCP option map (keyed by option number, DhcpOption value). */
typedef std::map<uint8_t, std::shared_ptr<DhcpOption> > optmap_t;


/** Equal compare operator for mac address. */
DECLINLINE(bool) operator==(const RTMAC &l, const RTMAC &r)
{
    return memcmp(&l, &r, sizeof(RTMAC)) == 0;
}

/** Less-than compare operator for mac address. */
DECLINLINE(bool) operator<(const RTMAC &l, const RTMAC &r)
{
    return memcmp(&l, &r, sizeof(RTMAC)) < 0;
}


/** @def LogDHCP
 * Wrapper around LogRel.  */
#if 1
# define LogDHCP LogRel
#else
# include <iprt/stream.h>
# define LogDHCP(args) RTPrintf args
#endif

#endif /* !VBOX_INCLUDED_SRC_Dhcpd_Defs_h */
