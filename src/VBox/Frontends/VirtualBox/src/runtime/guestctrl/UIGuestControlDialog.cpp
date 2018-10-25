/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlDialog class implementation.
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
# if defined(RT_OS_SOLARIS)
#  include <QFontDatabase>
# endif
# include <QDialogButtonBox>
# include <QKeyEvent>
# include <QLabel>
# include <QPlainTextEdit>
# include <QPushButton>
# include <QScrollBar>
# include <QVBoxLayout>

/* GUI includes: */
# include "UIDesktopWidgetWatchdog.h"
# include "UIExtraDataManager.h"
# include "UIIconPool.h"
# include "UIGuestControlDialog.h"
# include "UIGuestControlWidget.h"
# include "VBoxGlobal.h"
# ifdef VBOX_WS_MAC
#  include "VBoxUtils-darwin.h"
# endif

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Class UIGuestControlDialogFactory implementation.                                                                            *
*********************************************************************************************************************************/

UIGuestControlDialogFactory::UIGuestControlDialogFactory(UIActionPool *pActionPool /* = 0 */,
                                                         const CGuest &comGuest /* = CGuest() */,
                                                         const QString &strMachineName /* = QString() */)
    : m_pActionPool(pActionPool)
    , m_comGuest(comGuest)
    , m_strMachineName(strMachineName)
{
}

void UIGuestControlDialogFactory::create(QIManagerDialog *&pDialog, QWidget *pCenterWidget)
{
    pDialog = new UIGuestControlDialog(pCenterWidget, m_pActionPool, m_comGuest, m_strMachineName);
}


/*********************************************************************************************************************************
*   Class UIGuestControlDialog implementation.                                                                                   *
*********************************************************************************************************************************/

UIGuestControlDialog::UIGuestControlDialog(QWidget *pCenterWidget,
                                           UIActionPool *pActionPool,
                                           const CGuest &comGuest,
                                           const QString &strMachineName /* = QString() */)
    : QIWithRetranslateUI<QIManagerDialog>(pCenterWidget)
    , m_pActionPool(pActionPool)
    , m_comGuest(comGuest)
    , m_strMachineName(strMachineName)
{
}

void UIGuestControlDialog::retranslateUi()
{
    /* Translate window title: */
    setWindowTitle(tr("%1 - Guest Control").arg(m_strMachineName));
    /* Translate buttons: */
    button(ButtonType_Close)->setText(UIGuestControlWidget::tr("Close"));
}

void UIGuestControlDialog::configure()
{
    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/vm_show_logs_32px.png", ":/vm_show_logs_16px.png"));
}

void UIGuestControlDialog::configureCentralWidget()
{
    /* Create widget: */
    UIGuestControlWidget *pWidget = new UIGuestControlWidget(EmbedTo_Dialog, m_pActionPool, m_comGuest, true /* show toolbar */,  this);

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

void UIGuestControlDialog::finalize()
{
    /* Apply language settings: */
    retranslateUi();
}

void UIGuestControlDialog::loadSettings()
{
    const QRect desktopRect = gpDesktop->availableGeometry(this);
    int iDefaultWidth = desktopRect.width() / 2;
    int iDefaultHeight = desktopRect.height() * 3 / 4;

    QRect defaultGeometry(0, 0, iDefaultWidth, iDefaultHeight);
    if (centerWidget())
        defaultGeometry.moveCenter(centerWidget()->geometry().center());

    /* Load geometry from extradata: */
    QRect geometry = gEDataManager->guestControlDialogGeometry(this, defaultGeometry);

    /* Restore geometry: */
    LogRel2(("GUI: UIGuestControlDialog: Restoring geometry to: Origin=%dx%d, Size=%dx%d\n",
             geometry.x(), geometry.y(), geometry.width(), geometry.height()));
    setDialogGeometry(geometry);
}

void UIGuestControlDialog::saveSettings() const
{
    /* Save window geometry to extradata: */
    const QRect saveGeometry = geometry();
#ifdef VBOX_WS_MAC
    /* darwinIsWindowMaximized expects a non-const QWidget*. thus const_cast: */
    QWidget *pw = const_cast<QWidget*>(qobject_cast<const QWidget*>(this));
    gEDataManager->setGuestControlDialogGeometry(saveGeometry, ::darwinIsWindowMaximized(pw));
#else /* !VBOX_WS_MAC */
    gEDataManager->setGuestControlDialogGeometry(saveGeometry, isMaximized());
#endif /* !VBOX_WS_MAC */
    LogRel2(("GUI: UIVMLogViewer: Geometry saved as: Origin=%dx%d, Size=%dx%d\n",
             saveGeometry.x(), saveGeometry.y(), saveGeometry.width(), saveGeometry.height()));
}

bool UIGuestControlDialog::shouldBeMaximized() const
{
    return gEDataManager->guestControlDialogShouldBeMaximized();
}

void UIGuestControlDialog::sltSetCloseButtonShortCut(QKeySequence shortcut)
{
    if (button(ButtonType_Close))
        button(ButtonType_Close)->setShortcut(shortcut);
}
