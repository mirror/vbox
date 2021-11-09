/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoCreator class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#include <QGridLayout>
#include <QMenuBar>
#include <QPushButton>
#include <QStyle>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "UIDesktopWidgetWatchdog.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "QIToolBar.h"
#include "UIVisoHostBrowser.h"
#include "UIVisoCreator.h"
#include "UIVisoConfigurationPanel.h"
#include "UIVisoCreatorOptionsPanel.h"
#include "UIVisoContentBrowser.h"
#include "UICommon.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils-darwin.h"
#endif

/*********************************************************************************************************************************
*   UIVisoCreatorWidget implementation.                                                                                        *
*********************************************************************************************************************************/

UIVisoCreatorWidget::UIVisoCreatorWidget(QWidget *pParent /* =0 */, const QString& strMachineName /* = QString() */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pActionConfiguration(0)
    , m_pActionOptions(0)
    , m_pAddAction(0)
    , m_pRemoveAction(0)
    , m_pNewDirectoryAction(0)
    , m_pRenameAction(0)
    , m_pResetAction(0)
    , m_pMainLayout(0)
    , m_pHostBrowser(0)
    , m_pVisoBrowser(0)
    , m_pToolBar(0)
    , m_pVerticalToolBar(0)
    , m_pMainMenu(0)
    , m_strMachineName(strMachineName)
    , m_pCreatorOptionsPanel(0)
    , m_pConfigurationPanel(0)
{
    m_visoOptions.m_strVisoName = !strMachineName.isEmpty() ? strMachineName : "ad-hoc";
    prepareActions();
    prepareWidgets();
    populateMenuMainToolbar();
    prepareConnections();
    manageEscapeShortCut();
    retranslateUi();
}

QStringList UIVisoCreatorWidget::entryList() const
{
    if (!m_pVisoBrowser)
        return QStringList();
    return m_pVisoBrowser->entryList();
}

const QString &UIVisoCreatorWidget::visoName() const
{
    return m_visoOptions.m_strVisoName;
}

const QStringList &UIVisoCreatorWidget::customOptions() const
{
    return m_visoOptions.m_customOptions;
}

QString UIVisoCreatorWidget::currentPath() const
{
    if (!m_pHostBrowser)
        return QString();
    return m_pHostBrowser->currentPath();
}

void UIVisoCreatorWidget::setCurrentPath(const QString &strPath)
{
    if (!m_pHostBrowser)
        return;
    m_pHostBrowser->setCurrentPath(strPath);
}

void UIVisoCreatorWidget::retranslateUi()
{
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

    if (m_pAddAction)
    {
        m_pAddAction->setToolTip(QApplication::translate("UIVisoCreator", "Add selected file objects to VISO"));
        m_pAddAction->setText(QApplication::translate("UIVisoCreator", "Add"));
    }

    if (m_pRemoveAction)
    {
        m_pRemoveAction->setToolTip(QApplication::translate("UIVisoCreator", "Remove selected file objects from VISO"));
        m_pRemoveAction->setText(QApplication::translate("UIVisoCreator", "Remove"));
    }
    if (m_pNewDirectoryAction)
    {
        m_pNewDirectoryAction->setToolTip(QApplication::translate("UIVisoCreator", "Create a new directory under the current location"));
        m_pNewDirectoryAction->setText(QApplication::translate("UIVisoCreator", "New Directory"));
    }
    if (m_pResetAction)
    {
        m_pResetAction->setToolTip(QApplication::translate("UIVisoCreator", "Reset VISO content."));
        m_pResetAction->setText(QApplication::translate("UIVisoCreator", "Reset"));
    }
    if (m_pRenameAction)
        m_pRenameAction->setToolTip(QApplication::translate("UIVisoCreator", "Rename the selected object"));
}

void UIVisoCreatorWidget::sltHandleAddObjectsToViso(QStringList pathList)
{
    if (m_pVisoBrowser)
        m_pVisoBrowser->addObjectsToViso(pathList);
}

void UIVisoCreatorWidget::sltPanelActionToggled(bool fChecked)
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

void UIVisoCreatorWidget::sltHandleVisoNameChanged(const QString &strVisoName)
{
    if (m_visoOptions.m_strVisoName == strVisoName)
        return;
    m_visoOptions.m_strVisoName = strVisoName;
    if(m_pVisoBrowser)
        m_pVisoBrowser->setVisoName(m_visoOptions.m_strVisoName);
}

void UIVisoCreatorWidget::sltHandleCustomVisoOptionsChanged(const QStringList &customVisoOptions)
{
    if (m_visoOptions.m_customOptions == customVisoOptions)
        return;
    m_visoOptions.m_customOptions = customVisoOptions;
}

void UIVisoCreatorWidget::sltHandleShowHiddenObjectsChange(bool fShow)
{
    if (m_browserOptions.m_fShowHiddenObjects == fShow)
        return;
    m_browserOptions.m_fShowHiddenObjects = fShow;
    m_pHostBrowser->showHideHiddenObjects(fShow);
}

void UIVisoCreatorWidget::sltHandleHidePanel(UIDialogPanel *pPanel)
{
    hidePanel(pPanel);
}

void UIVisoCreatorWidget::sltHandleBrowserTreeViewVisibilityChanged(bool fVisible)
{
    Q_UNUSED(fVisible);
    manageEscapeShortCut();
}

void UIVisoCreatorWidget::sltHandleHostBrowserTableSelectionChanged(bool fIsSelectionEmpty)
{
    if (m_pAddAction)
        m_pAddAction->setEnabled(!fIsSelectionEmpty);
}

void UIVisoCreatorWidget::sltHandleContentBrowserTableSelectionChanged(bool fIsSelectionEmpty)
{
    if (m_pRemoveAction)
        m_pRemoveAction->setEnabled(!fIsSelectionEmpty);
}

void UIVisoCreatorWidget::sltHandleShowContextMenu(const QWidget *pContextMenuRequester, const QPoint &point)
{
    if (!pContextMenuRequester)
        return;

    QMenu menu;

    if (sender() == m_pHostBrowser)
    {
        menu.addAction(m_pAddAction);
    }
    else if (sender() == m_pVisoBrowser)
    {
        menu.addAction(m_pRemoveAction);
        menu.addAction(m_pNewDirectoryAction);
        menu.addAction(m_pResetAction);
    }

    menu.exec(pContextMenuRequester->mapToGlobal(point));
}

void UIVisoCreatorWidget::prepareWidgets()
{
    m_pMainLayout = new QGridLayout(this);
    if (!m_pMainLayout)
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

    // m_pMainMenu = menuBar()->addMenu(tr("VISO"));

    // if (m_pActionConfiguration)
    //     m_pMainMenu->addAction(m_pActionConfiguration);
    // if (m_pActionOptions)
    //     m_pMainMenu->addAction(m_pActionOptions);

    // m_pToolBar = new QIToolBar;
    // if (m_pToolBar)
    // {
    //     /* Configure toolbar: */
    //     const int iIconMetric = (int)(QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize));
    //     m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
    //     m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    //     m_pMainLayout->addWidget(m_pToolBar, 0, 0, 1, 5);
    // }

    m_pHostBrowser = new UIVisoHostBrowser;
    if (m_pHostBrowser)
    {
        m_pMainLayout->addWidget(m_pHostBrowser, 0, 0, 1, 2);
        m_pMainLayout->setColumnStretch(m_pMainLayout->indexOf(m_pHostBrowser), 2);
    }

    prepareVerticalToolBar();
    if (m_pVerticalToolBar)
    {
        m_pMainLayout->addWidget(m_pVerticalToolBar, 0, 2, 1, 1);
        m_pMainLayout->setColumnStretch(m_pMainLayout->indexOf(m_pVerticalToolBar), 1);
    }

    m_pVisoBrowser = new UIVisoContentBrowser;
    if (m_pVisoBrowser)
    {
        m_pMainLayout->addWidget(m_pVisoBrowser, 0, 3, 1, 2);
        m_pVisoBrowser->setVisoName(m_visoOptions.m_strVisoName);
        m_pMainLayout->setColumnStretch(m_pMainLayout->indexOf(m_pVisoBrowser), 2);
    }

    m_pConfigurationPanel = new UIVisoConfigurationPanel(this);
    if (m_pConfigurationPanel)
    {
        m_pMainLayout->addWidget(m_pConfigurationPanel, 1, 0, 1, 5);
        m_pConfigurationPanel->hide();
        m_pConfigurationPanel->setVisoName(m_visoOptions.m_strVisoName);
        m_pConfigurationPanel->setVisoCustomOptions(m_visoOptions.m_customOptions);
    }

    m_pCreatorOptionsPanel = new UIVisoCreatorOptionsPanel;
    if (m_pCreatorOptionsPanel)
    {
        m_pCreatorOptionsPanel->setShowHiddenbjects(m_browserOptions.m_fShowHiddenObjects);
        m_pMainLayout->addWidget(m_pCreatorOptionsPanel, 2, 0, 1, 5);
        m_pCreatorOptionsPanel->hide();
    }
}

void UIVisoCreatorWidget::prepareConnections()
{
    if (m_pHostBrowser)
    {
        connect(m_pHostBrowser, &UIVisoHostBrowser::sigAddObjectsToViso,
                this, &UIVisoCreatorWidget::sltHandleAddObjectsToViso);
        connect(m_pHostBrowser, &UIVisoHostBrowser::sigTreeViewVisibilityChanged,
                this, &UIVisoCreatorWidget::sltHandleBrowserTreeViewVisibilityChanged);
        connect(m_pHostBrowser, &UIVisoHostBrowser::sigTableSelectionChanged,
                this, &UIVisoCreatorWidget::sltHandleHostBrowserTableSelectionChanged);
        connect(m_pHostBrowser, &UIVisoHostBrowser::sigCreateFileTableViewContextMenu,
                this, &UIVisoCreatorWidget::sltHandleShowContextMenu);
    }

    if (m_pVisoBrowser)
    {
        connect(m_pVisoBrowser, &UIVisoContentBrowser::sigTableSelectionChanged,
                this, &UIVisoCreatorWidget::sltHandleContentBrowserTableSelectionChanged);
        connect(m_pVisoBrowser, &UIVisoContentBrowser::sigCreateFileTableViewContextMenu,
                this, &UIVisoCreatorWidget::sltHandleShowContextMenu);
    }

    if (m_pActionConfiguration)
        connect(m_pActionConfiguration, &QAction::triggered, this, &UIVisoCreatorWidget::sltPanelActionToggled);
    if (m_pActionOptions)
        connect(m_pActionOptions, &QAction::triggered, this, &UIVisoCreatorWidget::sltPanelActionToggled);

    if (m_pConfigurationPanel)
    {
        connect(m_pConfigurationPanel, &UIVisoConfigurationPanel::sigVisoNameChanged,
                this, &UIVisoCreatorWidget::sltHandleVisoNameChanged);
        connect(m_pConfigurationPanel, &UIVisoConfigurationPanel::sigCustomVisoOptionsChanged,
                this, &UIVisoCreatorWidget::sltHandleCustomVisoOptionsChanged);
        connect(m_pConfigurationPanel, &UIVisoConfigurationPanel::sigHidePanel,
                this, &UIVisoCreatorWidget::sltHandleHidePanel);
        m_panelActionMap.insert(m_pConfigurationPanel, m_pActionConfiguration);
    }

    if (m_pCreatorOptionsPanel)
    {
        connect(m_pCreatorOptionsPanel, &UIVisoCreatorOptionsPanel::sigShowHiddenObjects,
                this, &UIVisoCreatorWidget::sltHandleShowHiddenObjectsChange);
        connect(m_pCreatorOptionsPanel, &UIVisoCreatorOptionsPanel::sigHidePanel,
                this, &UIVisoCreatorWidget::sltHandleHidePanel);
        m_panelActionMap.insert(m_pCreatorOptionsPanel, m_pActionOptions);
    }

    if (m_pAddAction)
        connect(m_pAddAction, &QAction::triggered,
                m_pHostBrowser, &UIVisoHostBrowser::sltHandleAddAction);

    if (m_pNewDirectoryAction)
        connect(m_pNewDirectoryAction, &QAction::triggered,
                m_pVisoBrowser, &UIVisoContentBrowser::sltHandleCreateNewDirectory);
    if (m_pRemoveAction)
        connect(m_pRemoveAction, &QAction::triggered,
                m_pVisoBrowser, &UIVisoContentBrowser::sltHandleRemoveItems);
    if (m_pResetAction)
        connect(m_pResetAction, &QAction::triggered,
                m_pVisoBrowser, &UIVisoContentBrowser::sltHandleResetAction);
    if (m_pRenameAction)
        connect(m_pRenameAction, &QAction::triggered,
                m_pVisoBrowser,&UIVisoContentBrowser::sltHandleItemRenameAction);
}

void UIVisoCreatorWidget::prepareActions()
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

    m_pAddAction = new QAction(this);
    if (m_pAddAction)
    {
        m_pAddAction->setIcon(UIIconPool::iconSetFull(":/file_manager_copy_to_guest_24px.png",
                                                      ":/file_manager_copy_to_guest_16px.png",
                                                      ":/file_manager_copy_to_guest_disabled_24px.png",
                                                      ":/file_manager_copy_to_guest_disabled_16px.png"));
        m_pAddAction->setText(QApplication::translate("UIVisoCreator", "Add"));
        m_pAddAction->setEnabled(false);
    }
    m_pRemoveAction = new QAction(this);
    if (m_pRemoveAction)
    {
        m_pRemoveAction->setIcon(UIIconPool::iconSetFull(":/file_manager_delete_24px.png", ":/file_manager_delete_16px.png",
                                                     ":/file_manager_delete_disabled_24px.png", ":/file_manager_delete_disabled_16px.png"));
        m_pRemoveAction->setEnabled(false);
    }

    m_pNewDirectoryAction = new QAction(this);
    if (m_pNewDirectoryAction)
    {
        m_pNewDirectoryAction->setIcon(UIIconPool::iconSetFull(":/file_manager_new_directory_24px.png", ":/file_manager_new_directory_16px.png",
                                                           ":/file_manager_new_directory_disabled_24px.png", ":/file_manager_new_directory_disabled_16px.png"));
        m_pNewDirectoryAction->setEnabled(true);
    }

    //m_pRenameAction = new QAction(this);
    if (m_pRenameAction)
    {
        /** @todo Handle rename correctly in the m_entryMap as well and then enable this rename action. */
        /* m_pVerticalToolBar->addAction(m_pRenameAction); */
        m_pRenameAction->setIcon(UIIconPool::iconSet(":/file_manager_rename_16px.png", ":/file_manager_rename_disabled_16px.png"));
        m_pRenameAction->setEnabled(false);
    }

    m_pResetAction = new QAction(this);
    if (m_pResetAction)
    {
        m_pResetAction->setIcon(UIIconPool::iconSet(":/cd_remove_16px.png", ":/cd_remove_disabled_16px.png"));
        m_pResetAction->setEnabled(true);
    }
}

void UIVisoCreatorWidget::populateMenuMainToolbar()
{
    if (!m_pMainMenu || !m_pToolBar)
        return;

    m_pToolBar->addAction(m_pActionConfiguration);
    m_pMainMenu->addAction(m_pActionConfiguration);

    m_pToolBar->addAction(m_pActionOptions);
    m_pMainMenu->addAction(m_pActionOptions);
    m_pMainMenu->addSeparator();
    m_pMainMenu->addAction(m_pAddAction);
    m_pMainMenu->addAction(m_pRemoveAction);
    m_pMainMenu->addAction(m_pNewDirectoryAction);
    m_pMainMenu->addAction(m_pResetAction);
}

void UIVisoCreatorWidget::hidePanel(UIDialogPanel* panel)
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

void UIVisoCreatorWidget::showPanel(UIDialogPanel* panel)
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

void UIVisoCreatorWidget::manageEscapeShortCut()
{
    // /* Take the escape key from m_pButtonBox and from the panels in case treeview(s) in
    //    host and/or content browser is open. We use the escape key to close those first: */
    // if ((m_pHostBrowser && m_pHostBrowser->isTreeViewVisible()) ||
    //     (m_pVisoBrowser && m_pVisoBrowser->isTreeViewVisible()))
    // {
    //     if (m_pButtonBox && m_pButtonBox->button(QDialogButtonBox::Cancel))
    //         m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(QKeySequence());
    //     for (int i = 0; i < m_visiblePanelsList.size(); ++i)
    //         m_visiblePanelsList[i]->setCloseButtonShortCut(QKeySequence());
    //     return;
    // }

    // /* if there are no visible panels then assign esc. key to cancel button of the button box: */
    // if (m_visiblePanelsList.isEmpty())
    // {
    //     if (m_pButtonBox && m_pButtonBox->button(QDialogButtonBox::Cancel))
    //         m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(QKeySequence(Qt::Key_Escape));
    //     return;
    // }
    // if (m_pButtonBox && m_pButtonBox->button(QDialogButtonBox::Cancel))
    //     m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(QKeySequence());

    /* Just loop thru the visible panel list and set the esc key to the
       panel which made visible latest */
    // for (int i = 0; i < m_visiblePanelsList.size() - 1; ++i)
    //     m_visiblePanelsList[i]->setCloseButtonShortCut(QKeySequence());
    // m_visiblePanelsList.back()->setCloseButtonShortCut(QKeySequence(Qt::Key_Escape));
}

void UIVisoCreatorWidget::prepareVerticalToolBar()
{
    m_pVerticalToolBar = new QIToolBar;
    if (!m_pVerticalToolBar)
        return;

    m_pVerticalToolBar->setOrientation(Qt::Vertical);

    /* Add to dummy QWidget to toolbar to center the action icons vertically: */
    QWidget *topSpacerWidget = new QWidget(this);
    topSpacerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    topSpacerWidget->setVisible(true);
    QWidget *bottomSpacerWidget = new QWidget(this);
    bottomSpacerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    bottomSpacerWidget->setVisible(true);

    m_pVerticalToolBar->addWidget(topSpacerWidget);
    m_pVerticalToolBar->addAction(m_pAddAction);
    m_pVerticalToolBar->addAction(m_pRemoveAction);
    m_pVerticalToolBar->addAction(m_pNewDirectoryAction);
    m_pVerticalToolBar->addAction(m_pResetAction);

    m_pVerticalToolBar->addWidget(bottomSpacerWidget);
}

/*********************************************************************************************************************************
*   UIVisoCreatorDialog implementation.                                                                                        *
*********************************************************************************************************************************/
UIVisoCreatorDialog::UIVisoCreatorDialog(QWidget *pParent /* = 0 */, const QString& strMachineName /* = QString() */)
    : QIWithRetranslateUI<QIMainDialog>(pParent)
    , m_strMachineName(strMachineName)
    , m_pVisoCreatorWidget(0)
    , m_pButtonBox(0)
{
    prepareWidgets();
    prepareConnections();
}

QStringList  UIVisoCreatorDialog::entryList() const
{
    if (m_pVisoCreatorWidget)
        return m_pVisoCreatorWidget->entryList();
    return QStringList();
}

QString UIVisoCreatorDialog::visoName() const
{
    if (m_pVisoCreatorWidget)
        return m_pVisoCreatorWidget->visoName();
    return QString();
}

QStringList UIVisoCreatorDialog::customOptions() const
{
    if (m_pVisoCreatorWidget)
        return m_pVisoCreatorWidget->customOptions();
    return QStringList();
}

QString UIVisoCreatorDialog::currentPath() const
{
    if (m_pVisoCreatorWidget)
        return m_pVisoCreatorWidget->currentPath();
    return QString();
}

void    UIVisoCreatorDialog::setCurrentPath(const QString &strPath)
{
    if (m_pVisoCreatorWidget)
        m_pVisoCreatorWidget->setCurrentPath(strPath);
}

void UIVisoCreatorDialog::prepareWidgets()
{
    QWidget *pCentralWidget = new QWidget;
    setCentralWidget(pCentralWidget);
    QVBoxLayout *pMainLayout = new QVBoxLayout;
    pCentralWidget->setLayout(pMainLayout);


    m_pVisoCreatorWidget = new UIVisoCreatorWidget(this, m_strMachineName);
    if (m_pVisoCreatorWidget)
    {
        pMainLayout->addWidget(m_pVisoCreatorWidget);
    }

    m_pButtonBox = new QIDialogButtonBox;
    if (m_pButtonBox)
    {
        m_pButtonBox->setStandardButtons(QDialogButtonBox::Help | QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
        m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(Qt::Key_Escape);
        pMainLayout->addWidget(m_pButtonBox);

        connect(m_pButtonBox->button(QIDialogButtonBox::Help), &QPushButton::pressed,
                &(msgCenter()), &UIMessageCenter::sltHandleHelpRequest);
        m_pButtonBox->button(QDialogButtonBox::Help)->setShortcut(QKeySequence::HelpContents);

        uiCommon().setHelpKeyword(m_pButtonBox->button(QIDialogButtonBox::Help), "viso");
    }
    retranslateUi();
}

void UIVisoCreatorDialog::prepareConnections()
{
    if (m_pButtonBox)
    {
        connect(m_pButtonBox, &QIDialogButtonBox::rejected, this, &UIVisoCreatorDialog::close);
        connect(m_pButtonBox, &QIDialogButtonBox::accepted, this, &UIVisoCreatorDialog::accept);
    }
}

void UIVisoCreatorDialog::retranslateUi()
{
    if (!m_strMachineName.isEmpty())
        setWindowTitle(QString("%1 - %2").arg(m_strMachineName).arg(tr("VISO Creator")));
    else
        setWindowTitle(QString("%1").arg(tr("VISO Creator")));
    if (m_pButtonBox && m_pButtonBox->button(QDialogButtonBox::Ok))
    {
        m_pButtonBox->button(QDialogButtonBox::Ok)->setText(QApplication::translate("UIVisoCreator", "C&reate"));
        m_pButtonBox->button(QDialogButtonBox::Ok)->setToolTip(QApplication::translate("UIVisoCreator", "Creates VISO file with the selected content"));
    }
    if (m_pButtonBox && m_pButtonBox->button(QDialogButtonBox::Help))
        m_pButtonBox->button(QDialogButtonBox::Help)->setToolTip(QApplication::translate("UIVisoCreator", "Opens the help browser and navigates to the related section"));
}
