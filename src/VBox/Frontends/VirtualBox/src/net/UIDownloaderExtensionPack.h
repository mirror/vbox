/* $Id$ */
/** @file
 * VBox Qt GUI - UIDownloaderExtensionPack class declaration.
 */

/*
 * Copyright (C) 2011-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIDownloaderExtensionPack_h___
#define ___UIDownloaderExtensionPack_h___

/* GUI includes: */
#include "UIDownloader.h"

/* Forward declarations: */
class QByteArray;

/** UIDownloader extension for background extension-pack downloading. */
class UIDownloaderExtensionPack : public UIDownloader
{
    Q_OBJECT;

signals:

    /** Notifies listeners about downloading finished.
      * @param  strSource  Brings the downloading source.
      * @param  strTarget  Brings the downloading target.
      * @param  strHash    Brings the downloaded file hash. */
    void sigDownloadFinished(const QString &strSource, const QString &strTarget, const QString &strHash);

public:

    /** Creates downloader instance. */
    static UIDownloaderExtensionPack *create();
    /** Returns current downloader instance. */
    static UIDownloaderExtensionPack *current() { return s_pInstance; }

private:

    /** Constructs downloader. */
    UIDownloaderExtensionPack();
    /** Destructs downloader. */
    ~UIDownloaderExtensionPack();

    /** Returns description of the current network operation. */
    virtual const QString description() const /* override */;

    /** Asks user for downloading confirmation for passed @a pReply. */
    virtual bool askForDownloadingConfirmation(UINetworkReply *pReply) /* override */;
    /** Handles downloaded object for passed @a pReply. */
    virtual void handleDownloadedObject(UINetworkReply *pReply) /* override */;
    /** Handles verified object for passed @a pReply. */
    virtual void handleVerifiedObject(UINetworkReply *pReply) /* override */;

    /** Holds the static singleton instance. */
    static UIDownloaderExtensionPack *s_pInstance;

    /** Holds the cached received data awaiting for verification. */
    QByteArray m_receivedData;
};

#endif /* !___UIDownloaderExtensionPack_h___ */

