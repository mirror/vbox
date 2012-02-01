/* $Id$ */
/** @file
 * VBox Qt GUI - UINetworkCustomer class implementation.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes: */
#include <QNetworkRequest>

/* Local includes: */
#include "UINetworkCustomer.h"
#include "UINetworkManager.h"

/* Constructor: */
UINetworkCustomer::UINetworkCustomer(QObject *pParent)
    : QObject(pParent)
{
}

/* Network request wrapper: */
void UINetworkCustomer::createNetworkRequest(const QNetworkRequest &request, UINetworkRequestType type, const QString &strDescription)
{
    gNetworkManager->createNetworkRequest(request, type, strDescription, this);
}

/* Network request wrapper (set): */
void UINetworkCustomer::createNetworkRequest(const QList<QNetworkRequest> &requests, UINetworkRequestType type, const QString &strDescription)
{
    gNetworkManager->createNetworkRequest(requests, type, strDescription, this);
}

#if 0
/* Downloader creation notification wrapper: */
void UINetworkCustomer::notifyDownloaderCreated(UIDownloadType downloaderType)
{
    gNetworkManager->notifyDownloaderCreated(downloaderType);
}
#endif

