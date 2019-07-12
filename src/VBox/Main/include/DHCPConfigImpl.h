/* $Id$ */
/** @file
 * VirtualBox Main - IDHCPConfig, IDHCPConfigGlobal, IDHCPConfigGroup, IDHCPConfigIndividual header.
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

#ifndef MAIN_INCLUDED_DHCPConfigImpl_h
#define MAIN_INCLUDED_DHCPConfigImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "DHCPGlobalConfigWrap.h"
#include "DHCPGroupConditionWrap.h"
#include "DHCPGroupConfigWrap.h"
#include "DHCPIndividualConfigWrap.h"
#include <VBox/settings.h>


class DHCPServer;
class DHCPGroupConfig;


/**
 * Base class for the a DHCP configration layer.
 *
 * This does not inherit from DHCPConfigWrap because its children need to
 * inherit from children of DHCPConfigWrap, which smells like trouble and thus
 * wasn't even attempted.  Instead, we have a hack for passing a pointer that we
 * can call setError and such on.
 */
class DHCPConfig
{
protected:
    /** Config scope (global, group, vm+nic, mac).  */
    DHCPConfigScope_T const m_enmScope;
    /** Minimum lease time. */
    ULONG                   m_secMinLeaseTime;
    /** Default lease time. */
    ULONG                   m_secDefaultLeaseTime;
    /** Maximum lease time. */
    ULONG                   m_secMaxLeaseTime;
    /** DHCP option map. */
    settings::DhcpOptionMap m_OptionMap;
    /** The DHCP server parent (weak).   */
    DHCPServer * const      m_pParent;
    /** The DHCP server parent (weak).   */
    VirtualBox * const      m_pVirtualBox;
private:
    /** For setError and such. */
    VirtualBoxBase * const  m_pHack;

protected:
    /** @name Constructors and destructors.
     * @{ */
    DHCPConfig(DHCPConfigScope_T a_enmScope, VirtualBoxBase *a_pHack)
        : m_enmScope(a_enmScope), m_secMinLeaseTime(0), m_secDefaultLeaseTime(0), m_secMaxLeaseTime(0), m_OptionMap()
        , m_pParent(NULL), m_pVirtualBox(NULL), m_pHack(a_pHack)
    {}
    DHCPConfig();
    virtual ~DHCPConfig()
    {}
    HRESULT i_initWithDefaults(VirtualBox *a_pVirtualBox, DHCPServer *a_pParent);
    HRESULT i_initWithSettings(VirtualBox *a_pVirtualBox, DHCPServer *a_pParent, const settings::DHCPConfig &rConfig);
    /** @} */

    /** @name IDHCPConfig properties
     * @{ */
    HRESULT i_getScope(DHCPConfigScope_T *aScope);
    HRESULT i_getMinLeaseTime(ULONG *aMinLeaseTime);
    HRESULT i_setMinLeaseTime(ULONG aMinLeaseTime);
    HRESULT i_getDefaultLeaseTime(ULONG *aDefaultLeaseTime);
    HRESULT i_setDefaultLeaseTime(ULONG aDefaultLeaseTime);
    HRESULT i_getMaxLeaseTime(ULONG *aMaxLeaseTime);
    HRESULT i_setMaxLeaseTime(ULONG aMaxLeaseTime);
    /** @} */

public:
    /** @name IDHCPConfig methods
     * @note public because the DHCPServer needs them for 6.0 interfaces.
     * @todo Make protected again when IDHCPServer is cleaned up.
     * @{ */
    virtual HRESULT i_setOption(DhcpOpt_T aOption, DHCPOptionEncoding_T aEncoding, const com::Utf8Str &aValue);

    virtual HRESULT i_removeOption(DhcpOpt_T aOption);
    virtual HRESULT i_removeAllOptions();
    HRESULT         i_getOption(DhcpOpt_T aOption, DHCPOptionEncoding_T *aEncoding, com::Utf8Str &aValue);
    HRESULT         i_getAllOptions(std::vector<DhcpOpt_T> &aOptions, std::vector<DHCPOptionEncoding_T> &aEncodings,
                                    std::vector<com::Utf8Str> &aValues);
    /** @} */


public:
    HRESULT             i_doWriteConfig();
    HRESULT             i_saveSettings(settings::DHCPConfig &a_rDst);
    DHCPConfigScope_T   i_getScope() const RT_NOEXCEPT { return m_enmScope; }
    virtual void        i_writeDhcpdConfig(xml::ElementNode *pElm);
};


/**
 * Global DHCP configuration.
 */
class DHCPGlobalConfig : public DHCPGlobalConfigWrap, public DHCPConfig
{
public:
    /** @name Constructors and destructors.
     * @{ */
    DHCPGlobalConfig()
        : DHCPConfig(DHCPConfigScope_Global, this)
    { }
    HRESULT FinalConstruct()
    {
        return BaseFinalConstruct();
    }
    void    FinalRelease()
    {
        uninit();
        BaseFinalRelease();
    }
    HRESULT initWithDefaults(VirtualBox *a_pVirtualBox, DHCPServer *a_pParent);
    HRESULT initWithSettings(VirtualBox *a_pVirtualBox, DHCPServer *a_pParent, const settings::DHCPConfig &rConfig);
    void    uninit();
    /** @} */

    HRESULT i_saveSettings(settings::DHCPConfig &a_rDst);
    HRESULT i_getNetworkMask(com::Utf8Str &a_rDst);
    HRESULT i_setNetworkMask(const com::Utf8Str &a_rSrc);

protected:
    /** @name wrapped IDHCPConfig properties
     * @{ */
    HRESULT getScope(DHCPConfigScope_T *aScope) RT_OVERRIDE             { return i_getScope(aScope); }
    HRESULT getMinLeaseTime(ULONG *aMinLeaseTime) RT_OVERRIDE           { return i_getMinLeaseTime(aMinLeaseTime); }
    HRESULT setMinLeaseTime(ULONG aMinLeaseTime) RT_OVERRIDE            { return i_setMinLeaseTime(aMinLeaseTime); }
    HRESULT getDefaultLeaseTime(ULONG *aDefaultLeaseTime) RT_OVERRIDE   { return i_getDefaultLeaseTime(aDefaultLeaseTime); }
    HRESULT setDefaultLeaseTime(ULONG aDefaultLeaseTime) RT_OVERRIDE    { return i_setDefaultLeaseTime(aDefaultLeaseTime); }
    HRESULT getMaxLeaseTime(ULONG *aMaxLeaseTime) RT_OVERRIDE           { return i_getMaxLeaseTime(aMaxLeaseTime); }
    HRESULT setMaxLeaseTime(ULONG aMaxLeaseTime) RT_OVERRIDE            { return i_setMaxLeaseTime(aMaxLeaseTime); }
    /** @} */

    /** @name wrapped IDHCPConfig methods
     * @{ */
    HRESULT setOption(DhcpOpt_T aOption, DHCPOptionEncoding_T aEncoding, const com::Utf8Str &aValue) RT_OVERRIDE
    {
        return i_setOption(aOption, aEncoding, aValue);
    }

    HRESULT removeOption(DhcpOpt_T aOption) RT_OVERRIDE
    {
        return i_removeOption(aOption);
    }

    HRESULT removeAllOptions() RT_OVERRIDE
    {
        return i_removeAllOptions();
    }

    HRESULT getOption(DhcpOpt_T aOption, DHCPOptionEncoding_T *aEncoding, com::Utf8Str &aValue) RT_OVERRIDE
    {
        return i_getOption(aOption, aEncoding, aValue);
    }

    HRESULT getAllOptions(std::vector<DhcpOpt_T> &aOptions, std::vector<DHCPOptionEncoding_T> &aEncodings,
                          std::vector<com::Utf8Str> &aValues) RT_OVERRIDE
    {
        return i_getAllOptions(aOptions, aEncodings, aValues);
    }
    /** @} */

public:
    HRESULT i_setOption(DhcpOpt_T aOption, DHCPOptionEncoding_T aEncoding, const com::Utf8Str &aValue) RT_OVERRIDE;
    HRESULT i_removeOption(DhcpOpt_T aOption) RT_OVERRIDE;
    HRESULT i_removeAllOptions() RT_OVERRIDE;
};


/**
 * DHCP Group inclusion/exclusion condition.
 */
class DHCPGroupCondition : public DHCPGroupConditionWrap
{
private:
    /** Inclusive or exclusive condition. */
    bool                        m_fInclusive;
    /** The condition type (or how m_strValue should be interpreted). */
    DHCPGroupConditionType_T    m_enmType;
    /** The value.  Interpreted according to m_enmType. */
    com::Utf8Str                m_strValue;
    /** Pointer to the parent (weak). */
    DHCPGroupConfig            *m_pParent;

public:
    /** @name Constructors and destructors.
     * @{ */
    DHCPGroupCondition()
        : m_enmType(DHCPGroupConditionType_MAC)
        , m_pParent(NULL)
    {}
    HRESULT FinalConstruct()
    {
        return BaseFinalConstruct();
    }
    void    FinalRelease()
    {
        uninit();
        BaseFinalRelease();
    }
    HRESULT initWithDefaults(DHCPGroupConfig *a_pParent, bool a_fInclusive, DHCPGroupConditionType_T a_enmType,
                             const com::Utf8Str a_strValue);
    HRESULT initWithSettings(DHCPGroupConfig *a_pParent, const settings::DHCPGroupCondition &a_rSrc);
    void    uninit();
    /** @} */

    HRESULT i_saveSettings(settings::DHCPGroupCondition &a_rDst);

protected:
    /** @name Wrapped IDHCPGroupCondition properties
     * @{ */
    HRESULT getInclusive(BOOL *aInclusive) RT_OVERRIDE;
    HRESULT setInclusive(BOOL aInclusive) RT_OVERRIDE;
    HRESULT getType(DHCPGroupConditionType_T *aType) RT_OVERRIDE;
    HRESULT setType(DHCPGroupConditionType_T aType) RT_OVERRIDE;
    HRESULT getValue(com::Utf8Str &aValue) RT_OVERRIDE;
    HRESULT setValue(const com::Utf8Str &aValue) RT_OVERRIDE;
    /** @} */

    /** @name Wrapped IDHCPGroupCondition methods
     * @{ */
    HRESULT remove() RT_OVERRIDE;
    /** @} */
};


/**
 * Group configuration.
 */
class DHCPGroupConfig : public DHCPGroupConfigWrap, public DHCPConfig
{
private:
    /** Group name. */
    com::Utf8Str                                m_strName;
    /** Group membership conditions.   */
    std::vector<ComObjPtr<DHCPGroupCondition> > m_Conditions;
    /** Iterator for m_Conditions. */
    typedef std::vector<ComObjPtr<DHCPGroupCondition> >::iterator ConditionsIterator;

public:
    /** @name Constructors and destructors.
     * @{ */
    DHCPGroupConfig()
        : DHCPConfig(DHCPConfigScope_Group, this)
    { }
    HRESULT FinalConstruct()
    {
        return BaseFinalConstruct();
    }
    void    FinalRelease()
    {
        uninit();
        BaseFinalRelease();
    }
    HRESULT initWithDefaults(VirtualBox *a_pVirtualBox, DHCPServer *a_pParent, const com::Utf8Str &a_rName);
    HRESULT initWithSettings(VirtualBox *a_pVirtualBox, DHCPServer *a_pParent, const settings::DHCPGroupConfig &a_rSrc);
    void    uninit();
    /** @} */

    HRESULT i_saveSettings(settings::DHCPGroupConfig &a_rDst);
    HRESULT i_removeCondition(DHCPGroupCondition *a_pCondition);

protected:
    /** @name Wrapped IDHCPConfig properties
     * @{ */
    HRESULT getScope(DHCPConfigScope_T *aScope) RT_OVERRIDE             { return i_getScope(aScope); }
    HRESULT getMinLeaseTime(ULONG *aMinLeaseTime) RT_OVERRIDE           { return i_getMinLeaseTime(aMinLeaseTime); }
    HRESULT setMinLeaseTime(ULONG aMinLeaseTime) RT_OVERRIDE            { return i_setMinLeaseTime(aMinLeaseTime); }
    HRESULT getDefaultLeaseTime(ULONG *aDefaultLeaseTime) RT_OVERRIDE   { return i_getDefaultLeaseTime(aDefaultLeaseTime); }
    HRESULT setDefaultLeaseTime(ULONG aDefaultLeaseTime) RT_OVERRIDE    { return i_setDefaultLeaseTime(aDefaultLeaseTime); }
    HRESULT getMaxLeaseTime(ULONG *aMaxLeaseTime) RT_OVERRIDE           { return i_getMaxLeaseTime(aMaxLeaseTime); }
    HRESULT setMaxLeaseTime(ULONG aMaxLeaseTime) RT_OVERRIDE            { return i_setMaxLeaseTime(aMaxLeaseTime); }
    /** @} */

    /** @name Wrapped IDHCPGroupConfig properties
     * @{ */
    HRESULT getName(com::Utf8Str &aName) RT_OVERRIDE;
    HRESULT setName(const com::Utf8Str &aName) RT_OVERRIDE;
    HRESULT getConditions(std::vector<ComPtr<IDHCPGroupCondition> > &aConditions) RT_OVERRIDE;
    /** @} */

    /** @name Wrapped IDHCPConfig methods
     * @{ */
    HRESULT setOption(DhcpOpt_T aOption, DHCPOptionEncoding_T aEncoding, const com::Utf8Str &aValue) RT_OVERRIDE
    {
        return i_setOption(aOption, aEncoding, aValue);
    }

    HRESULT removeOption(DhcpOpt_T aOption) RT_OVERRIDE
    {
        return i_removeOption(aOption);
    }

    HRESULT removeAllOptions() RT_OVERRIDE
    {
        return i_removeAllOptions();
    }

    HRESULT getOption(DhcpOpt_T aOption, DHCPOptionEncoding_T *aEncoding, com::Utf8Str &aValue) RT_OVERRIDE
    {
        return i_getOption(aOption, aEncoding, aValue);
    }

    HRESULT getAllOptions(std::vector<DhcpOpt_T> &aOptions, std::vector<DHCPOptionEncoding_T> &aEncodings,
                          std::vector<com::Utf8Str> &aValues) RT_OVERRIDE
    {
        return i_getAllOptions(aOptions, aEncodings, aValues);
    }
    /** @} */

    /** @name Wrapped IDHCPGroupConfig methods
     * @{ */
    HRESULT addCondition(BOOL aInclusive, DHCPGroupConditionType_T aType, const com::Utf8Str &aValue,
                         ComPtr<IDHCPGroupCondition> &aCondition) RT_OVERRIDE;
    /** @} */
};


/**
 * Individual DHCP configuration.
 */
class DHCPIndividualConfig : public DHCPIndividualConfigWrap, public DHCPConfig
{
private:
    /** The MAC address or all zeros. */
    RTMAC               m_MACAddress;
    /** The VM ID or all zeros. */
    com::Guid const     m_idMachine;
    /** The VM NIC slot number, or ~(ULONG)0. */
    ULONG const         m_uSlot;
    /** This is part of a hack to resolve the MAC address for
     * DHCPConfigScope_MachineNIC instances.  If non-zero, we m_MACAddress is valid.
     * To deal with the impossibly theoretical scenario that the DHCP server is
     * being started by more than one thread, this is a version number and not just
     * a boolean indicator.   */
    uint32_t volatile   m_uMACAddressResolvedVersion;

    /** The fixed IPv4 address, empty if dynamic. */
    com::Utf8Str        m_strFixedAddress;

public:
    /** @name Constructors and destructors.
     * @{ */
    DHCPIndividualConfig()
        : DHCPConfig(DHCPConfigScope_MAC, this)
        , m_uSlot(~(ULONG)0)
        , m_uMACAddressResolvedVersion(false)
    {
        RT_ZERO(m_MACAddress);
    }
    HRESULT FinalConstruct()
    {
        return BaseFinalConstruct();
    }
    void    FinalRelease()
    {
        uninit();
        BaseFinalRelease();
    }
    HRESULT initWithMachineIdAndSlot(VirtualBox *a_pVirtualBox, DHCPServer *a_pParent, com::Guid const &a_idMachine,
                                     ULONG a_uSlot, uint32_t a_uMACAddressVersion);
    HRESULT initWithMACAddress(VirtualBox *a_pVirtualBox, DHCPServer *a_pParent, PCRTMAC a_pMACAddress);
    HRESULT initWithSettingsAndMachineIdAndSlot(VirtualBox *a_pVirtualBox, DHCPServer *a_pParent,
                                                const settings::DHCPIndividualConfig &rData, com::Guid const &a_idMachine,
                                                ULONG a_uSlot, uint32_t a_uMACAddressVersion);
    HRESULT initWithSettingsAndMACAddress(VirtualBox *a_pVirtualBox, DHCPServer *a_pParent,
                                          const settings::DHCPIndividualConfig &rData, PCRTMAC a_pMACAddress);
    void    uninit();
    /** @} */

    /** @name Internal methods that are public for various reasons
     * @{ */
    HRESULT             i_saveSettings(settings::DHCPIndividualConfig &a_rDst);
    RTMAC const        &i_getMACAddress() const RT_NOEXCEPT    { return m_MACAddress; }
    com::Guid const    &i_getMachineId() const RT_NOEXCEPT     { return m_idMachine; }
    ULONG               i_getSlot() const RT_NOEXCEPT          { return m_uSlot; }
    HRESULT             i_getMachineMAC(PRTMAC pMACAddress);

    HRESULT             i_resolveMACAddress(uint32_t uVersion);
    /** This is used to avoid producing bogus Dhcpd configuration elements. */
    bool                i_isMACAddressResolved(uint32_t uVersion) const
    {
        return m_enmScope != DHCPConfigScope_MachineNIC || (int32_t)(m_uMACAddressResolvedVersion - uVersion) >= 0;
    }
    void                i_writeDhcpdConfig(xml::ElementNode *pElm) RT_OVERRIDE;
    /** @} */

protected:
    /** @name wrapped IDHCPConfig properties
     * @{ */
    HRESULT getScope(DHCPConfigScope_T *aScope) RT_OVERRIDE             { return i_getScope(aScope); }
    HRESULT getMinLeaseTime(ULONG *aMinLeaseTime) RT_OVERRIDE           { return i_getMinLeaseTime(aMinLeaseTime); }
    HRESULT setMinLeaseTime(ULONG aMinLeaseTime) RT_OVERRIDE            { return i_setMinLeaseTime(aMinLeaseTime); }
    HRESULT getDefaultLeaseTime(ULONG *aDefaultLeaseTime) RT_OVERRIDE   { return i_getDefaultLeaseTime(aDefaultLeaseTime); }
    HRESULT setDefaultLeaseTime(ULONG aDefaultLeaseTime) RT_OVERRIDE    { return i_setDefaultLeaseTime(aDefaultLeaseTime); }
    HRESULT getMaxLeaseTime(ULONG *aMaxLeaseTime) RT_OVERRIDE           { return i_getMaxLeaseTime(aMaxLeaseTime); }
    HRESULT setMaxLeaseTime(ULONG aMaxLeaseTime) RT_OVERRIDE            { return i_setMaxLeaseTime(aMaxLeaseTime); }
    /** @} */

    /** @name wrapped IDHCPConfig methods
     * @{ */
    HRESULT setOption(DhcpOpt_T aOption, DHCPOptionEncoding_T aEncoding, const com::Utf8Str &aValue) RT_OVERRIDE
    {
        return i_setOption(aOption, aEncoding, aValue);
    }

    HRESULT removeOption(DhcpOpt_T aOption) RT_OVERRIDE
    {
        return i_removeOption(aOption);
    }

    HRESULT removeAllOptions() RT_OVERRIDE
    {
        return i_removeAllOptions();
    }

    HRESULT getOption(DhcpOpt_T aOption, DHCPOptionEncoding_T *aEncoding, com::Utf8Str &aValue) RT_OVERRIDE
    {
        return i_getOption(aOption, aEncoding, aValue);
    }

    HRESULT getAllOptions(std::vector<DhcpOpt_T> &aOptions, std::vector<DHCPOptionEncoding_T> &aEncodings,
                          std::vector<com::Utf8Str> &aValues) RT_OVERRIDE
    {
        return i_getAllOptions(aOptions, aEncodings, aValues);
    }
    /** @} */

    /** @name IDHCPIndividualConfig properties
     * @{ */
    HRESULT getMACAddress(com::Utf8Str &aMacAddress) RT_OVERRIDE;
    HRESULT getMachineId(com::Guid &aId) RT_OVERRIDE;
    HRESULT getSlot(ULONG *aSlot) RT_OVERRIDE;
    HRESULT getFixedAddress(com::Utf8Str &aFixedAddress) RT_OVERRIDE;
    HRESULT setFixedAddress(const com::Utf8Str &aFixedAddress) RT_OVERRIDE;
    /** @} */
};

#endif /* !MAIN_INCLUDED_DHCPConfigImpl_h */

