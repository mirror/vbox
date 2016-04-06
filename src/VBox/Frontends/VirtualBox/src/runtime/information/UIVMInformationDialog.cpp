/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMInformationDialog class implementation.
 */

/*
 * Copyright (C) 2016 Oracle Corporation
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
# include <QScrollBar>
# include <QPushButton>

/* GUI includes: */
# include "UIVMInformationDialog.h"
# include "UIExtraDataManager.h"
# include "UISession.h"
# include "UIMachineLogic.h"
# include "UIMachineWindow.h"
# include "UIMachineView.h"
# include "UIConverter.h"
# include "UIIconPool.h"
# include "QITabWidget.h"
# include "QIDialogButtonBox.h"
# include "VBoxGlobal.h"
# include "VBoxUtils.h"
# include "UIInformation.h"
# include "UIMachine.h"
# include "UIVMItem.h"
# include "UIInformationRuntime.h"

/* COM includes: */
# include "COMEnums.h"
# include "CMachine.h"
# include "CConsole.h"
# include "CSystemProperties.h"
# include "CMachineDebugger.h"
# include "CDisplay.h"
# include "CGuest.h"
# include "CStorageController.h"
# include "CMediumAttachment.h"
# include "CNetworkAdapter.h"

/* Other VBox includes: */
# include <iprt/time.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#include "CVRDEServerInfo.h"

/* static */
UIVMInformationDialog* UIVMInformationDialog::m_spInstance = 0;

void UIVMInformationDialog::invoke(UIMachineWindow *pMachineWindow)
{
    /* Make sure dialog instance exists: */
    if (!m_spInstance)
    {
        /* Create new dialog instance if it doesn't exists yet: */
        new UIVMInformationDialog(pMachineWindow);
    }

    /* Show dialog: */
    m_spInstance->show();
    /* Raise it: */
    m_spInstance->raise();
    /* De-miniaturize if necessary: */
    m_spInstance->setWindowState(m_spInstance->windowState() & ~Qt::WindowMinimized);
    /* And activate finally: */
    m_spInstance->activateWindow();
}

UIVMInformationDialog::UIVMInformationDialog(UIMachineWindow *pMachineWindow)
    : QIWithRetranslateUI<QMainWindow>(0)
    , m_pTabWidget(0)
    , m_pMachineWindow(pMachineWindow)
{    
    /* Initialize instance: */
    m_spInstance = this;

    /* Prepare: */
    prepare();
}

UIVMInformationDialog::~UIVMInformationDialog()
{
    /* Cleanup: */
    cleanup();

    /* Deinitialize instance: */
    m_spInstance = 0;
}

void UIVMInformationDialog::retranslateUi()
{
    CMachine machine = gpMachine->uisession()->machine();
    AssertReturnVoid(!machine.isNull());

    /* Setup dialog title: */
    setWindowTitle(tr("%1 - Session Information").arg(machine.GetName()));

    /* Translate tabs: */
    m_pTabWidget->setTabText(0, tr("Configuration &Details"));
    m_pTabWidget->setTabText(1, tr("&Runtime Information"));
}

bool UIVMInformationDialog::event(QEvent *pEvent)
{
    /* Pre-process through base-class: */
    bool fResult = QMainWindow::event(pEvent);

    /* Process required events: */
    switch (pEvent->type())
    {
        /* Handle every Resize and Move we keep track of the geometry. */
        case QEvent::Resize:
        {
            if (isVisible() && (windowState() & (Qt::WindowMaximized | Qt::WindowMinimized | Qt::WindowFullScreen)) == 0)
            {
                QResizeEvent *pResizeEvent = static_cast<QResizeEvent*>(pEvent);
                m_geometry.setSize(pResizeEvent->size());
            }
            break;
        }
        case QEvent::Move:
        {
            if (isVisible() && (windowState() & (Qt::WindowMaximized | Qt::WindowMinimized | Qt::WindowFullScreen)) == 0)
            {
#ifdef VBOX_WS_MAC
                QMoveEvent *pMoveEvent = static_cast<QMoveEvent*>(pEvent);
                m_geometry.moveTo(pMoveEvent->pos());
#else /* VBOX_WS_MAC */
                m_geometry.moveTo(geometry().x(), geometry().y());
#endif /* !VBOX_WS_MAC */
            }
            break;
        }
        default:
            break;
    }

    /* Return result: */
    return fResult;
}

void UIVMInformationDialog::sltHandlePageChanged(int iIndex)
{
    /* Focus the browser on shown page: */
    m_pTabWidget->widget(iIndex)->setFocus();
}

void UIVMInformationDialog::prepare()
{
    /* Prepare dialog: */
    prepareThis();
    /* Load settings: */
    loadSettings();
}

void UIVMInformationDialog::prepareThis()
{
    /* Delete dialog on close: */
    setAttribute(Qt::WA_DeleteOnClose);
    /* Delete dialog on machine-window destruction: */
    connect(m_pMachineWindow, SIGNAL(destroyed(QObject*)), this, SLOT(suicide()));

#ifdef VBOX_WS_MAC
    /* No window-icon on Mac OX X, because it acts as proxy icon which isn't necessary here. */
    setWindowIcon(QIcon());
#else /* !VBOX_WS_MAC */
    /* Assign window-icon(s: */
    setWindowIcon(UIIconPool::iconSetFull(":/session_info_32px.png", ":/session_info_16px.png"));
#endif /* !VBOX_WS_MAC */

    /* Prepare central-widget: */
    prepareCentralWidget();

    /* Configure handlers: */
    //connect(m_pMachineWindow->uisession(), SIGNAL(sigMediumChange(const CMediumAttachment&)), this, SLOT(sltUpdateDetails()));
    //connect(m_pMachineWindow->uisession(), SIGNAL(sigSharedFolderChange()), this, SLOT(sltUpdateDetails()));
    /* TODO_NEW_CORE: this is ofc not really right in the mm sense. There are more than one screens. */
    //connect(m_pMachineWindow->machineView(), SIGNAL(sigFrameBufferResize()), this, SLOT(sltProcessStatistics()));
    connect(m_pTabWidget, SIGNAL(currentChanged(int)), this, SLOT(sltHandlePageChanged(int)));
    //connect(&vboxGlobal(), SIGNAL(sigMediumEnumerationFinished()), this, SLOT(sltUpdateDetails()));
    //connect(m_pTimer, SIGNAL(timeout()), this, SLOT(sltProcessStatistics()));

    /* Retranslate: */
    retranslateUi();
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
    /* List of VM items: */
    QList<UIVMItem*> items;
    items << new UIVMItem(gpMachine->uisession()->machine());

    /* Create tab-widget: */
    m_pTabWidget = new QITabWidget;
    AssertPtrReturnVoid(m_pTabWidget);
    {
        /* Prepare tab-widget: */
        m_pTabWidget->setTabIcon(0, UIIconPool::iconSet(":/session_info_details_16px.png"));
        m_pTabWidget->setTabIcon(1, UIIconPool::iconSet(":/session_info_runtime_16px.png"));
        m_pTabWidget->setCurrentIndex(1);
        /* Add tab-widget into main-layout: */
        centralWidget()->layout()->addWidget(m_pTabWidget);

        /* Create tabs: */
        /* Create Configuration details tab: */
        UIInformation *pInformationWidget = new UIInformation(this, gpMachine->uisession()->machine(), gpMachine->uisession()->console());
        AssertPtrReturnVoid(pInformationWidget);
        {
            //pInformationWidget->setItems(items);
            m_tabs.insert(0, pInformationWidget);
            m_pTabWidget->addTab(m_tabs.value(0), QString());
        }

        /* Create Runtime information tab: */
        UIInformationRuntime *pInformationRuntimeWidget = new UIInformationRuntime(this, gpMachine->uisession()->machine(), gpMachine->uisession()->console());
        AssertPtrReturnVoid(pInformationRuntimeWidget);
        {
            m_tabs.insert(1, pInformationRuntimeWidget);
            m_pTabWidget->addTab(m_tabs.value(1), QString());
        }
    }
}

void UIVMInformationDialog::prepareButtonBox()
{
    /* Create button-box: */
    m_pButtonBox = new QIDialogButtonBox;
    AssertPtrReturnVoid(m_pButtonBox);
    {
        /* Configure button-box: */
        m_pButtonBox->setStandardButtons(QDialogButtonBox::Close);
        m_pButtonBox->button(QDialogButtonBox::Close)->setShortcut(Qt::Key_Escape);
        connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(close()));
        /* Add button-box into main-layout: */
        centralWidget()->layout()->addWidget(m_pButtonBox);
    }
}

void UIVMInformationDialog::loadSettings()
{
    /* Restore window geometry: */
    {
        /* Load geometry: */
        m_geometry = gEDataManager->informationWindowGeometry(this, m_pMachineWindow, vboxGlobal().managedVMUuid());
#ifdef VBOX_WS_MAC
        move(m_geometry.topLeft());
        resize(m_geometry.size());
#else /* VBOX_WS_MAC */
        setGeometry(m_geometry);
#endif /* !VBOX_WS_MAC */
        LogRel2(("GUI: UIVMInformationDialog: Geometry loaded to: Origin=%dx%d, Size=%dx%d\n",
                 m_geometry.x(), m_geometry.y(), m_geometry.width(), m_geometry.height()));

        /* Maximize (if necessary): */
        if (gEDataManager->informationWindowShouldBeMaximized(vboxGlobal().managedVMUuid()))
            showMaximized();
    }
}

void UIVMInformationDialog::saveSettings()
{
    /* Save window geometry: */
    {
        /* Save geometry: */
#ifdef VBOX_WS_MAC
        gEDataManager->setInformationWindowGeometry(m_geometry, ::darwinIsWindowMaximized(this), vboxGlobal().managedVMUuid());
#else /* VBOX_WS_MAC */
        gEDataManager->setInformationWindowGeometry(m_geometry, isMaximized(), vboxGlobal().managedVMUuid());
#endif /* !VBOX_WS_MAC */
        LogRel2(("GUI: UIVMInformationDialog: Geometry saved as: Origin=%dx%d, Size=%dx%d\n",
                 m_geometry.x(), m_geometry.y(), m_geometry.width(), m_geometry.height()));
    }
}

void UIVMInformationDialog::cleanup()
{
    /* Save settings: */
    saveSettings();
}

