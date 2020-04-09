/* $Id$ */
/** @file
 * VBox Qt GUI - UITaskCloudAcquireInstances class implementation.
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
#include "UITaskCloudAcquireInstances.h"


UITaskCloudAcquireInstances::UITaskCloudAcquireInstances(const CCloudClient &comCloudClient, UIChooserNode *pParentNode)
    : UITask(Type_CloudAcquireInstances)
    , m_comCloudClient(comCloudClient)
    , m_pParentNode(pParentNode)
{
}

QVector<CCloudMachine> UITaskCloudAcquireInstances::result() const
{
    m_mutex.lock();
    const QVector<CCloudMachine> resultVector = m_result;
    m_mutex.unlock();
    return resultVector;
}

QString UITaskCloudAcquireInstances::errorInfo()
{
    m_mutex.lock();
    QString strErrorInfo = m_strErrorInfo;
    m_mutex.unlock();
    return strErrorInfo;
}

void UITaskCloudAcquireInstances::run()
{
    m_mutex.lock();
    m_result = listCloudMachines(m_comCloudClient, m_strErrorInfo);
    m_mutex.unlock();
}
