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
# include <QHBoxLayout>
# include <QHeaderView>
# include <QPlainTextEdit>
# include <QPushButton>
# include <QSplitter>
# include <QGridLayout>

/* GUI includes: */
# include "QILabel.h"
# include "QILineEdit.h"
# include "QIWithRetranslateUI.h"
# include "UIExtraDataManager.h"
# include "UIGuestControlFileManager.h"
# include "UIGuestControlFileTable.h"
# include "UIGuestControlInterface.h"
# include "UIVMInformationDialog.h"
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


class UIGuestSessionCreateWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigCreateSession(QString strUserName, QString strPassword);
    void sigCloseButtonClick();

public:

    UIGuestSessionCreateWidget(QWidget *pParent = 0);
    void switchSessionCreateMode();
    void switchSessionCloseMode();

protected:

    void retranslateUi();

private slots:

    void sltCreateButtonClick();

private:
    void         prepareWidgets();
    QILineEdit   *m_pUserNameEdit;
    QILineEdit   *m_pPasswordEdit;

    QILabel      *m_pUserNameLabel;
    QILabel      *m_pPasswordLabel;
    QPushButton  *m_pCreateButton;
    QPushButton  *m_pCloseButton;

    QHBoxLayout *m_pMainLayout;

};



/*********************************************************************************************************************************
*   UIGuestSessionCreateWidget implementation.                                                                                   *
*********************************************************************************************************************************/

UIGuestSessionCreateWidget::UIGuestSessionCreateWidget(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pUserNameEdit(0)
    , m_pPasswordEdit(0)
    , m_pUserNameLabel(0)
    , m_pPasswordLabel(0)
    , m_pCreateButton(0)
    , m_pCloseButton(0)
    , m_pMainLayout(0)
{
    prepareWidgets();
}

void UIGuestSessionCreateWidget::prepareWidgets()
{
    m_pMainLayout = new QHBoxLayout(this);
    if (!m_pMainLayout)
        return;
    m_pUserNameEdit = new QILineEdit;
    if (m_pUserNameEdit)
    {
        m_pMainLayout->addWidget(m_pUserNameEdit);
    }
    m_pUserNameLabel = new QILabel;
    if (m_pUserNameLabel)
    {
        m_pMainLayout->addWidget(m_pUserNameLabel);
    }
    m_pPasswordEdit = new QILineEdit;
    if (m_pPasswordEdit)
    {
        m_pMainLayout->addWidget(m_pPasswordEdit);
    }

    m_pPasswordLabel = new QILabel;
    if (m_pPasswordLabel)
    {
        m_pMainLayout->addWidget(m_pPasswordLabel);
    }

    m_pCreateButton = new QPushButton;
    if (m_pCreateButton)
    {
        m_pMainLayout->addWidget(m_pCreateButton);
        connect(m_pCreateButton, &QPushButton::clicked, this, &UIGuestSessionCreateWidget::sltCreateButtonClick);
    }

    m_pCloseButton = new QPushButton;
    if (m_pCloseButton)
    {
        m_pMainLayout->addWidget(m_pCloseButton);
        connect(m_pCloseButton, &QPushButton::clicked, this, &UIGuestSessionCreateWidget::sigCloseButtonClick);
    }

    retranslateUi();
}

void UIGuestSessionCreateWidget::sltCreateButtonClick()
{
    if(m_pUserNameEdit && m_pPasswordEdit)
        emit sigCreateSession(m_pUserNameEdit->text(), m_pPasswordEdit->text());
}

void UIGuestSessionCreateWidget::retranslateUi()
{
    if (m_pUserNameEdit)
    {
        m_pUserNameEdit->setToolTip(UIVMInformationDialog::tr("User name to authenticate session creation"));
    }
    if (m_pPasswordEdit)
    {
        m_pPasswordEdit->setToolTip(UIVMInformationDialog::tr("Password to authenticate session creation"));
    }
    if (m_pUserNameLabel)
    {
        m_pUserNameLabel->setToolTip(UIVMInformationDialog::tr("User name to authenticate session creation"));
        m_pUserNameLabel->setText(UIVMInformationDialog::tr("User name"));
    }
    if (m_pPasswordLabel)
    {
        m_pPasswordLabel->setToolTip(UIVMInformationDialog::tr("Password to authenticate session creation"));
        m_pPasswordLabel->setText(UIVMInformationDialog::tr("Password"));
    }
    if(m_pCreateButton)
        m_pCreateButton->setText(UIVMInformationDialog::tr("Create Session"));
    if(m_pCloseButton)
        m_pCloseButton->setText(UIVMInformationDialog::tr("Close Session"));

}

void UIGuestSessionCreateWidget::switchSessionCreateMode()
{
    m_pUserNameEdit->setEnabled(true);
    m_pPasswordEdit->setEnabled(true);
    m_pUserNameLabel->setEnabled(true);
    m_pPasswordLabel->setEnabled(true);
    m_pCreateButton->setEnabled(true);
    m_pCloseButton->setEnabled(false);
}

void UIGuestSessionCreateWidget::switchSessionCloseMode()
{
    m_pUserNameEdit->setEnabled(false);
    m_pPasswordEdit->setEnabled(false);
    m_pUserNameLabel->setEnabled(false);
    m_pPasswordLabel->setEnabled(false);
    m_pCreateButton->setEnabled(false);
    m_pCloseButton->setEnabled(true);
}


/*********************************************************************************************************************************
*   UIGuestControlFileManager implementation.                                                                                    *
*********************************************************************************************************************************/

UIGuestControlFileManager::UIGuestControlFileManager(QWidget *pParent, const CGuest &comGuest)
    : QWidget(pParent)
    , m_iMaxRecursionDepth(1)
    , m_comGuest(comGuest)
    , m_pMainLayout(0)
    , m_pVerticalSplitter(0)
    , m_pLogOutput(0)
    , m_pSessionCreateWidget(0)
    , m_pGuestFileTable(0)
    , m_pHostFileTable(0)
{
    prepareGuestListener();
    prepareObjects();
    prepareConnections();
}

UIGuestControlFileManager::~UIGuestControlFileManager()
{
    if(m_comGuest.isOk() && m_pQtGuestListener && m_comGuestListener.isOk())
        cleanupListener(m_pQtGuestListener, m_comGuestListener, m_comGuest.GetEventSource());
    if(m_comGuestSession.isOk() && m_pQtSessionListener && m_comSessionListener.isOk())
        cleanupListener(m_pQtSessionListener, m_comSessionListener, m_comGuestSession.GetEventSource());
}

void UIGuestControlFileManager::prepareGuestListener()
{
    if(m_comGuest.isOk())
    {
        QVector<KVBoxEventType> eventTypes;
        eventTypes << KVBoxEventType_OnGuestSessionRegistered;

        prepareListener(m_pQtGuestListener, m_comGuestListener,
                        m_comGuest.GetEventSource(), eventTypes);
    }
}

void UIGuestControlFileManager::prepareObjects()
{
    /* Create layout: */
    m_pMainLayout = new QVBoxLayout(this);
    if (!m_pMainLayout)
        return;

    /* Configure layout: */
    m_pMainLayout->setSpacing(0);

    m_pSessionCreateWidget = new UIGuestSessionCreateWidget();
    if (m_pSessionCreateWidget)
    {
        m_pMainLayout->addWidget(m_pSessionCreateWidget);
    }

    m_pVerticalSplitter = new QSplitter;
    if (m_pVerticalSplitter)
    {
        m_pMainLayout->addWidget(m_pVerticalSplitter);
        m_pVerticalSplitter->setOrientation(Qt::Vertical);
        m_pVerticalSplitter->setHandleWidth(2);
    }

    QWidget *fileTableContainer = new QWidget;
    QHBoxLayout *containerLayout = new QHBoxLayout;
    if(fileTableContainer)
    {
        if(containerLayout)
        {
            fileTableContainer->setLayout(containerLayout);
            containerLayout->setSpacing(0);
            containerLayout->setContentsMargins(0, 0, 0, 0);
            m_pGuestFileTable = new UIGuestFileTable;
            if (m_pGuestFileTable)
                containerLayout->addWidget(m_pGuestFileTable);
            m_pHostFileTable = new UIHostFileTable;
            if (m_pHostFileTable)
                containerLayout->addWidget(m_pHostFileTable);
        }
        m_pVerticalSplitter->addWidget(fileTableContainer);
    }


    m_pLogOutput = new QPlainTextEdit;
    if (m_pLogOutput)
    {
        //m_pLogOutput->setMaximumHeight(80);
        m_pLogOutput->setReadOnly(true);
        m_pVerticalSplitter->addWidget(m_pLogOutput);
    }


    m_pVerticalSplitter->setStretchFactor(0, 3);
    m_pVerticalSplitter->setStretchFactor(1, 1);
}

void UIGuestControlFileManager::prepareConnections()
{
    if (m_pQtGuestListener)
    {
        connect(m_pQtGuestListener->getWrapped(), &UIMainEventListener::sigGuestSessionUnregistered,
                this, &UIGuestControlFileManager::sltGuestSessionUnregistered, Qt::DirectConnection);
    }
    if(m_pSessionCreateWidget)
    {
        connect(m_pSessionCreateWidget, &UIGuestSessionCreateWidget::sigCreateSession,
                this, &UIGuestControlFileManager::sltCreateSession);
        connect(m_pSessionCreateWidget, &UIGuestSessionCreateWidget::sigCloseButtonClick,
                this, &UIGuestControlFileManager::sltCloseSession);
    }
}

void UIGuestControlFileManager::sltGuestSessionUnregistered(CGuestSession guestSession)
{
    if (!guestSession.isOk())
        return;
    if(guestSession == m_comGuestSession && m_comGuestSession.isOk())
        m_comGuestSession.detach();
    if (m_pSessionCreateWidget)
        m_pSessionCreateWidget->switchSessionCreateMode();
}

void UIGuestControlFileManager::sltCreateSession(QString strUserName, QString strPassword)
{
    if(strUserName.isEmpty())
    {
        m_pLogOutput->appendPlainText("No user name is given");
        return;
    }
    createSession(strUserName, strPassword);
}

void UIGuestControlFileManager::sltCloseSession()
{
    if (!m_comGuestSession.isOk())
    {
        m_pLogOutput->appendPlainText("Guest session is not valid");
        return;
    }
    if (m_pGuestFileTable)
        m_pGuestFileTable->reset();

    if(m_comGuestSession.isOk() && m_pQtSessionListener && m_comSessionListener.isOk())
        cleanupListener(m_pQtSessionListener, m_comSessionListener, m_comGuestSession.GetEventSource());

    m_comGuestSession.Close();
    m_pLogOutput->appendPlainText("Guest session is closed");
    if(m_pSessionCreateWidget)
        m_pSessionCreateWidget->switchSessionCreateMode();
}

void UIGuestControlFileManager::sltGuestSessionStateChanged(const CGuestSessionStateChangedEvent &cEvent)
{
    if (cEvent.isOk() && m_comGuestSession.isOk())// && m_comGuestProcess.GetStatus() == KProcessStatus_Error)
    {
        CVirtualBoxErrorInfo cErrorInfo = cEvent.GetError();
        if (cErrorInfo.isOk())// && cErrorInfo.GetResultCode() != S_OK)
        {
            m_pLogOutput->appendPlainText(cErrorInfo.GetText());
        }
    }
    if (m_comGuestSession.GetStatus() == KGuestSessionStatus_Started)
    {
        initFileTable();
    }
    else
    {
        m_pLogOutput->appendPlainText("Session status has changed");
    }
}

void UIGuestControlFileManager::initFileTable()
{
    if (!m_comGuestSession.isOk() || m_comGuestSession.GetStatus() != KGuestSessionStatus_Started)
        return;
    if (!m_pGuestFileTable)
        return;
    m_pGuestFileTable->initGuestFileTable(m_comGuestSession);
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
        m_pLogOutput->appendPlainText("Guest session could not be created");
        return false;
    }

    m_pLogOutput->appendPlainText("Guest session has been created");
    if(m_pSessionCreateWidget)
        m_pSessionCreateWidget->switchSessionCloseMode();

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
    // /* Wait session to start: */
    // const ULONG waitTimeout = 2000;
    // KGuestSessionWaitResult waitResult = guestSession.WaitFor(KGuestSessionWaitForFlag_Start, waitTimeout);
    // if (waitResult != KGuestSessionWaitResult_Start)
    //     return false;

    // outSession = guestSession;

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
QStringList   UIGuestControlFileManager::getFsObjInfoStringList(const T &fsObjectInfo) const
{
    QStringList objectInfo;
    if (!fsObjectInfo.isOk())
        return objectInfo;

    //objectInfo << QString(UIGuestControlInterface::getFsObjTypeString(fsObjectInfo.GetType()).append("\t"));
    objectInfo << fsObjectInfo.GetName();
    //objectInfo << QString::number(fsObjectInfo.GetObjectSize());

    /* Currently I dont know a way to convert these into a meaningful date/time: */
    // strObjectInfo.append("BirthTime", QString::number(fsObjectInfo.GetBirthTime()));
    // strObjectInfo.append("ChangeTime", QString::number(fsObjectInfo.GetChangeTime()));

    return objectInfo;
}

#include "UIGuestControlFileManager.moc"
