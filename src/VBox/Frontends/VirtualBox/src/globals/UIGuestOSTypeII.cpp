/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestOSTypeII class implementation.
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
#include "UIGuestOSTypeII.h"


void UIGuestOSTypeManager::reCacheGuestOSTypes(const CGuestOSTypeVector &guestOSTypes)
{
    m_guestOSTypes.clear();
    m_guestOSFamilies.clear();
    //m_guestOSTypesPerFamily.clear();
    QList<CGuestOSType> otherOSTypes;
    foreach (const CGuestOSType &comType, guestOSTypes)
    {
        if (comType.GetFamilyId().contains("other", Qt::CaseInsensitive))
        {
            otherOSTypes << comType;
            continue;
        }
        addGuestOSType(comType);
    }
    /* Add OS types with family other to the end of the lists: */
    foreach (const CGuestOSType &comType, otherOSTypes)
        addGuestOSType(comType);
}

void UIGuestOSTypeManager::addGuestOSType(const CGuestOSType &comType)
{
    m_guestOSTypes << UIGuestOSTypeII(comType);
    QPair<QString, QString> family = QPair<QString, QString>(m_guestOSTypes.last().getFamilyId(), m_guestOSTypes.last().getFamilyDescription());
    if (!m_guestOSFamilies.contains(family))
        m_guestOSFamilies << family;
}

const UIGuestOSTypeManager::UIGuestOSTypeFamilyInfo &UIGuestOSTypeManager::getFamilies() const
{
    return m_guestOSFamilies;
}

QStringList UIGuestOSTypeManager::getVariantListForFamilyId(const QString &strFamilyId) const
{
    QStringList variantList;
    foreach (const UIGuestOSTypeII &type, m_guestOSTypes)
    {
        if (type.getFamilyId() != strFamilyId)
            continue;
        const QString &strVariant = type.getVariant();
        if (!strVariant.isEmpty() && !variantList.contains(strVariant))
            variantList << strVariant;
    }
    return variantList;
}

UIGuestOSTypeManager::UIGuestOSTypeInfo UIGuestOSTypeManager::getTypeListForFamilyId(const QString &strFamilyId) const
{
    UIGuestOSTypeInfo typeInfoList;
    foreach (const UIGuestOSTypeII &type, m_guestOSTypes)
    {
        if (type.getFamilyId() != strFamilyId)
            continue;
        QPair<QString, QString> info(type.getId(), type.getDescription());

        if (!typeInfoList.contains(info))
            typeInfoList << info;
    }
    return typeInfoList;
}

UIGuestOSTypeManager::UIGuestOSTypeInfo UIGuestOSTypeManager::getTypeListForVariant(const QString &strVariant) const
{
    UIGuestOSTypeInfo typeInfoList;
    if (strVariant.isEmpty())
        return typeInfoList;

    foreach (const UIGuestOSTypeII &type, m_guestOSTypes)
    {
        if (type.getVariant() != strVariant)
            continue;
        QPair<QString, QString> info(type.getId(), type.getDescription());
        if (!typeInfoList.contains(info))
            typeInfoList << info;
    }
    return typeInfoList;
}

UIGuestOSTypeII UIGuestOSTypeManager::findGuestTypeById(const QString &strTypeId) const
{
    if (strTypeId.isEmpty())
        return UIGuestOSTypeII();
    foreach (const UIGuestOSTypeII &type, m_guestOSTypes)
    {
        if (type.getId() == strTypeId)
            return type;
    }
    return UIGuestOSTypeII();
}

KGraphicsControllerType UIGuestOSTypeManager::getRecommendedGraphicsController(const QString &strTypeId) const
{
    if (strTypeId.isEmpty())
        return KGraphicsControllerType_Null;
    foreach (const UIGuestOSTypeII &type, m_guestOSTypes)
    {
        if (type.getId() == strTypeId)
            return type.getRecommendedGraphicsController();
    }
    return KGraphicsControllerType_Null;
}

ULONG UIGuestOSTypeManager::getRecommendedRAM(const QString &strTypeId) const
{
    if (strTypeId.isEmpty())
        return 0;
    foreach (const UIGuestOSTypeII &type, m_guestOSTypes)
    {
        if (type.getId() == strTypeId)
            return type.getRecommendedRAM();
    }
    return 0;
}

ULONG UIGuestOSTypeManager::getRecommendedCPUCount(const QString &strTypeId) const
{
    if (strTypeId.isEmpty())
        return 0;
    foreach (const UIGuestOSTypeII &type, m_guestOSTypes)
    {
        if (type.getId() == strTypeId)
            return type.getRecommendedCPUCount();
    }
    return 0;
}

KFirmwareType UIGuestOSTypeManager::getRecommendedFirmware(const QString &strTypeId) const
{
    if (strTypeId.isEmpty())
        return KFirmwareType_Max;
    foreach (const UIGuestOSTypeII &type, m_guestOSTypes)
    {
        if (type.getId() == strTypeId)
            return type.getRecommendedFirmware();
    }
    return KFirmwareType_Max;
}

QString UIGuestOSTypeManager::getDescription(const QString &strTypeId) const
{
    if (strTypeId.isEmpty())
        return QString();
    foreach (const UIGuestOSTypeII &type, m_guestOSTypes)
    {
        if (type.getId() == strTypeId)
            return type.getDescription();
    }
    return QString();
}

LONG64 UIGuestOSTypeManager::getRecommendedHDD(const QString &strTypeId) const
{
    if (strTypeId.isEmpty())
        return 0;
    foreach (const UIGuestOSTypeII &type, m_guestOSTypes)
    {
        if (type.getId() == strTypeId)
            return type.getRecommendedHDD();
    }
    return 0;
}

KStorageBus UIGuestOSTypeManager::getRecommendedHDStorageBus(const QString &strTypeId) const
{
    if (strTypeId.isEmpty())
        return KStorageBus_Null;
    foreach (const UIGuestOSTypeII &type, m_guestOSTypes)
    {
        if (type.getId() == strTypeId)
            return type.getRecommendedHDStorageBus();
    }
    return KStorageBus_Null;
}

KStorageBus UIGuestOSTypeManager::getRecommendedDVDStorageBus(const QString &strTypeId) const
{
    if (strTypeId.isEmpty())
        return KStorageBus_Null;
    foreach (const UIGuestOSTypeII &type, m_guestOSTypes)
    {
        if (type.getId() == strTypeId)
            return type.getRecommendedDVDStorageBus();
    }
    return KStorageBus_Null;
}


bool UIGuestOSTypeManager::getRecommendedFloppy(const QString &strTypeId) const
{
    if (strTypeId.isEmpty())
        return false;
    foreach (const UIGuestOSTypeII &type, m_guestOSTypes)
    {
        if (type.getId() == strTypeId)
            return type.getRecommendedFloppy();
    }
    return false;
}

UIGuestOSTypeII::UIGuestOSTypeII()
{
}

UIGuestOSTypeII::UIGuestOSTypeII(const CGuestOSType &comGuestOSType)
    : m_comGuestOSType(comGuestOSType)
{
}

bool UIGuestOSTypeII::isOk() const
{
    return (!m_comGuestOSType.isNull() && m_comGuestOSType.isOk());
}

const QString &UIGuestOSTypeII::getFamilyId() const
{
    if (m_strFamilyId.isEmpty() && m_comGuestOSType.isOk())
        m_strFamilyId = m_comGuestOSType.GetFamilyId();
    return m_strFamilyId;
}

const QString &UIGuestOSTypeII::getFamilyDescription() const
{
    if (m_strFamilyDescription.isEmpty() && m_comGuestOSType.isOk())
        m_strFamilyDescription = m_comGuestOSType.GetFamilyDescription();
    return m_strFamilyDescription;
}

const QString &UIGuestOSTypeII::getId() const
{
    if (m_strId.isEmpty() && m_comGuestOSType.isOk())
        m_strId = m_comGuestOSType.GetId();
    return m_strId;
}

const QString &UIGuestOSTypeII::getVariant() const
{
    if (m_strVariant.isEmpty() && m_comGuestOSType.isOk())
        m_strVariant = m_comGuestOSType.GetVariant();
    return m_strVariant;
}

const QString &UIGuestOSTypeII::getDescription() const
{
    if (m_strDescription.isEmpty() && m_comGuestOSType.isOk())
        m_strDescription = m_comGuestOSType.GetDescription();
    return m_strDescription;
}

KStorageBus UIGuestOSTypeII::getRecommendedHDStorageBus() const
{
    if (m_comGuestOSType.isOk())
        return m_comGuestOSType.GetRecommendedHDStorageBus();
    return KStorageBus_Null;
}

ULONG UIGuestOSTypeII::getRecommendedRAM() const
{
    if (m_comGuestOSType.isOk())
        return m_comGuestOSType.GetRecommendedRAM();
    return 0;
}

KStorageBus UIGuestOSTypeII::getRecommendedDVDStorageBus() const
{
    if (m_comGuestOSType.isOk())
        return m_comGuestOSType.GetRecommendedDVDStorageBus();
    return KStorageBus_Null;
}

ULONG UIGuestOSTypeII::getRecommendedCPUCount() const
{
    if (m_comGuestOSType.isOk())
        return m_comGuestOSType.GetRecommendedCPUCount();
    return 0;
}

KFirmwareType UIGuestOSTypeII::getRecommendedFirmware() const
{
    if (m_comGuestOSType.isOk())
        return m_comGuestOSType.GetRecommendedFirmware();
    return  KFirmwareType_Max;
}

bool UIGuestOSTypeII::getRecommendedFloppy() const
{
    if (m_comGuestOSType.isOk())
        return m_comGuestOSType.GetRecommendedFloppy();
    return false;
}

LONG64 UIGuestOSTypeII::getRecommendedHDD() const
{
    if (m_comGuestOSType.isOk())
        return m_comGuestOSType.GetRecommendedHDD();
    return 0;
}

KGraphicsControllerType UIGuestOSTypeII::getRecommendedGraphicsController() const
{
    if (m_comGuestOSType.isOk())
        return m_comGuestOSType.GetRecommendedGraphicsController();
    return KGraphicsControllerType_Null;
}

bool UIGuestOSTypeII::operator==(const UIGuestOSTypeII &other)
{
    return m_comGuestOSType == other.m_comGuestOSType;
}

bool UIGuestOSTypeII::operator!=(const UIGuestOSTypeII &other)
{
    return m_comGuestOSType != other.m_comGuestOSType;
}
