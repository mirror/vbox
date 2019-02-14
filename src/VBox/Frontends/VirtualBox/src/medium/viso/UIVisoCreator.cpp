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
#include "UIVisoConfigurationPanel.h"
#include "UIVisoCreatorOptionsPanel.h"
#include "UIVisoContentBrowser.h"


UIVisoCreator::UIVisoCreator(QWidget *pParent /* =0 */, const QString& strMachineName /* = QString() */)
    : QIWithRetranslateUI<QIMainDialog>(pParent)
    , m_pMainLayout(0)
    , m_pHorizontalSplitter(0)
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
    , m_pCreatorOptionsPanel(0)
    , m_pConfigurationPanel(0)
{
    m_visoOptions.m_strVisoName = !strMachineName.isEmpty() ? strMachineName : "ad-hoc";
    prepareActions();
    prepareObjects();
    prepareConnections();
    manageEscapeShortCut();
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

QString UIVisoCreator::currentPath() const
{
    if (!m_pHostBrowser)
        return QString();
    return m_pHostBrowser->currentPath();
}

void UIVisoCreator::setCurrentPath(const QString &strPath)
{
    if (!m_pHostBrowser)
        return;
    m_pHostBrowser->setCurrentPath(strPath);
}

void UIVisoCreator::retranslateUi()
{
    if (!m_strMachineName.isEmpty())
        setWindowTitle(QString("%1 - %2").arg(m_strMachineName).arg(tr("VISO Creator")));
    else
        setWindowTitle(QString("%1").arg(tr("VISO Creator")));
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
        m_pMainMenu->setTitle(tr("VISO"));
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

void UIVisoCreator::sltPanelActionToggled(bool fChecked)
{
    QAction *pSenderAction = qobject_cast<QAction*>(sender());
    if (!pSenderAction)
        return;
    UIDialogPanel* pPanel = 0;
    /* Look for the sender() within the m_panelActionMap's values: */
    for (QMap<UIDialogPanel*, QAction*>::const_iterator iterator = m_panelActionMap.begin();
        iterator != m_panelActionMap.end(); ++iterator)
    {
        if (iterator.value() == pSenderAction)
            pPanel = iterator.key();
    }
    if (!pPanel)
        return;
    if (fChecked)
        showPanel(pPanel);
    else
        hidePanel(pPanel);
}

void UIVisoCreator::sltHandleVisoNameChanged(const QString &strVisoName)
{
    if (m_visoOptions.m_strVisoName == strVisoName)
        return;
    m_visoOptions.m_strVisoName = strVisoName;
    if(m_pVisoBrowser)
        m_pVisoBrowser->setVisoName(m_visoOptions.m_strVisoName);
}

void UIVisoCreator::sltHandleCustomVisoOptionsChanged(const QStringList &customVisoOptions)
{
    if (m_visoOptions.m_customOptions == customVisoOptions)
        return;
    m_visoOptions.m_customOptions = customVisoOptions;
}

void UIVisoCreator::sltHandleShowHiddenObjectsChange(bool fShow)
{
    if (m_browserOptions.m_fShowHiddenObjects == fShow)
        return;
    m_browserOptions.m_fShowHiddenObjects = fShow;
    m_pHostBrowser->showHideHiddenObjects(fShow);
}

void UIVisoCreator::sltHandleHidePanel(UIDialogPanel *pPanel)
{
    hidePanel(pPanel);
}

void UIVisoCreator::sltHandleBrowserTreeViewVisibilityChanged(bool fVisible)
{
    Q_UNUSED(fVisible);
    manageEscapeShortCut();
}

void UIVisoCreator::prepareObjects()
{
    m_pCentralWidget = new QWidget;
    if (!m_pCentralWidget)
        return;
    setCentralWidget(m_pCentralWidget);

    m_pMainLayout = new QVBoxLayout;
    m_pCentralWidget->setLayout(m_pMainLayout);
    if (!m_pMainLayout || !menuBar())
        return;

    /* Configure layout: */
    const int iL = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 2;
    const int iT = qApp->style()->pixelMetric(QStyle::PM_LayoutTopMargin) / 2;
    const int iR = qApp->style()->pixelMetric(QStyle::PM_LayoutRightMargin) / 2;
    const int iB = qApp->style()->pixelMetric(QStyle::PM_LayoutBottomMargin) / 2;
    m_pMainLayout->setContentsMargins(iL, iT, iR, iB);
#ifdef VBOX_WS_MAC
    m_pMainLayout->setSpacing(10);
#else
    m_pMainLayout->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing) / 2);
#endif

    m_pMainMenu = menuBar()->addMenu(tr("VISO"));
    if (m_pActionConfiguration)
        m_pMainMenu->addAction(m_pActionConfiguration);
    if (m_pActionOptions)
        m_pMainMenu->addAction(m_pActionOptions);
    m_pHostBrowserMenu = m_pMainMenu->addMenu(tr("Host Browser"));
    m_pVisoContentBrowserMenu = m_pMainMenu->addMenu(tr("VISO Browser"));

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

    m_pHorizontalSplitter = new QSplitter;
    if (!m_pHorizontalSplitter)
        return;

    m_pMainLayout->addWidget(m_pHorizontalSplitter);
    /* Make sure m_pHorizontalSplitter takes all the extra space: */
    m_pMainLayout->setStretch(m_pMainLayout->indexOf(m_pHorizontalSplitter), 2);
    m_pHorizontalSplitter->setOrientation(Qt::Horizontal);
    m_pHorizontalSplitter->setHandleWidth(1);

    m_pHostBrowser = new UIVisoHostBrowser(0 /* parent */, m_pHostBrowserMenu);
    if (m_pHostBrowser)
    {
        m_pHorizontalSplitter->addWidget(m_pHostBrowser);
        connect(m_pHostBrowser, &UIVisoHostBrowser::sigAddObjectsToViso,
                this, &UIVisoCreator::sltHandleAddObjectsToViso);
        connect(m_pHostBrowser, &UIVisoHostBrowser::sigTreeViewVisibilityChanged,
                this, &UIVisoCreator::sltHandleBrowserTreeViewVisibilityChanged);
    }

    m_pVisoBrowser = new UIVisoContentBrowser(0 /* parent */, m_pVisoContentBrowserMenu);
    if (m_pVisoBrowser)
    {
        m_pHorizontalSplitter->addWidget(m_pVisoBrowser);
        m_pVisoBrowser->setVisoName(m_visoOptions.m_strVisoName);
    }

    m_pConfigurationPanel = new UIVisoConfigurationPanel(this);
    if (m_pConfigurationPanel)
    {
        m_pMainLayout->addWidget(m_pConfigurationPanel);
        m_panelActionMap.insert(m_pConfigurationPanel, m_pActionConfiguration);
        m_pConfigurationPanel->hide();
        m_pConfigurationPanel->setVisoName(m_visoOptions.m_strVisoName);
        m_pConfigurationPanel->setVisoCustomOptions(m_visoOptions.m_customOptions);
        installEventFilter(m_pConfigurationPanel);
    }

    m_pCreatorOptionsPanel = new UIVisoCreatorOptionsPanel(this);
    if (m_pCreatorOptionsPanel)
    {
        m_pCreatorOptionsPanel->setShowHiddenbjects(m_browserOptions.m_fShowHiddenObjects);
        m_pMainLayout->addWidget(m_pCreatorOptionsPanel);
        m_panelActionMap.insert(m_pCreatorOptionsPanel, m_pActionOptions);
        m_pCreatorOptionsPanel->hide();
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
        connect(m_pActionConfiguration, &QAction::triggered, this, &UIVisoCreator::sltPanelActionToggled);
    if (m_pActionOptions)
        connect(m_pActionOptions, &QAction::triggered, this, &UIVisoCreator::sltPanelActionToggled);

    if (m_pConfigurationPanel)
    {
        connect(m_pConfigurationPanel, &UIVisoConfigurationPanel::sigVisoNameChanged,
                this, &UIVisoCreator::sltHandleVisoNameChanged);
        connect(m_pConfigurationPanel, &UIVisoConfigurationPanel::sigCustomVisoOptionsChanged,
                this, &UIVisoCreator::sltHandleCustomVisoOptionsChanged);
        connect(m_pConfigurationPanel, &UIVisoConfigurationPanel::sigHidePanel,
                this, &UIVisoCreator::sltHandleHidePanel);
    }
    if (m_pCreatorOptionsPanel)
    {
        connect(m_pCreatorOptionsPanel, &UIVisoCreatorOptionsPanel::sigShowHiddenObjects,
                this, &UIVisoCreator::sltHandleShowHiddenObjectsChange);
        connect(m_pCreatorOptionsPanel, &UIVisoCreatorOptionsPanel::sigHidePanel,
                this, &UIVisoCreator::sltHandleHidePanel);
    }
}

void UIVisoCreator::prepareActions()
{
    m_pActionConfiguration = new QAction(this);
    if (m_pActionConfiguration)
    {
        m_pActionConfiguration->setCheckable(true);
        m_pActionConfiguration->setIcon(UIIconPool::iconSetFull(":/file_manager_options_32px.png",
                                                          ":/%file_manager_options_16px.png",
                                                          ":/file_manager_options_disabled_32px.png",
                                                          ":/file_manager_options_disabled_16px.png"));
    }

    m_pActionOptions = new QAction(this);
    if (m_pActionOptions)
    {
        m_pActionOptions->setCheckable(true);

        m_pActionOptions->setIcon(UIIconPool::iconSetFull(":/file_manager_options_32px.png",
                                                          ":/%file_manager_options_16px.png",
                                                          ":/file_manager_options_disabled_32px.png",
                                                          ":/file_manager_options_disabled_16px.png"));
    }
}


void UIVisoCreator::hidePanel(UIDialogPanel* panel)
{
    if (panel && panel->isVisible())
        panel->setVisible(false);
    QMap<UIDialogPanel*, QAction*>::iterator iterator = m_panelActionMap.find(panel);
    if (iterator != m_panelActionMap.end())
    {
        if (iterator.value() && iterator.value()->isChecked())
            iterator.value()->setChecked(false);
    }
    m_visiblePanelsList.removeAll(panel);
    manageEscapeShortCut();
}

void UIVisoCreator::showPanel(UIDialogPanel* panel)
{
    if (panel && panel->isHidden())
        panel->setVisible(true);
    QMap<UIDialogPanel*, QAction*>::iterator iterator = m_panelActionMap.find(panel);
    if (iterator != m_panelActionMap.end())
    {
        if (!iterator.value()->isChecked())
            iterator.value()->setChecked(true);
    }
    if (!m_visiblePanelsList.contains(panel))
        m_visiblePanelsList.push_back(panel);
    manageEscapeShortCut();
}

void UIVisoCreator::manageEscapeShortCut()
{
    /* Take the escape key from m_pButtonBox and from the panels in case treeview(s) in
       host and/or content browser is open. We use the escape key to close those first: */
    if ((m_pHostBrowser && m_pHostBrowser->isTreeViewVisible()) ||
        (m_pVisoBrowser && m_pVisoBrowser->isTreeViewVisible()))
    {
        if (m_pButtonBox && m_pButtonBox->button(QDialogButtonBox::Cancel))
            m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(QKeySequence());
        for (int i = 0; i < m_visiblePanelsList.size(); ++i)
            m_visiblePanelsList[i]->setCloseButtonShortCut(QKeySequence());
        return;
    }

    /* if there are no visible panels then assign esc. key to cancel button of the button box: */
    if (m_visiblePanelsList.isEmpty())
    {
        if (m_pButtonBox && m_pButtonBox->button(QDialogButtonBox::Cancel))
            m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(QKeySequence(Qt::Key_Escape));
        return;
    }
    if (m_pButtonBox && m_pButtonBox->button(QDialogButtonBox::Cancel))
        m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(QKeySequence());

    /* Just loop thru the visible panel list and set the esc key to the
       panel which made visible latest */
    for (int i = 0; i < m_visiblePanelsList.size() - 1; ++i)
        m_visiblePanelsList[i]->setCloseButtonShortCut(QKeySequence());
    m_visiblePanelsList.back()->setCloseButtonShortCut(QKeySequence(Qt::Key_Escape));
}
