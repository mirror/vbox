/* $Id$ */
/** @file
 * VBox Qt GUI - UIHelpBrowserDialog class implementation.
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
#include <QMenuBar>
#include <QStatusBar>

/* GUI includes: */
#include "UIDesktopWidgetWatchdog.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIHelpBrowserDialog.h"
#include "UIHelpBrowserWidget.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils-darwin.h"
#endif


/*********************************************************************************************************************************
*   Class UIHelpBrowserDialog implementation.                                                                                    *
*********************************************************************************************************************************/

UIHelpBrowserDialog::UIHelpBrowserDialog(QWidget *pParent, QWidget *pCenterWidget, const QString &strHelpFilePath)
    : QIWithRetranslateUI<QIWithRestorableGeometry<QMainWindow> >(pParent)
    , m_strHelpFilePath(strHelpFilePath)
    , m_pWidget(0)
    , m_pCenterWidget(pCenterWidget)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowIcon(UIIconPool::iconSetFull(":/vm_show_logs_32px.png", ":/vm_show_logs_16px.png"));
    prepareCentralWidget();
    statusBar()->show();
    loadSettings();
    retranslateUi();
}

UIHelpBrowserDialog::~UIHelpBrowserDialog()
{
    saveSettings();
}

void UIHelpBrowserDialog::showHelpForKeyword(const QString &strKeyword)
{
#if defined(RT_OS_LINUX) && (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
    if (m_pWidget)
        m_pWidget->showHelpForKeyword(strKeyword);
#else
    Q_UNUSED(strKeyword);
#endif
}

void UIHelpBrowserDialog::retranslateUi()
{
#if defined(RT_OS_LINUX) && (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
    setWindowTitle(UIHelpBrowserWidget::tr("Oracle VM VirtualBox User Manual"));
#endif
}


void UIHelpBrowserDialog::prepareCentralWidget()
{
#if defined(RT_OS_LINUX) && (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
    m_pWidget = new UIHelpBrowserWidget(EmbedTo_Dialog, m_strHelpFilePath);
    AssertPtrReturnVoid(m_pWidget);
#ifdef VBOX_WS_MAC
    setWidgetToolbar(m_pWidget->toolbar());
#endif
    setCentralWidget((m_pWidget));

    connect(m_pWidget, &UIHelpBrowserWidget::sigCloseDialog,
            this, &UIHelpBrowserDialog::close);
    connect(m_pWidget, &UIHelpBrowserWidget::sigLinkHighlighted,
            this, &UIHelpBrowserDialog::sltHandleLinkHighlighted);
    connect(m_pWidget, &UIHelpBrowserWidget::sigStatusBarVisible,
            this, &UIHelpBrowserDialog::sltHandleStatusBarVisibilityChange);

    const QList<QMenu*> menuList = m_pWidget->menus();
    foreach (QMenu *pMenu, menuList)
        menuBar()->addMenu(pMenu);
#endif
}

void UIHelpBrowserDialog::loadSettings()
{
    const QRect availableGeo = gpDesktop->availableGeometry(this);
    int iDefaultWidth = availableGeo.width() / 2;
    int iDefaultHeight = availableGeo.height() * 3 / 4;
    QRect defaultGeo(0, 0, iDefaultWidth, iDefaultHeight);

    const QRect geo = gEDataManager->helpBrowserDialogGeometry(this, m_pCenterWidget, defaultGeo);
    restoreGeometry(geo);
}

void UIHelpBrowserDialog::saveSettings()
{
    const QRect geo = currentGeometry();
    gEDataManager->setHelpBrowserDialogGeometry(geo, isCurrentlyMaximized());
}

bool UIHelpBrowserDialog::shouldBeMaximized() const
{
    return gEDataManager->helpBrowserDialogShouldBeMaximized();
}

void UIHelpBrowserDialog::sltHandleLinkHighlighted(const QString& strLink)
{
    statusBar()->showMessage(strLink);
}

void UIHelpBrowserDialog::sltHandleStatusBarVisibilityChange(bool fVisible)
{
    statusBar()->setVisible(fVisible);
}
