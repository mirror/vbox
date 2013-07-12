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

/* Popup integration types: */
enum UIPopupIntegrationType
{
    UIPopupIntegrationType_Embedded,
    UIPopupIntegrationType_Toplevel
};

/* Popup-center singleton: */
class UIPopupCenter: public QObject
{
    Q_OBJECT;

signals:

    /* Notifier: Popup-pane stuff: */
    void sigPopupPaneDone(QString strPopupPaneID, int iResultCode);

public:

    /* Static API: Create/destroy stuff: */
    static void create();
    static void destroy();

    /* API: Stack layout stuff: */
    void setStackIntegrationType(UIPopupIntegrationType type);

    /* API: Main message function.
     * Provides one-two buttons.
     * Used directly only in exceptional cases: */
    void message(QWidget *pParent, const QString &strPopupPaneID,
                 const QString &strMessage, const QString &strDetails,
                 int iButton1 = 0, int iButton2 = 0,
                 const QString &strButtonText1 = QString(),
                 const QString &strButtonText2 = QString(),
                 bool fProposeAutoConfirmation = false);

    /* API: Wrapper to 'message' function.
     * Provides single OK button: */
    void error(QWidget *pParent, const QString &strPopupPaneID,
               const QString &strMessage, const QString &strDetails,
               bool fProposeAutoConfirmation = false);

    /* API: Wrapper to 'error' function.
     * Omits details: */
    void alert(QWidget *pParent, const QString &strPopupPaneID,
               const QString &strMessage,
               bool fProposeAutoConfirmation = false);

    /* API: Wrapper to 'message' function.
     * Omits details, provides up to two buttons.
     * Used directly only in exceptional cases: */
    void question(QWidget *pParent, const QString &strPopupPaneID,
                  const QString &strMessage,
                  int iButton1 = 0, int iButton2 = 0,
                  const QString &strButtonText1 = QString(),
                  const QString &strButtonText2 = QString(),
                  bool fProposeAutoConfirmation = false);

    /* API: Wrapper to 'question' function,
     * Question providing two buttons (OK and Cancel by default): */
    void questionBinary(QWidget *pParent, const QString &strPopupPaneID,
                        const QString &strMessage,
                        const QString &strOkButtonText = QString(),
                        const QString &strCancelButtonText = QString(),
                        bool fProposeAutoConfirmation = false);

    /* API: Runtime UI stuff: */
    void cannotSendACPIToMachine(QWidget *pParent);
    void remindAboutAutoCapture(QWidget *pParent);
    void remindAboutMouseIntegration(QWidget *pParent, bool fSupportsAbsolute);
    void remindAboutPausedVMInput(QWidget *pParent);
    void remindAboutGuestAdditionsAreNotActive(QWidget *pParent);
    void remindAboutWrongColorDepth(QWidget *pParent, ulong uRealBPP, ulong uWantedBPP);

private slots:

    /* Handler: Popup-pane stuff: */
    void sltPopupPaneDone(QString strPopupPaneID, int iResultCode);

    /* Handlers: Popup-stack stuff: */
    void sltShowPopupStack();
    void sltHidePopupStack();
    void sltRaisePopupStack();
    void sltRemovePopupStack();

private:

    /* Constructor/destructor: */
    UIPopupCenter();
    ~UIPopupCenter();

    /* Helpers: Prepare/cleanup stuff: */
    void prepare();
    void cleanup();

    /* Helper: Popup-pane stuff: */
    void showPopupPane(QWidget *pParent, const QString &strPopupPaneID,
                       const QString &strMessage, const QString &strDetails,
                       int iButton1, int iButton2,
                       QString strButtonText1, QString strButtonText2,
                       bool fProposeAutoConfirmation);

    /* Helpers: Popup-stack stuff: */
    void showPopupStack(QWidget *pParent);
    void hidePopupStack(QWidget *pParent);
    void raisePopupStack(QWidget *pParent);
    void assignPopupStackParent(UIPopupStack *pPopupStack, QWidget *pParent);
    void unassignPopupStackParent(UIPopupStack *pPopupStack, QWidget *pParent);

    /* Static helper: Popup-stack stuff: */
    static QString popupStackID(QWidget *pParent);

    /* Variable: Popup-stack stuff: */
    UIPopupIntegrationType m_type;
    QMap<QString, QPointer<UIPopupStack> > m_stacks;

    /* Instance stuff: */
    static UIPopupCenter* m_spInstance;
    static UIPopupCenter* instance();
    friend UIPopupCenter& popupCenter();
};

/* Shortcut to the static UIPopupCenter::instance() method: */
inline UIPopupCenter& popupCenter() { return *UIPopupCenter::instance(); }

#endif /* __UIPopupCenter_h__ */
