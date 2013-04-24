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
class UIPopupPaneTextPane;
class UIPopupPaneButtonPane;

/* UIAnimationFramework namespace: */
namespace UIAnimationFramework
{
    /* API: Animation stuff: */
    void installPropertyAnimation(QWidget *pParent, const QByteArray &strPropertyName,
                                  int iStartValue, int iFinalValue,
                                  const char *pSignalForward, const char *pSignalBackward,
                                  int iAnimationDuration = 300);

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
    Q_PROPERTY(int opacity READ opacity WRITE setOpacity);

signals:

    /* Notifiers: Hover stuff: */
    void sigHoverEnter();
    void sigHoverLeave();

    /* Notifiers: Focus stuff: */
    void sigFocusEnter();
    void sigFocusLeave();

    /* Notifier: Layout stuff: */
    void sigSizeHintChanged();

    /* Notifier: Complete stuff: */
    void sigDone(int iResultCode) const;

public:

    /* Constructor: */
    UIPopupPane(QWidget *pParent,
                const QString &strMessage, const QString &strDetails,
                const QMap<int, QString> &buttonDescriptions,
                bool fProposeAutoConfirmation);

    /* API: Text stuff: */
    void setMessage(const QString &strMessage);
    void setDetails(const QString &strDetails);

    /* API: Auto-confirmation stuff: */
    void setProposeAutoConfirmation(bool fPropose);

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

    /* Handler: Event stuff: */
    void paintEvent(QPaintEvent *pEvent);

    /* Helper: Complete stuff: */
    void done(int iResultCode);

    /* Property: Hover stuff: */
    int opacity() const { return m_iOpacity; }
    void setOpacity(int iOpacity) { m_iOpacity = iOpacity; update(); }

    /* Variables: General stuff: */
    const QString m_strId;
    const int m_iLayoutMargin;
    const int m_iLayoutSpacing;

    /* Variables: Text stuff: */
    QString m_strMessage, m_strDetails;

    /* Variables: Auto-confirmation stuff: */
    bool m_fProposeAutoConfirmation;

    /* Variables: Button stuff: */
    QMap<int, QString> m_buttonDescriptions;

    /* Variables: Hover stuff: */
    bool m_fHovered;
    const int m_iDefaultOpacity;
    const int m_iHoveredOpacity;
    int m_iOpacity;

    /* Variables: Focus stuff: */
    bool m_fFocused;

    /* Widgets: */
    UIPopupPaneTextPane *m_pTextPane;
    UIPopupPaneButtonPane *m_pButtonPane;
};

#endif /* __UIPopupPane_h__ */
