/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestOSTypeII class declaration.
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

#ifndef FEQT_INCLUDED_SRC_globals_UIGuestOSTypeII_h
#define FEQT_INCLUDED_SRC_globals_UIGuestOSTypeII_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QList>
#include <QMap>
#include <QString>

/* COM includes: */
#include "COMEnums.h"
#include "CGuestOSType.h"

class UIGuestOSTypeII;



class UIGuestOSTypeManager
{

public:

    typedef QList<QPair<QString, QString> > UIGuestOSTypeFamilyInfo;
    typedef QList<QPair<QString, QString> > UIGuestOSTypeInfo;

    void reCacheGuestOSTypes(const CGuestOSTypeVector &guestOSTypes);

    const UIGuestOSTypeFamilyInfo &getFamilies() const;
    QStringList getVariantListForFamilyId(const QString &strFamilyId) const;

    UIGuestOSTypeInfo getTypeListForFamilyId(const QString &strFamilyId) const;
    UIGuestOSTypeInfo getTypeListForVariant(const QString &strVariant) const;

private:

    void addGuestOSType(const CGuestOSType &comType);

    QList<UIGuestOSTypeII> m_guestOSTypes;
    /* First item of the pair is family id and the 2nd is family description. */
    UIGuestOSTypeInfo m_guestOSFamilies;

};

/** A wrapper around CGuestOSType. */
class SHARED_LIBRARY_STUFF UIGuestOSTypeII
{
public:


    UIGuestOSTypeII(const CGuestOSType &comGuestOSType);

    const QString &getFamilyId() const;
    const QString &getFamilyDescription() const;
    const QString &getId() const;
    const QString &getVariant() const;
    const QString &getDescription() const;


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

#endif /* !FEQT_INCLUDED_SRC_globals_UIGuestOSTypeII_h */
