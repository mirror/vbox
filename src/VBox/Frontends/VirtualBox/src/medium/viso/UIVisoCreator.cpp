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
#include "UIActionPool.h"
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

#include <iprt/getopt.h>


/*********************************************************************************************************************************
*   UIVisoCreatorWidget implementation.                                                                                          *
*********************************************************************************************************************************/

UIVisoCreatorWidget::UIVisoCreatorWidget(UIActionPool *pActionPool, QWidget *pParent,
                                         bool fShowToolBar,const QString& strMachineName /* = QString() */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pActionConfiguration(0)
    , m_pActionOptions(0)
    , m_pAddAction(0)
    , m_pRemoveAction(0)
    , m_pCreateNewDirectoryAction(0)
    , m_pRenameAction(0)
    , m_pResetAction(0)
    , m_pMainLayout(0)
    , m_pHostBrowser(0)
    , m_pVISOContentBrowser(0)
    , m_pToolBar(0)
    , m_pVerticalToolBar(0)
    , m_pMainMenu(0)
    , m_strMachineName(strMachineName)
    , m_pCreatorOptionsPanel(0)
    , m_pConfigurationPanel(0)
    , m_pActionPool(pActionPool)
    , m_fShowToolBar(fShowToolBar)
{
    m_visoOptions.m_strVisoName = !strMachineName.isEmpty() ? strMachineName : "ad-hoc";
    prepareWidgets();
    populateMenuMainToolbar();
    prepareConnections();
    manageEscapeShortCut();
    retranslateUi();
}

QStringList UIVisoCreatorWidget::entryList() const
{
    if (!m_pVISOContentBrowser)
        return QStringList();
    return m_pVISOContentBrowser->entryList();
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

QMenu *UIVisoCreatorWidget::menu() const
{
    return m_pMainMenu;
}

void UIVisoCreatorWidget::retranslateUi()
{
    if (m_pHostBrowser)
        m_pHostBrowser->setTitle(UIVisoCreatorWidget::tr("Host File System"));
    if (m_pVISOContentBrowser)
        m_pVISOContentBrowser->setTitle(UIVisoCreatorWidget::tr("VISO Content"));
}

void UIVisoCreatorWidget::sltHandleAddObjectsToViso(QStringList pathList)
{
    if (m_pVISOContentBrowser)
        m_pVISOContentBrowser->addObjectsToViso(pathList);
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
    if(m_pVISOContentBrowser)
        m_pVISOContentBrowser->setVisoName(m_visoOptions.m_strVisoName);
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
    else if (sender() == m_pVISOContentBrowser)
    {
        menu.addAction(m_pRemoveAction);
        menu.addAction(m_pCreateNewDirectoryAction);
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

    if (m_pActionPool && m_pActionPool->action(UIActionIndex_M_VISOCreator))
        m_pMainMenu = m_pActionPool->action(UIActionIndex_M_VISOCreator)->menu();
    int iLayoutRow = 0;
    if (m_fShowToolBar)
    {
        m_pToolBar = new QIToolBar(parentWidget());
        if (m_pToolBar)
        {
            /* Configure toolbar: */
            const int iIconMetric = (int)(QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize));
            m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
            m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            m_pMainLayout->addWidget(m_pToolBar, iLayoutRow++, 0, 1, 5);
        }
    }

    m_pHostBrowser = new UIVisoHostBrowser;
    if (m_pHostBrowser)
    {
        m_pMainLayout->addWidget(m_pHostBrowser, iLayoutRow, 0, 1, 4);
        //m_pMainLayout->setColumnStretch(m_pMainLayout->indexOf(m_pHostBrowser), 2);
    }

    prepareVerticalToolBar();
    if (m_pVerticalToolBar)
    {
        m_pMainLayout->addWidget(m_pVerticalToolBar, iLayoutRow, 4, 1, 1);
        //m_pMainLayout->setColumnStretch(m_pMainLayout->indexOf(m_pVerticalToolBar), 1);
    }

    m_pVISOContentBrowser = new UIVisoContentBrowser;
    if (m_pVISOContentBrowser)
    {
        m_pMainLayout->addWidget(m_pVISOContentBrowser, iLayoutRow, 5, 1, 4);
        m_pVISOContentBrowser->setVisoName(m_visoOptions.m_strVisoName);
        //m_pMainLayout->setColumnStretch(m_pMainLayout->indexOf(m_pVISOContentBrowser), 2);
    }
    ++iLayoutRow;
    m_pConfigurationPanel = new UIVisoConfigurationPanel(this);
    if (m_pConfigurationPanel)
    {
        m_pMainLayout->addWidget(m_pConfigurationPanel, iLayoutRow++, 0, 1, 9);
        m_pConfigurationPanel->hide();
        m_pConfigurationPanel->setVisoName(m_visoOptions.m_strVisoName);
        m_pConfigurationPanel->setVisoCustomOptions(m_visoOptions.m_customOptions);
    }

    m_pCreatorOptionsPanel = new UIVisoCreatorOptionsPanel;
    if (m_pCreatorOptionsPanel)
    {
        m_pCreatorOptionsPanel->setShowHiddenbjects(m_browserOptions.m_fShowHiddenObjects);
        m_pMainLayout->addWidget(m_pCreatorOptionsPanel, iLayoutRow++, 0, 1, 9);
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

    if (m_pVISOContentBrowser)
    {
        connect(m_pVISOContentBrowser, &UIVisoContentBrowser::sigTableSelectionChanged,
                this, &UIVisoCreatorWidget::sltHandleContentBrowserTableSelectionChanged);
        connect(m_pVISOContentBrowser, &UIVisoContentBrowser::sigCreateFileTableViewContextMenu,
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

    if (m_pCreateNewDirectoryAction)
        connect(m_pCreateNewDirectoryAction, &QAction::triggered,
                m_pVISOContentBrowser, &UIVisoContentBrowser::sltHandleCreateNewDirectory);
    if (m_pRemoveAction)
        connect(m_pRemoveAction, &QAction::triggered,
                m_pVISOContentBrowser, &UIVisoContentBrowser::sltHandleRemoveItems);
    if (m_pResetAction)
        connect(m_pResetAction, &QAction::triggered,
                m_pVISOContentBrowser, &UIVisoContentBrowser::sltHandleResetAction);
    if (m_pRenameAction)
        connect(m_pRenameAction, &QAction::triggered,
                m_pVISOContentBrowser,&UIVisoContentBrowser::sltHandleItemRenameAction);
}

void UIVisoCreatorWidget::prepareActions()
{
    if (!m_pActionPool)
        return;

    m_pActionConfiguration = m_pActionPool->action(UIActionIndex_M_VISOCreator_ToggleConfigPanel);
    m_pActionOptions = m_pActionPool->action(UIActionIndex_M_VISOCreator_ToggleOptionsPanel);

    m_pAddAction = m_pActionPool->action(UIActionIndex_M_VISOCreator_Add);
    m_pRemoveAction = m_pActionPool->action(UIActionIndex_M_VISOCreator_Remove);
    m_pCreateNewDirectoryAction = m_pActionPool->action(UIActionIndex_M_VISOCreator_CreateNewDirectory);
    m_pRenameAction = m_pActionPool->action(UIActionIndex_M_VISOCreator_Rename);
    m_pResetAction = m_pActionPool->action(UIActionIndex_M_VISOCreator_Reset);
}

void UIVisoCreatorWidget::populateMenuMainToolbar()
{
    prepareActions();
    if (m_pToolBar)
    {
        if (m_pActionConfiguration)
            m_pToolBar->addAction(m_pActionConfiguration);
        if (m_pActionOptions)
            m_pToolBar->addAction(m_pActionOptions);
    }
    if (m_pMainMenu)
    {
        m_pMainMenu->addAction(m_pActionConfiguration);
        m_pMainMenu->addAction(m_pActionOptions);
        m_pMainMenu->addSeparator();
        m_pMainMenu->addAction(m_pAddAction);
        m_pMainMenu->addAction(m_pRemoveAction);
        m_pMainMenu->addAction(m_pCreateNewDirectoryAction);
        m_pMainMenu->addAction(m_pResetAction);
    }

    if (m_pVerticalToolBar)
    {
        /* Add to dummy QWidget to toolbar to center the action icons vertically: */
        QWidget *topSpacerWidget = new QWidget(this);
        topSpacerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        topSpacerWidget->setVisible(true);
        QWidget *bottomSpacerWidget = new QWidget(this);
        bottomSpacerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        bottomSpacerWidget->setVisible(true);

        m_pVerticalToolBar->addWidget(topSpacerWidget);
        if (m_pAddAction)
            m_pVerticalToolBar->addAction(m_pAddAction);
        if (m_pRemoveAction)
            m_pVerticalToolBar->addAction(m_pRemoveAction);
        if (m_pCreateNewDirectoryAction)
            m_pVerticalToolBar->addAction(m_pCreateNewDirectoryAction);
        if (m_pResetAction)
            m_pVerticalToolBar->addAction(m_pResetAction);

        m_pVerticalToolBar->addWidget(bottomSpacerWidget);
    }
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
    //     (m_pVISOContentBrowser && m_pVISOContentBrowser->isTreeViewVisible()))
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
}

/* static */
int UIVisoCreatorWidget::visoWriteQuotedString(PRTSTREAM pStrmDst, const char *pszPrefix,
                                               QString const &rStr, const char *pszPostFix)
{
    QByteArray const utf8Array   = rStr.toUtf8();
    const char      *apszArgv[2] = { utf8Array.constData(), NULL };
    char            *pszQuoted;
    int vrc = RTGetOptArgvToString(&pszQuoted, apszArgv, RTGETOPTARGV_CNV_QUOTE_BOURNE_SH);
    if (RT_SUCCESS(vrc))
    {
        if (pszPrefix)
            vrc = RTStrmPutStr(pStrmDst, pszPrefix);
        if (RT_SUCCESS(vrc))
        {
            vrc = RTStrmPutStr(pStrmDst, pszQuoted);
            if (pszPostFix && RT_SUCCESS(vrc))
                vrc = RTStrmPutStr(pStrmDst, pszPostFix);
        }
        RTStrFree(pszQuoted);
    }
    return vrc;
}


/*********************************************************************************************************************************
*   UIVisoCreatorDialog implementation.                                                                                          *
*********************************************************************************************************************************/
UIVisoCreatorDialog::UIVisoCreatorDialog(UIActionPool *pActionPool, QWidget *pParent, const QString& strMachineName /* = QString() */)
    : QIWithRetranslateUI<QIMainDialog>(pParent)
    , m_strMachineName(strMachineName)
    , m_pVisoCreatorWidget(0)
    , m_pButtonBox(0)
    , m_pActionPool(pActionPool)
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


    m_pVisoCreatorWidget = new UIVisoCreatorWidget(m_pActionPool, this, true /* show toolbar */, m_strMachineName);
    if (m_pVisoCreatorWidget)
    {
        menuBar()->addMenu(m_pVisoCreatorWidget->menu());
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
        setWindowTitle(QString("%1 - %2").arg(m_strMachineName).arg(UIVisoCreatorWidget::tr("VISO Creator")));
    else
        setWindowTitle(QString("%1").arg(UIVisoCreatorWidget::tr("VISO Creator")));
    if (m_pButtonBox && m_pButtonBox->button(QDialogButtonBox::Ok))
    {
        m_pButtonBox->button(QDialogButtonBox::Ok)->setText(UIVisoCreatorWidget::tr("C&reate"));
        m_pButtonBox->button(QDialogButtonBox::Ok)->setToolTip(UIVisoCreatorWidget::tr("Creates VISO file with the selected content"));
    }
    if (m_pButtonBox && m_pButtonBox->button(QDialogButtonBox::Help))
        m_pButtonBox->button(QDialogButtonBox::Help)->setToolTip(UIVisoCreatorWidget::tr("Opens the help browser and navigates to the related section"));
}
