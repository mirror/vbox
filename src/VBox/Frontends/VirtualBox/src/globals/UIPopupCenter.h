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
#include <QPointer>

/* Forward declaration: */
class QWidget;
class UIPopupStack;
class UIPopupPane;

/* Popup-center singleton: */
class UIPopupCenter: public QObject
{
    Q_OBJECT;

signals:

    /* Notifier: Popup-pane done stuff: */
    void sigPopupPaneDone(QString strID, int iButtonCode);

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
                 const QString &strButtonText3 = QString());

    /* API: Wrapper to 'message' function.
     * Provides single OK button: */
    void error(QWidget *pParent, const QString &strId,
               const QString &strMessage, const QString &strDetails);

    /* API: Wrapper to 'error' function.
     * Omits details: */
    void alert(QWidget *pParent, const QString &strId,
               const QString &strMessage);

    /* API: Wrapper to 'message' function.
     * Omits details, provides two or three buttons: */
    void question(QWidget *pParent, const QString &strId,
                  const QString &strMessage,
                  int iButton1 = 0, int iButton2 = 0, int iButton3 = 0,
                  const QString &strButtonText1 = QString(),
                  const QString &strButtonText2 = QString(),
                  const QString &strButtonText3 = QString());

    /* API: Wrapper to 'question' function,
     * Question providing two buttons (OK and Cancel by default): */
    void questionBinary(QWidget *pParent, const QString &strId,
                        const QString &strMessage,
                        const QString &strOkButtonText = QString(),
                        const QString &strCancelButtonText = QString());

    /* API: Wrapper to 'question' function,
     * Question providing three buttons (Yes, No and Cancel by default): */
    void questionTrinary(QWidget *pParent, const QString &strId,
                         const QString &strMessage,
                         const QString &strChoice1ButtonText = QString(),
                         const QString &strChoice2ButtonText = QString(),
                         const QString &strCancelButtonText = QString());

    /* API: Runtime UI stuff: */
    void remindAboutMouseIntegration(bool fSupportsAbsolute, QWidget *pParent);

private slots:

    /* Handler: Popup-stack stuff: */
    void sltRemovePopupStack();

private:

    /* Constructor/destructor: */
    UIPopupCenter();
    ~UIPopupCenter();

    /* Helper: Popup-pane stuff: */
    void showPopupPane(QWidget *pParent, const QString &strPopupPaneID,
                       const QString &strMessage, const QString &strDetails,
                       int iButton1, int iButton2, int iButton3,
                       const QString &strButtonText1, const QString &strButtonText2, const QString &strButtonText3);

    /* Variables: Popup-stack stuff: */
    QMap<QString, QPointer<UIPopupStack> > m_stacks;

    /* Instance stuff: */
    static UIPopupCenter* m_spInstance;
    static UIPopupCenter* instance();
    friend UIPopupCenter& popupCenter();
};

/* Shortcut to the static UIPopupCenter::instance() method: */
inline UIPopupCenter& popupCenter() { return *UIPopupCenter::instance(); }


/* Qt includes: */
#include <QWidget>

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

    /* Prepare stuff: */
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
    const int m_iStackLayoutMargin;
    const int m_iStackLayoutSpacing;
    QMap<QString, UIPopupPane*> m_panes;
    const int m_iParentStatusBarHeight;
};

#endif /* __UIPopupCenter_h__ */
