/* $Id$ */
/** @file
 * VBox Qt GUI - UIFileManagerDialog class implementation.
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
#include "UIFileManager.h"
#include "UIFileManagerDialog.h"
#include "UICommon.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils-darwin.h"
#endif


/*********************************************************************************************************************************
*   Class UIFileManagerDialogFactory implementation.                                                                 *
*********************************************************************************************************************************/

UIFileManagerDialogFactory::UIFileManagerDialogFactory(UIActionPool *pActionPool /* = 0 */,
                                                         const CGuest &comGuest /* = CGuest() */,
                                                         const QString &strMachineName /* = QString() */)
    : m_pActionPool(pActionPool)
    , m_comGuest(comGuest)
    , m_strMachineName(strMachineName)
{
}

void UIFileManagerDialogFactory::create(QIManagerDialog *&pDialog, QWidget *pCenterWidget)
{
    pDialog = new UIFileManagerDialog(pCenterWidget, m_pActionPool, m_comGuest, m_strMachineName);
}


/*********************************************************************************************************************************
*   Class UIFileManagerDialog implementation.                                                                        *
*********************************************************************************************************************************/

UIFileManagerDialog::UIFileManagerDialog(QWidget *pCenterWidget,
                                           UIActionPool *pActionPool,
                                           const CGuest &comGuest,
                                           const QString &strMachineName /* = QString() */)
    : QIWithRetranslateUI<QIManagerDialog>(pCenterWidget)
    , m_pActionPool(pActionPool)
    , m_comGuest(comGuest)
    , m_strMachineName(strMachineName)
{
}

void UIFileManagerDialog::retranslateUi()
{
    /* Translate window title: */
    setWindowTitle(UIFileManager::tr("%1 - File Manager").arg(m_strMachineName));
    /* Translate buttons: */
    button(ButtonType_Close)->setText(UIFileManager::tr("Close"));
}

void UIFileManagerDialog::configure()
{
    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/file_manager_32px.png", ":/file_manager_16px.png"));
}

void UIFileManagerDialog::configureCentralWidget()
{
    /* Create widget: */
    UIFileManager *pWidget = new UIFileManager(EmbedTo_Dialog, m_pActionPool,
                                                                       m_comGuest, this);

    if (pWidget)
    {
        /* Configure widget: */
        setWidget(pWidget);
        setWidgetMenu(pWidget->menu());
#ifdef VBOX_WS_MAC
        setWidgetToolbar(pWidget->toolbar());
#endif
        connect(pWidget, &UIFileManager::sigSetCloseButtonShortCut,
                this, &UIFileManagerDialog::sltSetCloseButtonShortCut);

        /* Add into layout: */
        centralWidget()->layout()->addWidget(pWidget);
    }
}

void UIFileManagerDialog::finalize()
{
    /* Apply language settings: */
    retranslateUi();
    manageEscapeShortCut();
}

void UIFileManagerDialog::loadSettings()
{
    /* Load geometry from extradata: */
    const QRect geo = gEDataManager->fileManagerDialogGeometry(this, centerWidget());
    LogRel2(("GUI: UIFileManagerDialog: Restoring geometry to: Origin=%dx%d, Size=%dx%d\n",
             geo.x(), geo.y(), geo.width(), geo.height()));
    restoreGeometry(geo);
}

void UIFileManagerDialog::saveSettings() const
{
    /* Save geometry to extradata: */
    const QRect geo = currentGeometry();
    LogRel2(("GUI: UIFileManagerDialog: Saving geometry as: Origin=%dx%d, Size=%dx%d\n",
             geo.x(), geo.y(), geo.width(), geo.height()));
    gEDataManager->setFileManagerDialogGeometry(geo, isCurrentlyMaximized());
}

bool UIFileManagerDialog::shouldBeMaximized() const
{
    return gEDataManager->fileManagerDialogShouldBeMaximized();
}

void UIFileManagerDialog::sltSetCloseButtonShortCut(QKeySequence shortcut)
{
    if (button(ButtonType_Close))
        button(ButtonType_Close)->setShortcut(shortcut);
}

void UIFileManagerDialog::manageEscapeShortCut()
{
    UIFileManager *pWidget = qobject_cast<UIFileManager*>(widget());
    if (!pWidget)
        return;
    pWidget->manageEscapeShortCut();
}
