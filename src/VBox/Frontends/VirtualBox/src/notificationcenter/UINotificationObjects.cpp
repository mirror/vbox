/* $Id$ */
/** @file
 * VBox Qt GUI - Various UINotificationObjects implementations.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
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
#include "UINotificationObjects.h"


UINotificationProgressMediumMove::UINotificationProgressMediumMove(const CMedium &comMedium,
                                                                   const QString &strFrom,
                                                                   const QString &strTo)
    : m_comMedium(comMedium)
    , m_strFrom(strFrom)
    , m_strTo(strTo)
{
}

QString UINotificationProgressMediumMove::name() const
{
    return UINotificationProgress::tr("Moving medium ...");
}

QString UINotificationProgressMediumMove::details() const
{
    return UINotificationProgress::tr("<b>From</b>: %1<br><b>To</b>: %2").arg(m_strFrom, m_strTo);
}

CProgress UINotificationProgressMediumMove::createProgress(COMResult &comResult)
{
    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comMedium.MoveTo(m_strTo);
    /* Store COM result: */
    comResult = m_comMedium;
    /* Return progress-wrapper: */
    return comProgress;
}
