/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestProcessControlDialog class implementation.
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
# include "UIGuestProcessControlDialog.h"
# include "UIGuestProcessControlWidget.h"
# include "VBoxGlobal.h"
# ifdef VBOX_WS_MAC
#  include "VBoxUtils-darwin.h"
# endif

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


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
    button(ButtonType_Close)->setText(UIGuestProcessControlWidget::tr("Close"));
}

void UIGuestProcessControlDialog::configure()
{
    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/vm_show_logs_32px.png", ":/vm_show_logs_16px.png"));
}

void UIGuestProcessControlDialog::configureCentralWidget()
{
    /* Create widget: */
    UIGuestProcessControlWidget *pWidget = new UIGuestProcessControlWidget(EmbedTo_Dialog, m_pActionPool, m_comGuest, this);

    if (pWidget)
    {
        /* Configure widget: */
        setWidget(pWidget);
        //setWidgetMenu(pWidget->menu());
#ifdef VBOX_WS_MAC
        //setWidgetToolbar(pWidget->toolbar());
#endif
        // connect(pWidget, &UIGuestControlWidget::sigSetCloseButtonShortCut,
        //         this, &UIGuestControlWidget::sltSetCloseButtonShortCut);

        /* Add into layout: */
        centralWidget()->layout()->addWidget(pWidget);
    }
}

void UIGuestProcessControlDialog::finalize()
{
    /* Apply language settings: */
    retranslateUi();
}

void UIGuestProcessControlDialog::loadSettings()
{
    const QRect desktopRect = gpDesktop->availableGeometry(this);
    int iDefaultWidth = desktopRect.width() / 2;
    int iDefaultHeight = desktopRect.height() * 3 / 4;

    QRect defaultGeometry(0, 0, iDefaultWidth, iDefaultHeight);
    if (centerWidget())
        defaultGeometry.moveCenter(centerWidget()->geometry().center());

    /* Load geometry from extradata: */
    QRect geometry = gEDataManager->guestProcessControlDialogGeometry(this, defaultGeometry);

    /* Restore geometry: */
    LogRel2(("GUI: UIGuestProcessControlDialog: Restoring geometry to: Origin=%dx%d, Size=%dx%d\n",
             geometry.x(), geometry.y(), geometry.width(), geometry.height()));
    setDialogGeometry(geometry);
}

void UIGuestProcessControlDialog::saveSettings() const
{
    /* Save window geometry to extradata: */
    const QRect saveGeometry = geometry();
#ifdef VBOX_WS_MAC
    /* darwinIsWindowMaximized expects a non-const QWidget*. thus const_cast: */
    QWidget *pw = const_cast<QWidget*>(qobject_cast<const QWidget*>(this));
    gEDataManager->setGuestProcessControlDialogGeometry(saveGeometry, ::darwinIsWindowMaximized(pw));
#else /* !VBOX_WS_MAC */
    gEDataManager->setGuestProcessControlDialogGeometry(saveGeometry, isMaximized());
#endif /* !VBOX_WS_MAC */
    LogRel2(("GUI: Guest Process Control Dialog: Geometry saved as: Origin=%dx%d, Size=%dx%d\n",
             saveGeometry.x(), saveGeometry.y(), saveGeometry.width(), saveGeometry.height()));
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
