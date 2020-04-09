/* $Id$ */
/** @file
 * VBox Qt GUI - UITaskCloudGetInstanceInfo class implementation.
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
#include "UITaskCloudGetInstanceInfo.h"


UITaskCloudGetInstanceInfo::UITaskCloudGetInstanceInfo(const CCloudMachine &comCloudMachine)
    : UITask(Type_CloudGetInstanceState)
    , m_comCloudMachine(comCloudMachine)
{
}

QString UITaskCloudGetInstanceInfo::errorInfo()
{
    m_mutex.lock();
    const QString strErrorInfo = m_strErrorInfo;
    m_mutex.unlock();
    return strErrorInfo;
}

void UITaskCloudGetInstanceInfo::run()
{
    m_mutex.lock();
    refreshCloudMachineInfo(m_comCloudMachine, m_strErrorInfo);
    m_mutex.unlock();
}
