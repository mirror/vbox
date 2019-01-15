/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoCreator class implementation.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
#include <QMenuBar>
#include <QPushButton>
#include <QSplitter>
#include <QStyle>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "UIIconPool.h"
#include "UIToolBar.h"
#include "UIVisoHostBrowser.h"
#include "UIVisoCreator.h"
#include "UIVisoConfigurationDialog.h"
#include "UIVisoCreatorOptionsDialog.h"
#include "UIVisoContentBrowser.h"


UIVisoCreator::UIVisoCreator(QWidget *pParent /* =0 */, const QString& strMachineName /* = QString() */)
    : QIWithRetranslateUI<QIMainDialog>(pParent)
    , m_pMainLayout(0)
    , m_pVerticalSplitter(0)
    , m_pHostBrowser(0)
    , m_pVisoBrowser(0)
    , m_pButtonBox(0)
    , m_pToolBar(0)
    , m_pActionConfiguration(0)
    , m_pActionOptions(0)
    , m_pCentralWidget(0)
    , m_pMainMenu(0)
    , m_pHostBrowserMenu(0)
    , m_pVisoContentBrowserMenu(0)
    , m_strMachineName(strMachineName)
{
    prepareActions();
    prepareObjects();
    prepareConnections();
}

UIVisoCreator::~UIVisoCreator()
{
}

QStringList UIVisoCreator::entryList() const
{
    if (!m_pVisoBrowser)
        return QStringList();
    return m_pVisoBrowser->entryList();
}

const QString &UIVisoCreator::visoName() const
{
    return m_visoOptions.m_strVisoName;
}

const QStringList &UIVisoCreator::customOptions() const
{
    return m_visoOptions.m_customOptions;
}

void UIVisoCreator::retranslateUi()
{
    setWindowTitle(QString("%1 - %2").arg(m_strMachineName).arg(tr("VISO Creator")));
    if (m_pActionConfiguration)
    {
        m_pActionConfiguration->setText(tr("&Configuration..."));
        m_pActionConfiguration->setToolTip(tr("VISO Configuration"));
        m_pActionConfiguration->setStatusTip(tr("Manage VISO Configuration"));
    }
    if (m_pActionOptions)
    {
        m_pActionOptions->setText(tr("&Options..."));
        m_pActionOptions->setToolTip(tr("Dialog Options"));
        m_pActionOptions->setStatusTip(tr("Manage Dialog Options"));
    }
    if (m_pMainMenu)
        m_pMainMenu->setTitle(tr("Main Menu"));
    if (m_pHostBrowserMenu)
        m_pHostBrowserMenu->setTitle(tr("Host Browser"));
    if (m_pVisoContentBrowserMenu)
        m_pVisoContentBrowserMenu->setTitle(tr("VISO Browser"));
}

void UIVisoCreator::sltHandleAddObjectsToViso(QStringList pathList)
{
    if (m_pVisoBrowser)
        m_pVisoBrowser->addObjectsToViso(pathList);
}

void UIVisoCreator::sltHandleOptionsAction()
{
    UIVisoCreatorOptionsDialog *pDialog = new UIVisoCreatorOptionsDialog(m_browserOptions, this);

    if(!pDialog)
        return;
    if (pDialog->execute(true, false))
    {
        /** Check if any of the options has been modified: */
        checkBrowserOptions(pDialog->browserOptions());
    }
    delete pDialog;
}

void UIVisoCreator::sltHandleConfigurationAction()
{
    UIVisoConfigurationDialog *pDialog = new UIVisoConfigurationDialog(m_visoOptions, this);

    if(!pDialog)
        return;
    if (pDialog->execute(true, false))
    {
        /** Check if any of the options has been modified: */
        checkVisoOptions(pDialog->visoOptions());
    }
    delete pDialog;
}

void UIVisoCreator::prepareObjects()
{
    m_pCentralWidget = new QWidget;
    if (!m_pCentralWidget)
        return;
    setCentralWidget(m_pCentralWidget);

    m_pMainLayout = new QVBoxLayout;
        m_pCentralWidget->setLayout(m_pMainLayout);
    if (!m_pMainLayout)
        return;

    QMenuBar *pMenuBar = new QMenuBar;

    setMenuBar(pMenuBar);
    if (pMenuBar)
    {
        m_pMainMenu = pMenuBar->addMenu(tr("Main Menu"));
        if (m_pActionConfiguration)
            m_pMainMenu->addAction(m_pActionConfiguration);
        if (m_pActionOptions)
            m_pMainMenu->addAction(m_pActionOptions);
        m_pHostBrowserMenu = m_pMainMenu->addMenu(tr("Host Browser"));
        m_pVisoContentBrowserMenu = m_pMainMenu->addMenu(tr("VISO Browser"));
    }
    m_pToolBar = new UIToolBar(parentWidget());
    if (m_pToolBar)
    {
        /* Configure toolbar: */
        const int iIconMetric = (int)(QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize));
        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        /* Add toolbar actions: */
        if (m_pActionConfiguration)
            m_pToolBar->addAction(m_pActionConfiguration);
        if (m_pActionOptions)
            m_pToolBar->addAction(m_pActionOptions);

        m_pMainLayout->addWidget(m_pToolBar);
    }

    m_pVerticalSplitter = new QSplitter;
    if (!m_pVerticalSplitter)
        return;

    m_pMainLayout->addWidget(m_pVerticalSplitter);
    m_pVerticalSplitter->setOrientation(Qt::Vertical);
    m_pVerticalSplitter->setHandleWidth(1);

    m_pHostBrowser = new UIVisoHostBrowser(0 /* parent */, m_pHostBrowserMenu);
    if (m_pHostBrowser)
    {
        m_pVerticalSplitter->addWidget(m_pHostBrowser);
        connect(m_pHostBrowser, &UIVisoHostBrowser::sigAddObjectsToViso,
                this, &UIVisoCreator::sltHandleAddObjectsToViso);
    }
    m_pVisoBrowser = new UIVisoContentBrowser(0 /* parent */, m_pVisoContentBrowserMenu);
    if (m_pVisoBrowser)
    {
        m_pVerticalSplitter->addWidget(m_pVisoBrowser);
        m_pVisoBrowser->setVisoName(m_visoOptions.m_strVisoName);
    }
    m_pButtonBox = new QIDialogButtonBox;
    if (m_pButtonBox)
    {
        m_pButtonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
        m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(Qt::Key_Escape);
        m_pMainLayout->addWidget(m_pButtonBox);
    }
    retranslateUi();
}

void UIVisoCreator::prepareConnections()
{
    if (m_pButtonBox)
    {
        connect(m_pButtonBox, &QIDialogButtonBox::rejected, this, &UIVisoCreator::close);
        connect(m_pButtonBox, &QIDialogButtonBox::accepted, this, &UIVisoCreator::accept);
    }
    if (m_pActionConfiguration)
        connect(m_pActionConfiguration, &QAction::triggered, this, &UIVisoCreator::sltHandleConfigurationAction);
    if (m_pActionOptions)
        connect(m_pActionOptions, &QAction::triggered, this, &UIVisoCreator::sltHandleOptionsAction);
}

void UIVisoCreator::prepareActions()
{
    m_pActionConfiguration = new QAction(this);
    if (m_pActionConfiguration)
    {
        m_pActionConfiguration->setIcon(UIIconPool::iconSetFull(":/file_manager_options_32px.png",
                                                          ":/%file_manager_options_16px.png",
                                                          ":/file_manager_options_disabled_32px.png",
                                                          ":/file_manager_options_disabled_16px.png"));
    }

    m_pActionOptions = new QAction(this);
    if (m_pActionOptions)
    {
        m_pActionOptions->setIcon(UIIconPool::iconSetFull(":/file_manager_options_32px.png",
                                                          ":/%file_manager_options_16px.png",
                                                          ":/file_manager_options_disabled_32px.png",
                                                          ":/file_manager_options_disabled_16px.png"));
    }
}

void UIVisoCreator::checkBrowserOptions(const BrowserOptions &browserOptions)
{
    if (browserOptions == m_browserOptions)
        return;
    if (browserOptions.m_bShowHiddenObjects != m_browserOptions.m_bShowHiddenObjects)
    {
        if (m_pHostBrowser)
            m_pHostBrowser->showHideHiddenObjects(browserOptions.m_bShowHiddenObjects);
        if(m_pVisoBrowser)
            m_pVisoBrowser->showHideHiddenObjects(browserOptions.m_bShowHiddenObjects);
    }
    m_browserOptions = browserOptions;
}

void UIVisoCreator::checkVisoOptions(const VisoOptions &visoOptions)
{
    if (visoOptions == m_visoOptions)
        return;
    if (visoOptions.m_strVisoName != m_visoOptions.m_strVisoName)
    {
        if(m_pVisoBrowser)
            m_pVisoBrowser->setVisoName(visoOptions.m_strVisoName);
    }
    m_visoOptions = visoOptions;
}
