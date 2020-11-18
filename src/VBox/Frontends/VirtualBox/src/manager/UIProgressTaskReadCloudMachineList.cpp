/* $Id$ */
/** @file
 * VBox Qt GUI - UIProgressTaskReadCloudMachineList class implementation.
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
#include "UIProgressTaskReadCloudMachineList.h"


UIProgressTaskReadCloudMachineList::UIProgressTaskReadCloudMachineList(QObject *pParent,
                                                                       const UICloudEntityKey &guiCloudProfileKey,
                                                                       bool fWithRefresh)
    : UIProgressTask(pParent)
    , m_guiCloudProfileKey(guiCloudProfileKey)
    , m_fWithRefresh(fWithRefresh)
{
}

UICloudEntityKey UIProgressTaskReadCloudMachineList::cloudProfileKey() const
{
    return m_guiCloudProfileKey;
}

QVector<CCloudMachine> UIProgressTaskReadCloudMachineList::machines() const
{
    return m_machines;
}

QString UIProgressTaskReadCloudMachineList::errorMessage() const
{
    return m_strErrorMessage;
}

CProgress UIProgressTaskReadCloudMachineList::createProgress()
{
    /* Create cloud client: */
    m_comCloudClient = cloudClientByName(m_guiCloudProfileKey.m_strProviderShortName,
                                         m_guiCloudProfileKey.m_strProfileName,
                                         m_strErrorMessage);
    if (m_comCloudClient.isNull())
        return CProgress();

    /* Initialize progress-wrapper: */
    CProgress comProgress = m_fWithRefresh
                          ? m_comCloudClient.ReadCloudMachineList()
                          : m_comCloudClient.ReadCloudMachineStubList();
    if (!m_comCloudClient.isOk())
    {
        m_strErrorMessage = UIErrorString::formatErrorInfo(m_comCloudClient);
        return CProgress();
    }

    /* Return progress-wrapper: */
    return comProgress;
}

void UIProgressTaskReadCloudMachineList::handleProgressFinished(CProgress &comProgress)
{
    /* Handle progress-wrapper errors: */
    if (!comProgress.GetCanceled() && (!comProgress.isOk() || comProgress.GetResultCode() != 0))
    {
        m_strErrorMessage = UIErrorString::formatErrorInfo(comProgress);
        return;
    }

    /* Fill the result: */
    m_machines = m_fWithRefresh
               ? m_comCloudClient.GetCloudMachineList()
               : m_comCloudClient.GetCloudMachineStubList();
    if (!m_comCloudClient.isOk())
        m_strErrorMessage = UIErrorString::formatErrorInfo(m_comCloudClient);
}
