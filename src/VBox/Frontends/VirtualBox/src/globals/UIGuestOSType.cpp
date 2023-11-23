/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestOSType class implementation.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* GUI includes: */
#include "UICommon.h"
#include "UIGuestOSType.h"

/* COM includes: */
#include "CGuestOSType.h"
#include "CPlatformProperties.h"
#include "CSystemProperties.h"


void UIGuestOSTypeManager::reCacheGuestOSTypes()
{
    /* Acquire a list of guest OS types supported by this host: */
    CGuestOSTypeVector guestOSTypes;
    CSystemProperties comSysProps = uiCommon().virtualBox().GetSystemProperties();
    foreach (const KPlatformArchitecture &enmArch, comSysProps.GetSupportedPlatformArchitectures())
    {
        CPlatformProperties comPlatProps = uiCommon().virtualBox().GetPlatformProperties(enmArch);
        guestOSTypes += comPlatProps.GetSupportedGuestOSTypes();
    }

    /* Wipe out cache: */
    m_typeIdIndexMap.clear();
    m_guestOSTypes.clear();
    m_guestOSFamilies.clear();
    m_guestOSFamilyArch.clear();
    m_guestOSSubtypeArch.clear();

    /* Enumerate guest OS types: */
    QVector<CGuestOSType> otherOSTypes;
    foreach (const CGuestOSType &comType, guestOSTypes)
    {
        /* Filter out "other" family types: */
        if (comType.GetFamilyId().contains("other", Qt::CaseInsensitive))
        {
            otherOSTypes << comType;
            continue;
        }
        addGuestOSType(comType);
    }

    /* Add OS types with family "other" to the end of the lists: */
    foreach (const CGuestOSType &comType, otherOSTypes)
        addGuestOSType(comType);
}

void UIGuestOSTypeManager::addGuestOSType(const CGuestOSType &comType)
{
    m_guestOSTypes.append(UIGuestOSType(comType));
    m_typeIdIndexMap[m_guestOSTypes.last().getId()] = m_guestOSTypes.size() - 1;
    const QString strFamilyId = m_guestOSTypes.last().getFamilyId();
    const QString strFamilyDesc = m_guestOSTypes.last().getFamilyDescription();
    const QString strSubtype = m_guestOSTypes.last().getSubtype();
    QPair<QString, QString> family = QPair<QString, QString>(strFamilyId, strFamilyDesc);
    if (!m_guestOSFamilies.contains(family))
        m_guestOSFamilies << family;

    /* Acquire arch type: */
    const KPlatformArchitecture enmArch = m_guestOSTypes.last().getPlatformArchitecture();
    /* Cache family arch type; That will be x86, ARM or None (for *any*): */
    if (!m_guestOSFamilyArch.contains(strFamilyId))
        m_guestOSFamilyArch[strFamilyId] = enmArch;
    else if (m_guestOSFamilyArch.value(strFamilyId) != enmArch)
        m_guestOSFamilyArch[strFamilyId] = KPlatformArchitecture_None;
    /* Cache subtype arch type; That will be x86, ARM or None (for *any*): */
    if (!m_guestOSSubtypeArch.contains(strSubtype))
        m_guestOSSubtypeArch[strSubtype] = enmArch;
    else if (m_guestOSSubtypeArch.value(strSubtype) != enmArch)
        m_guestOSSubtypeArch[strSubtype] = KPlatformArchitecture_None;
}

UIGuestOSTypeManager::UIGuestOSFamilyInfo
UIGuestOSTypeManager::getFamilies(KPlatformArchitecture enmArch /* = KPlatformArchitecture_None */) const
{
    /* Return all families by default: */
    if (enmArch == KPlatformArchitecture_None)
        return m_guestOSFamilies;

    /* Otherwise we'll have to prepare list by arch type: */
    UIGuestOSTypeManager::UIGuestOSFamilyInfo families;
    foreach (const UIGuestInfoPair &family, m_guestOSFamilies)
    {
        const KPlatformArchitecture enmCurrentArch = m_guestOSFamilyArch.value(family.first, KPlatformArchitecture_Max);
        if (   enmCurrentArch == enmArch
            || enmCurrentArch == KPlatformArchitecture_None)
            families << family;
    }
    return families;
}

QStringList
UIGuestOSTypeManager::getSubtypeListForFamilyId(const QString &strFamilyId,
                                                KPlatformArchitecture enmArch /* = KPlatformArchitecture_None */) const
{
    /* Prepare list by arch type: */
    QStringList subtypes;
    foreach (const UIGuestOSType &type, m_guestOSTypes)
    {
        if (type.getFamilyId() != strFamilyId)
            continue;
        const QString strSubtype = type.getSubtype();
        if (strSubtype.isEmpty() || subtypes.contains(strSubtype))
            continue;
        const KPlatformArchitecture enmCurrentArch = m_guestOSSubtypeArch.value(strSubtype, KPlatformArchitecture_Max);
        if (   enmCurrentArch == enmArch
            || enmArch == KPlatformArchitecture_None
            || enmCurrentArch == KPlatformArchitecture_None)
            subtypes << strSubtype;
    }
    return subtypes;
}

UIGuestOSTypeManager::UIGuestOSTypeInfo
UIGuestOSTypeManager::getTypeListForFamilyId(const QString &strFamilyId,
                                             KPlatformArchitecture enmArch /* = KPlatformArchitecture_None */) const
{
    UIGuestOSTypeInfo typeInfoList;
    foreach (const UIGuestOSType &type, m_guestOSTypes)
    {
        if (type.getFamilyId() != strFamilyId)
            continue;
        QPair<QString, QString> info(type.getId(), type.getDescription());
        if (typeInfoList.contains(info))
            continue;
        if (   enmArch == KPlatformArchitecture_None
            || type.getPlatformArchitecture() == enmArch)
            typeInfoList << info;
    }
    return typeInfoList;
}

UIGuestOSTypeManager::UIGuestOSTypeInfo
UIGuestOSTypeManager::getTypeListForSubtype(const QString &strSubtype,
                                            KPlatformArchitecture enmArch /* = KPlatformArchitecture_None */) const
{
    UIGuestOSTypeInfo typeInfoList;
    if (strSubtype.isEmpty())
        return typeInfoList;
    foreach (const UIGuestOSType &type, m_guestOSTypes)
    {
        if (type.getSubtype() != strSubtype)
            continue;
        QPair<QString, QString> info(type.getId(), type.getDescription());
        if (typeInfoList.contains(info))
            continue;
        if (   enmArch == KPlatformArchitecture_None
            || type.getPlatformArchitecture() == enmArch)
            typeInfoList << info;
    }
    return typeInfoList;
}

QString UIGuestOSTypeManager::getFamilyId(const QString &strTypeId) const
{
    /* Let QVector<>::value check for the bounds. It returns a default constructed value when it is out of bounds. */
    return m_guestOSTypes.value(m_typeIdIndexMap.value(strTypeId, -1)).getFamilyId();
}

QString UIGuestOSTypeManager::getSubtype(const QString  &strTypeId) const
{
    return m_guestOSTypes.value(m_typeIdIndexMap.value(strTypeId, -1)).getSubtype();
}

KGraphicsControllerType UIGuestOSTypeManager::getRecommendedGraphicsController(const QString &strTypeId) const
{
    return m_guestOSTypes.value(m_typeIdIndexMap.value(strTypeId, -1)).getRecommendedGraphicsController();
}

KStorageControllerType UIGuestOSTypeManager::getRecommendedDVDStorageController(const QString &strTypeId) const
{
    return m_guestOSTypes.value(m_typeIdIndexMap.value(strTypeId, -1)).getRecommendedDVDStorageController();
}

ULONG UIGuestOSTypeManager::getRecommendedRAM(const QString &strTypeId) const
{
    return m_guestOSTypes.value(m_typeIdIndexMap.value(strTypeId, -1)).getRecommendedRAM();
}

ULONG UIGuestOSTypeManager::getRecommendedCPUCount(const QString &strTypeId) const
{
    return m_guestOSTypes.value(m_typeIdIndexMap.value(strTypeId, -1)).getRecommendedCPUCount();
}

KFirmwareType UIGuestOSTypeManager::getRecommendedFirmware(const QString &strTypeId) const
{
    return m_guestOSTypes.value(m_typeIdIndexMap.value(strTypeId, -1)).getRecommendedFirmware();
}

QString UIGuestOSTypeManager::getDescription(const QString &strTypeId) const
{
    return m_guestOSTypes.value(m_typeIdIndexMap.value(strTypeId, -1)).getDescription();
}

LONG64 UIGuestOSTypeManager::getRecommendedHDD(const QString &strTypeId) const
{
    return m_guestOSTypes.value(m_typeIdIndexMap.value(strTypeId, -1)).getRecommendedHDD();
}

KStorageBus UIGuestOSTypeManager::getRecommendedHDStorageBus(const QString &strTypeId) const
{
    return m_guestOSTypes.value(m_typeIdIndexMap.value(strTypeId, -1)).getRecommendedHDStorageBus();
}

KStorageBus UIGuestOSTypeManager::getRecommendedDVDStorageBus(const QString &strTypeId) const
{
    return m_guestOSTypes.value(m_typeIdIndexMap.value(strTypeId, -1)).getRecommendedDVDStorageBus();
}

bool UIGuestOSTypeManager::getRecommendedFloppy(const QString &strTypeId) const
{
    return m_guestOSTypes.value(m_typeIdIndexMap.value(strTypeId, -1)).getRecommendedFloppy();
}

bool UIGuestOSTypeManager::isLinux(const QString &strTypeId) const
{
    QString strFamilyId = getFamilyId(strTypeId);
    if (strFamilyId.contains("linux", Qt::CaseInsensitive))
        return true;
    return false;
}

bool UIGuestOSTypeManager::isWindows(const QString &strTypeId) const
{
    QString strFamilyId = getFamilyId(strTypeId);
    if (strFamilyId.contains("windows", Qt::CaseInsensitive))
        return true;
    return false;
}

bool UIGuestOSTypeManager::is64Bit(const QString &strTypeId) const
{
    return m_guestOSTypes.value(m_typeIdIndexMap.value(strTypeId, -1)).is64Bit();
}

KPlatformArchitecture UIGuestOSTypeManager::getPlatformArchitecture(const QString &strTypeId) const
{
    return m_guestOSTypes.value(m_typeIdIndexMap.value(strTypeId, -1)).getPlatformArchitecture();
}

/* static */
bool UIGuestOSTypeManager::isDOSType(const QString &strOSTypeId)
{
    if (   strOSTypeId.left(3) == "dos"
        || strOSTypeId.left(3) == "win"
        || strOSTypeId.left(3) == "os2")
        return true;

    return false;
}

/*********************************************************************************************************************************
*   UIGuestOSType implementaion.                                                                                     *
*********************************************************************************************************************************/

UIGuestOSType::UIGuestOSType()
{
}

UIGuestOSType::UIGuestOSType(const CGuestOSType &comGuestOSType)
    : m_comGuestOSType(comGuestOSType)
{
}

bool UIGuestOSType::isOk() const
{
    return (!m_comGuestOSType.isNull() && m_comGuestOSType.isOk());
}

const QString &UIGuestOSType::getFamilyId() const
{
    if (m_strFamilyId.isEmpty() && m_comGuestOSType.isOk())
        m_strFamilyId = m_comGuestOSType.GetFamilyId();
    return m_strFamilyId;
}

const QString &UIGuestOSType::getFamilyDescription() const
{
    if (m_strFamilyDescription.isEmpty() && m_comGuestOSType.isOk())
        m_strFamilyDescription = m_comGuestOSType.GetFamilyDescription();
    return m_strFamilyDescription;
}

const QString &UIGuestOSType::getId() const
{
    if (m_strId.isEmpty() && m_comGuestOSType.isOk())
        m_strId = m_comGuestOSType.GetId();
    return m_strId;
}

const QString &UIGuestOSType::getSubtype() const
{
    if (m_strSubtype.isEmpty() && m_comGuestOSType.isOk())
        m_strSubtype = m_comGuestOSType.GetSubtype();
    return m_strSubtype;
}

const QString &UIGuestOSType::getDescription() const
{
    if (m_strDescription.isEmpty() && m_comGuestOSType.isOk())
        m_strDescription = m_comGuestOSType.GetDescription();
    return m_strDescription;
}

KStorageBus UIGuestOSType::getRecommendedHDStorageBus() const
{
    if (m_comGuestOSType.isOk())
        return m_comGuestOSType.GetRecommendedHDStorageBus();
    return KStorageBus_Null;
}

ULONG UIGuestOSType::getRecommendedRAM() const
{
    if (m_comGuestOSType.isOk())
        return m_comGuestOSType.GetRecommendedRAM();
    return 0;
}

KStorageBus UIGuestOSType::getRecommendedDVDStorageBus() const
{
    if (m_comGuestOSType.isOk())
        return m_comGuestOSType.GetRecommendedDVDStorageBus();
    return KStorageBus_Null;
}

ULONG UIGuestOSType::getRecommendedCPUCount() const
{
    if (m_comGuestOSType.isOk())
        return m_comGuestOSType.GetRecommendedCPUCount();
    return 0;
}

KFirmwareType UIGuestOSType::getRecommendedFirmware() const
{
    if (m_comGuestOSType.isOk())
        return m_comGuestOSType.GetRecommendedFirmware();
    return  KFirmwareType_Max;
}

bool UIGuestOSType::getRecommendedFloppy() const
{
    if (m_comGuestOSType.isOk())
        return m_comGuestOSType.GetRecommendedFloppy();
    return false;
}

LONG64 UIGuestOSType::getRecommendedHDD() const
{
    if (m_comGuestOSType.isOk())
        return m_comGuestOSType.GetRecommendedHDD();
    return 0;
}

KGraphicsControllerType UIGuestOSType::getRecommendedGraphicsController() const
{
    if (m_comGuestOSType.isOk())
        return m_comGuestOSType.GetRecommendedGraphicsController();
    return KGraphicsControllerType_Null;
}

KStorageControllerType UIGuestOSType::getRecommendedDVDStorageController() const
{
    if (m_comGuestOSType.isOk())
        return m_comGuestOSType.GetRecommendedDVDStorageController();
    return KStorageControllerType_Null;
}

bool UIGuestOSType::is64Bit() const
{
    if (m_comGuestOSType.isOk())
        return m_comGuestOSType.GetIs64Bit();
    return false;
}

KPlatformArchitecture UIGuestOSType::getPlatformArchitecture() const
{
    if (m_comGuestOSType.isOk())
        return m_comGuestOSType.GetPlatformArchitecture();
    return KPlatformArchitecture_Max;
}
