/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UISettingsDialog class declaration
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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

/* VBox includes: */
#include "QIMainDialog.h"
#include "QIWithRetranslateUI.h"
#include "UISettingsDialog.gen.h"
#include "UISettingsDefs.h"

/* Forward declarations: */
class UIPageValidator;
class QProgressBar;
class QStackedWidget;
class QTimer;
class UIWarningPane;
class VBoxSettingsSelector;
class UISettingsPage;

/* Using declarations: */
using namespace UISettingsDefs;

/* Base dialog class for both Global & VM settings which encapsulates most of their similar functionalities */
class UISettingsDialog : public QIWithRetranslateUI<QIMainDialog>, public Ui::UISettingsDialog
{
    Q_OBJECT;

public:

    /* Settings Dialog Constructor/Destructor: */
    UISettingsDialog(QWidget *pParent);
   ~UISettingsDialog();

    /* Execute API: */
    void execute();

protected slots:

    /* Category-change slot: */
    virtual void sltCategoryChanged(int cId);

    /* Mark dialog as loaded: */
    virtual void sltMarkLoaded();
    /* Mark dialog as saved: */
    virtual void sltMarkSaved();

    /* Handlers for process bar: */
    void sltHandleProcessStarted();
    void sltHandlePageProcessed();

protected:

    /* Save/load API: */
    virtual void loadData();
    virtual void saveData();

    /* UI translator: */
    virtual void retranslateUi();

    /* Dialog type: */
    SettingsDialogType dialogType() { return m_dialogType; }
    void setDialogType(SettingsDialogType settingsDialogType);
    /* Dialog title: */
    virtual QString title() const = 0;
    /* Dialog title extension: */
    virtual QString titleExtension() const;

    /* Add settings page: */
    void addItem(const QString &strBigIcon, const QString &strBigIconDisabled,
                 const QString &strSmallIcon, const QString &strSmallIconDisabled,
                 int cId, const QString &strLink,
                 UISettingsPage* pSettingsPage = 0, int iParentId = -1);

    /* Helpers: Validation stuff: */
    virtual void recorrelate(UISettingsPage *pSettingsPage) { Q_UNUSED(pSettingsPage); }
    void revalidate(UIPageValidator *pValidator);
    void revalidate();

    /* Protected variables: */
    VBoxSettingsSelector *m_pSelector;
    QStackedWidget *m_pStack;

private slots:

    /* Handlers: Validation stuff: */
    void sltHandleValidityChange(UIPageValidator *pValidator);
    void sltHandleWarningPaneHovered(UIPageValidator *pValidator);
    void sltHandleWarningPaneUnhovered(UIPageValidator *pValidator);

    /* Slot to update whats-this: */
    void sltUpdateWhatsThis(bool fGotFocus = false);

    /* Slot to handle reject: */
    void reject();

private:

    /* Event-handlers: */
    bool eventFilter(QObject *pObject, QEvent *pEvent);
    void showEvent(QShowEvent *pEvent);

    /* Helper: Validation stuff: */
    void assignValidator(UISettingsPage *pPage);

    /* Global Flags: */
    SettingsDialogType m_dialogType;
    bool m_fPolished;

    /* Loading/saving stuff: */
    bool m_fLoaded;
    bool m_fSaved;

    /* Status bar widget: */
    QStackedWidget *m_pStatusBar;

    /* Process bar widget: */
    QProgressBar *m_pProcessBar;

    /* Error & Warning stuff: */
    UIWarningPane *m_pWarningPane;
    bool m_fValid;
    bool m_fSilent;
    QString m_strWarningHint;

    /* Whats-This stuff: */
    QTimer *m_pWhatsThisTimer;
    QPointer<QWidget> m_pWhatsThisCandidate;

    QMap<int, int> m_pages;
#ifdef Q_WS_MAC
    QList<QSize> m_sizeList;
#endif /* Q_WS_MAC */
};

#endif // __UISettingsDialog_h__

