/* $Id$ */
/** @file
 * VBox Qt GUI - UISoftKeyboardDialog class implementation.
 */

/*
 * Copyright (C) 2010-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QPushButton>
#include <QVBoxLayout>

/* GUI includes: */
#include "UIDesktopWidgetWatchdog.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIGuestControlConsole.h"
#include "UISession.h"
#include "UISoftKeyboardDialog.h"
#include "UISoftKeyboard.h"
#include "VBoxGlobal.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils-darwin.h"
#endif


/*********************************************************************************************************************************
*   Class UISoftKeyboardDialog implementation.                                                                            *
*********************************************************************************************************************************/

UISoftKeyboardDialog::UISoftKeyboardDialog(QWidget *pParent,
                                           UISession *pSession,
                                           const QString &strMachineName /* = QString() */)
    : QMainWindow(pParent)
    , m_pSession(pSession)
    , m_strMachineName(strMachineName)
{
}

void UISoftKeyboardDialog::retranslateUi()
{
    /* Translate window title: */
    setWindowTitle(tr("%1 - Guest Control").arg(m_strMachineName));
    /* Translate buttons: */
    //button(ButtonType_Close)->setText(tr("Close"));
}

void UISoftKeyboardDialog::configure()
{
    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/vm_show_logs_32px.png", ":/vm_show_logs_16px.png"));
}

void UISoftKeyboardDialog::configureCentralWidget()
{
    /* Create widget: */
    //UISoftKeyboard  *pSoftKeyboard = new UISoftKeyboard(EmbedTo_Dialog, 0, m_pSession, "");

    //if (pSoftKeyboard)
    {
        /* Configure widget: */
        //setWidget(pSoftKeyboard);
        //setWidgetMenu(pWidget->menu());
#ifdef VBOX_WS_MAC
        //setWidgetToolbar(pWidget->toolbar());
#endif
        /* Add into layout: */
        //centralWidget()->layout()->addWidget(pSoftKeyboard);
    }
}

void UISoftKeyboardDialog::finalize()
{
    /* Apply language settings: */
    retranslateUi();
}

void UISoftKeyboardDialog::loadSettings()
{
    // const QRect desktopRect = gpDesktop->availableGeometry(this);
    // int iDefaultWidth = desktopRect.width() / 2;
    // int iDefaultHeight = 0.5 * iDefaultWidth;

    // QRect defaultGeometry(0, 0, iDefaultWidth, iDefaultHeight);
    // if (centerWidget())
    //     defaultGeometry.moveCenter(centerWidget()->geometry().center());

    // /* Load geometry from extradata: */
    // QRect geometry = gEDataManager->softKeyboardDialogGeometry(this, defaultGeometry);

    // /* Restore geometry: */
    // LogRel2(("GUI: UISoftKeyboardDialog: Restoring geometry to: Origin=%dx%d, Size=%dx%d\n",
    //          geometry.x(), geometry.y(), geometry.width(), geometry.height()));
    // setDialogGeometry(geometry);
}

void UISoftKeyboardDialog::saveSettings() const
{
//     /* Save window geometry to extradata: */
//     const QRect saveGeometry = geometry();
// #ifdef VBOX_WS_MAC
//     /* darwinIsWindowMaximized expects a non-const QWidget*. thus const_cast: */
//     QWidget *pw = const_cast<QWidget*>(qobject_cast<const QWidget*>(this));
//     gEDataManager->setSoftKeyboardDialogGeometry(saveGeometry, ::darwinIsWindowMaximized(pw));
// #else /* !VBOX_WS_MAC */
//     gEDataManager->setSoftKeyboardDialogGeometry(saveGeometry, isMaximized());
// #endif /* !VBOX_WS_MAC */
//     LogRel2(("GUI: Soft Keyboard Dialog: Geometry saved as: Origin=%dx%d, Size=%dx%d\n",
//              saveGeometry.x(), saveGeometry.y(), saveGeometry.width(), saveGeometry.height()));
}

bool UISoftKeyboardDialog::shouldBeMaximized() const
{
    return gEDataManager->softKeyboardDialogShouldBeMaximized();
}

void UISoftKeyboardDialog::sltSetCloseButtonShortCut(QKeySequence )
{
    // if (button(ButtonType_Close))
    //     button(ButtonType_Close)->setShortcut(shortcut);
}
