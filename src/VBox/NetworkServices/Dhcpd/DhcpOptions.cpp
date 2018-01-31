/* $Id$ */
/** @file
 * DHCP server - DHCP options
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

#include "DhcpOptions.h"
#include "DhcpMessage.h"


optmap_t &operator<<(optmap_t &optmap, DhcpOption *option)
{
    if (option == NULL)
        return optmap;

    if (option->present())
        optmap[option->optcode()] = std::shared_ptr<DhcpOption>(option);
    else
        optmap.erase(option->optcode());

    return optmap;
}


optmap_t &operator<<(optmap_t &optmap, const std::shared_ptr<DhcpOption> &option)
{
    if (option == NULL)
        return optmap;

    if (option->present())
        optmap[option->optcode()] = option;
    else
        optmap.erase(option->optcode());

    return optmap;
}


int DhcpOption::decode(const rawopts_t &map)
{
    const octets_t *rawopt = DhcpOption::findOption(map, m_OptCode);
    if (rawopt == NULL)
        return VERR_NOT_FOUND;

    int rc = decodeValue(*rawopt, rawopt->size());
    if (RT_FAILURE(rc))
        return VERR_INVALID_PARAMETER;

    return VINF_SUCCESS;
}


int DhcpOption::decode(const DhcpClientMessage &req)
{
    return decode(req.rawopts());
}
