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
    void sigPopupPaneDone(QString strID, int iButtonCode);

    /* Notifier: Popup-stack stuff: */
    void sigRemove();

public:

    /* Constructor: */
    UIPopupStack(QWidget *pParent);

    /* API: Popup-pane stuff: */
    void updatePopupPane(const QString &strPopupPaneID,
                         const QString &strMessage, const QString &strDetails,
                         const QMap<int, QString> &buttonDescriptions);

private slots:

    /* Handler: Layout stuff: */
    void sltAdjustGeometry();

    /* Handler: Popup-pane stuff: */
    void sltPopupPaneDone(int iButtonCode);

private:

    /* Helper: Prepare stuff: */
    void prepare();

    /* Helpers: Layout stuff: */
    int minimumWidthHint();
    int minimumHeightHint();
    QSize minimumSizeHint();
    void setDesiredWidth(int iWidth);
    void layoutContent();

    /* Handler: Event-filter stuff: */
    bool eventFilter(QObject *pWatched, QEvent *pEvent);

    /* Handlers: Event stuff: */
    virtual void showEvent(QShowEvent *pEvent);
    virtual void polishEvent(QShowEvent *pEvent);

    /* Static helpers: Prepare stuff: */
    static int parentStatusBarHeight(QWidget *pParent);

    /* Variables: */
    bool m_fPolished;
    const int m_iLayoutMargin;
    const int m_iLayoutSpacing;
    const int m_iParentStatusBarHeight;
    QMap<QString, UIPopupPane*> m_panes;
};

#endif /* __UIPopupStack_h__ */
