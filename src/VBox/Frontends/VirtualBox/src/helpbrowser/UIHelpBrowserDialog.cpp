/* $Id$ */
/** @file
 * VBox Qt GUI - UIHelpBrowserDialog class implementation.
 */

/*
 * Copyright (C) 2010-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Qt includes: */
#if defined(RT_OS_SOLARIS)
# include <QFontDatabase>
#endif
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>

/* GUI includes: */
#include "UIDesktopWidgetWatchdog.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIHelpBrowserDialog.h"
#include "UIHelpBrowserWidget.h"
#include "UINotificationObjects.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils-darwin.h"
#endif

#include <iprt/assert.h>

QPointer<UIHelpBrowserDialog> UIHelpBrowserDialog::m_pInstance;


/*********************************************************************************************************************************
*   Class UIHelpBrowserDialog implementation.                                                                                    *
*********************************************************************************************************************************/

UIHelpBrowserDialog::UIHelpBrowserDialog(QWidget *pParent, QWidget *pCenterWidget, const QString &strHelpFilePath)
    : QIWithRetranslateUI<QIWithRestorableGeometry<QMainWindow> >(pParent)
    , m_strHelpFilePath(strHelpFilePath)
    , m_pWidget(0)
    , m_pCenterWidget(pCenterWidget)
    , m_iGeometrySaveTimerId(-1)
    , m_pZoomLabel(0)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowIcon(UIIconPool::iconSetFull(":/vm_show_logs_32px.png", ":/vm_show_logs_16px.png"));
    statusBar()->show();
    m_pZoomLabel = new QLabel;
    statusBar()->addPermanentWidget(m_pZoomLabel);

    prepareCentralWidget();
    loadSettings();
    retranslateUi();
}

void UIHelpBrowserDialog::showHelpForKeyword(const QString &strKeyword)
{
    if (m_pWidget)
        m_pWidget->showHelpForKeyword(strKeyword);
}

void UIHelpBrowserDialog::retranslateUi()
{
    setWindowTitle(UIHelpBrowserWidget::tr("Oracle VM VirtualBox User Manual"));
}

bool UIHelpBrowserDialog::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case QEvent::Resize:
        case QEvent::Move:
        {
            if (m_iGeometrySaveTimerId != -1)
                killTimer(m_iGeometrySaveTimerId);
            m_iGeometrySaveTimerId = startTimer(300);
            break;
        }
        case QEvent::Timer:
        {
            QTimerEvent *pTimerEvent = static_cast<QTimerEvent*>(pEvent);
            if (pTimerEvent->timerId() == m_iGeometrySaveTimerId)
            {
                killTimer(m_iGeometrySaveTimerId);
                m_iGeometrySaveTimerId = -1;
                saveDialogGeometry();
            }
            break;
        }
        default:
            break;
    }
    return QIWithRetranslateUI<QIWithRestorableGeometry<QMainWindow> >::event(pEvent);
}


void UIHelpBrowserDialog::prepareCentralWidget()
{
    m_pWidget = new UIHelpBrowserWidget(EmbedTo_Dialog, m_strHelpFilePath);
    AssertPtrReturnVoid(m_pWidget);
    setCentralWidget((m_pWidget));
    sltZoomPercentageChanged(m_pWidget->zoomPercentage());
    connect(m_pWidget, &UIHelpBrowserWidget::sigCloseDialog,
            this, &UIHelpBrowserDialog::close);
    connect(m_pWidget, &UIHelpBrowserWidget::sigStatusBarMessage,
            this, &UIHelpBrowserDialog::sltStatusBarMessage);
    connect(m_pWidget, &UIHelpBrowserWidget::sigStatusBarVisible,
            this, &UIHelpBrowserDialog::sltStatusBarVisibilityChange);
    connect(m_pWidget, &UIHelpBrowserWidget::sigZoomPercentageChanged,
            this, &UIHelpBrowserDialog::sltZoomPercentageChanged);

    const QList<QMenu*> menuList = m_pWidget->menus();
    foreach (QMenu *pMenu, menuList)
        menuBar()->addMenu(pMenu);
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

void UIHelpBrowserDialog::saveDialogGeometry()
{
    const QRect geo = currentGeometry();
    gEDataManager->setHelpBrowserDialogGeometry(geo, isCurrentlyMaximized());
}

bool UIHelpBrowserDialog::shouldBeMaximized() const
{
    return gEDataManager->helpBrowserDialogShouldBeMaximized();
}

void UIHelpBrowserDialog::sltStatusBarMessage(const QString& strLink, int iTimeOut)
{
    statusBar()->showMessage(strLink, iTimeOut);
}

void UIHelpBrowserDialog::sltStatusBarVisibilityChange(bool fVisible)
{
    statusBar()->setVisible(fVisible);
}

void UIHelpBrowserDialog::sltZoomPercentageChanged(int iPercentage)
{
    if (m_pZoomLabel)
        m_pZoomLabel->setText(QString("%1%").arg(QString::number(iPercentage)));
}

/* static */
void UIHelpBrowserDialog::findManualFileAndShow(const QString &strKeyword /*= QString() */)
{
#ifndef VBOX_OSE
    /* For non-OSE version we just open it: */
    showUserManual(uiCommon().helpFile(), strKeyword);
#else /* #ifndef VBOX_OSE */
    /* For OSE version we have to check if it present first: */
    QString strUserManualFileName1 = uiCommon().helpFile();
    QString strShortFileName = QFileInfo(strUserManualFileName1).fileName();
    QString strUserManualFileName2 = QDir(uiCommon().homeFolder()).absoluteFilePath(strShortFileName);
    /* Show if user manual already present: */
    if (QFile::exists(strUserManualFileName1))
        showUserManual(strUserManualFileName1, strKeyword);
    else if (QFile::exists(strUserManualFileName2))
        showUserManual(strUserManualFileName2, strKeyword);
# ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    /* If downloader is running already: */
    if (UINotificationDownloaderUserManual::exists())
        gpNotificationCenter->invoke();
    /* Else propose to download user manual: */
    else if (confirmLookingForUserManual(strUserManualFileName1))
    {
        /* Download user manual: */
        UINotificationDownloaderUserManual *pNotification = UINotificationDownloaderUserManual::instance(UICommon::helpFile());
        /* After downloading finished => show User Manual: */
        /// @todo
        // connect(pNotification, &UINotificationDownloaderUserManual::sigUserManualDownloaded,
        //         this, &UIMessageCenter::showUserManual);
        /* Append and start notification: */
        gpNotificationCenter->append(pNotification);
    }
# endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
#endif /* #ifdef VBOX_OSE */
}

/* static */
void UIHelpBrowserDialog::showUserManual(const QString &strHelpFilePath, const QString &strKeyword)
{
    if (!QFileInfo(strHelpFilePath).exists())
    {
        UINotificationMessage::cannotFindHelpFile(strHelpFilePath);
        return;
    }
    if (!m_pInstance)
    {
        m_pInstance = new UIHelpBrowserDialog(0 /* parent */, 0 /* Center Widget */, strHelpFilePath);
        AssertReturnVoid(m_pInstance);
    }

    if (!strKeyword.isEmpty())
        m_pInstance->showHelpForKeyword(strKeyword);
    m_pInstance->show();
    m_pInstance->setWindowState(m_pInstance->windowState() & ~Qt::WindowMinimized);
    m_pInstance->activateWindow();
}
