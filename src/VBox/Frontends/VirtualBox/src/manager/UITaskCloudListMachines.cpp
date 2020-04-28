/* $Id$ */
/** @file
 * VBox Qt GUI - UITaskCloudListMachines class implementation.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
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
#include "UICloudNetworkingStuff.h"
#include "UITaskCloudListMachines.h"

/* COM includes: */
#include "CCloudClient.h"


UITaskCloudListMachines::UITaskCloudListMachines(const QString &strProviderShortName, const QString &strProfileName)
    : UITask(Type_CloudListMachines)
    , m_strProviderShortName(strProviderShortName)
    , m_strProfileName(strProfileName)
{
}

QVector<CCloudMachine> UITaskCloudListMachines::result() const
{
    m_mutex.lock();
    const QVector<CCloudMachine> resultVector = m_result;
    m_mutex.unlock();
    return resultVector;
}

QString UITaskCloudListMachines::errorInfo() const
{
    m_mutex.lock();
    QString strErrorInfo = m_strErrorInfo;
    m_mutex.unlock();
    return strErrorInfo;
}

void UITaskCloudListMachines::run()
{
    m_mutex.lock();
    CCloudClient comCloudClient = cloudClientByName(m_strProviderShortName, m_strProfileName, m_strErrorInfo);
    if (m_strErrorInfo.isNull())
        m_result = listCloudMachines(comCloudClient, m_strErrorInfo);
    m_mutex.unlock();
}
