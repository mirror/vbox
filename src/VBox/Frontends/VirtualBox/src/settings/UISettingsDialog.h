/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UISettingsDialog class declaration
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UISettingsDialog_h__
#define __UISettingsDialog_h__

/* Local includes */
#include "QIMainDialog.h"
#include "QIWithRetranslateUI.h"
#include "UISettingsDialog.gen.h"

/* Forward declarations */
class QIWidgetValidator;
class QStackedWidget;
class QTimer;
class VBoxWarningPane;
class VBoxSettingsSelector;
class UISettingsPage;

/* Base dialog class for both Global & VM settings which encapsulates most of their similar functionalities */
class UISettingsDialog : public QIWithRetranslateUI<QIMainDialog>, public Ui::UISettingsDialog
{
    Q_OBJECT;

public:

    /* Settings Dialog Constructor/Destructor: */
    UISettingsDialog(QWidget *pParent = 0);
   ~UISettingsDialog();

    /* Save/Load interface: */
    virtual void getFrom() = 0;
    virtual void putBackTo() = 0;

protected slots:

    /* Validation handler: */
    virtual void sltRevalidate(QIWidgetValidator *pValidator);

    /* Category-change slot: */
    virtual void sltCategoryChanged(int cId);

protected:

    /* UI translator: */
    virtual void retranslateUi();

    /* Dialog title: */
    virtual QString title() const = 0;
    /* Dialog title extension: */
    virtual QString titleExtension() const;

    /* Setters for error/warning: */
    void setError(const QString &strError);
    void setWarning(const QString &strWarning);

    /* Add settings page: */
    void addItem(const QString &strBigIcon, const QString &strBigIconDisabled,
                 const QString &strSmallIcon, const QString &strSmallIconDisabled,
                 int cId, const QString &strLink,
                 UISettingsPage* pSettingsPage = 0, int iParentId = -1);

    /* Correlation handler: */
    virtual bool recorrelate(QWidget *pPage, QString &strWarning);

    /* Protected variables: */
    VBoxSettingsSelector *m_pSelector;
    QStackedWidget *m_pStack;

private slots:

    /* Slot to handle validity-changes: */
    void sltHandleValidityChanged(const QIWidgetValidator *pValidator);

    /* Slot to update whats-this: */
    void sltUpdateWhatsThis(bool fGotFocus = false);

private:

    /* Event-handlers: */
    bool eventFilter(QObject *pObject, QEvent *pEvent);
    void showEvent(QShowEvent *pEvent);

    void assignValidator(UISettingsPage *pPage);

    /* Global Flags: */
    bool m_fPolished;

    /* Error & Warning stuff: */
    bool m_fValid;
    bool m_fSilent;
    QString m_strErrorHint;
    QString m_strWarningHint;
    QString m_strErrorString;
    QString m_strWarningString;
    QPixmap m_errorIcon;
    QPixmap m_warningIcon;
    VBoxWarningPane *m_pWarningPane;

    /* Whats-This stuff: */
    QTimer *m_pWhatsThisTimer;
    QPointer<QWidget> m_pWhatsThisCandidate;

#ifdef Q_WS_MAC
    QList<QSize> m_sizeList;
#endif /* Q_WS_MAC */
};

#endif // __UISettingsDialog_h__

