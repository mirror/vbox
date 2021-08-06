/* $Id$ */
/** @file
 * VBox Qt GUI - UINetworkCustomer class implementation.
 */

/*
 * Copyright (C) 2012-2021 Oracle Corporation
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
#include "UINetworkCustomer.h"
#include "UINetworkRequestManager.h"


UINetworkCustomer::UINetworkCustomer(QObject *pParent /* = 0 */, bool fForceCall /* = true */)
    : QObject(pParent)
    , m_fForceCall(fForceCall)
{
}

UINetworkCustomer::~UINetworkCustomer()
{
    emit sigBeingDestroyed(this);
}

void UINetworkCustomer::createNetworkRequest(UINetworkRequestType enmType,
                                             const QList<QUrl> urls,
                                             const QString &strTarget /* = QString() */,
                                             const UserDictionary requestHeaders /* = UserDictionary() */)
{
    gNetworkManager->createNetworkRequest(enmType, urls, strTarget, requestHeaders, this);
}
