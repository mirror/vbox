/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UISettingsDialogSpecific class implementation
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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
#include <QStackedWidget>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QTimer>

/* GUI includes: */
#include "UISettingsDialogSpecific.h"
#include "UISettingsDefs.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "QIWidgetValidator.h"
#include "VBoxSettingsSelector.h"
#include "UIVirtualBoxEventHandler.h"

#include "UIGlobalSettingsGeneral.h"
#include "UIGlobalSettingsInput.h"
#include "UIGlobalSettingsUpdate.h"
#include "UIGlobalSettingsLanguage.h"
#include "UIGlobalSettingsDisplay.h"
#include "UIGlobalSettingsNetwork.h"
#include "UIGlobalSettingsExtension.h"
#include "UIGlobalSettingsProxy.h"

#include "UIMachineSettingsGeneral.h"
#include "UIMachineSettingsSystem.h"
#include "UIMachineSettingsDisplay.h"
#include "UIMachineSettingsStorage.h"
#include "UIMachineSettingsAudio.h"
#include "UIMachineSettingsNetwork.h"
#include "UIMachineSettingsSerial.h"
#include "UIMachineSettingsParallel.h"
#include "UIMachineSettingsUSB.h"
#include "UIMachineSettingsSF.h"

/* COM includes: */
#include "CUSBController.h"

#if 0 /* Global USB filters are DISABLED now: */
# define ENABLE_GLOBAL_USB
#endif /* Global USB filters are DISABLED now: */

/* Settings page list: */
typedef QList<UISettingsPage*> UISettingsPageList;
typedef QMap<int, UISettingsPage*> UISettingsPageMap;

/* Serializer direction: */
enum UISettingsSerializeDirection
{
    UISettingsSerializeDirection_Load,
    UISettingsSerializeDirection_Save
};

/* QThread reimplementation for loading/saving settings in async mode: */
class UISettingsSerializer : public QThread
{
    Q_OBJECT;

signals:

    /* Signal to notify main GUI thread about process has been started: */
    void sigNotifyAboutProcessStarted();

    /* Signal to notify main GUI thread about some page was processed: */
    void sigNotifyAboutPageProcessed(int iPageId);

    /* Signal to notify main GUI thread about all pages were processed: */
    void sigNotifyAboutPagesProcessed();

public:

    /* Settings serializer instance: */
    static UISettingsSerializer* instance() { return m_pInstance; }

    /* Settings serializer constructor: */
    UISettingsSerializer(QObject *pParent, const QVariant &data, UISettingsSerializeDirection direction)
        : QThread(pParent)
        , m_direction(direction)
        , m_data(data)
        , m_fSavingComplete(m_direction == UISettingsSerializeDirection_Load)
        , m_fAllowToDestroySerializer(m_direction == UISettingsSerializeDirection_Load)
        , m_iIdOfHighPriorityPage(-1)
    {
        /* Set instance: */
        m_pInstance = this;

        /* Connecting this signals: */
        connect(this, SIGNAL(sigNotifyAboutPageProcessed(int)), this, SLOT(sltHandleProcessedPage(int)), Qt::QueuedConnection);
        connect(this, SIGNAL(sigNotifyAboutPagesProcessed()), this, SLOT(sltHandleProcessedPages()), Qt::QueuedConnection);
        connect(this, SIGNAL(finished()), this, SLOT(sltDestroySerializer()), Qt::QueuedConnection);
        /* Connecting parent signals: */
        connect(this, SIGNAL(sigNotifyAboutProcessStarted()), parent(), SLOT(sltHandleProcessStarted()), Qt::QueuedConnection);
        connect(this, SIGNAL(sigNotifyAboutPageProcessed(int)), parent(), SLOT(sltHandlePageProcessed()), Qt::QueuedConnection);
    }

    /* Settings serializer destructor: */
    ~UISettingsSerializer()
    {
        /* If serializer is being destructed by it's parent,
         * thread could still be running, we have to wait
         * for it to be finished! */
        if (isRunning())
            wait();

        /* Clear instance: */
        m_pInstance = 0;
    }

    /* Set pages list: */
    void setPageList(const UISettingsPageList &pageList)
    {
        for (int iPageIndex = 0; iPageIndex < pageList.size(); ++iPageIndex)
        {
            UISettingsPage *pPage = pageList[iPageIndex];
            m_pages.insert(pPage->id(), pPage);
        }
    }

    /* Raise priority of page: */
    void raisePriorityOfPage(int iPageId)
    {
        /* If that page is not present or was processed already: */
        if (!m_pages.contains(iPageId) || m_pages[iPageId]->processed())
        {
            /* We just ignoring that request: */
            return;
        }
        else
        {
            /* Else remember which page we should be processed next: */
            m_iIdOfHighPriorityPage = iPageId;
        }
    }

    /* Return current m_data content: */
    QVariant& data() { return m_data; }

public slots:

    void start(Priority priority = InheritPriority)
    {
        /* Notify listeners about we are starting: */
        emit sigNotifyAboutProcessStarted();

        /* If serializer saves settings: */
        if (m_direction == UISettingsSerializeDirection_Save)
        {
            /* We should update internal page cache first: */
            for (int iPageIndex = 0; iPageIndex < m_pages.values().size(); ++iPageIndex)
                m_pages.values()[iPageIndex]->putToCache();
        }

        /* Start async serializing thread: */
        QThread::start(priority);

        /* If serializer saves settings: */
        if (m_direction == UISettingsSerializeDirection_Save)
        {
            /* We should block calling thread until all pages will be saved: */
            while (!m_fSavingComplete)
            {
                /* Lock mutex initially: */
                m_mutex.lock();
                /* Perform idle-processing every 100ms,
                 * and waiting for direct wake up signal: */
                m_condition.wait(&m_mutex, 100);
                /* Process queued signals posted to GUI thread: */
                qApp->processEvents();
                /* Unlock mutex finally: */
                m_mutex.unlock();
            }
            m_fAllowToDestroySerializer = true;
        }
    }

protected slots:

    /* Slot to handle the fact of some page was processed: */
    void sltHandleProcessedPage(int iPageId)
    {
        /* If serializer loads settings: */
        if (m_direction == UISettingsSerializeDirection_Load)
        {
            /* If such page present we should fetch internal page cache: */
            if (m_pages.contains(iPageId))
                m_pages[iPageId]->getFromCache();
        }
    }

    /* Slot to handle the fact of all pages were processed: */
    void sltHandleProcessedPages()
    {
        /* If serializer saves settings: */
        if (m_direction == UISettingsSerializeDirection_Save)
        {
            /* We should flag GUI thread to unlock itself: */
            if (!m_fSavingComplete)
                m_fSavingComplete = true;
        }
    }

    /* Slot to destroy serializer: */
    void sltDestroySerializer()
    {
        /* If not yet all events were processed,
         * we should postpone destruction for now: */
        if (!m_fAllowToDestroySerializer)
            QTimer::singleShot(0, this, SLOT(sltDestroySerializer()));
        else
            deleteLater();
    }

protected:

    /* Settings processor: */
    void run()
    {
        /* Mark all the pages initially as NOT processed: */
        QList<UISettingsPage*> pageList = m_pages.values();
        for (int iPageNumber = 0; iPageNumber < pageList.size(); ++iPageNumber)
            pageList[iPageNumber]->setProcessed(false);

        /* Iterate over the all left settings pages: */
        UISettingsPageMap pages(m_pages);
        while (!pages.empty())
        {
            /* Get required page pointer, protect map by mutex while getting pointer: */
            UISettingsPage *pPage = m_iIdOfHighPriorityPage != -1 && pages.contains(m_iIdOfHighPriorityPage) ?
                                    pages[m_iIdOfHighPriorityPage] : *pages.begin();
            /* Reset request of high priority: */
            if (m_iIdOfHighPriorityPage != -1)
                m_iIdOfHighPriorityPage = -1;
            /* Process this page if its enabled: */
            if (pPage->isEnabled())
            {
                if (m_direction == UISettingsSerializeDirection_Load)
                    pPage->loadToCacheFrom(m_data);
                if (m_direction == UISettingsSerializeDirection_Save)
                    pPage->saveFromCacheTo(m_data);
            }
            /* Remember what page was processed: */
            pPage->setProcessed(true);
            /* Remove processed page from our map: */
            pages.remove(pPage->id());
            /* Notify listeners about page was processed: */
            emit sigNotifyAboutPageProcessed(pPage->id());
            /* If serializer saves settings => wake up GUI thread: */
            if (m_direction == UISettingsSerializeDirection_Save)
                m_condition.wakeAll();
            /* Break further processing if page had failed: */
            if (pPage->failed())
                break;
        }
        /* Notify listeners about all pages were processed: */
        emit sigNotifyAboutPagesProcessed();
        /* If serializer saves settings => wake up GUI thread: */
        if (m_direction == UISettingsSerializeDirection_Save)
            m_condition.wakeAll();
    }

    /* Variables: */
    UISettingsSerializeDirection m_direction;
    QVariant m_data;
    UISettingsPageMap m_pages;
    bool m_fSavingComplete;
    bool m_fAllowToDestroySerializer;
    int m_iIdOfHighPriorityPage;
    QMutex m_mutex;
    QWaitCondition m_condition;
    static UISettingsSerializer *m_pInstance;
};

UISettingsSerializer* UISettingsSerializer::m_pInstance = 0;

UISettingsDialogGlobal::UISettingsDialogGlobal(QWidget *pParent)
    : UISettingsDialog(pParent)
{
    /* Window icon: */
#ifndef Q_WS_MAC
    setWindowIcon(QIcon(":/global_settings_16px.png"));
#endif /* !Q_WS_MAC */

    /* Assign default dialog type: */
    setDialogType(SettingsDialogType_Offline);

    /* Creating settings pages: */
    for (int iPageIndex = GLSettingsPage_General; iPageIndex < GLSettingsPage_MAX; ++iPageIndex)
    {
        if (isPageAvailable(iPageIndex))
        {
            UISettingsPage *pSettingsPage = 0;
            switch (iPageIndex)
            {
                /* General page: */
                case GLSettingsPage_General:
                {
                    pSettingsPage = new UIGlobalSettingsGeneral;
                    addItem(":/machine_32px.png", ":/machine_disabled_32px.png",
                            ":/machine_16px.png", ":/machine_disabled_16px.png",
                            iPageIndex, "#general", pSettingsPage);
                    break;
                }
                /* Input page: */
                case GLSettingsPage_Input:
                {
                    pSettingsPage = new UIGlobalSettingsInput;
                    addItem(":/hostkey_32px.png", ":/hostkey_disabled_32px.png",
                            ":/hostkey_16px.png", ":/hostkey_disabled_16px.png",
                            iPageIndex, "#input", pSettingsPage);
                    break;
                }
                /* Update page: */
                case GLSettingsPage_Update:
                {
                    pSettingsPage = new UIGlobalSettingsUpdate;
                    addItem(":/refresh_32px.png", ":/refresh_disabled_32px.png",
                            ":/refresh_16px.png", ":/refresh_disabled_16px.png",
                            iPageIndex, "#update", pSettingsPage);
                    break;
                }
                /* Language page: */
                case GLSettingsPage_Language:
                {
                    pSettingsPage = new UIGlobalSettingsLanguage;
                    addItem(":/site_32px.png", ":/site_disabled_32px.png",
                            ":/site_16px.png", ":/site_disabled_16px.png",
                            iPageIndex, "#language", pSettingsPage);
                    break;
                }
                /* Display page: */
                case GLSettingsPage_Display:
                {
                    pSettingsPage = new UIGlobalSettingsDisplay;
                    addItem(":/vrdp_32px.png", ":/vrdp_disabled_32px.png",
                            ":/vrdp_16px.png", ":/vrdp_disabled_16px.png",
                            iPageIndex, "#display", pSettingsPage);
                    break;
                }
                /* USB page: */
                case GLSettingsPage_USB:
                {
                    pSettingsPage = new UIMachineSettingsUSB(UISettingsPageType_Global);
                    addItem(":/usb_32px.png", ":/usb_disabled_32px.png",
                            ":/usb_16px.png", ":/usb_disabled_16px.png",
                            iPageIndex, "#usb", pSettingsPage);
                    break;
                }
                /* Network page: */
                case GLSettingsPage_Network:
                {
                    pSettingsPage = new UIGlobalSettingsNetwork;
                    addItem(":/nw_32px.png", ":/nw_disabled_32px.png",
                            ":/nw_16px.png", ":/nw_disabled_16px.png",
                            iPageIndex, "#language", pSettingsPage);
                    break;
                }
                /* Extension page: */
                case GLSettingsPage_Extension:
                {
                    pSettingsPage = new UIGlobalSettingsExtension;
                    addItem(":/extension_pack_32px.png", ":/extension_pack_disabled_32px.png",
                            ":/extension_pack_16px.png", ":/extension_pack_disabled_16px.png",
                            iPageIndex, "#extension", pSettingsPage);
                    break;
                }
                /* Proxy page: */
                case GLSettingsPage_Proxy:
                {
                    pSettingsPage = new UIGlobalSettingsProxy;
                    addItem(":/proxy_32px.png", ":/proxy_disabled_32px.png",
                            ":/proxy_16px.png", ":/proxy_disabled_16px.png",
                            iPageIndex, "#proxy", pSettingsPage);
                    break;
                }
                default:
                    break;
            }
            if (pSettingsPage)
            {
                pSettingsPage->setDialogType(dialogType());
                pSettingsPage->setId(iPageIndex);
            }
        }
    }

    /* Retranslate UI: */
    retranslateUi();

    /* Choose first item by default: */
    m_pSelector->selectById(0);
}

UISettingsDialogGlobal::~UISettingsDialogGlobal()
{
    /* Delete serializer early if exists: */
    if (UISettingsSerializer::instance())
        delete UISettingsSerializer::instance();
}

void UISettingsDialogGlobal::loadData()
{
    /* Call for base-class: */
    UISettingsDialog::loadData();

    /* Prepare global data: */
    qRegisterMetaType<UISettingsDataGlobal>();
    UISettingsDataGlobal data(vboxGlobal().virtualBox().GetSystemProperties(), vboxGlobal().settings());
    /* Create global settings loader,
     * it will load global settings & delete itself in the appropriate time: */
    UISettingsSerializer *pGlobalSettingsLoader = new UISettingsSerializer(this, QVariant::fromValue(data), UISettingsSerializeDirection_Load);
    connect(pGlobalSettingsLoader, SIGNAL(destroyed(QObject*)), this, SLOT(sltMarkLoaded()));
    /* Set pages to be loaded: */
    pGlobalSettingsLoader->setPageList(m_pSelector->settingPages());
    /* Start loader: */
    pGlobalSettingsLoader->start();
}

void UISettingsDialogGlobal::saveData()
{
    /* Call for base-class: */
    UISettingsDialog::saveData();

    /* Get properties and settings: */
    CSystemProperties properties = vboxGlobal().virtualBox().GetSystemProperties();
    VBoxGlobalSettings settings = vboxGlobal().settings();
    /* Prepare global data: */
    qRegisterMetaType<UISettingsDataGlobal>();
    UISettingsDataGlobal data(properties, settings);
    /* Create global settings saver,
     * it will save global settings & delete itself in the appropriate time: */
    UISettingsSerializer *pGlobalSettingsSaver = new UISettingsSerializer(this, QVariant::fromValue(data), UISettingsSerializeDirection_Save);
    /* Set pages to be saved: */
    pGlobalSettingsSaver->setPageList(m_pSelector->settingPages());
    /* Start saver: */
    pGlobalSettingsSaver->start();

    /* Get updated properties & settings: */
    CSystemProperties newProperties = pGlobalSettingsSaver->data().value<UISettingsDataGlobal>().m_properties;
    VBoxGlobalSettings newSettings = pGlobalSettingsSaver->data().value<UISettingsDataGlobal>().m_settings;
    /* If properties are not OK => show the error: */
    if (!newProperties.isOk())
        msgCenter().cannotSetSystemProperties(newProperties);
    /* Else save the new settings if they were changed: */
    else if (!(newSettings == settings))
        vboxGlobal().setSettings(newSettings);

    /* Mark page processed: */
    sltMarkSaved();
}

void UISettingsDialogGlobal::retranslateUi()
{
    /* General page: */
    m_pSelector->setItemText(GLSettingsPage_General, tr("General"));

    /* Input page: */
    m_pSelector->setItemText(GLSettingsPage_Input, tr("Input"));

    /* Update page: */
    m_pSelector->setItemText(GLSettingsPage_Update, tr("Update"));

    /* Language page: */
    m_pSelector->setItemText(GLSettingsPage_Language, tr("Language"));

    /* Display page: */
    m_pSelector->setItemText(GLSettingsPage_Display, tr("Display"));

    /* USB page: */
    m_pSelector->setItemText(GLSettingsPage_USB, tr("USB"));

    /* Network page: */
    m_pSelector->setItemText(GLSettingsPage_Network, tr("Network"));

    /* Extension page: */
    m_pSelector->setItemText(GLSettingsPage_Extension, tr("Extensions"));

    /* Proxy page: */
    m_pSelector->setItemText(GLSettingsPage_Proxy, tr("Proxy"));

    /* Polish the selector: */
    m_pSelector->polish();

    /* Base-class UI translation: */
    UISettingsDialog::retranslateUi();

    /* Set dialog's name: */
    setWindowTitle(title());
}

QString UISettingsDialogGlobal::title() const
{
    return tr("VirtualBox - %1").arg(titleExtension());
}

bool UISettingsDialogGlobal::isPageAvailable(int iPageId)
{
    /* Show the host error message for particular group if present.
     * We don't use the generic cannotLoadGlobalConfig()
     * call here because we want this message to be suppressible: */
    switch (iPageId)
    {
        case GLSettingsPage_USB:
        {
#ifdef ENABLE_GLOBAL_USB
            /* Get the host object: */
            CHost host = vboxGlobal().host();
            /* Show the host error message if any: */
            if (!host.isReallyOk())
                msgCenter().cannotAccessUSB(host);
            /* Check if USB is implemented: */
            CHostUSBDeviceFilterVector filters = host.GetUSBDeviceFilters();
            Q_UNUSED(filters);
            if (host.lastRC() == E_NOTIMPL)
                return false;
#else
            return false;
#endif
            break;
        }
        case GLSettingsPage_Network:
        {
#ifndef VBOX_WITH_NETFLT
            return false;
#endif /* !VBOX_WITH_NETFLT */
            break;
        }
        default:
            break;
    }
    return true;
}

UISettingsDialogMachine::UISettingsDialogMachine(QWidget *pParent, const QString &strMachineId,
                                                 const QString &strCategory, const QString &strControl)
    : UISettingsDialog(pParent)
    , m_strMachineId(strMachineId)
    , m_fAllowResetFirstRunFlag(false)
    , m_fResetFirstRunFlag(false)
{
    /* Window icon: */
#ifndef Q_WS_MAC
    setWindowIcon(QIcon(":/settings_16px.png"));
#endif /* Q_WS_MAC */

    /* Allow to reset first-run flag just when medium enumeration was finished: */
    connect(&vboxGlobal(), SIGNAL(mediumEnumFinished(const VBoxMediaList &)), this, SLOT(sltAllowResetFirstRunFlag()));

    /* Get corresponding machine (required to determine dialog type and page availability): */
    m_machine = vboxGlobal().virtualBox().FindMachine(m_strMachineId);
    AssertMsg(!m_machine.isNull(), ("Can't find corresponding machine!\n"));
    /* Assign current dialog type: */
    setDialogType(determineSettingsDialogType(m_machine.GetSessionState(), m_machine.GetState()));

    /* Creating settings pages: */
    for (int iPageIndex = VMSettingsPage_General; iPageIndex < VMSettingsPage_MAX; ++iPageIndex)
    {
        if (isPageAvailable(iPageIndex))
        {
            UISettingsPage *pSettingsPage = 0;
            switch (iPageIndex)
            {
                /* General page: */
                case VMSettingsPage_General:
                {
                    pSettingsPage = new UIMachineSettingsGeneral;
                    addItem(":/machine_32px.png", ":/machine_disabled_32px.png",
                            ":/machine_16px.png", ":/machine_disabled_16px.png",
                            iPageIndex, "#general", pSettingsPage);
                    break;
                }
                /* System page: */
                case VMSettingsPage_System:
                {
                    pSettingsPage = new UIMachineSettingsSystem;
                    connect(pSettingsPage, SIGNAL(tableChanged()), this, SLOT(sltResetFirstRunFlag()));
                    addItem(":/chipset_32px.png", ":/chipset_disabled_32px.png",
                            ":/chipset_16px.png", ":/chipset_disabled_16px.png",
                            iPageIndex, "#system", pSettingsPage);
                    break;
                }
                /* Display page: */
                case VMSettingsPage_Display:
                {
                    pSettingsPage = new UIMachineSettingsDisplay;
                    addItem(":/vrdp_32px.png", ":/vrdp_disabled_32px.png",
                            ":/vrdp_16px.png", ":/vrdp_disabled_16px.png",
                            iPageIndex, "#display", pSettingsPage);
                    break;
                }
                /* Storage page: */
                case VMSettingsPage_Storage:
                {
                    pSettingsPage = new UIMachineSettingsStorage;
                    connect(pSettingsPage, SIGNAL(storageChanged()), this, SLOT(sltResetFirstRunFlag()));
                    addItem(":/hd_32px.png", ":/hd_disabled_32px.png",
                            ":/attachment_16px.png", ":/attachment_disabled_16px.png",
                            iPageIndex, "#storage", pSettingsPage);
                    break;
                }
                /* Audio page: */
                case VMSettingsPage_Audio:
                {
                    pSettingsPage = new UIMachineSettingsAudio;
                    addItem(":/sound_32px.png", ":/sound_disabled_32px.png",
                            ":/sound_16px.png", ":/sound_disabled_16px.png",
                            iPageIndex, "#audio", pSettingsPage);
                    break;
                }
                /* Network page: */
                case VMSettingsPage_Network:
                {
                    pSettingsPage = new UIMachineSettingsNetworkPage;
                    addItem(":/nw_32px.png", ":/nw_disabled_32px.png",
                            ":/nw_16px.png", ":/nw_disabled_16px.png",
                            iPageIndex, "#network", pSettingsPage);
                    break;
                }
                /* Ports page: */
                case VMSettingsPage_Ports:
                {
                    addItem(":/serial_port_32px.png", ":/serial_port_disabled_32px.png",
                            ":/serial_port_16px.png", ":/serial_port_disabled_16px.png",
                            iPageIndex, "#ports");
                    break;
                }
                /* Serial page: */
                case VMSettingsPage_Serial:
                {
                    pSettingsPage = new UIMachineSettingsSerialPage;
                    addItem(":/serial_port_32px.png", ":/serial_port_disabled_32px.png",
                            ":/serial_port_16px.png", ":/serial_port_disabled_16px.png",
                            iPageIndex, "#serialPorts", pSettingsPage, VMSettingsPage_Ports);
                    break;
                }
                /* Parallel page: */
                case VMSettingsPage_Parallel:
                {
                    pSettingsPage = new UIMachineSettingsParallelPage;
                    addItem(":/parallel_port_32px.png", ":/parallel_port_disabled_32px.png",
                            ":/parallel_port_16px.png", ":/parallel_port_disabled_16px.png",
                            iPageIndex, "#parallelPorts", pSettingsPage, VMSettingsPage_Ports);
                    break;
                }
                /* USB page: */
                case VMSettingsPage_USB:
                {
                    pSettingsPage = new UIMachineSettingsUSB(UISettingsPageType_Machine);
                    addItem(":/usb_32px.png", ":/usb_disabled_32px.png",
                            ":/usb_16px.png", ":/usb_disabled_16px.png",
                            iPageIndex, "#usb", pSettingsPage, VMSettingsPage_Ports);
                    break;
                }
                /* Shared Folders page: */
                case VMSettingsPage_SF:
                {
                    pSettingsPage = new UIMachineSettingsSF;
                    addItem(":/shared_folder_32px.png", ":/shared_folder_disabled_32px.png",
                            ":/shared_folder_16px.png", ":/shared_folder_disabled_16px.png",
                            iPageIndex, "#sharedFolders", pSettingsPage);
                    break;
                }
                default:
                    break;
            }
            if (pSettingsPage)
            {
                pSettingsPage->setDialogType(dialogType());
                pSettingsPage->setId(iPageIndex);
            }
        }
    }

    /* Retranslate UI: */
    retranslateUi();

    /* Setup settings dialog: */
    if (!strCategory.isNull())
    {
        m_pSelector->selectByLink(strCategory);
        /* Search for a widget with the given name: */
        if (!strControl.isNull())
        {
            if (QWidget *pWidget = m_pStack->currentWidget()->findChild<QWidget*>(strControl))
            {
                QList<QWidget*> parents;
                QWidget *pParentWidget = pWidget;
                while ((pParentWidget = pParentWidget->parentWidget()) != 0)
                {
                    if (QTabWidget *pTabWidget = qobject_cast<QTabWidget*>(pParentWidget))
                    {
                        /* The tab contents widget is two steps down
                         * (QTabWidget -> QStackedWidget -> QWidget): */
                        QWidget *pTabPage = parents[parents.count() - 1];
                        if (pTabPage)
                            pTabPage = parents[parents.count() - 2];
                        if (pTabPage)
                            pTabWidget->setCurrentWidget(pTabPage);
                    }
                    parents.append(pParentWidget);
                }
                pWidget->setFocus();
            }
        }
    }
    /* First item as default: */
    else
        m_pSelector->selectById(0);
}

UISettingsDialogMachine::~UISettingsDialogMachine()
{
    /* Delete serializer early if exists: */
    if (UISettingsSerializer::instance())
        delete UISettingsSerializer::instance();
}

void UISettingsDialogMachine::loadData()
{
    /* Check that session is NOT created: */
    if (!m_session.isNull())
        return;

    /* Call for base-class: */
    UISettingsDialog::loadData();

    /* Disconnect global VBox events from this dialog: */
    gVBoxEvents->disconnect(this);

    /* Prepare session: */
    m_session = dialogType() == SettingsDialogType_Wrong ? CSession() : vboxGlobal().openExistingSession(m_strMachineId);
    /* Check that session was created: */
    if (m_session.isNull())
        return;

    /* Get machine from session: */
    m_machine = m_session.GetMachine();
    /* Get console from session: */
    m_console = dialogType() == SettingsDialogType_Offline ? CConsole() : m_session.GetConsole();

    /* Prepare machine data: */
    qRegisterMetaType<UISettingsDataMachine>();
    UISettingsDataMachine data(m_machine, m_console);
    /* Create machine settings loader,
     * it will load machine settings & delete itself in the appropriate time: */
    UISettingsSerializer *pMachineSettingsLoader = new UISettingsSerializer(this, QVariant::fromValue(data), UISettingsSerializeDirection_Load);
    connect(pMachineSettingsLoader, SIGNAL(destroyed(QObject*)), this, SLOT(sltMarkLoaded()));
    connect(pMachineSettingsLoader, SIGNAL(sigNotifyAboutPagesProcessed()), this, SLOT(sltSetFirstRunFlag()));
    /* Set pages to be loaded: */
    pMachineSettingsLoader->setPageList(m_pSelector->settingPages());
    /* Ask to raise required page priority: */
    pMachineSettingsLoader->raisePriorityOfPage(m_pSelector->currentId());
    /* Start page loader: */
    pMachineSettingsLoader->start();
}

void UISettingsDialogMachine::saveData()
{
    /* Check that session is NOT created: */
    if (!m_session.isNull())
        return;

    /* Call for base-class: */
    UISettingsDialog::saveData();

    /* Disconnect global VBox events from this dialog: */
    gVBoxEvents->disconnect(this);

    /* Prepare session: */
    if (dialogType() == SettingsDialogType_Wrong)
        m_session = CSession();
    else if (dialogType() != SettingsDialogType_Offline)
        m_session = vboxGlobal().openExistingSession(m_strMachineId);
    else
        m_session = vboxGlobal().openSession(m_strMachineId);
    /* Check that session was created: */
    if (m_session.isNull())
        return;

    /* Get machine from session: */
    m_machine = m_session.GetMachine();
    /* Get console from session: */
    m_console = dialogType() == SettingsDialogType_Offline ? CConsole() : m_session.GetConsole();

    /* Prepare machine data: */
    qRegisterMetaType<UISettingsDataMachine>();
    UISettingsDataMachine data(m_machine, m_console);
    /* Create machine settings saver,
     * it will save machine settings & delete itself in the appropriate time: */
    UISettingsSerializer *pMachineSettingsSaver = new UISettingsSerializer(this, QVariant::fromValue(data), UISettingsSerializeDirection_Save);
    /* Set pages to be saved: */
    pMachineSettingsSaver->setPageList(m_pSelector->settingPages());
    /* Start saver: */
    pMachineSettingsSaver->start();

    /* Get updated machine: */
    m_machine = pMachineSettingsSaver->data().value<UISettingsDataMachine>().m_machine;
    /* If machine is ok => perform final operations: */
    if (m_machine.isOk())
    {
        /* Guest OS type & VT-x/AMD-V option correlation auto-fix: */
        UIMachineSettingsGeneral *pGeneralPage =
            qobject_cast<UIMachineSettingsGeneral*>(m_pSelector->idToPage(VMSettingsPage_General));
        UIMachineSettingsSystem *pSystemPage =
            qobject_cast<UIMachineSettingsSystem*>(m_pSelector->idToPage(VMSettingsPage_System));
        if (pGeneralPage && pSystemPage &&
            pGeneralPage->is64BitOSTypeSelected() && !pSystemPage->isHWVirtExEnabled())
            m_machine.SetHWVirtExProperty(KHWVirtExPropertyType_Enabled, true);

#ifdef VBOX_WITH_VIDEOHWACCEL
        /* Disable 2D Video Acceleration for non-Windows guests: */
        if (pGeneralPage && !pGeneralPage->isWindowsOSTypeSelected())
        {
            UIMachineSettingsDisplay *pDisplayPage =
                qobject_cast<UIMachineSettingsDisplay*>(m_pSelector->idToPage(VMSettingsPage_Display));
            if (pDisplayPage && pDisplayPage->isAcceleration2DVideoSelected())
                m_machine.SetAccelerate2DVideoEnabled(false);
        }
#endif /* VBOX_WITH_VIDEOHWACCEL */

        /* Enable OHCI controller if HID is enabled: */
        if (pSystemPage && pSystemPage->isHIDEnabled())
        {
            CUSBController controller = m_machine.GetUSBController();
            if (!controller.isNull())
                controller.SetEnabled(true);
        }

        /* Clear the "GUI_FirstRun" extra data key in case if
         * the boot order or disk configuration were changed: */
        if (m_fResetFirstRunFlag)
            m_machine.SetExtraData(GUI_FirstRun, QString::null);

        /* Save settings finally: */
        m_machine.SaveSettings();
    }

    /* If machine is NOT ok => show the error message: */
    if (!m_machine.isOk())
        msgCenter().cannotSaveMachineSettings(m_machine);

    /* Mark page processed: */
    sltMarkSaved();
}

void UISettingsDialogMachine::retranslateUi()
{
    /* We have to make sure that the Network, Serial & Parallel pages are retranslated
     * before they are revalidated. Cause: They do string comparing within
     * vboxGlobal which is retranslated at that point already: */
    QEvent event(QEvent::LanguageChange);
    if (QWidget *pPage = m_pSelector->idToPage(VMSettingsPage_Network))
        qApp->sendEvent(pPage, &event);
    if (QWidget *pPage = m_pSelector->idToPage(VMSettingsPage_Serial))
        qApp->sendEvent(pPage, &event);
    if (QWidget *pPage = m_pSelector->idToPage(VMSettingsPage_Parallel))
        qApp->sendEvent(pPage, &event);

    /* General page: */
    m_pSelector->setItemText(VMSettingsPage_General, tr("General"));

    /* System page: */
    m_pSelector->setItemText(VMSettingsPage_System, tr("System"));

    /* Display page: */
    m_pSelector->setItemText(VMSettingsPage_Display, tr("Display"));

    /* Storage page: */
    m_pSelector->setItemText(VMSettingsPage_Storage, tr("Storage"));

    /* Audio page: */
    m_pSelector->setItemText(VMSettingsPage_Audio, tr("Audio"));

    /* Network page: */
    m_pSelector->setItemText(VMSettingsPage_Network, tr("Network"));

    /* Ports page: */
    m_pSelector->setItemText(VMSettingsPage_Ports, tr("Ports"));

    /* Serial page: */
    m_pSelector->setItemText(VMSettingsPage_Serial, tr("Serial Ports"));

    /* Parallel page: */
    m_pSelector->setItemText(VMSettingsPage_Parallel, tr("Parallel Ports"));

    /* USB page: */
    m_pSelector->setItemText(VMSettingsPage_USB, tr("USB"));

    /* SFolders page: */
    m_pSelector->setItemText(VMSettingsPage_SF, tr("Shared Folders"));

    /* Polish the selector: */
    m_pSelector->polish();

    /* Base-class UI translation: */
    UISettingsDialog::retranslateUi();

    /* Set dialog's name: */
    setWindowTitle(title());
}

QString UISettingsDialogMachine::title() const
{
    QString strDialogTitle;
    /* Get corresponding machine (required to compose dialog title): */
    const CMachine &machine = vboxGlobal().virtualBox().FindMachine(m_strMachineId);
    if (!machine.isNull())
        strDialogTitle = tr("%1 - %2").arg(machine.GetName()).arg(titleExtension());
    return strDialogTitle;
}

void UISettingsDialogMachine::recorrelate(UISettingsPage *pSettingsPage)
{
    switch (pSettingsPage->id())
    {
        case VMSettingsPage_General:
        {
            UIMachineSettingsGeneral *pGeneralPage = qobject_cast<UIMachineSettingsGeneral*>(pSettingsPage);
            UIMachineSettingsSystem *pSystemPage = qobject_cast<UIMachineSettingsSystem*>(m_pSelector->idToPage(VMSettingsPage_System));
            if (pGeneralPage && pSystemPage)
                pGeneralPage->setHWVirtExEnabled(pSystemPage->isHWVirtExEnabled());
            break;
        }
        case VMSettingsPage_Display:
        {
            UIMachineSettingsDisplay *pDisplayPage = qobject_cast<UIMachineSettingsDisplay*>(pSettingsPage);
            UIMachineSettingsGeneral *pGeneralPage = qobject_cast<UIMachineSettingsGeneral*>(m_pSelector->idToPage(VMSettingsPage_General));
            if (pDisplayPage && pGeneralPage)
                pDisplayPage->setGuestOSType(pGeneralPage->guestOSType());
            break;
        }
        case VMSettingsPage_System:
        {
            UIMachineSettingsSystem *pSystemPage = qobject_cast<UIMachineSettingsSystem*>(pSettingsPage);
            UIMachineSettingsUSB *pUsbPage = qobject_cast<UIMachineSettingsUSB*>(m_pSelector->idToPage(VMSettingsPage_USB));
            if (pSystemPage && pUsbPage)
                pSystemPage->setOHCIEnabled(pUsbPage->isOHCIEnabled());
            break;
        }
        case VMSettingsPage_Storage:
        {
            UIMachineSettingsStorage *pStoragePage = qobject_cast<UIMachineSettingsStorage*>(pSettingsPage);
            UIMachineSettingsSystem *pSystemPage = qobject_cast<UIMachineSettingsSystem*>(m_pSelector->idToPage(VMSettingsPage_System));
            if (pStoragePage && pSystemPage)
                pStoragePage->setChipsetType(pSystemPage->chipsetType());
            break;
        }
        default:
            break;
    }
}

void UISettingsDialogMachine::sltMarkLoaded()
{
    /* Call for base-class: */
    UISettingsDialog::sltMarkLoaded();

    /* Unlock the session if exists: */
    if (!m_session.isNull())
    {
        m_session.UnlockMachine();
        m_session = CSession();
        m_machine = CMachine();
        m_console = CConsole();
    }

    /* Make sure settings dialog will be updated on machine state/data changes: */
    connect(gVBoxEvents, SIGNAL(sigMachineStateChange(QString, KMachineState)),
            this, SLOT(sltMachineStateChanged(QString, KMachineState)));
    connect(gVBoxEvents, SIGNAL(sigMachineDataChange(QString)),
            this, SLOT(sltMachineDataChanged(QString)));
}

void UISettingsDialogMachine::sltMarkSaved()
{
    /* Call for base-class: */
    UISettingsDialog::sltMarkSaved();

    /* Unlock the session if exists: */
    if (!m_session.isNull())
    {
        m_session.UnlockMachine();
        m_session = CSession();
        m_machine = CMachine();
        m_console = CConsole();
    }
}

void UISettingsDialogMachine::sltMachineStateChanged(QString strMachineId, KMachineState machineState)
{
    /* Ignore if thats NOT our VM: */
    if (strMachineId != m_strMachineId)
        return;

    /* Ignore if state was NOT actually changed: */
    if (m_machineState == machineState)
        return;

    /* Update current machine state: */
    m_machineState = machineState;

    /* Get new dialog type: */
    SettingsDialogType newDialogType = determineSettingsDialogType(m_machine.GetSessionState(), m_machineState);

    /* Ignore if dialog type was NOT actually changed: */
    if (dialogType() == newDialogType)
        return;

    /* Should we show a warning about leaving 'offline' state? */
    bool fShouldWe = dialogType() == SettingsDialogType_Offline;

    /* Update current dialog type: */
    setDialogType(newDialogType);

    /* Show a warning about leaving 'offline' state if we should: */
    if (isSettingsChanged() && fShouldWe)
        msgCenter().warnAboutStateChange(this);
}

void UISettingsDialogMachine::sltMachineDataChanged(QString strMachineId)
{
    /* Ignore if thats NOT our VM: */
    if (strMachineId != m_strMachineId)
        return;

    /* Check if user had changed something and warn him about he will loose settings on reloading: */
    if (isSettingsChanged() && !msgCenter().confirmedSettingsReloading(this))
        return;

    /* Reload data: */
    loadData();
}

void UISettingsDialogMachine::sltCategoryChanged(int cId)
{
    if (UISettingsSerializer::instance())
        UISettingsSerializer::instance()->raisePriorityOfPage(cId);

    UISettingsDialog::sltCategoryChanged(cId);
}

void UISettingsDialogMachine::sltAllowResetFirstRunFlag()
{
    m_fAllowResetFirstRunFlag = true;
}

void UISettingsDialogMachine::sltSetFirstRunFlag()
{
    m_fResetFirstRunFlag = false;
}

void UISettingsDialogMachine::sltResetFirstRunFlag()
{
    if (m_fAllowResetFirstRunFlag)
        m_fResetFirstRunFlag = true;
}

bool UISettingsDialogMachine::isPageAvailable(int iPageId)
{
    if (m_machine.isNull())
        return false;

    /* Show the machine error message for particular group if present.
     * We don't use the generic cannotLoadMachineSettings()
     * call here because we want this message to be suppressible. */
    switch (iPageId)
    {
        case VMSettingsPage_Serial:
        {
            /* Depends on ports availability: */
            if (!isPageAvailable(VMSettingsPage_Ports))
                return false;
            break;
        }
        case VMSettingsPage_Parallel:
        {
            /* Depends on ports availability: */
            if (!isPageAvailable(VMSettingsPage_Ports))
                return false;
            /* But for now this page is always disabled: */
            return false;
        }
        case VMSettingsPage_USB:
        {
            /* Depends on ports availability: */
            if (!isPageAvailable(VMSettingsPage_Ports))
                return false;
            /* Get the USB controller object: */
            CUSBController controller = m_machine.GetUSBController();
            /* Show the machine error message if any: */
            if (!m_machine.isReallyOk() && !controller.isNull() && controller.GetEnabled())
                msgCenter().cannotAccessUSB(m_machine);
            /* Check if USB is implemented: */
            if (controller.isNull() || !controller.GetProxyAvailable())
                return false;
            break;
        }
        default:
            break;
    }
    return true;
}

bool UISettingsDialogMachine::isSettingsChanged()
{
    bool fIsSettingsChanged = false;
    for (int iWidgetNumber = 0; iWidgetNumber < m_pStack->count() && !fIsSettingsChanged; ++iWidgetNumber)
    {
        UISettingsPage *pPage = static_cast<UISettingsPage*>(m_pStack->widget(iWidgetNumber));
        pPage->putToCache();
        if (pPage->changed())
            fIsSettingsChanged = true;
    }
    return fIsSettingsChanged;
}

# include "UISettingsDialogSpecific.moc"

