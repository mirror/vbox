/* $Id$ */
/** @file
 * VBox Qt GUI - UIFileManager class implementation.
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
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
#include <QHBoxLayout>
#include <QPushButton>
#include <QSplitter>

/* GUI includes: */
#include "QITabWidget.h"
#include "QITreeWidget.h"
#include "QIToolBar.h"
#include "UIActionPool.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIErrorString.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIFileManager.h"
#include "UIFileManagerGuestSessionPanel.h"
#include "UIFileManagerOptionsPanel.h"
#include "UIFileManagerLogPanel.h"
#include "UIFileManagerOperationsPanel.h"
#include "UIFileManagerGuestTable.h"
#include "UIFileManagerHostTable.h"
#include "UIGuestControlInterface.h"
#include "UIVirtualMachineItem.h"

/* COM includes: */
#include "CConsole.h"
#include "CFsObjInfo.h"
#include "CGuestDirectory.h"
#include "CGuestFsObjInfo.h"
#include "CGuestSession.h"
#include "CGuestSessionStateChangedEvent.h"


/*********************************************************************************************************************************
*   UIFileOperationsList definition.                                                                                   *
*********************************************************************************************************************************/

class UIFileOperationsList : public QITreeWidget
{
    Q_OBJECT;
public:

    UIFileOperationsList(QWidget *pParent = 0);
};


/*********************************************************************************************************************************
*   UIFileManagerOptions implementation.                                                                             *
*********************************************************************************************************************************/

UIFileManagerOptions *UIFileManagerOptions::m_pInstance = 0;

UIFileManagerOptions* UIFileManagerOptions::instance()
{
    if (!m_pInstance)
    m_pInstance = new UIFileManagerOptions;
    return m_pInstance;
}

void UIFileManagerOptions::create()
{
    if (m_pInstance)
        return;
    m_pInstance = new UIFileManagerOptions;
}

void UIFileManagerOptions::destroy()
{
    delete m_pInstance;
    m_pInstance = 0;
}

 UIFileManagerOptions::~UIFileManagerOptions()
{
}

UIFileManagerOptions::UIFileManagerOptions()
    : fListDirectoriesOnTop(true)
    , fAskDeleteConfirmation(false)
    , fShowHumanReadableSizes(true)
    , fShowHiddenObjects(true)
{
}

/*********************************************************************************************************************************
*   UIFileOperationsList implementation.                                                                                   *
*********************************************************************************************************************************/

UIFileOperationsList::UIFileOperationsList(QWidget *pParent)
    :QITreeWidget(pParent)
{}


/*********************************************************************************************************************************
*   UIFileManager implementation.                                                                                    *
*********************************************************************************************************************************/

UIFileManager::UIFileManager(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                             const CMachine &comMachine, QWidget *pParent, bool fShowToolbar)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pMainLayout(0)
    , m_pVerticalSplitter(0)
    , m_pFileTableSplitter(0)
    , m_pToolBar(0)
    , m_pVerticalToolBar(0)\
    , m_pHostFileTable(0)
    , m_pGuestTablesContainer(0)
    , m_enmEmbedding(enmEmbedding)
    , m_pActionPool(pActionPool)
    , m_fShowToolbar(fShowToolbar)
    , m_pOptionsPanel(0)
    , m_pLogPanel(0)
    , m_pGuestSessionPanel(0)
    , m_pOperationsPanel(0)
    , m_fDialogBeingClosed(false)
{
    loadOptions();
    prepareObjects();
    prepareConnections();
    retranslateUi();
    restorePanelVisibility();
    UIFileManagerOptions::create();
    uiCommon().setHelpKeyword(this, "guestadd-gc-file-manager");
    //connect(&uiCommon(), &UICommon::sigAskToDetachCOM, this, &UIFileManager::sltCleanupListenerAndGuest);
    Q_UNUSED(comMachine);
}

UIFileManager::~UIFileManager()
{
    UIFileManagerOptions::destroy();
}

void UIFileManager::setDialogBeingClosed(bool fFlag)
{
    m_fDialogBeingClosed = fFlag;
}

QMenu *UIFileManager::menu() const
{
    if (!m_pActionPool)
        return 0;
    return m_pActionPool->action(UIActionIndex_M_FileManager)->menu();
}

void UIFileManager::retranslateUi()
{
}

void UIFileManager::prepareObjects()
{
    /* m_pMainLayout is the outer most layout containing the main toolbar and splitter widget: */
    m_pMainLayout = new QVBoxLayout(this);
    if (!m_pMainLayout)
        return;

    /* Configure layout: */
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);
#ifdef VBOX_WS_MAC
    m_pMainLayout->setSpacing(10);
#else
    m_pMainLayout->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing) / 2);
#endif

    if (m_fShowToolbar)
        prepareToolBar();

    QHBoxLayout *pFileTableContainerLayout = new QHBoxLayout;
    pFileTableContainerLayout->setContentsMargins(0, 0, 0, 0);
#ifdef VBOX_WS_MAC
    pFileTableContainerLayout->setSpacing(10);
#else
    pFileTableContainerLayout->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing) / 2);
#endif

    QWidget *pTopWidget = new QWidget;
    QVBoxLayout *pTopLayout = new QVBoxLayout;
    pTopLayout->setSpacing(0);
    pTopLayout->setContentsMargins(0, 0, 0, 0);
    pTopWidget->setLayout(pTopLayout);

    m_pFileTableSplitter = new QSplitter;

    if (m_pFileTableSplitter)
    {
        m_pFileTableSplitter->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        m_pFileTableSplitter->setContentsMargins(0, 0, 0, 0);

        /* This widget hosts host file table and vertical toolbar. */
        QWidget *pHostTableAndVerticalToolbarWidget = new QWidget;
        QHBoxLayout *pHostTableAndVerticalToolbarLayout = new QHBoxLayout(pHostTableAndVerticalToolbarWidget);
        pHostTableAndVerticalToolbarLayout->setSpacing(0);
        pHostTableAndVerticalToolbarLayout->setContentsMargins(0, 0, 0, 0);

        m_pHostFileTable = new UIFileManagerHostTable(m_pActionPool);
        if (m_pHostFileTable)
            pHostTableAndVerticalToolbarLayout->addWidget(m_pHostFileTable);

        m_pFileTableSplitter->addWidget(pHostTableAndVerticalToolbarWidget);
        prepareVerticalToolBar(pHostTableAndVerticalToolbarLayout);

        m_pGuestTablesContainer = new QITabWidget;
        if (m_pGuestTablesContainer)
        {
            m_pGuestTablesContainer->setTabBarAutoHide(true);
            m_pFileTableSplitter->addWidget(m_pGuestTablesContainer);
        }
    }

    pTopLayout->addWidget(m_pFileTableSplitter);
    for (int i = 0; i < m_pFileTableSplitter->count(); ++i)
        m_pFileTableSplitter->setCollapsible(i, false);

    /* Create options and session panels and insert them into pTopLayout: */
    prepareOptionsAndSessionPanels(pTopLayout);

    /** Vertical splitter has 3 widgets. Log panel as bottom most one, operations panel on top of it,
     * and pTopWidget which contains everthing else: */
    m_pVerticalSplitter = new QSplitter;
    if (m_pVerticalSplitter)
    {

        m_pMainLayout->addWidget(m_pVerticalSplitter);
        m_pVerticalSplitter->setOrientation(Qt::Vertical);
        m_pVerticalSplitter->setHandleWidth(4);

        m_pVerticalSplitter->addWidget(pTopWidget);
        /* Prepare operations and log panels and insert them into splitter: */
        prepareOperationsAndLogPanels(m_pVerticalSplitter);

        for (int i = 0; i < m_pVerticalSplitter->count(); ++i)
            m_pVerticalSplitter->setCollapsible(i, false);
        m_pVerticalSplitter->setStretchFactor(0, 3);
        m_pVerticalSplitter->setStretchFactor(1, 1);
        m_pVerticalSplitter->setStretchFactor(2, 1);
    }
}

void UIFileManager::prepareVerticalToolBar(QHBoxLayout *layout)
{
    m_pVerticalToolBar = new QIToolBar;
    if (!m_pVerticalToolBar && !m_pActionPool)
        return;

    m_pVerticalToolBar->setOrientation(Qt::Vertical);
    m_pVerticalToolBar->setEnabled(false);

    /* Add to dummy QWidget to toolbar to center the action icons vertically: */
    QWidget *topSpacerWidget = new QWidget(this);
    topSpacerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    topSpacerWidget->setVisible(true);
    QWidget *bottomSpacerWidget = new QWidget(this);
    bottomSpacerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    bottomSpacerWidget->setVisible(true);

    m_pVerticalToolBar->addWidget(topSpacerWidget);
    m_pVerticalToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_CopyToHost));
    m_pVerticalToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_CopyToGuest));
    m_pVerticalToolBar->addWidget(bottomSpacerWidget);

    layout ->addWidget(m_pVerticalToolBar);
}

void UIFileManager::prepareConnections()
{
    if (m_pActionPool)
    {
        if (m_pActionPool->action(UIActionIndex_M_FileManager_T_Options))
            connect(m_pActionPool->action(UIActionIndex_M_FileManager_T_Options), &QAction::toggled,
                    this, &UIFileManager::sltPanelActionToggled);
        if (m_pActionPool->action(UIActionIndex_M_FileManager_T_Log))
            connect(m_pActionPool->action(UIActionIndex_M_FileManager_T_Log), &QAction::toggled,
                    this, &UIFileManager::sltPanelActionToggled);
        if (m_pActionPool->action(UIActionIndex_M_FileManager_T_GuestSession))
            connect(m_pActionPool->action(UIActionIndex_M_FileManager_T_GuestSession), &QAction::toggled,
                    this, &UIFileManager::sltPanelActionToggled);
        if (m_pActionPool->action(UIActionIndex_M_FileManager_T_Operations))
            connect(m_pActionPool->action(UIActionIndex_M_FileManager_T_Operations), &QAction::toggled,
                    this, &UIFileManager::sltPanelActionToggled);
        if (m_pActionPool->action(UIActionIndex_M_FileManager_S_CopyToHost))
            connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_CopyToHost), &QAction::triggered,
                    this, &UIFileManager::sltCopyGuestToHost);
        if (m_pActionPool->action(UIActionIndex_M_FileManager_S_CopyToGuest))
            connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_CopyToGuest), &QAction::triggered,
                    this, &UIFileManager::sltCopyHostToGuest);
    }
    if (m_pOptionsPanel)
    {
        connect(m_pOptionsPanel, &UIFileManagerOptionsPanel::sigHidePanel,
                this, &UIFileManager::sltHandleHidePanel);
        connect(m_pOptionsPanel, &UIFileManagerOptionsPanel::sigOptionsChanged,
                this, &UIFileManager::sltHandleOptionsUpdated);
    }
    if (m_pLogPanel)
        connect(m_pLogPanel, &UIFileManagerLogPanel::sigHidePanel,
                this, &UIFileManager::sltHandleHidePanel);

    if (m_pOperationsPanel)
        connect(m_pOperationsPanel, &UIFileManagerOperationsPanel::sigHidePanel,
                this, &UIFileManager::sltHandleHidePanel);
    if (m_pHostFileTable)
    {
        connect(m_pHostFileTable, &UIFileManagerHostTable::sigLogOutput,
                this, &UIFileManager::sltReceieveLogOutput);
        connect(m_pHostFileTable, &UIFileManagerHostTable::sigDeleteConfirmationOptionChanged,
                this, &UIFileManager::sltHandleOptionsUpdated);
    }
}

void UIFileManager::prepareToolBar()
{
    /* Create toolbar: */
    m_pToolBar = new QIToolBar(parentWidget());
    if (m_pToolBar)
    {
        /* Configure toolbar: */
        const int iIconMetric = (int)(QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize));
        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_T_GuestSession));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_T_Options));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_T_Operations));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_T_Log));

#ifdef VBOX_WS_MAC
        /* Check whether we are embedded into a stack: */
        if (m_enmEmbedding == EmbedTo_Stack)
        {
            /* Add into layout: */
            m_pMainLayout->addWidget(m_pToolBar);
        }
#else
        /* Add into layout: */
        m_pMainLayout->addWidget(m_pToolBar);
#endif
    }
}

void UIFileManager::sltReceieveLogOutput(QString strOutput, FileManagerLogType eLogType)
{
    appendLog(strOutput, eLogType);
}

void UIFileManager::sltCopyGuestToHost()
{
    copyToHost();
}

void UIFileManager::sltCopyHostToGuest()
{
    copyToGuest();
}

void UIFileManager::sltPanelActionToggled(bool fChecked)
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

// void UIFileManager::sltReceieveNewFileOperation(const CProgress &comProgress)
// {
//     if (m_pOperationsPanel)
//         m_pOperationsPanel->addNewProgress(comProgress);
// }

void UIFileManager::sltFileOperationComplete(QUuid progressId)
{
    Q_UNUSED(progressId);
    if (m_pHostFileTable)
        m_pHostFileTable->refresh();
    /// @todo we need to refresh only the table from which the completed file operation has originated
    for (int i = 0; i < m_pGuestTablesContainer->count(); ++i)
    {
        UIFileManagerGuestTable *pTable = qobject_cast<UIFileManagerGuestTable*>(m_pGuestTablesContainer->widget(i));
        if (pTable)
            pTable->refresh();
    }
}

void UIFileManager::sltHandleOptionsUpdated()
{
    if (m_pOptionsPanel)
        m_pOptionsPanel->update();

    for (int i = 0; i < m_pGuestTablesContainer->count(); ++i)
    {
        UIFileManagerGuestTable *pTable = qobject_cast<UIFileManagerGuestTable*>(m_pGuestTablesContainer->widget(i));
        if (pTable)
            pTable->optionsUpdated();
    }
    if (m_pHostFileTable)
        m_pHostFileTable->optionsUpdated();
    saveOptions();
}

void UIFileManager::sltHandleHidePanel(UIDialogPanel *pPanel)
{
    hidePanel(pPanel);
}

void UIFileManager::copyToHost()
{
    if (m_pGuestTablesContainer && m_pHostFileTable)
    {
        UIFileManagerGuestTable *pGuestFileTable = qobject_cast<UIFileManagerGuestTable*>(m_pGuestTablesContainer->currentWidget());
        if (pGuestFileTable)
            pGuestFileTable->copyGuestToHost(m_pHostFileTable->currentDirectoryPath());
    }
}

void UIFileManager::copyToGuest()
{
    if (m_pGuestTablesContainer && m_pHostFileTable)
    {
        UIFileManagerGuestTable *pGuestFileTable = qobject_cast<UIFileManagerGuestTable*>(m_pGuestTablesContainer->currentWidget());
        if (pGuestFileTable)
            pGuestFileTable->copyHostToGuest(m_pHostFileTable->selectedItemPathList());
    }
}

void UIFileManager::prepareOptionsAndSessionPanels(QVBoxLayout *pLayout)
{
    if (!pLayout)
        return;
    m_pGuestSessionPanel = new UIFileManagerGuestSessionPanel;
    if (m_pGuestSessionPanel)
    {
        m_pGuestSessionPanel->hide();
        m_panelActionMap.insert(m_pGuestSessionPanel, m_pActionPool->action(UIActionIndex_M_FileManager_T_GuestSession));
        pLayout->addWidget(m_pGuestSessionPanel);
    }

    m_pOptionsPanel = new UIFileManagerOptionsPanel(0 /*parent */, UIFileManagerOptions::instance());
    if (m_pOptionsPanel)
    {
        m_pOptionsPanel->hide();
        m_panelActionMap.insert(m_pOptionsPanel, m_pActionPool->action(UIActionIndex_M_FileManager_T_Options));
        pLayout->addWidget(m_pOptionsPanel);
    }
}

void UIFileManager::prepareOperationsAndLogPanels(QSplitter *pSplitter)
{
    if (!pSplitter)
        return;
    m_pOperationsPanel = new UIFileManagerOperationsPanel;
    if (m_pOperationsPanel)
    {
        m_pOperationsPanel->hide();
        connect(m_pOperationsPanel, &UIFileManagerOperationsPanel::sigFileOperationComplete,
                this, &UIFileManager::sltFileOperationComplete);
        connect(m_pOperationsPanel, &UIFileManagerOperationsPanel::sigFileOperationFail,
                this, &UIFileManager::sltReceieveLogOutput);
        m_panelActionMap.insert(m_pOperationsPanel, m_pActionPool->action(UIActionIndex_M_FileManager_T_Operations));
    }
    pSplitter->addWidget(m_pOperationsPanel);
    m_pLogPanel = new UIFileManagerLogPanel;
    if (m_pLogPanel)
    {
        m_pLogPanel->hide();
        m_panelActionMap.insert(m_pLogPanel, m_pActionPool->action(UIActionIndex_M_FileManager_T_Log));
    }
    pSplitter->addWidget(m_pLogPanel);
}


template<typename T>
QStringList UIFileManager::getFsObjInfoStringList(const T &fsObjectInfo) const
{
    QStringList objectInfo;
    if (!fsObjectInfo.isOk())
        return objectInfo;
    objectInfo << fsObjectInfo.GetName();
    return objectInfo;
}

void UIFileManager::saveOptions()
{
    /* Save the options: */
    UIFileManagerOptions *pOptions = UIFileManagerOptions::instance();
    if (pOptions)
    {
        gEDataManager->setFileManagerOptions(pOptions->fListDirectoriesOnTop,
                                             pOptions->fAskDeleteConfirmation,
                                             pOptions->fShowHumanReadableSizes,
                                             pOptions->fShowHiddenObjects);
    }
}

void UIFileManager::restorePanelVisibility()
{
    /** Make sure the actions are set to not-checked. this prevents an unlikely
     *  bug when the extrakey for the visible panels are manually modified: */
    foreach(QAction* pAction, m_panelActionMap.values())
    {
        pAction->blockSignals(true);
        pAction->setChecked(false);
        pAction->blockSignals(false);
    }

    /* Load the visible panel list and show them: */
    QStringList strNameList = gEDataManager->fileManagerVisiblePanels();
    foreach(const QString strName, strNameList)
    {
        foreach(UIDialogPanel* pPanel, m_panelActionMap.keys())
        {
            if (strName == pPanel->panelName())
            {
                showPanel(pPanel);
                break;
            }
        }
    }
    /* Make sure Session panel is visible: */
    if (m_pGuestSessionPanel && !m_pGuestSessionPanel->isVisible())
        showPanel(m_pGuestSessionPanel);
}

void UIFileManager::loadOptions()
{
    /* Load options: */
    UIFileManagerOptions *pOptions = UIFileManagerOptions::instance();
    if (pOptions)
    {
        pOptions->fListDirectoriesOnTop = gEDataManager->fileManagerListDirectoriesFirst();
        pOptions->fAskDeleteConfirmation = gEDataManager->fileManagerShowDeleteConfirmation();
        pOptions->fShowHumanReadableSizes = gEDataManager->fileManagerShowHumanReadableSizes();
        pOptions->fShowHiddenObjects = gEDataManager->fileManagerShowHiddenObjects();
    }
}

void UIFileManager::hidePanel(UIDialogPanel* panel)
{
    if (!m_pActionPool)
        return;
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
    savePanelVisibility();
}

void UIFileManager::showPanel(UIDialogPanel* panel)
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
    savePanelVisibility();
}

void UIFileManager::manageEscapeShortCut()
{
    /* if there is no visible panels give the escape shortcut to parent dialog: */
    if (m_visiblePanelsList.isEmpty())
    {
        emit sigSetCloseButtonShortCut(QKeySequence(Qt::Key_Escape));
        return;
    }
    /* Take the escape shortcut from the dialog: */
    emit sigSetCloseButtonShortCut(QKeySequence());
    /* Just loop thru the visible panel list and set the esc key to the
       panel which made visible latest */
    for (int i = 0; i < m_visiblePanelsList.size() - 1; ++i)
        m_visiblePanelsList[i]->setCloseButtonShortCut(QKeySequence());

    m_visiblePanelsList.back()->setCloseButtonShortCut(QKeySequence(Qt::Key_Escape));
}

void UIFileManager::appendLog(const QString &strLog, FileManagerLogType eLogType)
{
    if (!m_pLogPanel)
        return;
    m_pLogPanel->appendLog(strLog, eLogType);
}

void UIFileManager::savePanelVisibility()
{
    if (m_fDialogBeingClosed)
        return;
    /* Save a list of currently visible panels: */
    QStringList strNameList;
    foreach(UIDialogPanel* pPanel, m_visiblePanelsList)
        strNameList.append(pPanel->panelName());
    gEDataManager->setFileManagerVisiblePanels(strNameList);
}

void UIFileManager::setSelectedVMListItems(const QList<UIVirtualMachineItem*> &items)
{
    QVector<QUuid> selectedMachines;

    foreach (const UIVirtualMachineItem *item, items)
    {
        if (!item)
            continue;
        selectedMachines << item->id();
    }
    setMachines(selectedMachines);
}

void UIFileManager::setMachines(const QVector<QUuid> &machineIds)
{
    /* List of machines that are newly added to selected machine list: */
    QVector<QUuid> newSelections;
    QVector<QUuid> unselectedMachines(m_machineIds);

    foreach (const QUuid &id, machineIds)
    {
        unselectedMachines.removeAll(id);
        if (!m_machineIds.contains(id))
            newSelections << id;
    }
    m_machineIds = machineIds;

    addTabs(newSelections);
    removeTabs(unselectedMachines);
}

void UIFileManager::removeTabs(const QVector<QUuid> &machineIdsToRemove)
{
    if (!m_pGuestTablesContainer)
        return;
    QVector<UIFileManagerGuestTable*> removeList;

    for (int i = m_pGuestTablesContainer->count() - 1; i >= 0; --i)
    {
        UIFileManagerGuestTable *pTable = qobject_cast<UIFileManagerGuestTable*>(m_pGuestTablesContainer->widget(i));
        if (!pTable)
            continue;
        if (machineIdsToRemove.contains(pTable->machineId()))
        {
            removeList << pTable;
            m_pGuestTablesContainer->removeTab(i);
        }
    }
    qDeleteAll(removeList.begin(), removeList.end());
}

void UIFileManager::addTabs(const QVector<QUuid> &machineIdsToAdd)
{
    if (!m_pGuestTablesContainer)
        return;

    foreach (const QUuid &id, machineIdsToAdd)
    {
        CMachine comMachine = uiCommon().virtualBox().FindMachine(id.toString());
        if (comMachine.isNull())
            continue;
        m_pGuestTablesContainer->addTab(new UIFileManagerGuestTable(m_pActionPool, comMachine, m_pGuestTablesContainer), comMachine.GetName());
        // if (m_pGuestFileTable)
        // {
        //     connect(m_pGuestFileTable, &UIFileManagerGuestTable::sigLogOutput,
        //             this, &UIFileManager::sltReceieveLogOutput);
        //     connect(m_pGuestFileTable, &UIFileManagerGuestTable::sigNewFileOperation,
        //             this, &UIFileManager::sltReceieveNewFileOperation);
        //     connect(m_pGuestFileTable, &UIFileManagerGuestTable::sigDeleteConfirmationOptionChanged,
        //             this, &UIFileManager::sltHandleOptionsUpdated);
        // }
    }
}


#include "UIFileManager.moc"
