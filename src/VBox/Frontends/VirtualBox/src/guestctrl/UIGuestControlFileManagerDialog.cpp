/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlFileManagerDialog class implementation.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
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
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QPushButton>
# include <QVBoxLayout>

/* GUI includes: */
# include "UIDesktopWidgetWatchdog.h"
# include "UIExtraDataManager.h"
# include "UIIconPool.h"
# include "UIGuestControlFileManager.h"
# include "UIGuestControlFileManagerDialog.h"
# include "VBoxGlobal.h"
# ifdef VBOX_WS_MAC
#  include "VBoxUtils-darwin.h"
# endif

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Class UIGuestControlFileManagerDialogFactory implementation.                                                                 *
*********************************************************************************************************************************/

UIGuestControlFileManagerDialogFactory::UIGuestControlFileManagerDialogFactory(UIActionPool *pActionPool /* = 0 */,
                                                         const CGuest &comGuest /* = CGuest() */,
                                                         const QString &strMachineName /* = QString() */)
    : m_pActionPool(pActionPool)
    , m_comGuest(comGuest)
    , m_strMachineName(strMachineName)
{
}

void UIGuestControlFileManagerDialogFactory::create(QIManagerDialog *&pDialog, QWidget *pCenterWidget)
{
    pDialog = new UIGuestControlFileManagerDialog(pCenterWidget, m_pActionPool, m_comGuest, m_strMachineName);
}


/*********************************************************************************************************************************
*   Class UIGuestControlFileManagerDialog implementation.                                                                        *
*********************************************************************************************************************************/

UIGuestControlFileManagerDialog::UIGuestControlFileManagerDialog(QWidget *pCenterWidget,
                                           UIActionPool *pActionPool,
                                           const CGuest &comGuest,
                                           const QString &strMachineName /* = QString() */)
    : QIWithRetranslateUI<QIManagerDialog>(pCenterWidget)
    , m_pActionPool(pActionPool)
    , m_comGuest(comGuest)
    , m_strMachineName(strMachineName)
{
}

void UIGuestControlFileManagerDialog::retranslateUi()
{
    /* Translate window title: */
    setWindowTitle(UIGuestControlFileManager::tr("%1 - Guest Control File Manager").arg(m_strMachineName));
    /* Translate buttons: */
    button(ButtonType_Close)->setText(UIGuestControlFileManager::tr("Close"));
}

void UIGuestControlFileManagerDialog::configure()
{
    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/file_manager_32px.png", ":/file_manager_16px.png"));
}

void UIGuestControlFileManagerDialog::configureCentralWidget()
{
    /* Create widget: */
    UIGuestControlFileManager *pWidget = new UIGuestControlFileManager(EmbedTo_Dialog, m_pActionPool,
                                                                       m_comGuest, this);

    if (pWidget)
    {
        /* Configure widget: */
        setWidget(pWidget);
        setWidgetMenu(pWidget->menu());
#ifdef VBOX_WS_MAC
        //setWidgetToolbar(pWidget->toolbar());
#endif
        connect(pWidget, &UIGuestControlFileManager::sigSetCloseButtonShortCut,
                this, &UIGuestControlFileManagerDialog::sltSetCloseButtonShortCut);

        /* Add into layout: */
        centralWidget()->layout()->addWidget(pWidget);
    }
}

void UIGuestControlFileManagerDialog::finalize()
{
    /* Apply language settings: */
    retranslateUi();
    manageEscapeShortCut();
}

void UIGuestControlFileManagerDialog::loadSettings()
{
    const QRect desktopRect = gpDesktop->availableGeometry(this);
    int iDefaultWidth = desktopRect.width() / 2;
    int iDefaultHeight = desktopRect.height() * 3 / 4;

    QRect defaultGeometry(0, 0, iDefaultWidth, iDefaultHeight);
    if (centerWidget())
        defaultGeometry.moveCenter(centerWidget()->geometry().center());

    /* Load geometry from extradata: */
    QRect geometry = gEDataManager->guestControlFileManagerDialogGeometry(this, defaultGeometry);

    /* Restore geometry: */
    LogRel2(("GUI: UIGuestControlFileManagerDialog: Restoring geometry to: Origin=%dx%d, Size=%dx%d\n",
             geometry.x(), geometry.y(), geometry.width(), geometry.height()));
    setDialogGeometry(geometry);
}

void UIGuestControlFileManagerDialog::saveSettings() const
{
    /* Save window geometry to extradata: */
    const QRect saveGeometry = geometry();
#ifdef VBOX_WS_MAC
    /* darwinIsWindowMaximized expects a non-const QWidget*. thus const_cast: */
    QWidget *pw = const_cast<QWidget*>(qobject_cast<const QWidget*>(this));
    gEDataManager->setGuestControlFileManagerDialogGeometry(saveGeometry, ::darwinIsWindowMaximized(pw));
#else /* !VBOX_WS_MAC */
    gEDataManager->setGuestControlFileManagerDialogGeometry(saveGeometry, isMaximized());
#endif /* !VBOX_WS_MAC */
    LogRel2(("GUI: Guest Control File Manager Dialog: Geometry saved as: Origin=%dx%d, Size=%dx%d\n",
             saveGeometry.x(), saveGeometry.y(), saveGeometry.width(), saveGeometry.height()));
}

bool UIGuestControlFileManagerDialog::shouldBeMaximized() const
{
    return gEDataManager->guestControlFileManagerDialogShouldBeMaximized();
}

void UIGuestControlFileManagerDialog::sltSetCloseButtonShortCut(QKeySequence shortcut)
{
    if (button(ButtonType_Close))
        button(ButtonType_Close)->setShortcut(shortcut);
}

void UIGuestControlFileManagerDialog::manageEscapeShortCut()
{
    UIGuestControlFileManager *pWidget = qobject_cast<UIGuestControlFileManager*>(widget());
    if (!pWidget)
        return;
    pWidget->manageEscapeShortCut();
}
