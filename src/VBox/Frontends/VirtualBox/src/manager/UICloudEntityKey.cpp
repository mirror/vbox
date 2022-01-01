/* $Id$ */
/** @file
 * VBox Qt GUI - UICloudEntityKey class implementation.
 */

/*
 * Copyright (C) 2012-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "UICloudEntityKey.h"


UICloudEntityKey::UICloudEntityKey(const QString &strProviderShortName /* = QString() */,
                                   const QString &strProfileName /* = QString() */,
                                   const QUuid &uMachineId /* = QUuid() */)
    : m_strProviderShortName(strProviderShortName)
    , m_strProfileName(strProfileName)
    , m_uMachineId(uMachineId)
{
}

UICloudEntityKey::UICloudEntityKey(const UICloudEntityKey &another)
    : m_strProviderShortName(another.m_strProviderShortName)
    , m_strProfileName(another.m_strProfileName)
    , m_uMachineId(another.m_uMachineId)
{
}

bool UICloudEntityKey::operator==(const UICloudEntityKey &another) const
{
    return    true
           && toString() == another.toString()
              ;
}

bool UICloudEntityKey::operator<(const UICloudEntityKey &another) const
{
    return    true
           && toString() < another.toString()
              ;
}

QString UICloudEntityKey::toString() const
{
    QString strResult;
    if (m_strProviderShortName.isEmpty())
        return strResult;
    strResult += QString("/%1").arg(m_strProviderShortName);
    if (m_strProfileName.isEmpty())
        return strResult;
    strResult += QString("/%1").arg(m_strProfileName);
    if (m_uMachineId.isNull())
        return strResult;
    strResult += QString("/%1").arg(m_uMachineId.toString());
    return strResult;
}
