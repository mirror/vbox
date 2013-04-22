/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIPopupPane class declaration
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

#ifndef __UIPopupPane_h__
#define __UIPopupPane_h__

/* Qt includes: */
#include <QWidget>
#include <QMap>

/* GUI includes: */
#include "QIMessageBox.h"

/* Forward declaration: */
class QStateMachine;
class QLabel;
class QPushButton;
class UIPopupPane;
class UIPopupPaneFrame;
class UIPopupPaneTextPane;
class UIPopupPaneButtonPane;

/* UIAnimationFramework namespace: */
namespace UIAnimationFramework
{
    /* API: Animation stuff: */
    void installPropertyAnimation(QWidget *pParent, const QByteArray &strPropertyName,
                                  int iStartValue, int iFinalValue, int iAnimationDuration,
                                  const char *pSignalForward, const char *pSignalBackward);

    /* API: Animation stuff: */
    QStateMachine* installPropertyAnimation(QWidget *pTarget, const QByteArray &strPropertyName,
                                            const QByteArray &strValuePropertyNameStart, const QByteArray &strValuePropertyNameFinal,
                                            const char *pSignalForward, const char *pSignalBackward,
                                            bool fReversive = false, int iAnimationDuration = 300);
}

/* Popup-pane prototype class: */
class UIPopupPane : public QWidget
{
    Q_OBJECT;

signals:

    /* Notifiers: Animation stuff: */
    void sigHoverEnter();
    void sigHoverLeave();
    void sigFocusEnter();
    void sigFocusLeave();
    void sigSizeHintChanged();

    /* Notifier: Complete stuff: */
    void sigDone(int iButtonCode) const;

public:

    /* Constructor: */
    UIPopupPane(QWidget *pParent,
                const QString &strMessage, const QString &strDetails,
                const QMap<int, QString> &buttonDescriptions);

    /* API: Text stuff: */
    void setMessage(const QString &strMessage);
    void setDetails(const QString &strDetails);

    /* API: Layout stuff: */
    void setDesiredWidth(int iWidth);
    int minimumWidthHint() const;
    int minimumHeightHint() const;
    QSize minimumSizeHint() const;
    void layoutContent();

private slots:

    /* Handler: Button stuff: */
    void sltButtonClicked(int iButtonID);

private:

    /* Helpers: Prepare stuff: */
    void prepare();
    void prepareContent();

    /* Handler: Event-filter stuff: */
    bool eventFilter(QObject *pWatched, QEvent *pEvent);

    /* Helper: Complete stuff: */
    void done(int iButtonCode);

    /* Variables: */
    const QString m_strId;

    /* Variables: Layout stuff: */
    const int m_iMainLayoutMargin;
    const int m_iMainFrameLayoutMargin;
    const int m_iMainFrameLayoutSpacing;

    /* Variables: Text stuff: */
    QString m_strMessage, m_strDetails;

    /* Variables: Button stuff: */
    QMap<int, QString> m_buttonDescriptions;

    /* Variables: Animation stuff: */
    bool m_fHovered;
    bool m_fFocused;

    /* Widgets: */
    UIPopupPaneFrame *m_pMainFrame;
    UIPopupPaneTextPane *m_pTextPane;
    UIPopupPaneButtonPane *m_pButtonPane;
};

#endif /* __UIPopupPane_h__ */
