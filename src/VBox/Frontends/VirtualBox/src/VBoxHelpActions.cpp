/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxHelpActions class implementation
 */

/*
 * Copyright (C) 2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */
#include "VBoxHelpActions.h"
#include "UIExtraDataEventHandler.h"
#include "UIIconPool.h"
#include "UISelectorShortcuts.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"

/* Qt includes */
#include <QMenu>
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

void VBoxHelpActions::setup (QObject *aParent)
{
    AssertReturnVoid (contentsAction == NULL);

    contentsAction = new QAction (aParent);
    contentsAction->setIcon(UIIconPool::defaultIcon(UIIconPool::DialogHelpIcon));

    webAction = new QAction (aParent);
    webAction->setIcon (UIIconPool::iconSet (":/site_16px.png"));

    resetMessagesAction = new QAction (aParent);
    resetMessagesAction->setIcon (UIIconPool::iconSet (":/reset_16px.png"));

    registerAction = new QAction (aParent);
    registerAction->setIcon (UIIconPool::iconSet (":/register_16px.png",
                                                  ":/register_disabled_16px.png"));
    updateAction = new QAction (aParent);
    updateAction->setMenuRole(QAction::ApplicationSpecificRole);
    updateAction->setIcon (UIIconPool::iconSet (":/refresh_16px.png",
                                                ":/refresh_disabled_16px.png"));
    aboutAction = new QAction (aParent);
    aboutAction->setMenuRole (QAction::AboutRole);
    aboutAction->setIcon (UIIconPool::iconSet (":/about_16px.png"));

    QObject::connect (contentsAction, SIGNAL (triggered()),
                      &msgCenter(), SLOT (sltShowHelpHelpDialog()));
    QObject::connect (webAction, SIGNAL (triggered()),
                      &msgCenter(), SLOT (sltShowHelpWebDialog()));
    QObject::connect (resetMessagesAction, SIGNAL (triggered()),
                      &msgCenter(), SLOT (sltResetSuppressedMessages()));
    QObject::connect (registerAction, SIGNAL (triggered()),
                      &vboxGlobal(), SLOT (showRegistrationDialog()));
    QObject::connect (updateAction, SIGNAL (triggered()),
                      &vboxGlobal(), SLOT (showUpdateDialog()));
    QObject::connect (aboutAction, SIGNAL (triggered()),
                      &msgCenter(), SLOT (sltShowHelpAboutDialog()));

    QObject::connect (gEDataEvents, SIGNAL(sigCanShowRegistrationDlg(bool)),
                      registerAction, SLOT(setEnabled(bool)));
    QObject::connect (gEDataEvents, SIGNAL(sigCanShowUpdateDlg(bool)),
                      updateAction, SLOT(setEnabled(bool)));
}

void VBoxHelpActions::addTo (QMenu *aMenu)
{
    AssertReturnVoid (contentsAction != NULL);

    aMenu->addAction (contentsAction);
    aMenu->addAction (webAction);
    aMenu->addSeparator();

    aMenu->addAction (resetMessagesAction);
    aMenu->addSeparator();

#ifdef VBOX_WITH_REGISTRATION
    aMenu->addAction (registerAction);
    registerAction->setEnabled (vboxGlobal().virtualBox().
        GetExtraData (VBoxDefs::GUI_RegistrationDlgWinID).isEmpty());
#endif

    aMenu->addAction (updateAction);
    updateAction->setEnabled (vboxGlobal().virtualBox().
        GetExtraData (VBoxDefs::GUI_UpdateDlgWinID).isEmpty());

#ifndef Q_WS_MAC
    aMenu->addSeparator();
#endif /* Q_WS_MAC */
    aMenu->addAction (aboutAction);
}

void VBoxHelpActions::retranslateUi()
{
    AssertReturnVoid (contentsAction != NULL);

    contentsAction->setText (UIMessageCenter::tr ("&Contents..."));
    contentsAction->setShortcut (gSS->keySequence(UISelectorShortcuts::HelpShortcut));
    contentsAction->setStatusTip (UIMessageCenter::tr (
        "Show the online help contents"));

    webAction->setText (UIMessageCenter::tr ("&VirtualBox Web Site..."));
    webAction->setShortcut (gSS->keySequence(UISelectorShortcuts::WebShortcut));
    webAction->setStatusTip (UIMessageCenter::tr (
        "Open the browser and go to the VirtualBox product web site"));

    resetMessagesAction->setText (UIMessageCenter::tr ("&Reset All Warnings"));
    resetMessagesAction->setShortcut (gSS->keySequence(UISelectorShortcuts::ResetWarningsShortcut));
    resetMessagesAction->setStatusTip (UIMessageCenter::tr (
        "Go back to showing all suppressed warnings and messages"));

#ifdef VBOX_WITH_REGISTRATION
    registerAction->setText (UIMessageCenter::tr ("R&egister VirtualBox..."));
    registerAction->setShortcut (gSS->keySequence(UISelectorShortcuts::RegisterShortcut));
    registerAction->setStatusTip (UIMessageCenter::tr (
        "Open VirtualBox registration form"));
#endif /* VBOX_WITH_REGISTRATION */

    updateAction->setText (UIMessageCenter::tr ("C&heck for Updates..."));
    updateAction->setShortcut (gSS->keySequence(UISelectorShortcuts::UpdateShortcut));
    updateAction->setStatusTip (UIMessageCenter::tr (
        "Check for a new VirtualBox version"));

    aboutAction->setText (UIMessageCenter::tr ("&About VirtualBox..."));
    aboutAction->setShortcut (gSS->keySequence(UISelectorShortcuts::AboutShortcut));
    aboutAction->setStatusTip (UIMessageCenter::tr (
        "Show a dialog with product information"));
}

