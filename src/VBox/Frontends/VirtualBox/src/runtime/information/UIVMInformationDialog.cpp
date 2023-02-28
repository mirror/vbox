/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMInformationDialog class implementation.
 */

/*
 * Copyright (C) 2016-2023 Oracle and/or its affiliates.
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
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

/* GUI includes: */
#include "QITabWidget.h"
#include "QIDialogButtonBox.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIInformationConfiguration.h"
#include "UIInformationRuntime.h"
#include "UIGuestProcessControlWidget.h"
#include "UIMachineLogic.h"
#include "UIMachine.h"
#include "UIMachineView.h"
#include "UIMessageCenter.h"
#include "UIVMActivityMonitor.h"
#include "UISession.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIVMInformationDialog.h"
#include "VBoxUtils.h"

/* Other VBox includes: */
#include <iprt/time.h>

UIVMInformationDialog::UIVMInformationDialog(UIMachine *pMachine)
    : QMainWindowWithRestorableGeometryAndRetranslateUi(0)
    , m_pMachine(pMachine)
    , m_pTabWidget(0)
    , m_fCloseEmitted(false)
    , m_iGeometrySaveTimerId(-1)
{
    prepare();
}

bool UIVMInformationDialog::shouldBeMaximized() const
{
    return gEDataManager->sessionInformationDialogShouldBeMaximized();
}

void UIVMInformationDialog::retranslateUi()
{
    /* Setup dialog title: */
    setWindowTitle(tr("%1 - Session Information").arg(m_strMachineName));

    /* Translate tabs: */
    m_pTabWidget->setTabText(Tabs_ConfigurationDetails, tr("Configuration &Details"));
    m_pTabWidget->setTabText(Tabs_RuntimeInformation, tr("&Runtime Information"));
    m_pTabWidget->setTabText(Tabs_ActivityMonitor, tr("VM &Activity"));
    m_pTabWidget->setTabText(3, tr("&Guest Control"));

    /* Retranslate button box buttons: */
    if (m_pButtonBox)
    {
        m_pButtonBox->button(QDialogButtonBox::Close)->setText(tr("Close"));
        m_pButtonBox->button(QDialogButtonBox::Help)->setText(tr("Help"));
        m_pButtonBox->button(QDialogButtonBox::Close)->setStatusTip(tr("Close dialog without saving"));
        m_pButtonBox->button(QDialogButtonBox::Help)->setStatusTip(tr("Show dialog help"));
        m_pButtonBox->button(QDialogButtonBox::Close)->setShortcut(Qt::Key_Escape);
        m_pButtonBox->button(QDialogButtonBox::Help)->setShortcut(QKeySequence::HelpContents);
        m_pButtonBox->button(QDialogButtonBox::Close)->setToolTip(tr("Close this dialog (%1)").arg(m_pButtonBox->button(QDialogButtonBox::Close)->shortcut().toString()));
        m_pButtonBox->button(QDialogButtonBox::Help)->setToolTip(tr("Show Help (%1)").arg(m_pButtonBox->button(QDialogButtonBox::Help)->shortcut().toString()));
    }
}

void UIVMInformationDialog::closeEvent(QCloseEvent *pEvent)
{
    if (!m_fCloseEmitted)
    {
        m_fCloseEmitted = true;
        UIVMInformationDialog::sigClose();
        pEvent->ignore();
    }
}

bool UIVMInformationDialog::event(QEvent *pEvent)
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
    return QMainWindowWithRestorableGeometryAndRetranslateUi::event(pEvent);
}

void UIVMInformationDialog::sltHandlePageChanged(int iIndex)
{
    /* Focus the browser on shown page: */
    m_pTabWidget->widget(iIndex)->setFocus();
}

void UIVMInformationDialog::sltMachineStateChange(const QUuid &uMachineId, const KMachineState state)
{
    if (m_uMachineId != uMachineId)
        return;
    QWidget *pWidget = m_tabs.value(Tabs_GuestControl);
    if (!pWidget)
        return;
    pWidget->setEnabled(state == KMachineState_Running);
}

void UIVMInformationDialog::saveDialogGeometry()
{
    const QRect geo = currentGeometry();
    LogRel2(("GUI: UIVMInformationDialog: Saving geometry as: Origin=%dx%d, Size=%dx%d\n",
             geo.x(), geo.y(), geo.width(), geo.height()));
    gEDataManager->setSessionInformationDialogGeometry(geo, isCurrentlyMaximized());
}

void UIVMInformationDialog::prepare()
{
    /* Load and clear: */
    AssertPtrReturnVoid(m_pMachine);

    m_uMachineId = uiCommon().managedVMUuid();
    m_strMachineName = m_pMachine->machineName();
#ifdef VBOX_WS_MAC
    /* No window-icon on Mac OS X, because it acts as proxy icon which isn't necessary here. */
    setWindowIcon(QIcon());
#else
    /* Assign window-icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/session_info_32px.png", ":/session_info_16px.png"));
#endif

    /* Prepare stuff: */
    prepareCentralWidget();
    prepareConnections();

    /* Apply language settings: */
    retranslateUi();

    /* Load settings: */
    loadDialogGeometry();
}

void UIVMInformationDialog::prepareCentralWidget()
{
    /* Create central-widget: */
    setCentralWidget(new QWidget);
    AssertPtrReturnVoid(centralWidget());
    {
        /* Create main-layout: */
        new QVBoxLayout(centralWidget());
        AssertPtrReturnVoid(centralWidget()->layout());
        {
            /* Create tab-widget: */
            prepareTabWidget();
            /* Create button-box: */
            prepareButtonBox();
        }
    }
}

void UIVMInformationDialog::prepareTabWidget()
{
    /* Create tab-widget: */
    m_pTabWidget = new QITabWidget;
    AssertPtrReturnVoid(m_pTabWidget);
    {
        /* Prepare tab-widget: */
        m_pTabWidget->setTabIcon(Tabs_ConfigurationDetails, UIIconPool::iconSet(":/session_info_details_16px.png"));
        m_pTabWidget->setTabIcon(Tabs_RuntimeInformation, UIIconPool::iconSet(":/session_info_runtime_16px.png"));

        /* Create Configuration Details tab: */
        UIInformationConfiguration *pInformationConfigurationWidget = new UIInformationConfiguration(this);
        if (pInformationConfigurationWidget)
        {
            m_tabs.insert(Tabs_ConfigurationDetails, pInformationConfigurationWidget);
            m_pTabWidget->addTab(m_tabs.value(Tabs_ConfigurationDetails), QString());
        }

        /* Create Runtime Information tab: */
        UIInformationRuntime *pInformationRuntimeWidget =
            new UIInformationRuntime(this, m_pMachine->uisession()->machine(), m_pMachine->uisession()->console(), m_pMachine);
        if (pInformationRuntimeWidget)
        {
            m_tabs.insert(Tabs_RuntimeInformation, pInformationRuntimeWidget);
            m_pTabWidget->addTab(m_tabs.value(Tabs_RuntimeInformation), QString());
        }

        /* Create Performance Monitor tab: */
        UIVMActivityMonitor *pVMActivityMonitorWidget =
            new UIVMActivityMonitor(EmbedTo_Dialog, this, m_pMachine->uisession()->machine());
        if (pVMActivityMonitorWidget)
        {
            connect(m_pMachine, &UIMachine::sigAdditionsStateChange,
                    pVMActivityMonitorWidget, &UIVMActivityMonitor::sltGuestAdditionsStateChange);
            m_tabs.insert(Tabs_ActivityMonitor, pVMActivityMonitorWidget);
            m_pTabWidget->addTab(m_tabs.value(Tabs_ActivityMonitor), QString());
        }

        /* Create Guest Process Control tab: */
        UIGuestProcessControlWidget *pGuestProcessControlWidget =
            new UIGuestProcessControlWidget(EmbedTo_Dialog, m_pMachine->uisession()->guest(),
                                            this, m_strMachineName, false /* fShowToolbar */);
        if (pGuestProcessControlWidget)
        {
            m_tabs.insert(3, pGuestProcessControlWidget);
            m_pTabWidget->addTab(m_tabs.value(3), QString());
        }

        m_pTabWidget->setCurrentIndex(Tabs_ActivityMonitor);

        /* Assign tab-widget page change handler: */
        connect(m_pTabWidget, &QITabWidget::currentChanged, this, &UIVMInformationDialog::sltHandlePageChanged);

        /* Add tab-widget into main-layout: */
        centralWidget()->layout()->addWidget(m_pTabWidget);
    }
}

void UIVMInformationDialog::prepareButtonBox()
{
    /* Create button-box: */
    m_pButtonBox = new QIDialogButtonBox;
    AssertPtrReturnVoid(m_pButtonBox);
    {
        /* Configure button-box: */
        m_pButtonBox->setStandardButtons(QDialogButtonBox::Close | QDialogButtonBox::Help);
        m_pButtonBox->button(QDialogButtonBox::Close)->setShortcut(Qt::Key_Escape);
        m_pButtonBox->button(QDialogButtonBox::Help)->setShortcut(QKeySequence::HelpContents);
        uiCommon().setHelpKeyword(m_pButtonBox->button(QDialogButtonBox::Help), "vm-session-information");
        connect(m_pButtonBox, &QIDialogButtonBox::rejected, this, &UIVMInformationDialog::sigClose);
        connect(m_pButtonBox->button(QDialogButtonBox::Help), &QPushButton::pressed,
                &(msgCenter()), &UIMessageCenter::sltHandleHelpRequest);
        /* add button-box into main-layout: */
        centralWidget()->layout()->addWidget(m_pButtonBox);
    }
}

void UIVMInformationDialog::prepareConnections()
{
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineStateChange,
            this, &UIVMInformationDialog::sltMachineStateChange);
}

void UIVMInformationDialog::loadDialogGeometry()
{
    const QRect geo = gEDataManager->sessionInformationDialogGeometry(this, m_pMachine->activeWindow());
    LogRel2(("GUI: UIVMInformationDialog: Restoring geometry to: Origin=%dx%d, Size=%dx%d\n",
             geo.x(), geo.y(), geo.width(), geo.height()));
    restoreGeometry(geo);
}
