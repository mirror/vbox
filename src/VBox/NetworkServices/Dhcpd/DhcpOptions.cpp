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


int DhcpOption::encode(octets_t &dst) const
{
    if (!m_fPresent)
        return VERR_INVALID_STATE;

    size_t cbOrig = dst.size();

    append(dst, m_OptCode);
    appendLength(dst, 0);  /* placeholder */

    ssize_t cbValue = encodeValue(dst);
    if (cbValue < 0 || UINT8_MAX <= cbValue)
    {
        dst.resize(cbOrig); /* undo */
        return VERR_INVALID_PARAMETER;
    }

    dst[cbOrig+1] = cbValue;
    return VINF_SUCCESS;
}


/* static */
const octets_t *DhcpOption::findOption(const rawopts_t &aOptMap, uint8_t aOptCode)
{
    rawopts_t::const_iterator it(aOptMap.find(aOptCode));
    if (it == aOptMap.end())
        return NULL;

    return &it->second;
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


int DhcpOption::parse1(uint8_t &aValue, const char *pcszValue)
{
    int rc = RTStrToUInt8Full(RTStrStripL(pcszValue), 10, &aValue);

    if (rc == VERR_TRAILING_SPACES)
        rc = VINF_SUCCESS;
    return rc;
}


int DhcpOption::parse1(uint16_t &aValue, const char *pcszValue)
{
    int rc = RTStrToUInt16Full(RTStrStripL(pcszValue), 10, &aValue);

    if (rc == VERR_TRAILING_SPACES)
        rc = VINF_SUCCESS;
    return rc;
}


int DhcpOption::parse1(uint32_t &aValue, const char *pcszValue)
{
    int rc = RTStrToUInt32Full(RTStrStripL(pcszValue), 10, &aValue);

    if (rc == VERR_TRAILING_SPACES)
        rc = VINF_SUCCESS;
    return rc;
}


int DhcpOption::parse1(RTNETADDRIPV4 &aValue, const char *pcszValue)
{
    return RTNetStrToIPv4Addr(pcszValue, &aValue);
}


int DhcpOption::parseList(std::vector<RTNETADDRIPV4> &aList, const char *pcszValue)
{
    std::vector<RTNETADDRIPV4> l;
    int rc;

    pcszValue = RTStrStripL(pcszValue);
    do {
        RTNETADDRIPV4 Addr;
        char *pszNext;

        rc = RTNetStrToIPv4AddrEx(pcszValue, &Addr, &pszNext);
        if (RT_FAILURE(rc))
            return VERR_INVALID_PARAMETER;

        if (rc == VWRN_TRAILING_CHARS)
        {
            pcszValue = RTStrStripL(pszNext);
            if (pcszValue == pszNext) /* garbage after address */
                return VERR_INVALID_PARAMETER;
        }

        l.push_back(Addr);

        /*
         * If we got VINF_SUCCESS or VWRN_TRAILING_SPACES then this
         * was the last address and we are done.
         */
    } while (rc == VWRN_TRAILING_CHARS);

    aList.swap(l);
    return VINF_SUCCESS;
}


/*
 * XXX: See DHCPServer::encodeOption()
 */
int DhcpOption::parseHex(octets_t &aRawValue, const char *pcszValue)
{
    octets_t data;
    char *pszNext;
    int rc;

    if (pcszValue == NULL || *pcszValue == '\0')
        return VERR_INVALID_PARAMETER;

    while (*pcszValue != '\0')
    {
        if (data.size() > UINT8_MAX)
            return VERR_INVALID_PARAMETER;

        uint8_t u8Byte;
        rc = RTStrToUInt8Ex(pcszValue, &pszNext, 16, &u8Byte);
        if (!RT_SUCCESS(rc))
            return rc;

        if (*pszNext == ':')
            ++pszNext;
        else if (*pszNext != '\0')
            return VERR_PARSE_ERROR;

        data.push_back(u8Byte);
        pcszValue = pszNext;
    }

    aRawValue.swap(data);
    return VINF_SUCCESS;
}


DhcpOption *DhcpOption::parse(uint8_t aOptCode, int aEnc, const char *pcszValue)
{
    switch (aEnc)
    {
    case 0: /* DhcpOptEncoding_Legacy */
        switch (aOptCode)
        {
#define HANDLE(_OptClass)                               \
            case _OptClass::optcode:                    \
                return _OptClass::parse(pcszValue);

        HANDLE(OptSubnetMask);
        HANDLE(OptRouter);
        HANDLE(OptDNS);
        HANDLE(OptHostName);
        HANDLE(OptDomainName);
        HANDLE(OptRootPath);
        HANDLE(OptLeaseTime);
        HANDLE(OptRenewalTime);
        HANDLE(OptRebindingTime);

#undef HANDLE
        default:
            return NULL;
        }
        break;

    case 1:
        return RawOption::parse(aOptCode, pcszValue);

    default:
        return NULL;
    }
}
