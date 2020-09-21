/* $Id$ */
/** @file
 * VBox Qt GUI - UITaskCloudRefreshMachineInfo class implementation.
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
#include "UIErrorString.h"
#include "UIProgressDialog.h"
#include "UITaskCloudRefreshMachineInfo.h"


UITaskCloudRefreshMachineInfo::UITaskCloudRefreshMachineInfo(const CCloudMachine &comCloudMachine)
    : UITask(Type_CloudRefreshMachineInfo)
    , m_comCloudMachine(comCloudMachine)
{
}

void UITaskCloudRefreshMachineInfo::cancel()
{
    m_comCloudMachineRefreshProgress.Cancel();
}

QString UITaskCloudRefreshMachineInfo::errorInfo() const
{
    m_mutex.lock();
    const QString strErrorInfo = m_strErrorInfo;
    m_mutex.unlock();
    return strErrorInfo;
}

void UITaskCloudRefreshMachineInfo::run()
{
    m_mutex.lock();

    /* Execute Refresh async method: */
    m_comCloudMachineRefreshProgress = m_comCloudMachine.Refresh();
    if (m_comCloudMachine.isOk())
    {
        /* Show "Refresh cloud machine information" progress: */
        QPointer<UIProgress> pObject = new UIProgress(m_comCloudMachineRefreshProgress, this);
        pObject->run();
        if (pObject)
            delete pObject;
        else
        {
            // Premature application shutdown,
            // exit immediately:
            return;
        }
        if (!m_comCloudMachineRefreshProgress.isOk() || m_comCloudMachineRefreshProgress.GetResultCode() != 0)
            m_strErrorInfo = UIErrorString::formatErrorInfo(m_comCloudMachineRefreshProgress);
    }
    else
        m_strErrorInfo = UIErrorString::formatErrorInfo(m_comCloudMachine);

    m_mutex.unlock();
}
