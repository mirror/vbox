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

    /* API: Main message function.
     * Provides up to two buttons.
     * Used directly only in exceptional cases: */
    void message(QWidget *pParent, const QString &strId,
                 const QString &strMessage, const QString &strDetails,
                 int iButton1 = 0, int iButton2 = 0,
                 const QString &strButtonText1 = QString(),
                 const QString &strButtonText2 = QString());

    /* API: Wrapper to 'message' function.
     * Provides single OK button: */
    void error(QWidget *pParent, const QString &strId,
               const QString &strMessage, const QString &strDetails);

    /* API: Wrapper to 'error' function.
     * Omits details: */
    void alert(QWidget *pParent, const QString &strId,
               const QString &strMessage);

    /* API: Wrapper to 'message' function.
     * Omits details, provides up to two buttons.
     * Used directly only in exceptional cases: */
    void question(QWidget *pParent, const QString &strId,
                  const QString &strMessage,
                  int iButton1 = 0, int iButton2 = 0,
                  const QString &strButtonText1 = QString(),
                  const QString &strButtonText2 = QString());

    /* API: Wrapper to 'question' function,
     * Question providing two buttons (OK and Cancel by default): */
    void questionBinary(QWidget *pParent, const QString &strId,
                        const QString &strMessage,
                        const QString &strOkButtonText = QString(),
                        const QString &strCancelButtonText = QString());

    /* API: Runtime UI stuff: */
    void remindAboutMouseIntegration(QWidget *pParent, bool fSupportsAbsolute);

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
                       int iButton1, int iButton2,
                       const QString &strButtonText1, const QString &strButtonText2);

    /* Variable: Popup-stack stuff: */
    QMap<QString, QPointer<UIPopupStack> > m_stacks;

    /* Instance stuff: */
    static UIPopupCenter* m_spInstance;
    static UIPopupCenter* instance();
    friend UIPopupCenter& popupCenter();
};

/* Shortcut to the static UIPopupCenter::instance() method: */
inline UIPopupCenter& popupCenter() { return *UIPopupCenter::instance(); }

#endif /* __UIPopupCenter_h__ */
