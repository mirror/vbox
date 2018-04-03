/* $Id$ */
/** @file
 * VBox Qt GUI - UIDownloaderAdditions class declaration.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIDownloaderAdditions_h___
#define ___UIDownloaderAdditions_h___

/* GUI includes: */
#include "UIDownloader.h"

/* Forward declarations: */
class QByteArray;

/** UIDownloader extension for background additions downloading. */
class SHARED_LIBRARY_STUFF UIDownloaderAdditions : public UIDownloader
{
    Q_OBJECT;

signals:

    /** Notifies listeners about downloading finished.
      * @param  strFile  Brings the downloaded file-name. */
    void sigDownloadFinished(const QString &strFile);

public:

    /** Creates downloader instance. */
    static UIDownloaderAdditions *create();
    /** Returns current downloader instance. */
    static UIDownloaderAdditions *current() { return s_pInstance; }

private:

    /** Constructs downloader. */
    UIDownloaderAdditions();
    /** Destructs downloader. */
    ~UIDownloaderAdditions();

    /** Returns description of the current network operation. */
    virtual const QString description() const /* override */;

    /** Asks user for downloading confirmation for passed @a pReply. */
    virtual bool askForDownloadingConfirmation(UINetworkReply *pReply) /* override */;
    /** Handles downloaded object for passed @a pReply. */
    virtual void handleDownloadedObject(UINetworkReply *pReply) /* override */;
    /** Handles verified object for passed @a pReply. */
    virtual void handleVerifiedObject(UINetworkReply *pReply) /* override */;

    /** Holds the static singleton instance. */
    static UIDownloaderAdditions *s_pInstance;

    /** Holds the cached received data awaiting for verification. */
    QByteArray m_receivedData;
};

#endif /* !___UIDownloaderAdditions_h___ */

