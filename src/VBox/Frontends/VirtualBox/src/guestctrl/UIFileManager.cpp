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
    , m_comMachine(comMachine)
    , m_pMainLayout(0)
    , m_pVerticalSplitter(0)
    , m_pToolBar(0)
    , m_pVerticalToolBar(0)
    , m_pGuestFileTable(0)
    , m_pHostFileTable(0)
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
    connect(&uiCommon(), &UICommon::sigAskToDetachCOM, this, &UIFileManager::sltCleanupListenerAndGuest);
}

UIFileManager::~UIFileManager()
{
    UIFileManagerOptions::destroy();
}

void UIFileManager::setMachine(const QUuid &machineId)
{
    m_comMachine = uiCommon().virtualBox().FindMachine(machineId.toString());
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
    /* Two widgets are inserted into this splitter. Upper pWidget widget is a container with file tables and all the panels
       except the log panel and lower widget is the log panel: */
    m_pVerticalSplitter = new QSplitter;
    if (!m_pVerticalSplitter)
        return;

    m_pMainLayout->addWidget(m_pVerticalSplitter);
    m_pVerticalSplitter->setOrientation(Qt::Vertical);
    m_pVerticalSplitter->setHandleWidth(4);

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

    if (pFileTableContainerLayout)
    {
        pFileTableContainerLayout->setSpacing(0);
        pFileTableContainerLayout->setContentsMargins(0, 0, 0, 0);
        m_pGuestFileTable = new UIFileManagerGuestTable(m_pActionPool);
        m_pGuestFileTable->setEnabled(false);

        m_pHostFileTable = new UIFileManagerHostTable(m_pActionPool);
        if (m_pHostFileTable)
        {
            connect(m_pHostFileTable, &UIFileManagerHostTable::sigLogOutput,
                    this, &UIFileManager::sltReceieveLogOutput);
            connect(m_pHostFileTable, &UIFileManagerHostTable::sigDeleteConfirmationOptionChanged,
                    this, &UIFileManager::sltHandleOptionsUpdated);
            pFileTableContainerLayout->addWidget(m_pHostFileTable);
        }
        prepareVerticalToolBar(pFileTableContainerLayout);
        if (m_pGuestFileTable)
        {
            connect(m_pGuestFileTable, &UIFileManagerGuestTable::sigLogOutput,
                    this, &UIFileManager::sltReceieveLogOutput);
            connect(m_pGuestFileTable, &UIFileManagerGuestTable::sigNewFileOperation,
                    this, &UIFileManager::sltReceieveNewFileOperation);
            connect(m_pGuestFileTable, &UIFileManagerGuestTable::sigDeleteConfirmationOptionChanged,
                    this, &UIFileManager::sltHandleOptionsUpdated);
            pFileTableContainerLayout->addWidget(m_pGuestFileTable);
        }
    }

    pTopLayout->addLayout(pFileTableContainerLayout);
    m_pGuestSessionPanel = new UIFileManagerGuestSessionPanel;
    if (m_pGuestSessionPanel)
    {
        m_pGuestSessionPanel->hide();
        m_panelActionMap.insert(m_pGuestSessionPanel, m_pActionPool->action(UIActionIndex_M_FileManager_T_GuestSession));
        pTopLayout->addWidget(m_pGuestSessionPanel);
    }

    m_pOptionsPanel =
        new UIFileManagerOptionsPanel(0 /*parent */, UIFileManagerOptions::instance());
    if (m_pOptionsPanel)
    {
        m_pOptionsPanel->hide();
        m_panelActionMap.insert(m_pOptionsPanel, m_pActionPool->action(UIActionIndex_M_FileManager_T_Options));
        connect(m_pOptionsPanel, &UIFileManagerOptionsPanel::sigOptionsChanged,
                this, &UIFileManager::sltHandleOptionsUpdated);
        pTopLayout->addWidget(m_pOptionsPanel);
    }

    m_pVerticalSplitter->addWidget(pTopWidget);

    m_pOperationsPanel =
        new UIFileManagerOperationsPanel;
    if (m_pOperationsPanel)
    {
        m_pOperationsPanel->hide();
        connect(m_pOperationsPanel, &UIFileManagerOperationsPanel::sigFileOperationComplete,
                this, &UIFileManager::sltFileOperationComplete);
        connect(m_pOperationsPanel, &UIFileManagerOperationsPanel::sigFileOperationFail,
                this, &UIFileManager::sltReceieveLogOutput);
        m_panelActionMap.insert(m_pOperationsPanel, m_pActionPool->action(UIActionIndex_M_FileManager_T_Operations));
    }

    m_pLogPanel = new UIFileManagerLogPanel;
    if (m_pLogPanel)
    {
        m_pLogPanel->hide();
        m_panelActionMap.insert(m_pLogPanel, m_pActionPool->action(UIActionIndex_M_FileManager_T_Log));
    }

    m_pVerticalSplitter->addWidget(pTopWidget);
    m_pVerticalSplitter->addWidget(m_pOperationsPanel);
    m_pVerticalSplitter->addWidget(m_pLogPanel);
    m_pVerticalSplitter->setCollapsible(m_pVerticalSplitter->indexOf(pTopWidget), false);
    m_pVerticalSplitter->setCollapsible(m_pVerticalSplitter->indexOf(m_pOperationsPanel), false);
    m_pVerticalSplitter->setCollapsible(m_pVerticalSplitter->indexOf(m_pLogPanel), false);
    m_pVerticalSplitter->setStretchFactor(0, 3);
    m_pVerticalSplitter->setStretchFactor(1, 1);
    m_pVerticalSplitter->setStretchFactor(2, 1);
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

    if (m_pGuestSessionPanel)
    {
        connect(m_pGuestSessionPanel, &UIFileManagerGuestSessionPanel::sigCreateSession,
                this, &UIFileManager::sltCreateGuestSession);
        connect(m_pGuestSessionPanel, &UIFileManagerGuestSessionPanel::sigCloseSession,
                this, &UIFileManager::sltCloseGuestSession);
        connect(m_pGuestSessionPanel, &UIFileManagerGuestSessionPanel::sigHidePanel,
                this, &UIFileManager::sltHandleHidePanel);
    }
    if (m_pOptionsPanel)
        connect(m_pOptionsPanel, &UIFileManagerOptionsPanel::sigHidePanel,
                this, &UIFileManager::sltHandleHidePanel);

    if (m_pLogPanel)
        connect(m_pLogPanel, &UIFileManagerLogPanel::sigHidePanel,
                this, &UIFileManager::sltHandleHidePanel);

    if (m_pOperationsPanel)
        connect(m_pOperationsPanel, &UIFileManagerOperationsPanel::sigHidePanel,
                this, &UIFileManager::sltHandleHidePanel);
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

void UIFileManager::sltGuestSessionUnregistered(CGuestSession guestSession)
{
    if (guestSession.isNull())
        return;
    if (guestSession == m_comGuestSession && !m_comGuestSession.isNull())
    {
        m_comGuestSession.detach();
        postGuestSessionClosed();
        appendLog("Guest session unregistered", FileManagerLogType_Info);
    }
}

void UIFileManager::sltGuestSessionRegistered(CGuestSession guestSession)
{
    if (guestSession == m_comGuestSession && !m_comGuestSession.isNull())
        appendLog("Guest session registered", FileManagerLogType_Info);
}


void UIFileManager::sltCreateGuestSession(QString strUserName, QString strPassword)
{
    if (strUserName.isEmpty())
    {
        appendLog("No user name is given", FileManagerLogType_Error);
        if (m_pGuestSessionPanel)
            m_pGuestSessionPanel->markForError(true);
        return;
    }
    if (m_pGuestSessionPanel)
        m_pGuestSessionPanel->markForError(!openSession(strUserName, strPassword));
}

void UIFileManager::sltCloseGuestSession()
{
    if (!m_comGuestSession.isOk())
    {
        appendLog("Guest session is not valid", FileManagerLogType_Error);
        postGuestSessionClosed();
        return;
    }
    if (m_pGuestFileTable)
        m_pGuestFileTable->reset();

    if (m_comGuestSession.isOk() && m_pQtSessionListener && m_comSessionListener.isOk())
        cleanupListener(m_pQtSessionListener, m_comSessionListener, m_comGuestSession.GetEventSource());

    m_comGuestSession.Close();
    appendLog("Guest session is closed", FileManagerLogType_Info);
    postGuestSessionClosed();
}

void UIFileManager::sltGuestSessionStateChanged(const CGuestSessionStateChangedEvent &cEvent)
{
    if (cEvent.isOk())
    {
        CVirtualBoxErrorInfo cErrorInfo = cEvent.GetError();
        if (cErrorInfo.isOk() && !cErrorInfo.GetText().contains("success", Qt::CaseInsensitive))
            appendLog(cErrorInfo.GetText(), FileManagerLogType_Error);
    }
    if (m_comGuestSession.isOk())
    {
        if (m_comGuestSession.GetStatus() == KGuestSessionStatus_Started)
        {
            initFileTable();
            postGuestSessionCreated();
        }
        appendLog(QString("%1: %2").arg("Guest session status has changed").arg(gpConverter->toString(m_comGuestSession.GetStatus())),
                  FileManagerLogType_Info);
    }
    else
        appendLog("Guest session is not valid", FileManagerLogType_Error);
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

void UIFileManager::sltReceieveNewFileOperation(const CProgress &comProgress)
{
    if (m_pOperationsPanel)
        m_pOperationsPanel->addNewProgress(comProgress);
}

void UIFileManager::sltFileOperationComplete(QUuid progressId)
{
    Q_UNUSED(progressId);
    if (!m_pGuestFileTable || !m_pHostFileTable)
        return;

    m_pHostFileTable->refresh();
    m_pGuestFileTable->refresh();
}

void UIFileManager::sltHandleOptionsUpdated()
{
    if (m_pOptionsPanel)
    {
        m_pOptionsPanel->update();
    }

    if (m_pGuestFileTable)
        m_pGuestFileTable->optionsUpdated();
    if (m_pHostFileTable)
        m_pHostFileTable->optionsUpdated();
    saveOptions();
}

void UIFileManager::sltHandleHidePanel(UIDialogPanel *pPanel)
{
    hidePanel(pPanel);
}

void UIFileManager::sltCleanupListenerAndGuest()
{
    if (m_comGuest.isOk() && m_pQtGuestListener && m_comGuestListener.isOk())
        cleanupListener(m_pQtGuestListener, m_comGuestListener, m_comGuest.GetEventSource());
    if (m_comGuestSession.isOk() && m_pQtSessionListener && m_comSessionListener.isOk())
        cleanupListener(m_pQtSessionListener, m_comSessionListener, m_comGuestSession.GetEventSource());

    if (m_comGuestSession.isOk())
        m_comGuestSession.Close();
}

void UIFileManager::copyToHost()
{
    if (!m_pGuestFileTable || !m_pHostFileTable)
        return;
    QString hostDestinationPath = m_pHostFileTable->currentDirectoryPath();
    m_pGuestFileTable->copyGuestToHost(hostDestinationPath);
    m_pHostFileTable->refresh();
}

void UIFileManager::copyToGuest()
{
    if (!m_pGuestFileTable || !m_pHostFileTable)
        return;
    QStringList hostSourcePathList = m_pHostFileTable->selectedItemPathList();
    m_pGuestFileTable->copyHostToGuest(hostSourcePathList);
    m_pGuestFileTable->refresh();
}

void UIFileManager::initFileTable()
{
    if (!m_comGuestSession.isOk() || m_comGuestSession.GetStatus() != KGuestSessionStatus_Started)
        return;
    if (!m_pGuestFileTable)
        return;
    m_pGuestFileTable->initGuestFileTable(m_comGuestSession);
}

void UIFileManager::postGuestSessionCreated()
{
    if (m_pGuestSessionPanel)
        m_pGuestSessionPanel->switchSessionCloseMode();
    if (m_pGuestFileTable)
        m_pGuestFileTable->setEnabled(true);
    if (m_pVerticalToolBar)
        m_pVerticalToolBar->setEnabled(true);
}

void UIFileManager::postGuestSessionClosed()
{
    if (m_pGuestSessionPanel)
        m_pGuestSessionPanel->switchSessionCreateMode();
    if (m_pGuestFileTable)
        m_pGuestFileTable->setEnabled(false);
    if (m_pVerticalToolBar)
        m_pVerticalToolBar->setEnabled(false);
}

bool UIFileManager::openSession(const QString& strUserName, const QString& strPassword)
{
    if (m_comMachine.isNull())
    {
        appendLog("Invalid machine reference", FileManagerLogType_Error);
        return false;
    }
    m_comSession = uiCommon().openSession(m_comMachine.GetId(), KLockType_Shared);
    if (!m_comSession.isNull())
    {
        appendLog("Could not open machine session", FileManagerLogType_Error);
        return false;
    }

    CConsole comConsole = m_comSession.GetConsole();
    AssertReturn(!comConsole.isNull(), false);
    m_comGuest = comConsole.GetGuest();
    AssertReturn(!m_comGuest.isNull(), false);

    if (!isGuestAdditionsAvailable(m_comGuest))
    {
        appendLog("Could not find Guest Additions", FileManagerLogType_Error);
        postGuestSessionClosed();
        if (m_pGuestSessionPanel)
            m_pGuestSessionPanel->markForError(true);
        return false;
    }

    QVector<KVBoxEventType> eventTypes;
    eventTypes << KVBoxEventType_OnGuestSessionRegistered;

    prepareListener(m_pQtGuestListener, m_comGuestListener,
                    m_comGuest.GetEventSource(), eventTypes);
    connect(m_pQtGuestListener->getWrapped(), &UIMainEventListener::sigGuestSessionUnregistered,
            this, &UIFileManager::sltGuestSessionUnregistered);

    connect(m_pQtGuestListener->getWrapped(), &UIMainEventListener::sigGuestSessionRegistered,
            this, &UIFileManager::sltGuestSessionRegistered);

    m_comGuestSession = m_comGuest.CreateSession(strUserName, strPassword,
                                                 QString() /* Domain */, "File Manager Session");

    if (!m_comGuestSession.isOk())
    {
        appendLog(UIErrorString::formatErrorInfo(m_comGuestSession), FileManagerLogType_Error);
        return false;
    }

    if (m_pGuestSessionPanel)
        m_pGuestSessionPanel->switchSessionCloseMode();

    /* Prepare guest session listener */
    eventTypes.clear();
    eventTypes << KVBoxEventType_OnGuestSessionStateChanged;

    prepareListener(m_pQtSessionListener, m_comSessionListener,
                    m_comGuestSession.GetEventSource(), eventTypes);

    qRegisterMetaType<CGuestSessionStateChangedEvent>();
    connect(m_pQtSessionListener->getWrapped(), &UIMainEventListener::sigGuestSessionStatedChanged,
            this, &UIFileManager::sltGuestSessionStateChanged);

    return true;
}


void UIFileManager::closeSession()
{
}


void UIFileManager::prepareListener(ComObjPtr<UIMainEventListenerImpl> &QtListener,
                                    CEventListener &comEventListener,
                                    CEventSource comEventSource, QVector<KVBoxEventType>& eventTypes)
{
    if (!comEventSource.isOk())
        return;
    /* Create event listener instance: */
    QtListener.createObject();
    QtListener->init(new UIMainEventListener, this);
    comEventListener = CEventListener(QtListener);

    /* Register event listener for CProgress event source: */
    comEventSource.RegisterListener(comEventListener, eventTypes, FALSE /* active? */);

    /* Register event sources in their listeners as well: */
    QtListener->getWrapped()->registerSource(comEventSource, comEventListener);
}

void UIFileManager::cleanupListener(ComObjPtr<UIMainEventListenerImpl> &QtListener,
                                                CEventListener &comEventListener,
                                                CEventSource comEventSource)
{
    if (!comEventSource.isOk())
        return;
    /* Unregister everything: */
    QtListener->getWrapped()->unregisterSources();

    /* Make sure VBoxSVC is available: */
    if (!uiCommon().isVBoxSVCAvailable())
        return;

    /* Unregister event listener for CProgress event source: */
    comEventSource.UnregisterListener(comEventListener);
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
    {
        m_visiblePanelsList[i]->setCloseButtonShortCut(QKeySequence());
    }
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

bool UIFileManager::isGuestAdditionsAvailable(const CGuest &guest)
{
    if (!guest.isOk())
        return false;
    CGuest guestNonConst = const_cast<CGuest&>(guest);
    return guestNonConst.GetAdditionsStatus(guestNonConst.GetAdditionsRunLevel());
}

#include "UIFileManager.moc"
