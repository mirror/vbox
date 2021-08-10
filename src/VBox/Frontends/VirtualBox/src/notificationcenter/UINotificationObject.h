/* $Id$ */
/** @file
 * VBox Qt GUI - UINotificationObject class declaration.
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

#ifndef FEQT_INCLUDED_SRC_notificationcenter_UINotificationObject_h
#define FEQT_INCLUDED_SRC_notificationcenter_UINotificationObject_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QObject>
#include <QPointer>

/* GUI includes: */
#include "UILibraryDefs.h"

/* COM includes: */
#include "CProgress.h"

/* Forward declarations: */
class UINotificationProgressTask;
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
class UIDownloader;
#endif

/** QObject-based notification-object. */
class SHARED_LIBRARY_STUFF UINotificationObject : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies model about closing. */
    void sigAboutToClose();

public:

    /** Constructs notification-object. */
    UINotificationObject();

    /** Returns object name. */
    virtual QString name() const = 0;
    /** Returns object details. */
    virtual QString details() const = 0;
    /** Handles notification-object being added. */
    virtual void handle() = 0;

public slots:

    /** Notifies model about closing. */
    virtual void close();
};

/** UINotificationObject extension for notification-progress. */
class SHARED_LIBRARY_STUFF UINotificationProgress : public UINotificationObject
{
    Q_OBJECT;

signals:

    /** Notifies listeners about progress started. */
    void sigProgressStarted();
    /** Notifies listeners about progress changed.
      * @param  uPercent  Brings new progress percentage value. */
    void sigProgressChange(ulong uPercent);
    /** Notifies listeners about progress finished. */
    void sigProgressFinished();

public:

    /** Constructs notification-progress. */
    UINotificationProgress();
    /** Destructs notification-progress. */
    virtual ~UINotificationProgress() /* override final */;

    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) = 0;

    /** Returns current progress percentage value. */
    ulong percent() const;
    /** Returns whether progress is cancelable. */
    bool isCancelable() const;
    /** Returns error-message if any. */
    QString error() const;

    /** Handles notification-object being added. */
    virtual void handle() /* override final */;

public slots:

    /** Stops the progress and notifies model about closing. */
    virtual void close() /* override final */;

private slots:

    /** Handles signal about progress changed.
      * @param  uPercent  Brings new progress percentage value. */
    void sltHandleProgressChange(ulong uPercent);
    /** Handles signal about progress finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the instance of progress-task being wrapped by this notification-progress. */
    UINotificationProgressTask *m_pTask;

    /** Holds the last cached progress percentage value. */
    ulong  m_uPercent;
};

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
/** UINotificationObject extension for notification-downloader. */
class SHARED_LIBRARY_STUFF UINotificationDownloader : public UINotificationObject
{
    Q_OBJECT;

signals:

    /** Notifies listeners about progress started. */
    void sigProgressStarted();
    /** Notifies listeners about progress changed.
      * @param  uPercent  Brings new progress percentage value. */
    void sigProgressChange(ulong uPercent);
    /** Notifies listeners about progress failed. */
    void sigProgressFailed();
    /** Notifies listeners about progress canceled. */
    void sigProgressCanceled();
    /** Notifies listeners about progress finished. */
    void sigProgressFinished();

    /** Notifies listeners about downloader destroyed. */
    void sigDownloaderDestroyed();

public:

    /** Constructs notification-downloader. */
    UINotificationDownloader();
    /** Destructs notification-downloader. */
    virtual ~UINotificationDownloader() /* override final */;

    /** Creates and returns started downloader-wrapper. */
    virtual UIDownloader *createDownloader() = 0;

    /** Returns current progress percentage value. */
    ulong percent() const;
    /** Returns error-message if any. */
    QString error() const;

    /** Handles notification-object being added. */
    virtual void handle() /* override final */;

public slots:

    /** Stops the downloader and notifies model about closing. */
    virtual void close() /* override final */;

private slots:

    /** Handles signal about progress changed.
      * @param  uPercent  Brings new progress percentage value. */
    void sltHandleProgressChange(ulong uPercent);
    /** Handles signal about progress failed.
      * @param  strError  Brings error message if any. */
    void sltHandleProgressFailed(const QString &strError);

private:

    /** Holds the instance of downloader being wrapped by this notification-downloader. */
    QPointer<UIDownloader>  m_pDownloader;

    /** Holds the last cached progress percentage value. */
    ulong    m_uPercent;
    /** Holds the error message is any. */
    QString  m_strError;
};
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

#endif /* !FEQT_INCLUDED_SRC_notificationcenter_UINotificationObject_h */
