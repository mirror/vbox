/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestProcessControlDialog class implementation.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
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
#include "UIGuestProcessControlDialog.h"
#include "UICommon.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils-darwin.h"
#endif


/*********************************************************************************************************************************
*   Class UIGuestProcessControlDialogFactory implementation.                                                                     *
*********************************************************************************************************************************/

UIGuestProcessControlDialogFactory::UIGuestProcessControlDialogFactory(UIActionPool *pActionPool /* = 0 */,
                                                         const CGuest &comGuest /* = CGuest() */,
                                                         const QString &strMachineName /* = QString() */)
    : m_pActionPool(pActionPool)
    , m_comGuest(comGuest)
    , m_strMachineName(strMachineName)
{
}

void UIGuestProcessControlDialogFactory::create(QIManagerDialog *&pDialog, QWidget *pCenterWidget)
{
    pDialog = new UIGuestProcessControlDialog(pCenterWidget, m_pActionPool, m_comGuest, m_strMachineName);
}


/*********************************************************************************************************************************
*   Class UIGuestProcessControlDialog implementation.                                                                            *
*********************************************************************************************************************************/

UIGuestProcessControlDialog::UIGuestProcessControlDialog(QWidget *pCenterWidget,
                                           UIActionPool *pActionPool,
                                           const CGuest &comGuest,
                                           const QString &strMachineName /* = QString() */)
    : QIWithRetranslateUI<QIManagerDialog>(pCenterWidget)
    , m_pActionPool(pActionPool)
    , m_comGuest(comGuest)
    , m_strMachineName(strMachineName)
{
}

void UIGuestProcessControlDialog::retranslateUi()
{
    /* Translate window title: */
    setWindowTitle(tr("%1 - Guest Control").arg(m_strMachineName));
    /* Translate buttons: */
    button(ButtonType_Close)->setText(tr("Close"));
}

void UIGuestProcessControlDialog::configure()
{
    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/vm_show_logs_32px.png", ":/vm_show_logs_16px.png"));
}

void UIGuestProcessControlDialog::configureCentralWidget()
{
    /* Create widget: */
    UIGuestControlConsole *pConsole = new UIGuestControlConsole(m_comGuest);

    if (pConsole)
    {
        /* Configure widget: */
        setWidget(pConsole);
        //setWidgetMenu(pWidget->menu());
#ifdef VBOX_WS_MAC
        //setWidgetToolbar(pWidget->toolbar());
#endif
        /* Add into layout: */
        centralWidget()->layout()->addWidget(pConsole);
    }
}

void UIGuestProcessControlDialog::finalize()
{
    /* Apply language settings: */
    retranslateUi();
}

void UIGuestProcessControlDialog::loadSettings()
{
    /* Invent default window geometry: */
    const QRect availableGeo = gpDesktop->availableGeometry(this);
    const int iDefaultWidth = availableGeo.width() / 2;
    const int iDefaultHeight = availableGeo.height() * 3 / 4;
    QRect defaultGeo(0, 0, iDefaultWidth, iDefaultHeight);

    /* Load geometry from extradata: */
    QRect geo = gEDataManager->guestProcessControlDialogGeometry(this, centerWidget(), defaultGeo);
    LogRel2(("GUI: UIGuestProcessControlDialog: Restoring geometry to: Origin=%dx%d, Size=%dx%d\n",
             geo.x(), geo.y(), geo.width(), geo.height()));
    restoreGeometry(geo);
}

void UIGuestProcessControlDialog::saveSettings() const
{
    /* Save geometry to extradata: */
    const QRect geo = currentGeometry();
    LogRel2(("GUI: UIGuestProcessControlDialog: Saving geometry as: Origin=%dx%d, Size=%dx%d\n",
             geo.x(), geo.y(), geo.width(), geo.height()));
    gEDataManager->setGuestProcessControlDialogGeometry(geo, isCurrentlyMaximized());
}

bool UIGuestProcessControlDialog::shouldBeMaximized() const
{
    return gEDataManager->guestProcessControlDialogShouldBeMaximized();
}

void UIGuestProcessControlDialog::sltSetCloseButtonShortCut(QKeySequence shortcut)
{
    if (button(ButtonType_Close))
        button(ButtonType_Close)->setShortcut(shortcut);
}
