/* $Id$ */
/** @file
 * VBox Qt GUI - UICloudEntityKey class declaration.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_UICloudEntityKey_h
#define FEQT_INCLUDED_SRC_manager_UICloudEntityKey_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QHash>
#include <QString>
#include <QUuid>

/** Cloud entity key definition. */
struct UICloudEntityKey
{
    /** Constructs cloud entity key on the basis of passed parameters.
      * @param  strProviderShortName  Brings provider short name.
      * @param  strProfileName        Brings profile name.
      * @param  uMachineId            Brings machine id. */
    UICloudEntityKey(const QString &strProviderShortName = QString(),
                     const QString &strProfileName = QString(),
                     const QUuid &uMachineId = QUuid());
    /** Constructs cloud entity key on the basis of @a another one. */
    UICloudEntityKey(const UICloudEntityKey &another);

    /** Returns whether this one key equals to @a another one. */
    bool operator==(const UICloudEntityKey &another) const;

    /** Returns string key representation. */
    QString toString() const;

    /** Holds provider short name. */
    QString m_strProviderShortName;
    /** Holds profile name. */
    QString m_strProfileName;
    /** Holds machine id. */
    QUuid m_uMachineId;
};

inline uint qHash(const UICloudEntityKey &key, uint uSeed)
{
    return qHash(key.toString(), uSeed);
}

#endif /* !FEQT_INCLUDED_SRC_manager_UICloudEntityKey_h */
