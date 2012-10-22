/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloaderAdditions class declaration
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIDownloaderAdditions_h__
#define __UIDownloaderAdditions_h__

/* Local includes: */
#include "UIDownloader.h"

/* UIDownloader extension for background additions downloading. */
class UIDownloaderAdditions : public UIDownloader
{
    Q_OBJECT;

signals:

    /* Notifies listeners about downloading finished: */
    void sigDownloadFinished(const QString &strFile);

public:

    /* Static stuff: */
    static UIDownloaderAdditions* create();
    static UIDownloaderAdditions* current();

private:

    /* Constructor/destructor: */
    UIDownloaderAdditions();
    ~UIDownloaderAdditions();

    /* Virtual stuff reimplementations: */
    bool askForDownloadingConfirmation(UINetworkReply *pReply);
    void handleDownloadedObject(UINetworkReply *pReply);

    /* Variables: */
    static UIDownloaderAdditions *m_spInstance;
};

#endif // __UIDownloaderAdditions_h__

