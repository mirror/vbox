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

    void reCacheGuestOSTypes(const CGuestOSTypeVector &guestOSTypes);

private:

    void addGuestOSType(const CGuestOSType &comType);

    QList<UIGuestOSTypeII> m_guestOSTypes;
    QStringList m_guestOSFamilyIds;
    /* Key is family id (linux, windows, etc) and value is the list of indices to m_guestOSTypes. */
    QMap<QString, QList<int> > m_guestOSTypesPerFamily;
    /* Key is variant (debian, ubuntu, ) and value is the list of indices to m_guestOSTypes. */
    QMap<QString, QList<int> > m_guestOSTypesPerVariant;
};

/** A wrapper around CGuestOSType. */
class SHARED_LIBRARY_STUFF UIGuestOSTypeII
{
public:


    UIGuestOSTypeII(const CGuestOSType &comGuestOSType);

    const QString &getFamilyId();
    const QString &getFamilyDescription();
    const QString &getId();
    const QString &getVariant();
    const QString &getDescription();


private:

    /** @name CGuestOSType properties. Cached here for a faster access.
      * @{ */
        QString m_strFamilyId;
        QString m_strFamilyDescription;
        QString m_strId;
        QString m_strVariant;
        QString m_strDescription;
    /** @} */

    CGuestOSType m_comGuestOSType;

};

#endif /* !FEQT_INCLUDED_SRC_globals_UIGuestOSTypeII_h */
