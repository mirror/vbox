/* $Id$ */
/** @file
 * VBox Qt GUI - UINotificationObjectItem class declaration.
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

#ifndef FEQT_INCLUDED_SRC_notificationcenter_UINotificationObjectItem_h
#define FEQT_INCLUDED_SRC_notificationcenter_UINotificationObjectItem_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* VBox includes: */
#include <iprt/cdefs.h>

/* Forward declarations: */
class QHBoxLayout;
class QLabel;
class QProgressBar;
class QVBoxLayout;
class QIRichTextLabel;
class QIToolButton;
class UINotificationObject;
class UINotificationProgress;
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
class UINotificationDownloader;
class UINotificationNewVersionChecker;
#endif

/** QWidget-based notification-object item. */
class UINotificationObjectItem : public QWidget
{
    Q_OBJECT;

public:

    /** Constructs notification-object item, passing @a pParent to the base-class.
      * @param  pObject  Brings the notification-object this item created for. */
    UINotificationObjectItem(QWidget *pParent, UINotificationObject *pObject = 0);

protected:

    /** Handles any Qt @a pEvent. */
    virtual bool event(QEvent *pEvent) RT_OVERRIDE;

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) RT_OVERRIDE;

    /** Holds the notification-object this item created for. */
    UINotificationObject *m_pObject;

    /** Holds the main layout instance. */
    QVBoxLayout     *m_pLayoutMain;
    /** Holds the upper layout instance. */
    QHBoxLayout     *m_pLayoutUpper;
    /** Holds the name label instance. */
    QLabel          *m_pLabelName;
    /** Holds the help button instance. */
    QIToolButton    *m_pButtonHelp;
    /** Holds the forget button instance. */
    QIToolButton    *m_pButtonForget;
    /** Holds the close button instance. */
    QIToolButton    *m_pButtonClose;
    /** Holds the details label instance. */
    QIRichTextLabel *m_pLabelDetails;

    /** Holds whether item is hovered. */
    bool  m_fHovered;
    /** Holds whether item is toggled. */
    bool  m_fToggled;
};

/** UINotificationObjectItem extension for notification-progress. */
class UINotificationProgressItem : public UINotificationObjectItem
{
    Q_OBJECT;

public:

    /** Constructs notification-progress item, passing @a pParent to the base-class.
      * @param  pProgress  Brings the notification-progress this item created for. */
    UINotificationProgressItem(QWidget *pParent, UINotificationProgress *pProgress = 0);

private slots:

    /** Handles signal about progress started. */
    void sltHandleProgressStarted();
    /** Handles signal about progress changed.
      * @param  uPercent  Brings new progress percentage value. */
    void sltHandleProgressChange(ulong uPercent);
    /** Handles signal about progress finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the notification-progress this item created for. */
    UINotificationProgress *progress() const;

    /** Updates details. */
    void updateDetails();

    /** Holds the progress-bar instance. */
    QProgressBar *m_pProgressBar;
};

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
/** UINotificationObjectItem extension for notification-downloader. */
class UINotificationDownloaderItem : public UINotificationObjectItem
{
    Q_OBJECT;

public:

    /** Constructs notification-downloader item, passing @a pParent to the base-class.
      * @param  pDownloader  Brings the notification-downloader this item created for. */
    UINotificationDownloaderItem(QWidget *pParent, UINotificationDownloader *pDownloader = 0);

private slots:

    /** Handles signal about progress started. */
    void sltHandleProgressStarted();
    /** Handles signal about progress changed.
      * @param  uPercent  Brings new progress percentage value. */
    void sltHandleProgressChange(ulong uPercent);
    /** Handles signal about progress finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the notification-downloader this item created for. */
    UINotificationDownloader *downloader() const;

    /** Updates details. */
    void updateDetails();

    /** Holds the progress-bar instance. */
    QProgressBar *m_pProgressBar;
};

/** UINotificationObjectItem extension for notification-new-version-checker. */
class UINotificationNewVersionCheckerItem : public UINotificationObjectItem
{
    Q_OBJECT;

public:

    /** Constructs notification-new-version-checker item, passing @a pParent to the base-class.
      * @param  pChecker  Brings the notification-new-version-checker this item created for. */
    UINotificationNewVersionCheckerItem(QWidget *pParent, UINotificationNewVersionChecker *pChecker = 0);

private slots:

    /** Handles signal about progress NOT finished. */
    void sltHandleProgressNotFinished();
    /** Handles signal about progress finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the notification-new-version-checker this item created for. */
    UINotificationNewVersionChecker *checker() const;

    /** Updates details. */
    void updateDetails();

    /** Holds the progress-bar instance. */
    QProgressBar *m_pProgressBar;
};
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

/** Notification-object factory. */
namespace UINotificationItem
{
    /** Creates notification-object of required type.
      * @param  pParent  Brings the parent constructed item being attached to.
      * @param  pObject  Brings the notification-object item being constructed for. */
    UINotificationObjectItem *create(QWidget *pParent, UINotificationObject *pObject);
}

#endif /* !FEQT_INCLUDED_SRC_notificationcenter_UINotificationObjectItem_h */
