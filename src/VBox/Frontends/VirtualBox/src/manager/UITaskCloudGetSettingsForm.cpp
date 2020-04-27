/* $Id$ */
/** @file
 * VBox Qt GUI - UITaskCloudGetSettingsForm class implementation.
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
#include "UICommon.h"
#include "UICloudNetworkingStuff.h"
#include "UITaskCloudGetSettingsForm.h"
#include "UIThreadPool.h"


/*********************************************************************************************************************************
*   Class UITaskCloudGetSettingsForm implementation.                                                                             *
*********************************************************************************************************************************/

UITaskCloudGetSettingsForm::UITaskCloudGetSettingsForm(const CCloudMachine &comCloudMachine)
    : UITask(Type_CloudGetSettingsForm)
    , m_comCloudMachine(comCloudMachine)
{
}

CForm UITaskCloudGetSettingsForm::result() const
{
    m_mutex.lock();
    const CForm comResult = m_comResult;
    m_mutex.unlock();
    return comResult;
}

QString UITaskCloudGetSettingsForm::errorInfo() const
{
    m_mutex.lock();
    QString strErrorInfo = m_strErrorInfo;
    m_mutex.unlock();
    return strErrorInfo;
}

void UITaskCloudGetSettingsForm::run()
{
    m_mutex.lock();
    cloudMachineSettingsForm(m_comCloudMachine, m_comResult, m_strErrorInfo);
    m_mutex.unlock();
}


/*********************************************************************************************************************************
*   Class UIReceiverCloudGetSettingsForm implementation.                                                                         *
*********************************************************************************************************************************/

UIReceiverCloudGetSettingsForm::UIReceiverCloudGetSettingsForm(QObject *pParent)
    : QObject(pParent)
{
    /* Connect receiver: */
    connect(uiCommon().threadPoolCloud(), &UIThreadPool::sigTaskComplete,
            this, &UIReceiverCloudGetSettingsForm::sltHandleTaskComplete);
}

void UIReceiverCloudGetSettingsForm::sltHandleTaskComplete(UITask *pTask)
{
    /* Skip unrelated tasks: */
    if (!pTask || pTask->type() != UITask::Type_CloudGetSettingsForm)
        return;

    /* Cast task to corresponding sub-class: */
    UITaskCloudGetSettingsForm *pSettingsTask = static_cast<UITaskCloudGetSettingsForm*>(pTask);

    /* Redirect to another listeners: */
    emit sigTaskComplete(pSettingsTask->result());
}
