/* $Id$ */
/** @file
 * VBox Qt GUI - UIDownloaderGuestAdditions class declaration.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_networking_UIDownloaderGuestAdditions_h
#define FEQT_INCLUDED_SRC_networking_UIDownloaderGuestAdditions_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIDownloader.h"

/* Forward declarations: */
class QByteArray;

/** UIDownloader extension for background guest additions downloading. */
class SHARED_LIBRARY_STUFF UIDownloaderGuestAdditions : public UIDownloader
{
    Q_OBJECT;

signals:

    /** Notifies listeners about downloading finished.
      * @param  strFile  Brings the downloaded file-name. */
    void sigDownloadFinished(const QString &strFile);

public:

    /** Constructs downloader. */
    UIDownloaderGuestAdditions();

private:

    /** Returns description of the current network operation. */
    virtual QString description() const RT_OVERRIDE;

    /** Asks user for downloading confirmation for passed @a pReply. */
    virtual bool askForDownloadingConfirmation(UINetworkReply *pReply) RT_OVERRIDE;
    /** Handles downloaded object for passed @a pReply. */
    virtual void handleDownloadedObject(UINetworkReply *pReply) RT_OVERRIDE;
    /** Handles verified object for passed @a pReply. */
    virtual void handleVerifiedObject(UINetworkReply *pReply) RT_OVERRIDE;

    /** Holds the cached received data awaiting for verification. */
    QByteArray m_receivedData;
};

#endif /* !FEQT_INCLUDED_SRC_networking_UIDownloaderGuestAdditions_h */
