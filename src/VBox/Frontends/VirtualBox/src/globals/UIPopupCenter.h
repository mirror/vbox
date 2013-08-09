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

/* Popup-stack types: */
enum UIPopupStackType
{
    UIPopupStackType_Embedded,
    UIPopupStackType_Separate
};

/* Popup-stack orientations: */
enum UIPopupStackOrientation
{
    UIPopupStackOrientation_Top,
    UIPopupStackOrientation_Bottom
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

    /* API: Popup-stack stuff: */
    void showPopupStack(QWidget *pParent);
    void hidePopupStack(QWidget *pParent);
    void setPopupStackType(QWidget *pParent, UIPopupStackType newStackType);
    void setPopupStackOrientation(QWidget *pParent, UIPopupStackOrientation newStackOrientation);

    /* API: Main message function.
     * Provides up to two buttons.
     * Its interface, do NOT use it directly! */
    void message(QWidget *pParent, const QString &strPopupPaneID,
                 const QString &strMessage, const QString &strDetails,
                 const QString &strButtonText1 = QString(),
                 const QString &strButtonText2 = QString(),
                 bool fProposeAutoConfirmation = false);

    /* API: Wrapper to 'message' function.
     * Omits details, provides no buttons: */
    void popup(QWidget *pParent, const QString &strPopupPaneID,
               const QString &strMessage);

    /* API: Wrapper to 'message' function.
     * Omits details, provides one button: */
    void alert(QWidget *pParent, const QString &strPopupPaneID,
               const QString &strMessage,
               bool fProposeAutoConfirmation = false);

    /* API: Wrapper to 'message' function.
     * Omits details, provides up to two buttons: */
    void question(QWidget *pParent, const QString &strPopupPaneID,
                  const QString &strMessage,
                  const QString &strButtonText1 = QString(),
                  const QString &strButtonText2 = QString(),
                  bool fProposeAutoConfirmation = false);

    /* API: Recall function,
     * Close corresponding popup of passed parent: */
    void recall(QWidget *pParent, const QString &strPopupPaneID);

    /* API: Runtime UI stuff: */
    void cannotSendACPIToMachine(QWidget *pParent);
    void remindAboutAutoCapture(QWidget *pParent);
    void remindAboutMouseIntegration(QWidget *pParent, bool fSupportsAbsolute);
    void remindAboutPausedVMInput(QWidget *pParent);
    void forgetAboutPausedVMInput(QWidget *pParent);
    void remindAboutWrongColorDepth(QWidget *pParent, ulong uRealBPP, ulong uWantedBPP);
    void forgetAboutWrongColorDepth(QWidget *pParent);

private slots:

    /* Handler: Popup-pane stuff: */
    void sltPopupPaneDone(QString strPopupPaneID, int iResultCode);

    /* Handler: Popup-stack stuff: */
    void sltRemovePopupStack(QString strPopupStackID);

private:

    /* Constructor/destructor: */
    UIPopupCenter();
    ~UIPopupCenter();

    /* Helpers: Prepare/cleanup stuff: */
    void prepare();
    void cleanup();

    /* Helpers: Popup-pane stuff: */
    void showPopupPane(QWidget *pParent, const QString &strPopupPaneID,
                       const QString &strMessage, const QString &strDetails,
                       QString strButtonText1 = QString(), QString strButtonText2 = QString(),
                       bool fProposeAutoConfirmation = false);
    void hidePopupPane(QWidget *pParent, const QString &strPopupPaneID);

    /* Static helper: Popup-stack stuff: */
    static QString popupStackID(QWidget *pParent);
    static void assignPopupStackParent(UIPopupStack *pPopupStack, QWidget *pParent, UIPopupStackType stackType);
    static void unassignPopupStackParent(UIPopupStack *pPopupStack, QWidget *pParent);

    /* Variables: Popup-stack stuff: */
    QMap<QString, UIPopupStackType> m_stackTypes;
    QMap<QString, UIPopupStackOrientation> m_stackOrientations;
    QMap<QString, QPointer<UIPopupStack> > m_stacks;

    /* Instance stuff: */
    static UIPopupCenter* m_spInstance;
    static UIPopupCenter* instance();
    friend UIPopupCenter& popupCenter();
};

/* Shortcut to the static UIPopupCenter::instance() method: */
inline UIPopupCenter& popupCenter() { return *UIPopupCenter::instance(); }

#endif /* __UIPopupCenter_h__ */
