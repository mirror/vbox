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

/* GUI includes: */
#include "QIMessageBox.h"

/* Forward declaration: */
class QLabel;
class QPushButton;
class QIDialogButtonBox;
class UIPopupPane;
class UIPopupPaneFrame;
class UIPopupPaneTextPane;

/* UIAnimationFramework namespace: */
namespace UIAnimationFramework
{
    /* API: Animation stuff: */
    void installPropertyAnimation(QWidget *pParent, const QByteArray &strPropertyName,
                                  int iStartValue, int iFinalValue, int iAnimationDuration,
                                  const char *pSignalForward, const char *pSignalBackward);
}

/* Popup-pane prototype class: */
class UIPopupPane : public QWidget
{
    Q_OBJECT;

signals:

    /* Notifiers: Hover stuff: */
    void sigHoverEnter();
    void sigHoverLeave();

    /* Notifier: Complete stuff: */
    void sigDone(int iButtonCode) const;

public:

    /* Constructor/destructor: */
    UIPopupPane(QWidget *pParent, const QString &strId,
                const QString &strMessage, const QString &strDetails,
                int iButton1, int iButton2, int iButton3,
                const QString &strButtonText1, const QString &strButtonText2, const QString &strButtonText3);
    ~UIPopupPane();

    /* API: Id stuff: */
    const QString& id() const { return m_strId; }

    /* API: Text stuff: */
    void setMessage(const QString &strMessage);
    void setDetails(const QString &strDetails);

private slots:

    /* Handlers: Done slot variants for every button: */
    void done1() { done(m_iButton1 & AlertButtonMask); }
    void done2() { done(m_iButton2 & AlertButtonMask); }
    void done3() { done(m_iButton3 & AlertButtonMask); }

    /* Handler: Layout stuff: */
    void sltAdjustGeomerty();

private:

    /* Helpers: Prepare/cleanup stuff: */
    void prepare();
    void cleanup();

    /* Handler: Event-filter stuff: */
    bool eventFilter(QObject *pWatched, QEvent *pEvent);

    /* Handlers: Event stuff: */
    virtual void showEvent(QShowEvent *pEvent);
    virtual void polishEvent(QShowEvent *pEvent);
    virtual void keyPressEvent(QKeyEvent *pEvent);

    /* Helper: Layout stuff: */
    int minimumWidthHint() const;
    int minimumHeightHint() const;
    QSize minimumSizeHint() const;
    void updateLayout();

    /* Helpers: Prepare stuff: */
    void prepareContent();
    void prepareButtons();

    /* Helper: Complete stuff: */
    void done(int iButtonCode);

    /* Static helpers: Prepare stuff: */
    static int parentStatusBarHeight(QWidget *pParent);
    static QList<QPushButton*> createButtons(QIDialogButtonBox *pButtonBox, const QList<int> description);
    static QPushButton* createButton(QIDialogButtonBox *pButtonBox, int iButton);

    /* Variables: */
    bool m_fPolished;
    const QString m_strId;

    /* Variables: Layout stuff: */
    const int m_iMainLayoutMargin;
    const int m_iMainFrameLayoutMargin;
    const int m_iMainFrameLayoutSpacing;
    const int m_iParentStatusBarHeight;

    /* Variables: Text stuff: */
    QString m_strMessage, m_strDetails;

    /* Variables: Button stuff: */
    int m_iButton1, m_iButton2, m_iButton3;
    QString m_strButtonText1, m_strButtonText2, m_strButtonText3;
    int m_iButtonEsc;

    /* Variables: Hover stuff: */
    bool m_fHovered;

    /* Widgets: */
    UIPopupPaneFrame *m_pMainFrame;
    UIPopupPaneTextPane *m_pTextPane;
    QIDialogButtonBox *m_pButtonBox;
    QPushButton *m_pButton1, *m_pButton2, *m_pButton3;
};

#endif /* __UIPopupPane_h__ */
