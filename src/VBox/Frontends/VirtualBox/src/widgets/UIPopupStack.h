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
class UIPopupPane;

/* Popup-stack prototype class: */
class UIPopupStack : public QWidget
{
    Q_OBJECT;

signals:

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

    /* API: Parent stuff: */
    void setParent(QWidget *pParent);
    void setParent(QWidget *pParent, Qt::WindowFlags flags);

private slots:

    /* Handler: Layout stuff: */
    void sltAdjustGeometry();

    /* Handler: Popup-pane stuff: */
    void sltPopupPaneDone(int iButtonCode);

private:

    /* Helpers: Layout stuff: */
    int minimumWidthHint();
    int minimumHeightHint();
    QSize minimumSizeHint();
    void setDesiredWidth(int iWidth);
    void layoutContent();

    /* Handler: Event-filter stuff: */
    bool eventFilter(QObject *pWatched, QEvent *pEvent);

    /* Handler: Event stuff: */
    void showEvent(QShowEvent *pEvent);

    /* Static helpers: Prepare stuff: */
    static int parentStatusBarHeight(QWidget *pParent);

    /* Variables: */
    const int m_iLayoutMargin;
    const int m_iLayoutSpacing;
    int m_iParentStatusBarHeight;
    QMap<QString, UIPopupPane*> m_panes;
};

#endif /* __UIPopupStack_h__ */
