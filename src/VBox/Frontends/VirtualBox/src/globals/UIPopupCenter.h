/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIPopupCenter class declaration
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

#ifndef __UIPopupCenter_h__
#define __UIPopupCenter_h__

/* Qt includes: */
#include <QMap>
#include <QObject>
#include <QWidget>
#include <QPointer>

/* GUI includes: */
#include "QIMessageBox.h"

/* Forward declaration: */
class UIPopupPane;
class QVBoxLayout;
class QPushButton;
class QIRichTextLabel;
class QIDialogButtonBox;

/* Global popup-center object: */
class UIPopupCenter: public QObject
{
    Q_OBJECT;

signals:

    /* Notifier: Popup complete stuff: */
    void sigPopupDone(QString strId, int iButtonCode) const;

public:

    /* Static API: Create/destroy stuff: */
    static void create();
    static void destroy();

    /* API: Main message function, used directly only in exceptional cases: */
    void message(QWidget *pParent, const QString &strId,
                 const QString &strMessage, const QString &strDetails,
                 int iButton1 = 0, int iButton2 = 0, int iButton3 = 0,
                 const QString &strButtonText1 = QString(),
                 const QString &strButtonText2 = QString(),
                 const QString &strButtonText3 = QString()) const;

    /* API: Wrapper to 'message' function.
     * Provides single OK button: */
    void error(QWidget *pParent, const QString &strId,
               const QString &strMessage, const QString &strDetails) const;

    /* API: Wrapper to 'error' function.
     * Omits details: */
    void alert(QWidget *pParent, const QString &strId,
               const QString &strMessage) const;

    /* API: Wrapper to 'message' function.
     * Omits details, provides two or three buttons: */
    void question(QWidget *pParent, const QString &strId,
                  const QString &strMessage,
                  int iButton1 = 0, int iButton2 = 0, int iButton3 = 0,
                  const QString &strButtonText1 = QString(),
                  const QString &strButtonText2 = QString(),
                  const QString &strButtonText3 = QString()) const;

    /* API: Wrapper to 'question' function,
     * Question providing two buttons (OK and Cancel by default): */
    void questionBinary(QWidget *pParent, const QString &strId,
                        const QString &strMessage,
                        const QString &strOkButtonText = QString(),
                        const QString &strCancelButtonText = QString()) const;

    /* API: Wrapper to 'question' function,
     * Question providing three buttons (Yes, No and Cancel by default): */
    void questionTrinary(QWidget *pParent, const QString &strId,
                         const QString &strMessage,
                         const QString &strChoice1ButtonText = QString(),
                         const QString &strChoice2ButtonText = QString(),
                         const QString &strCancelButtonText = QString()) const;

    /* API: Runtime UI stuff: */
    void remindAboutMouseIntegration(bool fSupportsAbsolute) const;

private slots:

    /* Handler: Popup done stuff: */
    void sltPopupDone(int iButtonCode) const;

private:

    /* Constructor/destructor: */
    UIPopupCenter();
    ~UIPopupCenter();

    /* Helper: Popup-box stuff: */
    void showPopupBox(QWidget *pParent, const QString &strId,
                      const QString &strMessage, const QString &strDetails,
                      int iButton1, int iButton2, int iButton3,
                      const QString &strButtonText1, const QString &strButtonText2, const QString &strButtonText3) const;

    /* Variables: */
    mutable QMap<QString, QPointer<UIPopupPane> > m_popups;

    /* Instance stuff: */
    static UIPopupCenter* m_spInstance;
    static UIPopupCenter* instance();
    friend UIPopupCenter& popupCenter();
};

/* Shortcut to the static UIPopupCenter::instance() method: */
inline UIPopupCenter& popupCenter() { return *UIPopupCenter::instance(); }

/* Popup-pane prototype class: */
class UIPopupPane : public QWidget
{
    Q_OBJECT;

signals:

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

private slots:

    /* Handlers: Done slot variants for every button: */
    void done1() { done(m_iButton1 & AlertButtonMask); }
    void done2() { done(m_iButton2 & AlertButtonMask); }
    void done3() { done(m_iButton3 & AlertButtonMask); }

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

    /* Helper: Adjust stuff: */
    void adjustAccordingParent();

    /* Helpers: Prepare stuff: */
    void prepareContent();
    QPushButton* createButton(int iButton);

    /* Helper: Complete stuff: */
    void done(int iButtonCode);

    /* Helper: Parent stuff: */
    static int parentStatusBarHeight(QWidget *pParent);

    /* Variables: */
    bool m_fPolished;
    const QString m_strId;
    QString m_strMessage, m_strDetails;
    int m_iButton1, m_iButton2, m_iButton3;
    QString m_strButtonText1, m_strButtonText2, m_strButtonText3;
    int m_iButtonEsc;
    const int m_iParentStatusBarHeight;

    /* Widgets: */
    QVBoxLayout *m_pMainLayout;
    QVBoxLayout *m_pFrameLayout;
    QIRichTextLabel *m_pTextPane;
    QIDialogButtonBox *m_pButtonBox;
    QPushButton *m_pButton1, *m_pButton2, *m_pButton3;
};

/* Popup-pane frame prototype class: */
class UIPopupPaneFrame : public QWidget
{
    Q_OBJECT;
    Q_PROPERTY(int opacity READ opacity WRITE setOpacity);

signals:

    /* Notifiers: Hover-machine stuff: */
    void sigHoverEnter();
    void sigHoverLeave();

public:

    /* Constructor/destructor: */
    UIPopupPaneFrame(QWidget *pParent = 0);
    ~UIPopupPaneFrame();

private:

    /* Helpers: Prepare/cleanup stuff: */
    void prepare();
    void cleanup();

    /* Handlers: Event stuff: */
    bool eventFilter(QObject *pWatched, QEvent *pEvent);
    void paintEvent(QPaintEvent *pEvent);

    /* Property: Hover-machine stuff: */
    int opacity() const { return m_iOpacity; }
    void setOpacity(int iOpacity) { m_iOpacity = iOpacity; update(); }

    /* Static helper: Animation stuff: */
    static void installPropertyAnimation(QWidget *pParent, const QByteArray &strPropertyName,
                                         int iStartValue, int iFinalValue, int iAnimationDuration,
                                         const char *pSignalForward, const char *pSignalBackward);

    /* Hover-machine stuff: */
    bool m_fHovered;
    int m_iDefaultOpacity;
    int m_iHoveredOpacity;
    int m_iOpacity;
    int m_iHoverAnimationDuration;
};

#endif /* __UIPopupCenter_h__ */
