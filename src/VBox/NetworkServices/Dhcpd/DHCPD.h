/* $Id$ */
/** @file
 * DHCP server - protocol logic
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

#ifndef _DHCPD_DHCPD_H_
#define _DHCPD_DHCPD_H_

#include "Defs.h"
#include "Config.h"
#include "DhcpMessage.h"
#include "Db.h"


class DHCPD
{
    const Config *m_pConfig;
    std::string m_strLeasesFileName;
    Db m_db;

public:
    DHCPD();

    int init(const Config *);

    DhcpServerMessage *process(const std::unique_ptr<DhcpClientMessage> &req)
    {
        if (req.get() == NULL)
            return NULL;

        return process(*req.get());
    }

    DhcpServerMessage *process(DhcpClientMessage &req);

private:
    DhcpServerMessage *doDiscover(DhcpClientMessage &req);
    DhcpServerMessage *doRequest(DhcpClientMessage &req);
    DhcpServerMessage *doInform(DhcpClientMessage &req);

    DhcpServerMessage *doDecline(DhcpClientMessage &req);
    DhcpServerMessage *doRelease(DhcpClientMessage &req);

    DhcpServerMessage *createMessage(int type, DhcpClientMessage &req);

    void loadLeases();
    void saveLeases();
};

#endif /* _DHCPD_DHCPD_H_ */
