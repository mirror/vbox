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

/* Global popup-center object: */
class UIPopupCenter: public QObject
{
    Q_OBJECT;

public:

    /* Prepare/cleanup stuff: */
    static void prepare();
    static void cleanup();

    /* API: Main message function, used directly only in exceptional cases: */
    void message(QWidget *pParent,
                 const QString &strMessage, const QString &strDetails,
                 const char *pcszAutoConfirmId = 0,
                 int iButton1 = 0, int iButton2 = 0, int iButton3 = 0,
                 const QString &strButtonText1 = QString(),
                 const QString &strButtonText2 = QString(),
                 const QString &strButtonText3 = QString()) const;

    /* API: Wrapper to 'message' function.
     * Provides single OK button: */
    void error(QWidget *pParent,
               const QString &strMessage, const QString &strDetails,
               const char *pcszAutoConfirmId = 0) const;

    /* API: Wrapper to 'error' function.
     * Omits details: */
    void alert(QWidget *pParent,
               const QString &strMessage,
               const char *pcszAutoConfirmId = 0) const;

    /* API: Wrapper to 'message' function.
     * Omits details, provides two or three buttons: */
    void question(QWidget *pParent,
                  const QString &strMessage,
                  const char *pcszAutoConfirmId = 0,
                  int iButton1 = 0, int iButton2 = 0, int iButton3 = 0,
                  const QString &strButtonText1 = QString(),
                  const QString &strButtonText2 = QString(),
                  const QString &strButtonText3 = QString()) const;

    /* API: Wrapper to 'question' function,
     * Question providing two buttons (OK and Cancel by default): */
    void questionBinary(QWidget *pParent,
                        const QString &strMessage,
                        const char *pcszAutoConfirmId = 0,
                        const QString &strOkButtonText = QString(),
                        const QString &strCancelButtonText = QString()) const;

    /* API: Wrapper to 'question' function,
     * Question providing three buttons (Yes, No and Cancel by default): */
    void questionTrinary(QWidget *pParent,
                         const QString &strMessage,
                         const char *pcszAutoConfirmId = 0,
                         const QString &strChoice1ButtonText = QString(),
                         const QString &strChoice2ButtonText = QString(),
                         const QString &strCancelButtonText = QString()) const;

private:

    /* Constructor/destructor: */
    UIPopupCenter();
    ~UIPopupCenter();

    /* Instance stuff: */
    static UIPopupCenter* instance();
    friend UIPopupCenter& popupCenter();

    /* Helper: Popup-box stuff: */
    void showPopupBox(QWidget *pParent,
                      const QString &strMessage, const QString &strDetails,
                      int iButton1, int iButton2, int iButton3,
                      const QString &strButtonText1, const QString &strButtonText2, const QString &strButtonText3,
                      const QString &strAutoConfirmId) const;

    /* Variables: */
    static UIPopupCenter* m_spInstance;
    mutable QMap<QString, QPointer<QWidget> > m_popups;
};

/* Shortcut to the static UIPopupCenter::instance() method, for convenience: */
inline UIPopupCenter& popupCenter() { return *UIPopupCenter::instance(); }

#endif /* __UIPopupCenter_h__ */

