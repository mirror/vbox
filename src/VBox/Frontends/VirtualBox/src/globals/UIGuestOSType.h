/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestOSType class declaration.
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

#ifndef FEQT_INCLUDED_SRC_globals_UIGuestOSType_h
#define FEQT_INCLUDED_SRC_globals_UIGuestOSType_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QVector>
#include <QMap>
#include <QString>

/* COM includes: */
#include "COMEnums.h"
#include "CGuestOSType.h"

class UIGuestOSType;



class SHARED_LIBRARY_STUFF UIGuestOSTypeManager
{

public:

    typedef QVector<QPair<QString, QString> > UIGuestOSTypeFamilyInfo;
    typedef QVector<QPair<QString, QString> > UIGuestOSTypeInfo;

    void reCacheGuestOSTypes(const CGuestOSTypeVector &guestOSTypes);

    const UIGuestOSTypeFamilyInfo &getFamilies() const;
    QStringList getVariantListForFamilyId(const QString &strFamilyId) const;

    UIGuestOSTypeInfo getTypeListForFamilyId(const QString &strFamilyId) const;
    UIGuestOSTypeInfo getTypeListForVariant(const QString &strVariant) const;

    UIGuestOSType findGuestTypeById(const QString &strTypeId) const;

    KGraphicsControllerType getRecommendedGraphicsController(const QString &strTypeId) const;
    ULONG getRecommendedRAM(const QString &strTypeId) const;
    ULONG getRecommendedCPUCount(const QString &strTypeId) const;
    KFirmwareType getRecommendedFirmware(const QString &strTypeId) const;
    QString getDescription(const QString &strTypeId) const;
    LONG64 getRecommendedHDD(const QString &strTypeId) const;
    KStorageBus getRecommendedHDStorageBus(const QString &strTypeId) const;
    KStorageBus getRecommendedDVDStorageBus(const QString &strTypeId) const;
    bool getRecommendedFloppy(const QString &strTypeId) const;

private:

    void addGuestOSType(const CGuestOSType &comType);

    QVector<UIGuestOSType> m_guestOSTypes;
    /* First item of the pair is family id and the 2nd is family description. */
    UIGuestOSTypeInfo m_guestOSFamilies;

};

/** A wrapper around CGuestOSType. */
class SHARED_LIBRARY_STUFF UIGuestOSType
{

public:


    UIGuestOSType(const CGuestOSType &comGuestOSType);
    UIGuestOSType();

    const QString &getFamilyId() const;
    const QString &getFamilyDescription() const;
    const QString &getId() const;
    const QString &getVariant() const;
    const QString &getDescription() const;

    /** @name Wrapper getters for CGuestOSType member.
      * @{ */
        KStorageBus getRecommendedHDStorageBus() const;
        ULONG getRecommendedRAM() const;
        KStorageBus getRecommendedDVDStorageBus() const;
        ULONG getRecommendedCPUCount() const;
        KFirmwareType getRecommendedFirmware() const;
        bool getRecommendedFloppy() const;
        LONG64 getRecommendedHDD() const;
        KGraphicsControllerType getRecommendedGraphicsController() const;
    /** @} */

    bool isOk() const;
    bool operator==(const UIGuestOSType &other);
    bool operator!=(const UIGuestOSType &other);

private:

    /** @name CGuestOSType properties. Cached here for a faster access.
      * @{ */
        mutable QString m_strFamilyId;
        mutable QString m_strFamilyDescription;
        mutable QString m_strId;
        mutable QString m_strVariant;
        mutable QString m_strDescription;
    /** @} */

    CGuestOSType m_comGuestOSType;

};

#endif /* !FEQT_INCLUDED_SRC_globals_UIGuestOSType_h */
