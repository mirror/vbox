/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlFileManager class implementation.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
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
# include <QAbstractItemModel>
# include <QCheckBox>
# include <QHBoxLayout>
# include <QHeaderView>
# include <QTextEdit>
# include <QPushButton>
# include <QSplitter>
# include <QGridLayout>

/* GUI includes: */
# include "QILabel.h"
# include "QILineEdit.h"
# include "QITabWidget.h"
# include "QITreeWidget.h"
# include "QIWithRetranslateUI.h"
# include "UIActionPool.h"
# include "UIErrorString.h"
# include "UIExtraDataManager.h"
# include "UIIconPool.h"
# include "UIGuestControlConsole.h"
# include "UIGuestControlFileManager.h"
# include "UIGuestControlFileManagerSessionPanel.h"
# include "UIGuestControlFileManagerSettingsPanel.h"
# include "UIGuestControlFileManagerLogPanel.h"
# include "UIGuestFileTable.h"
# include "UIGuestControlInterface.h"
# include "UIHostFileTable.h"
# include "UIToolBar.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CFsObjInfo.h"
# include "CGuest.h"
# include "CGuestDirectory.h"
# include "CGuestFsObjInfo.h"
# include "CGuestProcess.h"
# include "CGuestSession.h"
# include "CGuestSessionStateChangedEvent.h"
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */



/*********************************************************************************************************************************
*   UIFileOperationsList definition.                                                                                   *
*********************************************************************************************************************************/

class UIFileOperationsList : public QITreeWidget
{
    Q_OBJECT;
public:

    UIFileOperationsList(QWidget *pParent = 0);

private:
};


/*********************************************************************************************************************************
*   UIGuestControlFileManagerSettings implementation.                                                                            *
*********************************************************************************************************************************/

UIGuestControlFileManagerSettings *UIGuestControlFileManagerSettings::m_pInstance = 0;

UIGuestControlFileManagerSettings* UIGuestControlFileManagerSettings::instance()
{
    if (!m_pInstance)
    m_pInstance = new UIGuestControlFileManagerSettings;
    return m_pInstance;
}

void UIGuestControlFileManagerSettings::create()
{
    if (m_pInstance)
        return;
    m_pInstance = new UIGuestControlFileManagerSettings;
}

void UIGuestControlFileManagerSettings::destroy()
{
    delete m_pInstance;
    m_pInstance = 0;
}

 UIGuestControlFileManagerSettings::~UIGuestControlFileManagerSettings()
{

}

UIGuestControlFileManagerSettings::UIGuestControlFileManagerSettings()
    : bListDirectoriesOnTop(true)
    , bAskDeleteConfirmation(false)
{
}

/*********************************************************************************************************************************
*   UIFileOperationsList implementation.                                                                                   *
*********************************************************************************************************************************/

UIFileOperationsList::UIFileOperationsList(QWidget *pParent)
    :QITreeWidget(pParent)
{}


/*********************************************************************************************************************************
*   UIGuestControlFileManager implementation.                                                                                    *
*********************************************************************************************************************************/

UIGuestControlFileManager::UIGuestControlFileManager(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                                                     const CGuest &comGuest, QWidget *pParent, bool fShowToolbar /* = true */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_iMaxRecursionDepth(1)
    , m_comGuest(comGuest)
    , m_pMainLayout(0)
    , m_pVerticalSplitter(0)
    , m_pToolBar(0)
    , m_pGuestFileTable(0)
    , m_pHostFileTable(0)
    , m_enmEmbedding(enmEmbedding)
    , m_pActionPool(pActionPool)
    , m_fShowToolbar(fShowToolbar)
    , m_pSettingsPanel(0)
    , m_pLogPanel(0)
    , m_pSessionPanel(0)
{
    prepareGuestListener();
    prepareObjects();
    prepareConnections();
    retranslateUi();
    loadSettings();
    UIGuestControlFileManagerSettings::create();
}

UIGuestControlFileManager::~UIGuestControlFileManager()
{
    if (m_comGuest.isOk() && m_pQtGuestListener && m_comGuestListener.isOk())
        cleanupListener(m_pQtGuestListener, m_comGuestListener, m_comGuest.GetEventSource());
    if (m_comGuestSession.isOk() && m_pQtSessionListener && m_comSessionListener.isOk())
        cleanupListener(m_pQtSessionListener, m_comSessionListener, m_comGuestSession.GetEventSource());
    saveSettings();
    UIGuestControlFileManagerSettings::destroy();
}

QMenu *UIGuestControlFileManager::menu() const
{
    if (!m_pActionPool)
        return 0;
    return m_pActionPool->action(UIActionIndex_M_GuestControlFileManager)->menu();
}

void UIGuestControlFileManager::retranslateUi()
{
}

void UIGuestControlFileManager::prepareGuestListener()
{
    if (m_comGuest.isOk())
    {
        QVector<KVBoxEventType> eventTypes;
        eventTypes << KVBoxEventType_OnGuestSessionRegistered;

        prepareListener(m_pQtGuestListener, m_comGuestListener,
                        m_comGuest.GetEventSource(), eventTypes);
    }
}

void UIGuestControlFileManager::prepareObjects()
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
        m_pGuestFileTable = new UIGuestFileTable(m_pActionPool);
        m_pGuestFileTable->setEnabled(false);

        m_pHostFileTable = new UIHostFileTable(m_pActionPool);
        if (m_pHostFileTable)
        {
            connect(m_pHostFileTable, &UIHostFileTable::sigLogOutput,
                    this, &UIGuestControlFileManager::sltReceieveLogOutput);
            pFileTableContainerLayout->addWidget(m_pHostFileTable);
        }
        prepareVerticalToolBar(pFileTableContainerLayout);
        if (m_pGuestFileTable)
        {
            connect(m_pGuestFileTable, &UIGuestFileTable::sigLogOutput,
                    this, &UIGuestControlFileManager::sltReceieveLogOutput);
            pFileTableContainerLayout->addWidget(m_pGuestFileTable);
        }
    }

    // m_pFileOperationsList = new UIFileOperationsList;
    // if (m_pFileOperationsList)
    // {
    //     m_pTabWidget->addTab(m_pFileOperationsList, "File Operatiions");
    //     m_pFileOperationsList->header()->hide();
    // }
    pTopLayout->addLayout(pFileTableContainerLayout);
    m_pSessionPanel = new UIGuestControlFileManagerSessionPanel(this /* manager dialog */, 0 /*parent */);
    if (m_pSessionPanel)
    {
        m_pSessionPanel->hide();
        m_panelActionMap.insert(m_pSessionPanel, m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_T_Session));
        pTopLayout->addWidget(m_pSessionPanel);
    }

    m_pSettingsPanel =
        new UIGuestControlFileManagerSettingsPanel(this /* manager dialog */,
                                                   0 /*parent */, UIGuestControlFileManagerSettings::instance());
    if (m_pSettingsPanel)
    {
        m_pSettingsPanel->hide();
        m_panelActionMap.insert(m_pSettingsPanel, m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_T_Settings));
        connect(m_pSettingsPanel, &UIGuestControlFileManagerSettingsPanel::sigListDirectoriesFirstChanged,
                this, &UIGuestControlFileManager::sltListDirectoriesBeforeChanged);
        pTopLayout->addWidget(m_pSettingsPanel);
    }
    m_pVerticalSplitter->addWidget(pTopWidget);

    m_pLogPanel = new UIGuestControlFileManagerLogPanel(this /* manager dialog */, 0 /*parent */);
    if (m_pLogPanel)
    {
        m_pLogPanel->hide();
        m_panelActionMap.insert(m_pLogPanel, m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_T_Log));
    }
    m_pVerticalSplitter->addWidget(pTopWidget);
    m_pVerticalSplitter->addWidget(m_pLogPanel);
    m_pVerticalSplitter->setCollapsible(m_pVerticalSplitter->indexOf(pTopWidget), false);
    m_pVerticalSplitter->setCollapsible(m_pVerticalSplitter->indexOf(m_pLogPanel), false);
    m_pVerticalSplitter->setStretchFactor(0, 3);
    m_pVerticalSplitter->setStretchFactor(1, 1);
}

void UIGuestControlFileManager::prepareVerticalToolBar(QHBoxLayout *layout)
{
    m_pToolBar = new UIToolBar;
    if (!m_pToolBar)
        return;

    m_pToolBar->setOrientation(Qt::Vertical);
    m_pToolBar->setEnabled(false);

    /* Add to dummy QWidget to toolbar to center the action icons vertically: */
    QWidget *topSpacerWidget = new QWidget(this);
    topSpacerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    topSpacerWidget->setVisible(true);
    QWidget *bottomSpacerWidget = new QWidget(this);
    bottomSpacerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    bottomSpacerWidget->setVisible(true);

    m_pToolBar->addWidget(topSpacerWidget);
    m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_CopyToHost));
    m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_CopyToGuest));
    m_pToolBar->addWidget(bottomSpacerWidget);

    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_CopyToHost), &QAction::triggered,
            this, &UIGuestControlFileManager::sltCopyHostToGuest);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_CopyToGuest), &QAction::triggered,
             this, &UIGuestControlFileManager::sltCopyGuestToHost);

    layout ->addWidget(m_pToolBar);
}

void UIGuestControlFileManager::prepareConnections()
{
    if (m_pQtGuestListener)
    {
        connect(m_pQtGuestListener->getWrapped(), &UIMainEventListener::sigGuestSessionUnregistered,
                this, &UIGuestControlFileManager::sltGuestSessionUnregistered);
    }
    if (m_pSessionPanel)
    {
        connect(m_pSessionPanel, &UIGuestControlFileManagerSessionPanel::sigCreateSession,
                this, &UIGuestControlFileManager::sltCreateSession);
        connect(m_pSessionPanel, &UIGuestControlFileManagerSessionPanel::sigCloseSession,
                this, &UIGuestControlFileManager::sltCloseSession);
    }
}

void UIGuestControlFileManager::prepareToolBar()
{
    /* Create toolbar: */
    m_pToolBar = new UIToolBar(parentWidget());
    if (m_pToolBar)
    {
        /* Configure toolbar: */
        const int iIconMetric = (int)(QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize));
        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_T_Session));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_T_Settings));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_T_Log));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_T_FileOperations));

        connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_T_Settings), &QAction::toggled,
                this, &UIGuestControlFileManager::sltPanelActionToggled);
        connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_T_Log), &QAction::toggled,
                this, &UIGuestControlFileManager::sltPanelActionToggled);
        connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_T_Session), &QAction::toggled,
                this, &UIGuestControlFileManager::sltPanelActionToggled);

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


void UIGuestControlFileManager::sltGuestSessionUnregistered(CGuestSession guestSession)
{
    if (guestSession.isNull())
        return;
    if (guestSession == m_comGuestSession && !m_comGuestSession.isNull())
    {
        m_comGuestSession.detach();
        postSessionClosed();
    }
}

void UIGuestControlFileManager::sltCreateSession(QString strUserName, QString strPassword)
{
    if (!UIGuestControlInterface::isGuestAdditionsAvailable(m_comGuest))
    {
        appendLog("Could not find Guest Additions", FileManagerLogType_Error);
        postSessionClosed();
        return;
    }
    if (strUserName.isEmpty())
    {
        appendLog("No user name is given", FileManagerLogType_Error);
        return;
    }
    createSession(strUserName, strPassword);
}

void UIGuestControlFileManager::sltCloseSession()
{
    if (!m_comGuestSession.isOk())
    {
        appendLog("Guest session is not valid", FileManagerLogType_Error);
        postSessionClosed();
        return;
    }
    if (m_pGuestFileTable)
        m_pGuestFileTable->reset();

    if (m_comGuestSession.isOk() && m_pQtSessionListener && m_comSessionListener.isOk())
        cleanupListener(m_pQtSessionListener, m_comSessionListener, m_comGuestSession.GetEventSource());

    m_comGuestSession.Close();
    appendLog("Guest session is closed", FileManagerLogType_Info);
    postSessionClosed();
}

void UIGuestControlFileManager::sltGuestSessionStateChanged(const CGuestSessionStateChangedEvent &cEvent)
{
    if (cEvent.isOk() /*&& m_comGuestSession.isOk()*/)
    {
        CVirtualBoxErrorInfo cErrorInfo = cEvent.GetError();
        if (cErrorInfo.isOk())
            appendLog(cErrorInfo.GetText(), FileManagerLogType_Error);
    }
    if (m_comGuestSession.GetStatus() == KGuestSessionStatus_Started)
    {
        initFileTable();
        postSessionCreated();
    }
    else
    {
        appendLog("Session status has changed", FileManagerLogType_Info);
    }
}

void UIGuestControlFileManager::sltReceieveLogOutput(QString strOutput, FileManagerLogType eLogType)
{
    appendLog(strOutput, eLogType);
}

void UIGuestControlFileManager::sltCopyGuestToHost()
{
    if (!m_pGuestFileTable || !m_pHostFileTable)
        return;
    QString hostDestinationPath = m_pHostFileTable->currentDirectoryPath();
    m_pGuestFileTable->copyGuestToHost(hostDestinationPath);
    m_pHostFileTable->refresh();
}

void UIGuestControlFileManager::sltCopyHostToGuest()
{
    if (!m_pGuestFileTable || !m_pHostFileTable)
        return;
    QStringList hostSourcePathList = m_pHostFileTable->selectedItemPathList();
    m_pGuestFileTable->copyHostToGuest(hostSourcePathList);
    m_pGuestFileTable->refresh();
}

void UIGuestControlFileManager::sltPanelActionToggled(bool fChecked)
{
    QAction *pSenderAction = qobject_cast<QAction*>(sender());
    if (!pSenderAction)
        return;
    UIGuestControlFileManagerPanel* pPanel = 0;
    /* Look for the sender() within the m_panelActionMap's values: */
    for (QMap<UIGuestControlFileManagerPanel*, QAction*>::const_iterator iterator = m_panelActionMap.begin();
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

void UIGuestControlFileManager::sltListDirectoriesBeforeChanged()
{
    if (m_pGuestFileTable)
        m_pGuestFileTable->relist();
    if (m_pHostFileTable)
        m_pHostFileTable->relist();
}

void UIGuestControlFileManager::initFileTable()
{
    if (!m_comGuestSession.isOk() || m_comGuestSession.GetStatus() != KGuestSessionStatus_Started)
        return;
    if (!m_pGuestFileTable)
        return;
    m_pGuestFileTable->initGuestFileTable(m_comGuestSession);
}

void UIGuestControlFileManager::postSessionCreated()
{
    if (m_pSessionPanel)
        m_pSessionPanel->switchSessionCloseMode();
    if (m_pGuestFileTable)
        m_pGuestFileTable->setEnabled(true);
    if (m_pToolBar)
        m_pToolBar->setEnabled(true);
}

void UIGuestControlFileManager::postSessionClosed()
{
    if (m_pSessionPanel)
        m_pSessionPanel->switchSessionCreateMode();
    if (m_pGuestFileTable)
        m_pGuestFileTable->setEnabled(false);
    if (m_pToolBar)
        m_pToolBar->setEnabled(false);

}


bool UIGuestControlFileManager::createSession(const QString& strUserName, const QString& strPassword,
                                              const QString& strDomain /* not used currently */)
{
    if (!m_comGuest.isOk())
        return false;
    m_comGuestSession = m_comGuest.CreateSession(strUserName, strPassword,
                                                               strDomain, "File Manager Session");

    if (!m_comGuestSession.isOk())
    {
        appendLog(UIErrorString::formatErrorInfo(m_comGuestSession), FileManagerLogType_Error);
        return false;
    }
    appendLog("Guest session has been created", FileManagerLogType_Info);
    if (m_pSessionPanel)
        m_pSessionPanel->switchSessionCloseMode();

    /* Prepare session listener */
    QVector<KVBoxEventType> eventTypes;
    eventTypes << KVBoxEventType_OnGuestSessionStateChanged;
    //<< KVBoxEventType_OnGuestProcessRegistered;
    prepareListener(m_pQtSessionListener, m_comSessionListener,
                    m_comGuestSession.GetEventSource(), eventTypes);

    /* Connect to session listener */
    qRegisterMetaType<CGuestSessionStateChangedEvent>();


    connect(m_pQtSessionListener->getWrapped(), &UIMainEventListener::sigGuestSessionStatedChanged,
            this, &UIGuestControlFileManager::sltGuestSessionStateChanged);
     /* Wait session to start. For some reason we cannot get GuestSessionStatusChanged event
        consistently. So we wait: */
    appendLog("Waiting the session to start", FileManagerLogType_Info);
    const ULONG waitTimeout = 2000;
    KGuestSessionWaitResult waitResult = m_comGuestSession.WaitFor(KGuestSessionWaitForFlag_Start, waitTimeout);
    if (waitResult != KGuestSessionWaitResult_Start)
    {
        appendLog("The session did not start", FileManagerLogType_Error);
        sltCloseSession();
        return false;
    }
    return true;
}

void UIGuestControlFileManager::prepareListener(ComObjPtr<UIMainEventListenerImpl> &QtListener,
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
    comEventSource.RegisterListener(comEventListener, eventTypes,
        gEDataManager->eventHandlingType() == EventHandlingType_Active ? TRUE : FALSE);

    /* If event listener registered as passive one: */
    if (gEDataManager->eventHandlingType() == EventHandlingType_Passive)
    {
        /* Register event sources in their listeners as well: */
        QtListener->getWrapped()->registerSource(comEventSource, comEventListener);
    }
}

void UIGuestControlFileManager::cleanupListener(ComObjPtr<UIMainEventListenerImpl> &QtListener,
                                                CEventListener &comEventListener,
                                                CEventSource comEventSource)
{
    if (!comEventSource.isOk())
        return;
    /* If event listener registered as passive one: */
    if (gEDataManager->eventHandlingType() == EventHandlingType_Passive)
    {
        /* Unregister everything: */
        QtListener->getWrapped()->unregisterSources();
    }

    /* Make sure VBoxSVC is available: */
    if (!vboxGlobal().isVBoxSVCAvailable())
        return;

    /* Unregister event listener for CProgress event source: */
    comEventSource.UnregisterListener(comEventListener);
}

template<typename T>
QStringList UIGuestControlFileManager::getFsObjInfoStringList(const T &fsObjectInfo) const
{
    QStringList objectInfo;
    if (!fsObjectInfo.isOk())
        return objectInfo;

    objectInfo << fsObjectInfo.GetName();

    return objectInfo;
}

void UIGuestControlFileManager::saveSettings()
{
    /* Save a list of currently visible panels: */
    QStringList strNameList;
    foreach(UIGuestControlFileManagerPanel* pPanel, m_visiblePanelsList)
        strNameList.append(pPanel->panelName());
    gEDataManager->setGuestControlFileManagerVisiblePanels(strNameList);
}

void UIGuestControlFileManager::loadSettings()
{
    /* Load the visible panel list and show them: */
    QStringList strNameList = gEDataManager->guestControlFileManagerVisiblePanels();
    foreach(const QString strName, strNameList)
    {
        foreach(UIGuestControlFileManagerPanel* pPanel, m_panelActionMap.keys())
        {
            if (strName == pPanel->panelName())
            {
                showPanel(pPanel);
                break;
            }
        }
    }
}

void UIGuestControlFileManager::hidePanel(UIGuestControlFileManagerPanel* panel)
{
    if (panel && panel->isVisible())
        panel->setVisible(false);
    QMap<UIGuestControlFileManagerPanel*, QAction*>::iterator iterator = m_panelActionMap.find(panel);
    if (iterator != m_panelActionMap.end())
    {
        if (iterator.value() && iterator.value()->isChecked())
            iterator.value()->setChecked(false);
    }
    m_visiblePanelsList.removeOne(panel);
    manageEscapeShortCut();
}

void UIGuestControlFileManager::showPanel(UIGuestControlFileManagerPanel* panel)
{
    if (panel && panel->isHidden())
        panel->setVisible(true);
    QMap<UIGuestControlFileManagerPanel*, QAction*>::iterator iterator = m_panelActionMap.find(panel);
    if (iterator != m_panelActionMap.end())
    {
        if (!iterator.value()->isChecked())
            iterator.value()->setChecked(true);
    }
    m_visiblePanelsList.push_back(panel);
    manageEscapeShortCut();
}

void UIGuestControlFileManager::manageEscapeShortCut()
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

void UIGuestControlFileManager::appendLog(const QString &strLog, FileManagerLogType eLogType)
{
    if (!m_pLogPanel)
        return;
    m_pLogPanel->appendLog(strLog, eLogType);
}

#include "UIGuestControlFileManager.moc"
