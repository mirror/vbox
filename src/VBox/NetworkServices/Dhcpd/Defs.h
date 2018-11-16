/* $Id$ */
/** @file
 * DHCP server - common definitions
 */

/*
 * Copyright (C) 2017-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef _DHCPD_DEFS_H_
#define _DHCPD_DEFS_H_

#include <iprt/stdint.h>
#include <iprt/string.h>
#include <VBox/log.h>

#include <map>
#include <memory>
#include <vector>

typedef std::vector<uint8_t> octets_t;

typedef std::map<uint8_t, octets_t> rawopts_t;

class DhcpOption;
typedef std::map<uint8_t, std::shared_ptr<DhcpOption>> optmap_t;

inline bool operator==(const RTMAC &l, const RTMAC &r)
{
    return memcmp(&l, &r, sizeof(RTMAC)) == 0;
}

inline bool operator<(const RTMAC &l, const RTMAC &r)
{
    return memcmp(&l, &r, sizeof(RTMAC)) < 0;
}

#if 1
#define LogDHCP(args) LogRel(args)
#else
#define LogDHCP(args) RTPrintf args
#endif

#endif /* _DHCPD_DEFS_H_ */
