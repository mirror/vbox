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
#include "UITaskCloudAcquireInstances.h"

/* COM includes: */
#include "CProgress.h"
#include "CStringArray.h"


UITaskCloudAcquireInstances::UITaskCloudAcquireInstances(const CCloudClient &comCloudClient, UIChooserNode *pParentNode)
    : UITask(Type_CloudAcquireInstances)
    , m_comCloudClient(comCloudClient)
    , m_pParentNode(pParentNode)
{
}

QList<UICloudMachine> UITaskCloudAcquireInstances::result() const
{
    m_mutex.lock();
    const QList<UICloudMachine> resultList = m_result;
    m_mutex.unlock();
    return resultList;
}

CVirtualBoxErrorInfo UITaskCloudAcquireInstances::errorInfo()
{
    m_mutex.lock();
    CVirtualBoxErrorInfo comErrorInfo = m_comErrorInfo;
    m_mutex.unlock();
    return comErrorInfo;
}

void UITaskCloudAcquireInstances::run()
{
    m_mutex.lock();

    /* Gather VM names, ids and states.
     * Currently we are interested in Running and Stopped VMs only. */
    CStringArray comNames;
    CStringArray comIDs;
    const QVector<KCloudMachineState> cloudMachineStates  = QVector<KCloudMachineState>()
                                                         << KCloudMachineState_Running
                                                         << KCloudMachineState_Stopped;

    /* Ask for cloud instance list: */
    CProgress comProgress = m_comCloudClient.ListInstances(cloudMachineStates, comNames, comIDs);
    if (!m_comCloudClient.isOk())
    {
        /// @todo pack error info to m_comErrorInfo
    }
    else
    {
        /* Wait for "Acquire cloud instances" progress: */
        comProgress.WaitForCompletion(-1);
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
        {
            /// @todo pack error info to m_comErrorInfo
        }
        else
        {
            /* Fetch acquired objects to lists: */
            const QVector<QString> instanceIds = comIDs.GetValues();
            const QVector<QString> instanceNames = comNames.GetValues();
            for (int i = 0; i < instanceIds.size(); ++i)
                m_result << UICloudMachine(m_comCloudClient, instanceIds.at(i), instanceNames.at(i));
        }
    }

    m_mutex.unlock();
}
