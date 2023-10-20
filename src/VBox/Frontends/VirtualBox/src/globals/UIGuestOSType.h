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

/** A wrapper around CGuestOSType. Some of the properties are cached here for performance. */
class SHARED_LIBRARY_STUFF UIGuestOSType
{

public:


    UIGuestOSType(const CGuestOSType &comGuestOSType);
    UIGuestOSType();

    const QString &getFamilyId() const;
    const QString &getFamilyDescription() const;
    const QString &getId() const;
    const QString &getSubtype() const;
    const QString &getDescription() const;

    /** @name Wrapper getters for CGuestOSType member.
      * @{ */
        KStorageBus             getRecommendedHDStorageBus() const;
        ULONG                   getRecommendedRAM() const;
        KStorageBus             getRecommendedDVDStorageBus() const;
        ULONG                   getRecommendedCPUCount() const;
        KFirmwareType           getRecommendedFirmware() const;
        bool                    getRecommendedFloppy() const;
        LONG64                  getRecommendedHDD() const;
        KGraphicsControllerType getRecommendedGraphicsController() const;
        KStorageControllerType  getRecommendedDVDStorageController() const;
    /** @} */

    bool isOk() const;

private:

    /** @name CGuestOSType properties. Cached here for a faster access.
      * @{ */
        mutable QString m_strFamilyId;
        mutable QString m_strFamilyDescription;
        mutable QString m_strId;
        mutable QString m_strSubtype;
        mutable QString m_strDescription;
    /** @} */

    CGuestOSType m_comGuestOSType;
};


/** A wrapper and manager class for Guest OS types (IGuestOSType). Logically we structure os types into families
  *  e.g. Window, Linux etc. Some families have so-called subtypes which for Linux corresponds to distros, while some
  *  families have no subtype. Under subtypes (and when no subtype exists direcly under family) we have guest os
  *  types, e.g. Debian12_x64 etc. */
class SHARED_LIBRARY_STUFF UIGuestOSTypeManager
{

public:

    UIGuestOSTypeManager();
    UIGuestOSTypeManager(const UIGuestOSTypeManager &other) = delete;

    /** A list of all OS families. 'first' of each pair is family Id and 'second' is family description. */
    typedef QVector<QPair<QString, QString> > UIGuestOSTypeFamilyInfo;
    /** Guest OS type info list for all type.  'first' is typeId and 'second' is description. */
    typedef QVector<QPair<QString, QString> > UIGuestOSTypeInfo;

    /** Re-create the guest OS type database. */
    void reCacheGuestOSTypes(const CGuestOSTypeVector &guestOSTypes);

    /** Returns a list of all families (id and description). */
    const UIGuestOSTypeFamilyInfo &getFamilies() const;
    /** Returns the list of subtypes for @p strFamilyId. This may be an empty list. */
    QStringList                    getSubtypeListForFamilyId(const QString &strFamilyId) const;
    /** Returns a list of OS types for the @p strFamilyId. */
    UIGuestOSTypeInfo              getTypeListForFamilyId(const QString &strFamilyId) const;
    /** Returns a list of OS types for the @p strSubtype. */
    UIGuestOSTypeInfo              getTypeListForSubtype(const QString &strSubtype) const;

    static bool isDOSType(const QString &strOSTypeId);

    /** @name Getters for UIGuestOSType properties. They utilize a map for faster access to UIGuestOSType instance with @p strTypeId
      * @{ */
        QString                 getFamilyId(const QString &strTypeId) const;
        QString                 getSubtype(const QString  &strTypeId) const;
        KGraphicsControllerType getRecommendedGraphicsController(const QString &strTypeId) const;
        ULONG                   getRecommendedRAM(const QString &strTypeId) const;
        ULONG                   getRecommendedCPUCount(const QString &strTypeId) const;
        KFirmwareType           getRecommendedFirmware(const QString &strTypeId) const;
        QString                 getDescription(const QString &strTypeId) const;
        LONG64                  getRecommendedHDD(const QString &strTypeId) const;
        KStorageBus             getRecommendedHDStorageBus(const QString &strTypeId) const;
        KStorageBus             getRecommendedDVDStorageBus(const QString &strTypeId) const;
        bool                    getRecommendedFloppy(const QString &strTypeId) const;
        KStorageControllerType  getRecommendedDVDStorageController(const QString &strTypeId) const;
        bool                    isLinux(const QString &strTypeId) const;
        bool                    isWindows(const QString &strTypeId) const;
    /** @} */

private:

    void addGuestOSType(const CGuestOSType &comType);

    /** The type list. Here it is a pointer to QVector to delay definition of UIGuestOSType. */
    QVector<UIGuestOSType> m_guestOSTypes;
    /** A map to prevent linear search of UIGuestOSType instances wrt. typeId. Key is typeId and value
      * is index to m_guestOSTypes list. */
    QMap<QString, int> m_typeIdIndexMap;
    /** First item of the pair is family id and the 2nd is family description. */
    UIGuestOSTypeInfo m_guestOSFamilies;
};


#endif /* !FEQT_INCLUDED_SRC_globals_UIGuestOSType_h */
