/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxHelpActions class implementation
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "VBoxHelpActions.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"

/* Qt includes */
#include <QMenu>

void VBoxHelpActions::setup (QObject *aParent)
{
    AssertReturnVoid (contentsAction == NULL);

    contentsAction = new QAction (aParent);
    contentsAction->setIcon (VBoxGlobal::iconSet (":/help_16px.png"));

    webAction = new QAction (aParent);
    webAction->setIcon (VBoxGlobal::iconSet (":/site_16px.png"));

    resetMessagesAction = new QAction (aParent);
    resetMessagesAction->setIcon (VBoxGlobal::iconSet (":/reset_16px.png"));

    registerAction = new QAction (aParent);
    registerAction->setIcon (VBoxGlobal::iconSet (":/register_16px.png",
                                                  ":/register_disabled_16px.png"));
    updateAction = new QAction (aParent);
    updateAction->setIcon (VBoxGlobal::iconSet (":/refresh_16px.png",
                                                ":/refresh_disabled_16px.png"));
    aboutAction = new QAction (aParent);
    aboutAction->setMenuRole (QAction::AboutRole);
    aboutAction->setIcon (VBoxGlobal::iconSet (":/about_16px.png"));

    QObject::connect (contentsAction, SIGNAL (triggered()),
                      &vboxProblem(), SLOT (showHelpHelpDialog()));
    QObject::connect (webAction, SIGNAL (triggered()),
                      &vboxProblem(), SLOT (showHelpWebDialog()));
    QObject::connect (resetMessagesAction, SIGNAL (triggered()),
                      &vboxProblem(), SLOT (resetSuppressedMessages()));
    QObject::connect (registerAction, SIGNAL (triggered()),
                      &vboxGlobal(), SLOT (showRegistrationDialog()));
    QObject::connect (updateAction, SIGNAL (triggered()),
                      &vboxGlobal(), SLOT (showUpdateDialog()));
    QObject::connect (aboutAction, SIGNAL (triggered()),
                      &vboxProblem(), SLOT (showHelpAboutDialog()));

    QObject::connect (&vboxGlobal(), SIGNAL (canShowRegDlg (bool)),
                      registerAction, SLOT (setEnabled (bool)));
    QObject::connect (&vboxGlobal(), SIGNAL (canShowUpdDlg (bool)),
                      updateAction, SLOT (setEnabled (bool)));
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

    contentsAction->setText (VBoxProblemReporter::tr ("&Contents..."));
    contentsAction->setShortcut (QKeySequence::HelpContents);
    contentsAction->setStatusTip (VBoxProblemReporter::tr (
        "Show the online help contents"));

    webAction->setText (VBoxProblemReporter::tr ("&VirtualBox Web Site..."));
    webAction->setStatusTip (VBoxProblemReporter::tr (
        "Open the browser and go to the VirtualBox product web site"));

    resetMessagesAction->setText (VBoxProblemReporter::tr ("&Reset All Warnings"));
    resetMessagesAction->setStatusTip (VBoxProblemReporter::tr (
        "Cause all suppressed warnings and messages to be shown again"));

    registerAction->setText (VBoxProblemReporter::tr ("R&egister VirtualBox..."));
    registerAction->setStatusTip (VBoxProblemReporter::tr (
        "Open VirtualBox registration form"));

    updateAction->setText (VBoxProblemReporter::tr ("C&heck for Updates..."));
    updateAction->setStatusTip (VBoxProblemReporter::tr (
        "Check for a new VirtualBox version"));

    aboutAction->setText (VBoxProblemReporter::tr ("&About VirtualBox..."));
    aboutAction->setStatusTip (VBoxProblemReporter::tr (
        "Show a dialog with product information"));
}

