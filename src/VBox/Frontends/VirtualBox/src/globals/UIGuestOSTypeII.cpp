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
    m_guestOSFamilyIds.clear();
    m_guestOSTypesPerFamily.clear();
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

    for (int i = 0; i < m_guestOSTypes.size(); ++i)
    {
        QList<int> &indexList = m_guestOSTypesPerFamily[m_guestOSTypes[i].getFamilyId()];
        indexList << i;
    }

    // foreach (const QString &strFamilyId, m_guestOSFamilyIds)
    // {
    //     printf(" ----family---%s\n", qPrintable(strFamilyId));
    //     const QList<int> &indices = m_guestOSTypesPerFamily[strFamilyId];
    //     if (!m_guestOSTypesPerFamily.contains(strFamilyId))
    //     {
    //         printf("\t-----empty\n");
    //         continue;
    //     }
    //     for (int i = 0; i < indices.size(); ++i)
    //     {
    //         if (indices[i] >= 0 && indices[i] < m_guestOSTypes.size())
    //         {
    //             //printf("\t%s %s\n", qPrintable(m_guestOSTypes[indices[i]].getDescription()), qPrintable(m_guestOSTypes[indices[i]].getVariant()));
    //         }
    //     }
    // }
}

void UIGuestOSTypeManager::addGuestOSType(const CGuestOSType &comType)
{
    m_guestOSTypes << UIGuestOSTypeII(comType);
    const QString &strFamilyId = m_guestOSTypes.last().getFamilyId();
    if (!m_guestOSFamilyIds.contains(strFamilyId))
        m_guestOSFamilyIds << strFamilyId;
}

UIGuestOSTypeII::UIGuestOSTypeII(const CGuestOSType &comGuestOSType)
    : m_comGuestOSType(comGuestOSType)
{
}

const QString &UIGuestOSTypeII::getFamilyId()
{
    if (m_strFamilyId.isEmpty() && m_comGuestOSType.isOk())
        m_strFamilyId = m_comGuestOSType.GetFamilyId();
    return m_strFamilyId;
}

const QString &UIGuestOSTypeII::getFamilyDescription()
{
    if (m_strFamilyDescription.isEmpty() && m_comGuestOSType.isOk())
        m_strFamilyDescription = m_comGuestOSType.GetFamilyDescription();
    return m_strFamilyDescription;
}

const QString &UIGuestOSTypeII::getId()
{
    if (m_strId.isEmpty() && m_comGuestOSType.isOk())
        m_strId = m_comGuestOSType.GetId();
    return m_strId;
}

const QString &UIGuestOSTypeII::getVariant()
{
    if (m_strVariant.isEmpty() && m_comGuestOSType.isOk())
        m_strVariant = m_comGuestOSType.GetVariant();
    return m_strVariant;
}

const QString &UIGuestOSTypeII::getDescription()
{
    if (m_strDescription.isEmpty() && m_comGuestOSType.isOk())
        m_strDescription = m_comGuestOSType.GetDescription();
    return m_strDescription;
}
