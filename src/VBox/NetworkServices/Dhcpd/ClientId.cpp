/* $Id$ */
/** @file
 * DHCP server - client identifier
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

#include <algorithm>

#include "ClientId.h"


bool ClientId::g_fFormatRegistered = false;


void ClientId::registerFormat()
{
    if (g_fFormatRegistered)
        return;

    int rc = RTStrFormatTypeRegister("id", rtStrFormat, NULL);
    AssertRC(rc);

    g_fFormatRegistered = true;
}


DECLCALLBACK(size_t)
ClientId::rtStrFormat(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
             const char *pszType, void const *pvValue,
             int cchWidth, int cchPrecision, unsigned fFlags,
             void *pvUser)
{
    const ClientId *id = static_cast<const ClientId *>(pvValue);
    size_t cb = 0;

    AssertReturn(strcmp(pszType, "id") == 0, 0);
    RT_NOREF(pszType);

    RT_NOREF(cchWidth, cchPrecision, fFlags);
    RT_NOREF(pvUser);

    if (id == NULL)
    {
        return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                           "<NULL>");
    }

    if (id->m_id.present())
    {
        const OptClientId::value_t &idopt = id->m_id.value();

        cb += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                          "[");

        for (size_t i = 0; i < idopt.size(); ++i)
        {
            cb += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                              "%s%02x", (i == 0 ? "" : ":"), idopt[i]);
        }

        cb += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                          "] (");
    }

    cb += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                      "%RTmac", &id->m_mac);

    if (id->m_id.present())
    {
        cb += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                          ")");
    }

    return 0;
}


bool operator==(const ClientId &l, const ClientId &r)
{
    if (l.m_id.present())
    {
        if (r.m_id.present())
            return l.m_id.value() == r.m_id.value();
    }
    else
    {
        if (!r.m_id.present())
            return l.m_mac == r.m_mac;
    }

    return false;
}


bool operator<(const ClientId &l, const ClientId &r)
{
    if (l.m_id.present())
    {
        if (r.m_id.present())
            return l.m_id.value() < r.m_id.value();
        else
            return false;       /* the one with id comes last */
    }
    else
    {
        if (r.m_id.present())
            return true;        /* the one with id comes last */
        else
            return l.m_mac < r.m_mac;
    }
}
