/* $Id$ */
/** @file
 * VirtualBox Main - IDHCPConfig, IDHCPConfigGlobal, IDHCPConfigGroup, IDHCPConfigIndividual implementation.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_MAIN_DHCPCONFIG
#include "DHCPConfigImpl.h"
#include "LoggingNew.h"

#include <iprt/ctype.h>
#include <iprt/errcore.h>
#include <iprt/net.h>
#include <iprt/cpp/utils.h>
#include <iprt/cpp/xml.h>

#include <VBox/com/array.h>
#include <VBox/settings.h>

#include "AutoCaller.h"
#include "DHCPServerImpl.h"
#include "MachineImpl.h"
#include "VirtualBoxImpl.h"

#include "../../NetworkServices/Dhcpd/DhcpOptions.h"



/*********************************************************************************************************************************
*   DHCPConfig Implementation                                                                                                    *
*********************************************************************************************************************************/

HRESULT DHCPConfig::i_initWithDefaults(VirtualBox *a_pVirtualBox, DHCPServer *a_pParent)
{
    unconst(m_pVirtualBox) = a_pVirtualBox;
    unconst(m_pParent)     = a_pParent;
    return S_OK;
}


HRESULT DHCPConfig::i_initWithSettings(VirtualBox *a_pVirtualBox, DHCPServer *a_pParent, const settings::DHCPConfig &rConfig)
{
    unconst(m_pVirtualBox) = a_pVirtualBox;
    unconst(m_pParent)     = a_pParent;

    m_secMinLeaseTime     = rConfig.secMinLeaseTime;
    m_secDefaultLeaseTime = rConfig.secDefaultLeaseTime;
    m_secMaxLeaseTime     = rConfig.secMaxLeaseTime;

    for (settings::DhcpOptionMap::const_iterator it = rConfig.mapOptions.begin(); it != rConfig.mapOptions.end(); ++it)
    {
        try
        {
            m_OptionMap[it->first] = settings::DhcpOptValue(it->second.strValue, it->second.enmEncoding);
        }
        catch (std::bad_alloc &)
        {
            return E_OUTOFMEMORY;
        }
    }

    return S_OK;
}


HRESULT DHCPConfig::i_saveSettings(settings::DHCPConfig &a_rDst)
{
    /* lease times */
    a_rDst.secMinLeaseTime     = m_secMinLeaseTime;
    a_rDst.secDefaultLeaseTime = m_secDefaultLeaseTime;
    a_rDst.secMaxLeaseTime     = m_secMaxLeaseTime;

    /* Options: */
    try
    {
        a_rDst.mapOptions = m_OptionMap;
    }
    catch (std::bad_alloc &)
    {
        return E_OUTOFMEMORY;
    }
    return S_OK;
}


HRESULT DHCPConfig::i_getScope(DHCPConfigScope_T *aScope)
{
    /* No locking needed. */
    *aScope = m_enmScope;
    return S_OK;
}


HRESULT DHCPConfig::i_getMinLeaseTime(ULONG *aMinLeaseTime)
{
    AutoReadLock alock(m_pHack COMMA_LOCKVAL_SRC_POS);
    *aMinLeaseTime = m_secMinLeaseTime;
    return S_OK;
}


HRESULT DHCPConfig::i_setMinLeaseTime(ULONG aMinLeaseTime)
{
    {
        AutoWriteLock alock(m_pHack COMMA_LOCKVAL_SRC_POS);
        m_secMinLeaseTime = aMinLeaseTime;
    }
    return i_doWriteConfig();
}


HRESULT DHCPConfig::i_getDefaultLeaseTime(ULONG *aDefaultLeaseTime)
{
    AutoReadLock alock(m_pHack COMMA_LOCKVAL_SRC_POS);
    *aDefaultLeaseTime = m_secDefaultLeaseTime;
    return S_OK;
}


HRESULT DHCPConfig::i_setDefaultLeaseTime(ULONG aDefaultLeaseTime)
{
    {
        AutoWriteLock alock(m_pHack COMMA_LOCKVAL_SRC_POS);
        m_secDefaultLeaseTime = aDefaultLeaseTime;
    }
    return i_doWriteConfig();
}


HRESULT DHCPConfig::i_getMaxLeaseTime(ULONG *aMaxLeaseTime)
{
    AutoReadLock alock(m_pHack COMMA_LOCKVAL_SRC_POS);
    *aMaxLeaseTime = m_secMaxLeaseTime;
    return S_OK;
}


HRESULT DHCPConfig::i_setMaxLeaseTime(ULONG aMaxLeaseTime)
{
    {
        AutoWriteLock alock(m_pHack COMMA_LOCKVAL_SRC_POS);
        m_secMaxLeaseTime = aMaxLeaseTime;
    }
    return i_doWriteConfig();
}


HRESULT DHCPConfig::i_setOption(DhcpOpt_T aOption, DHCPOptionEncoding_T aEncoding, const com::Utf8Str &aValue)
{
    /*
     * Validate the option as there is no point in allowing the user to set
     * something that the DHCP server does not grok.  It will only lead to
     * startup failures an no DHCP.  We share this code with the server.
     */
    DhcpOption *pParsed = NULL;
    int         rc      = VINF_SUCCESS;
    try
    {
        pParsed = DhcpOption::parse((uint8_t)aOption, aEncoding, aValue.c_str(), &rc);
    }
    catch (std::bad_alloc &)
    {
        return E_OUTOFMEMORY;
    }
    if (pParsed)
    {
        delete pParsed;

        /*
         * Add/change it.
         */
        {
            AutoWriteLock alock(m_pHack COMMA_LOCKVAL_SRC_POS);
            try
            {
                m_OptionMap[aOption] = settings::DhcpOptValue(aValue, aEncoding);
            }
            catch (std::bad_alloc &)
            {
                return E_OUTOFMEMORY;
            }
        }
        i_doWriteConfig();
        return S_OK;
    }

    if (rc == VERR_WRONG_TYPE)
        return m_pHack->setError(E_INVALIDARG, m_pHack->tr("Unsupported encoding %d (option %d, value %s)"),
                                 (int)aEncoding, (int)aOption, aValue.c_str());
    if (rc == VERR_NOT_SUPPORTED)
        return m_pHack->setError(E_INVALIDARG, m_pHack->tr("Unsupported option %d (encoding %d, value %s)"),
                                 (int)aOption, (int)aEncoding, aValue.c_str());
    return m_pHack->setError(E_INVALIDARG, m_pHack->tr("Malformed option %d value '%s' (encoding %d, rc=%Rrc)"),
                             (int)aOption, aValue.c_str(), (int)aEncoding, rc);
}


HRESULT DHCPConfig::i_removeOption(DhcpOpt_T aOption)
{
    {
        AutoWriteLock alock(m_pHack COMMA_LOCKVAL_SRC_POS);
        settings::DhcpOptionMap::iterator it = m_OptionMap.find(aOption);
        if (it != m_OptionMap.end())
            m_OptionMap.erase(it);
        else
            return m_pHack->setError(VBOX_E_OBJECT_NOT_FOUND, m_pHack->tr("DHCP option %u was not found"), aOption);
    }
    return i_doWriteConfig();
}


HRESULT DHCPConfig::i_removeAllOptions()
{
    {
        AutoWriteLock alock(m_pHack COMMA_LOCKVAL_SRC_POS);
        m_OptionMap.erase(m_OptionMap.begin(), m_OptionMap.end());
    }
    return i_doWriteConfig();
}


HRESULT DHCPConfig::i_getOption(DhcpOpt_T aOption, DHCPOptionEncoding_T *aEncoding, com::Utf8Str &aValue)
{
    AutoReadLock alock(m_pHack COMMA_LOCKVAL_SRC_POS);
    settings::DhcpOptionMap::const_iterator it = m_OptionMap.find(aOption);
    if (it != m_OptionMap.end())
    {
        *aEncoding = it->second.enmEncoding;
        return aValue.assignEx(it->second.strValue);
    }
    return m_pHack->setError(VBOX_E_OBJECT_NOT_FOUND, m_pHack->tr("DHCP option %u was not found"), aOption);
}


HRESULT DHCPConfig::i_getAllOptions(std::vector<DhcpOpt_T> &aOptions, std::vector<DHCPOptionEncoding_T> &aEncodings,
                                    std::vector<com::Utf8Str> &aValues)
{
    AutoReadLock alock(m_pHack COMMA_LOCKVAL_SRC_POS);
    try
    {
        aOptions.resize(m_OptionMap.size());
        aEncodings.resize(m_OptionMap.size());
        aValues.resize(m_OptionMap.size());
        size_t i = 0;
        for (settings::DhcpOptionMap::iterator it = m_OptionMap.begin(); it != m_OptionMap.end(); ++it, i++)
        {
            aOptions[i]   = it->first;
            aEncodings[i] = it->second.enmEncoding;
            aValues[i]    = it->second.strValue;
        }
    }
    catch (std::bad_alloc &)
    {
        return E_OUTOFMEMORY;
    }
    return S_OK;
}


HRESULT DHCPConfig::i_remove()
{
    return m_pParent->i_removeConfig(this, m_enmScope);
}



/**
 * Causes the global VirtualBox configuration file to be written
 *
 * @returns COM status code.
 *
 * @note    Must hold no locks when this is called!
 * @note    Public because DHCPGroupCondition needs to call it too.
 */
HRESULT DHCPConfig::i_doWriteConfig()
{
    AssertPtrReturn(m_pVirtualBox, E_FAIL);

    AutoWriteLock alock(m_pVirtualBox COMMA_LOCKVAL_SRC_POS);
    return m_pVirtualBox->i_saveSettings();
}


/**
 * Produces the Dhcpd configuration.
 *
 * The base class only saves DHCP options.
 *
 * @param   pElmConfig  The element where to put the configuration.
 * @throws  std::bad_alloc
 */
void DHCPConfig::i_writeDhcpdConfig(xml::ElementNode *pElmConfig)
{
    if (m_secMinLeaseTime > 0 )
        pElmConfig->setAttribute("secMinLeaseTime", (uint32_t)m_secMinLeaseTime);
    if (m_secDefaultLeaseTime > 0 )
        pElmConfig->setAttribute("secDefaultLeaseTime", (uint32_t)m_secDefaultLeaseTime);
    if (m_secMaxLeaseTime > 0 )
        pElmConfig->setAttribute("secMaxLeaseTime", (uint32_t)m_secMaxLeaseTime);

    for (settings::DhcpOptionMap::const_iterator it = m_OptionMap.begin(); it != m_OptionMap.end(); ++it)
    {
        xml::ElementNode *pOption = pElmConfig->createChild("Option");
        pOption->setAttribute("name", (int)it->first);
        pOption->setAttribute("encoding", it->second.enmEncoding);
        pOption->setAttribute("value", it->second.strValue);
    }
}



/*********************************************************************************************************************************
*   DHCPGlobalConfig Implementation                                                                                              *
*********************************************************************************************************************************/
#undef  LOG_GROUP
#define LOG_GROUP LOG_GROUP_MAIN_DHCPGLOBALCONFIG

HRESULT DHCPGlobalConfig::initWithDefaults(VirtualBox *a_pVirtualBox, DHCPServer *a_pParent)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    HRESULT hrc = DHCPConfig::i_initWithDefaults(a_pVirtualBox, a_pParent);
    if (SUCCEEDED(hrc))
        hrc = i_setOption(DhcpOpt_SubnetMask, DHCPOptionEncoding_Normal, "0.0.0.0");

    if (SUCCEEDED(hrc))
        autoInitSpan.setSucceeded();
    return hrc;
}


HRESULT DHCPGlobalConfig::initWithSettings(VirtualBox *a_pVirtualBox, DHCPServer *a_pParent, const settings::DHCPConfig &rConfig)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    HRESULT hrc = DHCPConfig::i_initWithSettings(a_pVirtualBox, a_pParent, rConfig);
    if (SUCCEEDED(hrc))
        autoInitSpan.setSucceeded();
    else
        autoInitSpan.setFailed(hrc);
    return hrc;
}


void DHCPGlobalConfig::uninit()
{
    AutoUninitSpan autoUninitSpan(this);
    if (!autoUninitSpan.uninitDone())
        autoUninitSpan.setSucceeded();
}


HRESULT DHCPGlobalConfig::i_saveSettings(settings::DHCPConfig &a_rDst)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    return DHCPConfig::i_saveSettings(a_rDst);
}


/**
 * For getting the network mask option value (IDHCPServer::netmask attrib).
 *
 * @returns COM status code.
 * @param   a_rDst          Where to return it.
 * @throws  nothing
 */
HRESULT DHCPGlobalConfig::i_getNetworkMask(com::Utf8Str &a_rDst)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    settings::DhcpOptionMap::const_iterator it = m_OptionMap.find(DhcpOpt_SubnetMask);
    if (it != m_OptionMap.end())
    {
        if (it->second.enmEncoding == DHCPOptionEncoding_Normal)
            return a_rDst.assignEx(it->second.strValue);
        return setError(VBOX_E_OBJECT_NOT_FOUND, tr("DHCP option DhcpOpt_SubnetMask is not in a legacy encoding"));
    }
    return setError(VBOX_E_OBJECT_NOT_FOUND, tr("DHCP option DhcpOpt_SubnetMask was not found"));
}


/**
 * For setting the network mask option value (IDHCPServer::netmask attrib).
 *
 * @returns COM status code.
 * @param   a_rSrc          The new value.
 * @throws  nothing
 */
HRESULT DHCPGlobalConfig::i_setNetworkMask(const com::Utf8Str &a_rSrc)
{
    /* Validate it before setting it: */
    RTNETADDRIPV4 AddrIgnored;
    int vrc = RTNetStrToIPv4Addr(a_rSrc.c_str(), &AddrIgnored);
    if (RT_FAILURE(vrc))
        return setErrorBoth(E_INVALIDARG, vrc, tr("Invalid IPv4 netmask '%s': %Rrc"), a_rSrc.c_str(), vrc);

    return i_setOption(DhcpOpt_SubnetMask, DHCPOptionEncoding_Normal, a_rSrc);
}


/**
 * Overriden to ensure the sanity of the DhcpOpt_SubnetMask option.
 */
HRESULT DHCPGlobalConfig::i_setOption(DhcpOpt_T aOption, DHCPOptionEncoding_T aEncoding, const com::Utf8Str &aValue)
{
    if (aOption != DhcpOpt_SubnetMask || aEncoding == DHCPOptionEncoding_Normal)
        return DHCPConfig::i_setOption(aOption, aEncoding, aValue);
    return setError(E_FAIL, tr("DhcpOpt_SubnetMask must use DHCPOptionEncoding_Normal as it is reflected by IDHCPServer::networkMask"));
}


/**
 * Overriden to ensure the sanity of the DhcpOpt_SubnetMask option.
 */
HRESULT DHCPGlobalConfig::i_removeOption(DhcpOpt_T aOption)
{
    if (aOption != DhcpOpt_SubnetMask)
        return DHCPConfig::i_removeOption(aOption);
    return setError(E_FAIL, tr("DhcpOpt_SubnetMask cannot be removed as it reflects IDHCPServer::networkMask"));
}


/**
 * Overriden to preserve the DhcpOpt_SubnetMask option.
 */
HRESULT DHCPGlobalConfig::i_removeAllOptions()
{
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        settings::DhcpOptionMap::iterator it = m_OptionMap.find(DhcpOpt_SubnetMask);
        m_OptionMap.erase(m_OptionMap.begin(), it);
        if (it != m_OptionMap.end())
        {
            ++it;
            if (it != m_OptionMap.end())
                m_OptionMap.erase(it, m_OptionMap.end());
        }
    }

    return i_doWriteConfig();
}


/**
 * Overriden to prevent removal.
 */
HRESULT DHCPGlobalConfig::i_remove()
{
    return setError(E_ACCESSDENIED, tr("Cannot delete the global config"));
}



/*********************************************************************************************************************************
*   DHCPGroupCondition Implementation                                                                                            *
*********************************************************************************************************************************/
#undef  LOG_GROUP
#define LOG_GROUP LOG_GROUP_MAIN_DHCPGROUPCONDITION

HRESULT DHCPGroupCondition::initWithDefaults(DHCPGroupConfig *a_pParent, bool a_fInclusive, DHCPGroupConditionType_T a_enmType,
                                             const com::Utf8Str a_strValue)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m_pParent    = a_pParent;
    m_fInclusive = a_fInclusive;
    m_enmType    = a_enmType;
    HRESULT hrc = m_strValue.assignEx(a_strValue);

    if (SUCCEEDED(hrc))
        autoInitSpan.setSucceeded();
    else
        autoInitSpan.setFailed(hrc);
    return hrc;
}


HRESULT DHCPGroupCondition::initWithSettings(DHCPGroupConfig *a_pParent, const settings::DHCPGroupCondition &a_rSrc)
{
    return initWithDefaults(a_pParent, a_rSrc.fInclusive, a_rSrc.enmType, a_rSrc.strValue);
}


void DHCPGroupCondition::uninit()
{
    AutoUninitSpan autoUninitSpan(this);
    if (!autoUninitSpan.uninitDone())
        autoUninitSpan.setSucceeded();
}


HRESULT DHCPGroupCondition::i_saveSettings(settings::DHCPGroupCondition &a_rDst)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    a_rDst.fInclusive = m_fInclusive;
    a_rDst.enmType    = m_enmType;
    return a_rDst.strValue.assignEx(m_strValue);
}


/**
 * Worker for validating the condition value according to the given type.
 *
 * @returns COM status code.
 * @param   enmType             The condition type.
 * @param   strValue            The condition value.
 * @param   pErrorDst           The object to use for reporting errors.
 */
/*static*/ HRESULT DHCPGroupCondition::i_validateTypeAndValue(DHCPGroupConditionType_T enmType, com::Utf8Str const &strValue,
                                                              VirtualBoxBase *pErrorDst)
{
    switch (enmType)
    {
        case DHCPGroupConditionType_MAC:
        {
            RTMAC MACAddress;
            int vrc = RTNetStrToMacAddr(strValue.c_str(), &MACAddress);
            if (RT_SUCCESS(vrc))
                return S_OK;
            return pErrorDst->setError(E_INVALIDARG, pErrorDst->tr("Not a valid MAC address: %s"), strValue.c_str());
        }

        case DHCPGroupConditionType_MACWildcard:
        {
            /* This must be colon separated double xdigit bytes.  Single bytes
               shorthand or raw hexstrings won't match anything.  For reasons of
               simplicity, '?' can only be used to match xdigits, '*' must match 1+
               chars. */
            /** @todo test this properly...   */
            const char *psz           = strValue.c_str();
            size_t      off           = 0;
            unsigned    cPairsLeft    = 6;
            bool        fSeenAsterisk = false;
            for (;;)
            {
                char ch = psz[off++];
                if (RT_C_IS_XDIGIT(ch) || ch == '?')
                {
                    ch = psz[off++];
                    if (RT_C_IS_XDIGIT(ch) || ch == '?')
                    {
                        ch = psz[off++];
                        cPairsLeft -= 1;
                        if (cPairsLeft == 0)
                        {
                            if (!ch)
                                return S_OK;
                            return pErrorDst->setError(E_INVALIDARG,
                                                       pErrorDst->tr("Trailing chars in MAC wildcard address: %s (offset %zu)"),
                                                       psz, off - 1);
                        }
                        if (ch == ':' || ch == '*')
                            continue;
                        if (ch == '\0' && fSeenAsterisk)
                            return S_OK;
                        return pErrorDst->setError(E_INVALIDARG,
                                                   pErrorDst->tr("Malformed MAC wildcard address: %s (offset %zu)"),
                                                   psz, off - 1);
                    }

                    if (ch == '*')
                    {
                        fSeenAsterisk = true;
                        do
                            ch = psz[off++];
                        while (ch == '*');
                        if (ch == '\0')
                            return S_OK;
                        cPairsLeft -= 1;
                        if (cPairsLeft == 0)
                            return pErrorDst->setError(E_INVALIDARG,
                                                       pErrorDst->tr("Trailing chars in MAC wildcard address: %s (offset %zu)"),
                                                       psz, off - 1);
                        if (ch == ':')
                            continue;
                    }
                    else
                        return pErrorDst->setError(E_INVALIDARG, pErrorDst->tr("Malformed MAC wildcard address: %s (offset %zu)"),
                                                   psz, off - 1);
                }
                else if (ch == '*')
                {
                    fSeenAsterisk = true;
                    do
                        ch = psz[off++];
                    while (ch == '*');
                    if (ch == '\0')
                        return S_OK;
                    if (ch == ':')
                    {
                        cPairsLeft -= 1;
                        if (cPairsLeft == 0)
                            return pErrorDst->setError(E_INVALIDARG,
                                                       pErrorDst->tr("Trailing chars in MAC wildcard address: %s (offset %zu)"),
                                                       psz, off - 1);
                        continue;
                    }

                }
                else
                    return pErrorDst->setError(E_INVALIDARG, pErrorDst->tr("Malformed MAC wildcard address: %s (offset %zu)"),
                                               psz, off - 1);

                /* Pick up after '*' in the two cases above: ch is not ':' or '\0'. */
                Assert(ch != ':' && ch != '\0');
                if (RT_C_IS_XDIGIT(ch) || ch == '?')
                {
                    ch = psz[off++];
                    if (RT_C_IS_XDIGIT(ch) || ch == '?' || ch == '*')
                    {
                        off -= 2;
                        continue;
                    }
                    if (ch == ':')
                    {
                        ch = psz[off++];
                        if (ch == '\0')
                            return S_OK;
                        cPairsLeft -= 1;
                        if (cPairsLeft == 0)
                            return pErrorDst->setError(E_INVALIDARG,
                                                       pErrorDst->tr("Trailing chars in MAC wildcard address: %s (offset %zu)"),
                                                       psz, off - 1);
                        continue;
                    }
                    if (ch == '\0')
                        return S_OK;
                    return pErrorDst->setError(E_INVALIDARG,
                                               pErrorDst->tr("Trailing chars in MAC wildcard address: %s (offset %zu)"),
                                               psz, off - 1);
                }
                return pErrorDst->setError(E_INVALIDARG,
                                           pErrorDst->tr("Malformed MAC wildcard address: %s (offset %zu)"),
                                           psz, off - 1);
            }
            break;
        }

        case DHCPGroupConditionType_vendorClassID:
        case DHCPGroupConditionType_vendorClassIDWildcard:
        case DHCPGroupConditionType_userClassID:
        case DHCPGroupConditionType_userClassIDWildcard:
            if (strValue.length() == 0)
                return pErrorDst->setError(E_INVALIDARG, pErrorDst->tr("Value cannot be empty"));
            if (strValue.length() < 255)
                return pErrorDst->setError(E_INVALIDARG, pErrorDst->tr("Value is too long: %zu bytes"), strValue.length());
            break;

        default:
            return pErrorDst->setError(E_INVALIDARG, pErrorDst->tr("Invalid condition type: %d"), enmType);
    }

    return S_OK;
}


HRESULT DHCPGroupCondition::getInclusive(BOOL *aInclusive)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    *aInclusive = m_fInclusive;
    return S_OK;
}


HRESULT DHCPGroupCondition::setInclusive(BOOL aInclusive)
{
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        if ((aInclusive != FALSE) == m_fInclusive)
            return S_OK;
        m_fInclusive = aInclusive != FALSE;
    }
    return m_pParent->i_doWriteConfig();
}


HRESULT DHCPGroupCondition::getType(DHCPGroupConditionType_T *aType)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    *aType = m_enmType;
    return S_OK;
}


HRESULT DHCPGroupCondition::setType(DHCPGroupConditionType_T aType)
{
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        if (aType == m_enmType)
            return S_OK;
        HRESULT hrc = i_validateTypeAndValue(aType, m_strValue, this);
        if (FAILED(hrc))
            return hrc;
        m_enmType = aType;
    }
    return m_pParent->i_doWriteConfig();
}


HRESULT DHCPGroupCondition::getValue(com::Utf8Str &aValue)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    return aValue.assignEx(m_strValue);
}


HRESULT DHCPGroupCondition::setValue(const com::Utf8Str &aValue)
{
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        if (aValue == m_strValue)
            return S_OK;
        HRESULT hrc = i_validateTypeAndValue(m_enmType, aValue, this);
        if (FAILED(hrc))
            return hrc;
        hrc = m_strValue.assignEx(aValue);
        if (FAILED(hrc))
            return hrc;
    }
    return m_pParent->i_doWriteConfig();
}


HRESULT DHCPGroupCondition::remove()
{
    return m_pParent->i_removeCondition(this);
}



/*********************************************************************************************************************************
*   DHCPGroupConfig Implementation                                                                                               *
*********************************************************************************************************************************/
#undef  LOG_GROUP
#define LOG_GROUP LOG_GROUP_MAIN_DHCPGROUPCONFIG


HRESULT DHCPGroupConfig::initWithDefaults(VirtualBox *a_pVirtualBox, DHCPServer *a_pParent, const com::Utf8Str &a_rName)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    Assert(m_Conditions.size() == 0);
    HRESULT hrc = DHCPConfig::i_initWithDefaults(a_pVirtualBox, a_pParent);
    if (SUCCEEDED(hrc))
        hrc = m_strName.assignEx(a_rName);

    if (SUCCEEDED(hrc))
        autoInitSpan.setSucceeded();
    else
        autoInitSpan.setFailed(hrc);
    return hrc;
}


HRESULT DHCPGroupConfig::initWithSettings(VirtualBox *a_pVirtualBox, DHCPServer *a_pParent, const settings::DHCPGroupConfig &a_rSrc)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    Assert(m_Conditions.size() == 0);
    HRESULT hrc = DHCPConfig::i_initWithSettings(a_pVirtualBox, a_pParent, a_rSrc);
    if (SUCCEEDED(hrc))
        hrc = m_strName.assignEx(a_rSrc.strName);

    for (settings::DHCPGroupConditionVec::const_iterator it = a_rSrc.vecConditions.begin();
         it != a_rSrc.vecConditions.end() && SUCCEEDED(hrc); ++it)
    {
        ComObjPtr<DHCPGroupCondition> ptrCondition;
        hrc = ptrCondition.createObject();
        if (SUCCEEDED(hrc))
        {
            hrc = ptrCondition->initWithSettings(this, *it);
            if (SUCCEEDED(hrc))
            {
                try
                {
                    m_Conditions.push_back(ptrCondition);
                }
                catch (std::bad_alloc &)
                {
                    hrc = E_OUTOFMEMORY;
                }
            }
        }
    }

    if (SUCCEEDED(hrc))
        autoInitSpan.setSucceeded();
    else
        autoInitSpan.setFailed(hrc);
    return hrc;
}


void    DHCPGroupConfig::uninit()
{
    AutoUninitSpan autoUninitSpan(this);
    if (!autoUninitSpan.uninitDone())
        autoUninitSpan.setSucceeded();
}


HRESULT DHCPGroupConfig::i_saveSettings(settings::DHCPGroupConfig &a_rDst)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT hrc = DHCPConfig::i_saveSettings(a_rDst);
    if (SUCCEEDED(hrc))
        hrc = a_rDst.strName.assignEx(m_strName);
    if (SUCCEEDED(hrc))
    {
        size_t const cConditions = m_Conditions.size();
        try
        {
            a_rDst.vecConditions.resize(cConditions);
        }
        catch (std::bad_alloc &)
        {
            hrc = E_OUTOFMEMORY;
        }

        for (size_t i = 0; i < cConditions && SUCCEEDED(hrc); i++)
            hrc = m_Conditions[i]->i_saveSettings(a_rDst.vecConditions[i]);
    }
    return hrc;
}


HRESULT DHCPGroupConfig::i_removeCondition(DHCPGroupCondition *a_pCondition)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    for (ConditionsIterator it = m_Conditions.begin(); it != m_Conditions.end();)
    {
        DHCPGroupCondition *pCurCondition = *it;
        if (pCurCondition == a_pCondition)
            it = m_Conditions.erase(it);
        else
            ++it;
    }

    /* Never mind if already delete, right? */
    return S_OK;
}


HRESULT DHCPGroupConfig::getName(com::Utf8Str &aName)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    return aName.assignEx(m_strName);
}


HRESULT DHCPGroupConfig::setName(const com::Utf8Str &aName)
{
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        if (aName == m_strName)
            return S_OK;
        HRESULT hrc = m_strName.assignEx(aName);
        if (FAILED(hrc))
            return hrc;
    }
    return i_doWriteConfig();
}


HRESULT DHCPGroupConfig::getConditions(std::vector<ComPtr<IDHCPGroupCondition> > &aConditions)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    size_t const cConditions = m_Conditions.size();
    try
    {
        aConditions.resize(cConditions);
    }
    catch (std::bad_alloc &)
    {
        return E_OUTOFMEMORY;
    }
    HRESULT hrc = S_OK;
    for (size_t i = 0; i < cConditions && SUCCEEDED(hrc); i++)
        hrc = m_Conditions[i].queryInterfaceTo(aConditions[i].asOutParam());
    return hrc;
}


HRESULT DHCPGroupConfig::addCondition(BOOL aInclusive, DHCPGroupConditionType_T aType, const com::Utf8Str &aValue,
                                      ComPtr<IDHCPGroupCondition> &aCondition)
{
    /*
     * Valdiate it.
     */
    HRESULT hrc = DHCPGroupCondition::i_validateTypeAndValue(aType, aValue, this);
    if (SUCCEEDED(hrc))
    {
        /*
         * Add it.
         */
        ComObjPtr<DHCPGroupCondition> ptrCondition;
        hrc = ptrCondition.createObject();
        if (SUCCEEDED(hrc))
            hrc = ptrCondition->initWithDefaults(this, aInclusive != FALSE, aType, aValue);
        if (SUCCEEDED(hrc))
        {
            hrc = ptrCondition.queryInterfaceTo(aCondition.asOutParam());
            if (SUCCEEDED(hrc))
            {
                {
                    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
                    try
                    {
                        m_Conditions.push_back(ptrCondition);
                    }
                    catch (std::bad_alloc &)
                    {
                        aCondition.setNull();
                        return E_OUTOFMEMORY;
                    }
                }

                hrc = i_doWriteConfig();
                if (FAILED(hrc))
                    aCondition.setNull();
            }
        }
    }

    return hrc;
}


HRESULT DHCPGroupConfig::removeAllConditions()
{
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        if (m_Conditions.size() == 0)
            return S_OK;

        /** @todo sever the weak parent link for each entry...  */
        m_Conditions.erase(m_Conditions.begin(), m_Conditions.end());
    }

    return i_doWriteConfig();
}



/*********************************************************************************************************************************
*   DHCPIndividualConfig Implementation                                                                                          *
*********************************************************************************************************************************/
#undef  LOG_GROUP
#define LOG_GROUP LOG_GROUP_MAIN_DHCPINDIVIDUALCONFIG

HRESULT DHCPIndividualConfig::initWithMachineIdAndSlot(VirtualBox *a_pVirtualBox, DHCPServer *a_pParent,
                                                       com::Guid const &a_idMachine, ULONG a_uSlot, uint32_t a_uMACAddressVersion)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    HRESULT hrc = DHCPConfig::i_initWithDefaults(a_pVirtualBox, a_pParent);
    if (SUCCEEDED(hrc))
    {
        unconst(m_enmScope)          = DHCPConfigScope_MachineNIC;
        unconst(m_idMachine)         = a_idMachine;
        unconst(m_uSlot)             = a_uSlot;
        m_uMACAddressResolvedVersion = a_uMACAddressVersion;

        autoInitSpan.setSucceeded();
    }
    return hrc;
}


HRESULT DHCPIndividualConfig::initWithMACAddress(VirtualBox *a_pVirtualBox, DHCPServer *a_pParent, PCRTMAC a_pMACAddress)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    HRESULT hrc = DHCPConfig::i_initWithDefaults(a_pVirtualBox, a_pParent);
    if (SUCCEEDED(hrc))
    {
        unconst(m_enmScope)   = DHCPConfigScope_MAC;
        unconst(m_MACAddress) = *a_pMACAddress;

        autoInitSpan.setSucceeded();
    }
    return hrc;
}


HRESULT DHCPIndividualConfig::initWithSettingsAndMachineIdAndSlot(VirtualBox *a_pVirtualBox, DHCPServer *a_pParent,
                                                                  settings::DHCPIndividualConfig const &rConfig,
                                                                  com::Guid const &a_idMachine, ULONG a_uSlot,
                                                                  uint32_t a_uMACAddressVersion)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    HRESULT hrc = DHCPConfig::i_initWithSettings(a_pVirtualBox, a_pParent, rConfig);
    if (SUCCEEDED(hrc))
    {
        unconst(m_enmScope)          = DHCPConfigScope_MachineNIC;
        unconst(m_idMachine)         = a_idMachine;
        unconst(m_uSlot)             = a_uSlot;
        m_uMACAddressResolvedVersion = a_uMACAddressVersion;
        m_strFixedAddress            = rConfig.strFixedAddress;

        autoInitSpan.setSucceeded();
    }
    return hrc;
}


HRESULT DHCPIndividualConfig::initWithSettingsAndMACAddress(VirtualBox *a_pVirtualBox, DHCPServer *a_pParent,
                                                            settings::DHCPIndividualConfig const &rConfig, PCRTMAC a_pMACAddress)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    HRESULT hrc = DHCPConfig::i_initWithSettings(a_pVirtualBox, a_pParent, rConfig);
    if (SUCCEEDED(hrc))
    {
        unconst(m_enmScope)   = DHCPConfigScope_MAC;
        unconst(m_MACAddress) = *a_pMACAddress;
        m_strFixedAddress     = rConfig.strFixedAddress;

        autoInitSpan.setSucceeded();
    }
    return hrc;
}


void    DHCPIndividualConfig::uninit()
{
    AutoUninitSpan autoUninitSpan(this);
    if (!autoUninitSpan.uninitDone())
        autoUninitSpan.setSucceeded();
}


HRESULT DHCPIndividualConfig::i_saveSettings(settings::DHCPIndividualConfig &a_rDst)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    a_rDst.uSlot = m_uSlot;
    int vrc = a_rDst.strMACAddress.printfNoThrow("%RTmac", &m_MACAddress);
    if (m_idMachine.isValid() && !m_idMachine.isZero() && RT_SUCCESS(vrc))
        vrc = a_rDst.strVMName.printfNoThrow("%RTuuid", m_idMachine.raw());
    if (RT_SUCCESS(vrc))
        vrc = a_rDst.strFixedAddress.assignNoThrow(m_strFixedAddress);
    if (RT_SUCCESS(vrc))
        return DHCPConfig::i_saveSettings(a_rDst);
    return E_OUTOFMEMORY;;
}


HRESULT DHCPIndividualConfig::getMACAddress(com::Utf8Str &aMACAddress)
{
    /* No locking needed here (the MAC address, machine UUID and NIC slot number cannot change). */
    RTMAC MACAddress;
    if (m_enmScope == DHCPConfigScope_MAC)
        MACAddress = m_MACAddress;
    else
    {
        HRESULT hrc = i_getMachineMAC(&MACAddress);
        if (FAILED(hrc))
            return hrc;
    }

    /* Format the return string: */
    int vrc = aMACAddress.printfNoThrow("%RTmac", &MACAddress);
    return RT_SUCCESS(vrc) ? S_OK : E_OUTOFMEMORY;
}


HRESULT DHCPIndividualConfig::getMachineId(com::Guid &aId)
{
    AutoReadLock(this COMMA_LOCKVAL_SRC_POS);
    aId = m_idMachine;
    return S_OK;
}


HRESULT DHCPIndividualConfig::getSlot(ULONG *aSlot)
{
    AutoReadLock(this COMMA_LOCKVAL_SRC_POS);
    *aSlot = m_uSlot;
    return S_OK;
}

HRESULT DHCPIndividualConfig::getFixedAddress(com::Utf8Str &aFixedAddress)
{
    AutoReadLock(this COMMA_LOCKVAL_SRC_POS);
    return aFixedAddress.assignEx(m_strFixedAddress);
}


HRESULT DHCPIndividualConfig::setFixedAddress(const com::Utf8Str &aFixedAddress)
{
    if (aFixedAddress.isNotEmpty())
    {
        RTNETADDRIPV4 AddrIgnored;
        int vrc = RTNetStrToIPv4Addr(aFixedAddress.c_str(), &AddrIgnored);
        if (RT_FAILURE(vrc))
            return setErrorBoth(E_INVALIDARG, vrc, tr("Invalid IPv4 address '%s': %Rrc"), aFixedAddress.c_str(), vrc);
    }

    {
        AutoWriteLock(this COMMA_LOCKVAL_SRC_POS);
        m_strFixedAddress = aFixedAddress;
    }
    return i_doWriteConfig();
}


/**
 * Gets the MAC address of m_idMachine + m_uSlot.
 *
 * @returns COM status code w/ setError.
 * @param   pMACAddress     Where to return the address.
 *
 * @note    Must be called without holding any DHCP related locks as that would
 *          be lock order violation.  The m_idMachine and m_uSlot values are
 *          practically const, so we don't need any locks here anyway.
 */
HRESULT DHCPIndividualConfig::i_getMachineMAC(PRTMAC pMACAddress)
{
    ComObjPtr<Machine> ptrMachine;
    HRESULT hrc = m_pVirtualBox->i_findMachine(m_idMachine, false /*fPermitInaccessible*/, true /*aSetError*/, &ptrMachine);
    if (SUCCEEDED(hrc))
    {
        ComPtr<INetworkAdapter> ptrNetworkAdapter;
        hrc = ptrMachine->GetNetworkAdapter(m_uSlot, ptrNetworkAdapter.asOutParam());
        if (SUCCEEDED(hrc))
        {
            com::Bstr bstrMACAddress;
            hrc = ptrNetworkAdapter->COMGETTER(MACAddress)(bstrMACAddress.asOutParam());
            if (SUCCEEDED(hrc))
            {
                Utf8Str strMACAddress;
                try
                {
                    strMACAddress = bstrMACAddress;
                }
                catch (std::bad_alloc &)
                {
                    return E_OUTOFMEMORY;
                }

                int vrc = RTNetStrToMacAddr(strMACAddress.c_str(), pMACAddress);
                if (RT_SUCCESS(vrc))
                    hrc = S_OK;
                else
                    hrc = setErrorBoth(E_FAIL, vrc, tr("INetworkAdapter returned bogus MAC address '%ls': %Rrc"),
                                       bstrMACAddress.raw(), vrc);
            }
        }
    }
    return hrc;
}


HRESULT DHCPIndividualConfig::i_resolveMACAddress(uint32_t uVersion)
{
    HRESULT hrc;
    if (m_enmScope == DHCPConfigScope_MachineNIC)
    {
        RTMAC MACAddress;
        hrc = i_getMachineMAC(&MACAddress);
        if (SUCCEEDED(hrc))
        {
            AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
            if ((int32_t)(m_uMACAddressResolvedVersion - uVersion) >= 0)
            {
                m_uMACAddressResolvedVersion = uVersion;
                m_MACAddress                 = MACAddress;
            }
        }
    }
    else
        hrc = S_OK;
    return hrc;
}


/**
 * Overridden to write out additional config.
 */
void DHCPIndividualConfig::i_writeDhcpdConfig(xml::ElementNode *pElmConfig)
{
    char szTmp[RTUUID_STR_LENGTH + 32];
    RTStrPrintf(szTmp, sizeof(szTmp), "%RTmac", &m_MACAddress);
    pElmConfig->setAttribute("MACAddress", szTmp);

    if (m_enmScope == DHCPConfigScope_MachineNIC)
    {
        RTStrPrintf(szTmp, sizeof(szTmp), "%RTuuid/%u", m_idMachine.raw(), m_uSlot);
        pElmConfig->setAttribute("name", szTmp);
    }

    pElmConfig->setAttribute("fixedAddress", m_strFixedAddress);

    DHCPConfig::i_writeDhcpdConfig(pElmConfig);
}

