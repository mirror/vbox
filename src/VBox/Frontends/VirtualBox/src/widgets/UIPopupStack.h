/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIPopupStack class declaration
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIPopupStack_h__
#define __UIPopupStack_h__

/* Qt includes: */
#include <QWidget>
#include <QMap>

/* Forward declaration: */
class QVBoxLayout;
class QScrollArea;
class UIPopupPane;
class UIPopupStackViewport;

/* Popup-stack prototype class: */
class UIPopupStack : public QWidget
{
    Q_OBJECT;

signals:

    /* Notifier: Layout stuff: */
    void sigProposeStackViewportWidth(int iWidth);

    /* Notifier: Popup-pane stuff: */
    void sigPopupPaneDone(QString strPopupPaneID, int iResultCode);

    /* Notifier: Popup-stack stuff: */
    void sigRemove();

public:

    /* Constructor: */
    UIPopupStack();

    /* API: Popup-pane stuff: */
    bool exists(const QString &strPopupPaneID) const;
    void createPopupPane(const QString &strPopupPaneID,
                         const QString &strMessage, const QString &strDetails,
                         const QMap<int, QString> &buttonDescriptions,
                         bool fProposeAutoConfirmation);
    void updatePopupPane(const QString &strPopupPaneID,
                         const QString &strMessage, const QString &strDetails);
    void recallPopupPane(const QString &strPopupPaneID);

    /* API: Parent stuff: */
    void setParent(QWidget *pParent);
    void setParent(QWidget *pParent, Qt::WindowFlags flags);

private slots:

    /* Handler: Layout stuff: */
    void sltAdjustGeometry();

    /* Handler: Popuyp-pane stuff: */
    void sltPopupPaneRemoved(QString strPopupPaneID);

private:

    /* Helpers: Prepare stuff: */
    void prepare();
    void prepareContent();

    /* Handler: Event-filter stuff: */
    bool eventFilter(QObject *pWatched, QEvent *pEvent);

    /* Handler: Event stuff: */
    void showEvent(QShowEvent *pEvent);

    /* Helper: Layout stuff: */
    void propagateWidth();

    /* Static helpers: Prepare stuff: */
    static int parentMenuBarHeight(QWidget *pParent);
    static int parentStatusBarHeight(QWidget *pParent);

    /* Variables: Widget stuff: */
    QVBoxLayout *m_pMainLayout;
    QScrollArea *m_pScrollArea;
    UIPopupStackViewport *m_pScrollViewport;

    /* Variables: Layout stuff: */
    int m_iParentMenuBarHeight;
    int m_iParentStatusBarHeight;
};

/* Popup-stack viewport prototype class: */
class UIPopupStackViewport : public QWidget
{
    Q_OBJECT;

signals:

    /* Notifier: Layout stuff: */
    void sigProposePopupPaneWidth(int iWidth);
    void sigSizeHintChanged();

    /* Notifier: Popup-pane stuff: */
    void sigPopupPaneDone(QString strPopupPaneID, int iResultCode);
    void sigPopupPaneRemoved(QString strPopupPaneID);

    /* Notifier: Popup-stack stuff: */
    void sigRemove();

public:

    /* Constructor: */
    UIPopupStackViewport();

    /* API: Popup-pane stuff: */
    bool exists(const QString &strPopupPaneID) const;
    void createPopupPane(const QString &strPopupPaneID,
                         const QString &strMessage, const QString &strDetails,
                         const QMap<int, QString> &buttonDescriptions,
                         bool fProposeAutoConfirmation);
    void updatePopupPane(const QString &strPopupPaneID,
                         const QString &strMessage, const QString &strDetails);
    void recallPopupPane(const QString &strPopupPaneID);

    /* API: Layout stuff: */
    QSize minimumSizeHint() const { return m_minimumSizeHint; }

private slots:

    /* Handlers: Layout stuff: */
    void sltHandleProposalForWidth(int iWidth);
    void sltAdjustGeometry();

    /* Handler: Popup-pane stuff: */
    void sltPopupPaneDone(int iButtonCode);

private:

    /* Helpers: Layout stuff: */
    void updateSizeHint();
    void layoutContent();

    /* Variables: Layout stuff: */
    const int m_iLayoutMargin;
    const int m_iLayoutSpacing;
    QSize m_minimumSizeHint;

    /* Variables: Children stuff: */
    QMap<QString, UIPopupPane*> m_panes;
};

#endif /* __UIPopupStack_h__ */
