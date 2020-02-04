/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewerDialog class implementation.
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
#if defined(RT_OS_SOLARIS)
# include <QFontDatabase>
#endif
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

/* GUI includes: */
#include "UIDesktopWidgetWatchdog.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIVMLogViewerDialog.h"
#include "UIVMLogViewerWidget.h"
#include "UICommon.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils-darwin.h"
#endif


/*********************************************************************************************************************************
*   Class UIVMLogViewerDialogFactory implementation.                                                                             *
*********************************************************************************************************************************/

UIVMLogViewerDialogFactory::UIVMLogViewerDialogFactory(UIActionPool *pActionPool /* = 0 */,
                                                       const CMachine &comMachine /* = CMachine() */)
    : m_pActionPool(pActionPool)
    , m_comMachine(comMachine)
{
}

void UIVMLogViewerDialogFactory::create(QIManagerDialog *&pDialog, QWidget *pCenterWidget)
{
    pDialog = new UIVMLogViewerDialog(pCenterWidget, m_pActionPool, m_comMachine);
}


/*********************************************************************************************************************************
*   Class UIVMLogViewerDialog implementation.                                                                                    *
*********************************************************************************************************************************/

UIVMLogViewerDialog::UIVMLogViewerDialog(QWidget *pCenterWidget, UIActionPool *pActionPool, const CMachine &comMachine)
    : QIWithRetranslateUI<QIManagerDialog>(pCenterWidget)
    , m_pActionPool(pActionPool)
    , m_comMachine(comMachine)
{
}

void UIVMLogViewerDialog::retranslateUi()
{
    /* Translate window title: */
    if (!m_comMachine.isNull())
        setWindowTitle(tr("%1 - Log Viewer").arg(m_comMachine.GetName()));
    else
        setWindowTitle(UIVMLogViewerWidget::tr("Log Viewer"));

    /* Translate buttons: */
    button(ButtonType_Close)->setText(UIVMLogViewerWidget::tr("Close"));
}

void UIVMLogViewerDialog::configure()
{
    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/vm_show_logs_32px.png", ":/vm_show_logs_16px.png"));
}

void UIVMLogViewerDialog::configureCentralWidget()
{
    /* Create widget: */
    UIVMLogViewerWidget *pWidget = new UIVMLogViewerWidget(EmbedTo_Dialog, m_pActionPool, true /* show toolbar */, m_comMachine, this);
    if (pWidget)
    {
        /* Configure widget: */
        setWidget(pWidget);
        setWidgetMenu(pWidget->menu());
#ifdef VBOX_WS_MAC
        setWidgetToolbar(pWidget->toolbar());
#endif
        connect(pWidget, &UIVMLogViewerWidget::sigSetCloseButtonShortCut,
                this, &UIVMLogViewerDialog::sltSetCloseButtonShortCut);

        /* Add into layout: */
        centralWidget()->layout()->addWidget(pWidget);
    }
}

void UIVMLogViewerDialog::finalize()
{
    /* Apply language settings: */
    retranslateUi();
    manageEscapeShortCut();
}

void UIVMLogViewerDialog::loadSettings()
{
    /* Invent default window geometry: */
    const QRect availableGeo = gpDesktop->availableGeometry(this);
    int iDefaultWidth = availableGeo.width() / 2;
    int iDefaultHeight = availableGeo.height() * 3 / 4;
    /* Try obtain the default width of the current logviewer: */
    const UIVMLogViewerWidget *pWidget = qobject_cast<const UIVMLogViewerWidget*>(widget());
    if (pWidget)
    {
        const int iWidth = pWidget->defaultLogPageWidth();
        if (iWidth != 0)
            iDefaultWidth = iWidth;
    }
    QRect defaultGeo(0, 0, iDefaultWidth, iDefaultHeight);

    /* Load geometry from extradata: */
    const QRect geo = gEDataManager->logWindowGeometry(this, centerWidget(), defaultGeo);
    LogRel2(("GUI: UIVMLogViewerDialog: Restoring geometry to: Origin=%dx%d, Size=%dx%d\n",
             geo.x(), geo.y(), geo.width(), geo.height()));
    restoreGeometry(geo);
}

void UIVMLogViewerDialog::saveSettings() const
{
    /* Save geometry to extradata: */
    const QRect geo = currentGeometry();
    LogRel2(("GUI: UIVMLogViewerDialog: Saving geometry as: Origin=%dx%d, Size=%dx%d\n",
             geo.x(), geo.y(), geo.width(), geo.height()));
    gEDataManager->setLogWindowGeometry(geo, isCurrentlyMaximized());
}

bool UIVMLogViewerDialog::shouldBeMaximized() const
{
    return gEDataManager->logWindowShouldBeMaximized();
}

void UIVMLogViewerDialog::sltSetCloseButtonShortCut(QKeySequence shortcut)
{
    if (button(ButtonType_Close))
        button(ButtonType_Close)->setShortcut(shortcut);
}

void UIVMLogViewerDialog::manageEscapeShortCut()
{
    UIVMLogViewerWidget *pWidget = qobject_cast<UIVMLogViewerWidget*>(widget());
    if (!pWidget)
        return;
    pWidget->manageEscapeShortCut();
}
