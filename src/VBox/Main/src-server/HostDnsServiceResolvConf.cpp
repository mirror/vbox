/* $Id$ */
/** @file
 * Base class fo Host DNS & Co services.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* -*- indent-tabs-mode: nil; -*- */
#include <VBox/com/string.h>
#include <VBox/com/ptr.h>


#ifdef RT_OS_OS2
# include <sys/socket.h>
typedef int socklen_t;
#endif

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/critsect.h>

#include <VBox/log.h>

#include <string>

#include "HostDnsService.h"
#include "../../Devices/Network/slirp/resolv_conf_parser.h"


struct HostDnsServiceResolvConf::Data
{
    Data(const char *fileName):resolvConfFilename(fileName){};

    std::string resolvConfFilename;
};

const std::string& HostDnsServiceResolvConf::resolvConf() const
{
    return m->resolvConfFilename;
}


HostDnsServiceResolvConf::~HostDnsServiceResolvConf()
{
    if (m)
    {
        delete m;
        m = NULL;
    }
}

HRESULT HostDnsServiceResolvConf::init(const char *aResolvConfFileName)
{
    m = new Data(aResolvConfFileName);

    HostDnsMonitor::init();

    readResolvConf();

    return S_OK;
}

HRESULT HostDnsServiceResolvConf::readResolvConf()
{
    struct rcp_state st;
    wchar_t *pwczTmpStr;

    st.rcps_flags = RCPSF_NO_STR2IPCONV;
    int rc = rcp_parse(&st, m->resolvConfFilename.c_str());
    if (rc == -1)
        return S_OK;

    HostDnsInformation info;
    for (unsigned i = 0; i != st.rcps_num_nameserver; ++i)
    {
        AssertBreak(st.rcps_str_nameserver[i]);

        pwczTmpStr = NULL;
        rc = RTStrToUtf16(st.rcps_str_nameserver[i], (RTUTF16 **)&pwczTmpStr);
        if (RT_SUCCESS(rc) && pwczTmpStr)
        {
            info.servers.push_back(std::wstring(pwczTmpStr));
            RTUtf16Free((RTUTF16 *)pwczTmpStr);
        }
    }

    if (st.rcps_domain)
    {
        pwczTmpStr = NULL;
        rc = RTStrToUtf16(st.rcps_domain, (RTUTF16 **)&pwczTmpStr);
        if (RT_SUCCESS(rc) && pwczTmpStr)
        {
            info.domain = std::wstring(pwczTmpStr);
            RTUtf16Free((RTUTF16 *)pwczTmpStr);
        }
    }

    for (unsigned i = 0; i != st.rcps_num_searchlist; ++i)
    {
        AssertBreak(st.rcps_searchlist[i]);
        pwczTmpStr = NULL;
        rc = RTStrToUtf16(st.rcps_searchlist[i], (RTUTF16 **)&pwczTmpStr);
        if (RT_SUCCESS(rc) && pwczTmpStr)
        {
            info.searchList.push_back(std::wstring(pwczTmpStr));
            RTUtf16Free((RTUTF16 *)pwczTmpStr);
        }
    }
    setInfo(info);

    return S_OK;
}
