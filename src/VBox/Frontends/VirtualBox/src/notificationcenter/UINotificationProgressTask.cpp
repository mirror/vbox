/* $Id$ */
/** @file
 * VBox Qt GUI - UINotificationProgressTask class implementation.
 */

/*
 * Copyright (C) 2021-2022 Oracle Corporation
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
#include "UIErrorString.h"
#include "UINotificationObject.h"
#include "UINotificationProgressTask.h"


UINotificationProgressTask::UINotificationProgressTask(UINotificationProgress *pParent)
    : UIProgressTask(pParent)
    , m_pParent(pParent)
{
}

QString UINotificationProgressTask::errorMessage() const
{
    return m_strErrorMessage;
}

CProgress UINotificationProgressTask::createProgress()
{
    /* Call to sub-class to create progress-wrapper: */
    COMResult comResult;
    CProgress comProgress = m_pParent->createProgress(comResult);
    if (!comResult.isOk())
    {
        m_strErrorMessage = UIErrorString::formatErrorInfo(comResult);
        return CProgress();
    }
    /* Return progress-wrapper: */
    return comProgress;
}

void UINotificationProgressTask::handleProgressFinished(CProgress &comProgress)
{
    /* Handle progress-wrapper errors: */
    if (comProgress.isNotNull() && !comProgress.GetCanceled() && (!comProgress.isOk() || comProgress.GetResultCode() != 0))
        m_strErrorMessage = UIErrorString::formatErrorInfo(comProgress);
}
