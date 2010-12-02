/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UISettingsDialogSpecific class implementation
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes */
#include <QStackedWidget>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QTimer>

/* Local includes */
#include "UISettingsDialogSpecific.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "QIWidgetValidator.h"
#include "VBoxSettingsSelector.h"

#include "UIGlobalSettingsGeneral.h"
#include "UIGlobalSettingsInput.h"
#include "UIGlobalSettingsUpdate.h"
#include "UIGlobalSettingsLanguage.h"
#include "UIGlobalSettingsNetwork.h"
#include "UIGlobalSettingsExtension.h"

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

public:

    /* Settings serializer instance: */
    static UISettingsSerializer* instance() { return m_pInstance; }

    /* Settings serializer constructor: */
    UISettingsSerializer(QObject *pParent, const QVariant &data, UISettingsSerializeDirection direction)
        : QThread(pParent)
        , m_direction(direction)
        , m_data(data)
        , m_fConditionDone(false)
        , m_fAllowToDestroySerializer(false)
        , m_iPageIdWeAreWaitingFor(-1)
        , m_iIdOfHighPriorityPage(-1)
    {
        /* Connecting thread signals: */
        connect(this, SIGNAL(sigNotifyAboutPageProcessed(int)), this, SLOT(sltHandleProcessedPage(int)), Qt::QueuedConnection);
        connect(this, SIGNAL(sigNotifyAboutPagesProcessed()), this, SLOT(sltHandleProcessedPages()), Qt::QueuedConnection);
        connect(this, SIGNAL(finished()), this, SLOT(sltDestroySerializer()), Qt::QueuedConnection);

        /* Set instance: */
        m_pInstance = this;
    }

    /* Settings serializer destructor: */
    ~UISettingsSerializer()
    {
        /* Reset instance: */
        m_pInstance = 0;

        /* If serializer is being destructed by it's parent,
         * thread could still be running, we have to wait
         * for it to be finished! */
        if (isRunning())
            wait();
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

    /* Blocks calling thread until requested page will be processed: */
    void waitForPageToBeProcessed(int iPageId)
    {
        m_iPageIdWeAreWaitingFor = iPageId;
        blockGUIthread();
    }

    /* Blocks calling thread until all pages will be processed: */
    void waitForPagesToBeProcessed()
    {
        m_iPageIdWeAreWaitingFor = -1;
        blockGUIthread();
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

signals:

    /* Signal to notify main GUI thread about some page was processed: */
    void sigNotifyAboutPageProcessed(int iPageId);

    /* Signal to notify main GUI thread about all pages were processed: */
    void sigNotifyAboutPagesProcessed();

public slots:

    void start(Priority priority = InheritPriority)
    {
        /* If serializer saves settings: */
        if (m_direction == UISettingsSerializeDirection_Save)
        {
            /* We should update internal page cache first: */
            for (int iPageIndex = 0; iPageIndex < m_pages.values().size(); ++iPageIndex)
                m_pages.values()[iPageIndex]->putToCache();
        }
        /* Start async thread: */
        QThread::start(priority);
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
        /* If thats the page we are waiting for,
         * we should flag GUI thread to unlock itself: */
        if (iPageId == m_iPageIdWeAreWaitingFor && !m_fConditionDone)
            m_fConditionDone = true;
    }

    /* Slot to handle the fact of some page was processed: */
    void sltHandleProcessedPages()
    {
        /* We should flag GUI thread to unlock itself: */
        if (!m_fConditionDone)
            m_fConditionDone = true;
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

    /* GUI thread locker: */
    void blockGUIthread()
    {
        m_fConditionDone = false;
        while (!m_fConditionDone)
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

    /* Settings processor: */
    void run()
    {
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
            /* Try to wake up GUI thread, but
             * it can be busy idle-processing for loaded pages: */
            if (!m_fConditionDone)
                m_condition.wakeAll();
        }
        /* Notify listeners about all pages were processed: */
        emit sigNotifyAboutPagesProcessed();
        /* Try to wake up GUI thread, but
         * it can be busy idle-processing loaded pages: */
        if (!m_fConditionDone)
            m_condition.wakeAll();
    }

    /* Variables: */
    UISettingsSerializeDirection m_direction;
    QVariant m_data;
    UISettingsPageMap m_pages;
    bool m_fConditionDone;
    bool m_fAllowToDestroySerializer;
    int m_iPageIdWeAreWaitingFor;
    int m_iIdOfHighPriorityPage;
    QMutex m_mutex;
    QWaitCondition m_condition;
    static UISettingsSerializer *m_pInstance;
};

UISettingsSerializer* UISettingsSerializer::m_pInstance = 0;

UIGLSettingsDlg::UIGLSettingsDlg(QWidget *pParent)
    : UISettingsDialog(pParent)
{
    /* Window icon: */
#ifndef Q_WS_MAC
    setWindowIcon(QIcon(":/global_settings_16px.png"));
#endif /* !Q_WS_MAC */

    /* Creating settings pages: */
    for (int i = GLSettingsPage_General; i < GLSettingsPage_MAX; ++i)
    {
        if (isAvailable(i))
        {
            switch (i)
            {
                /* General page: */
                case GLSettingsPage_General:
                {
                    UISettingsPage *pSettingsPage = new UIGlobalSettingsGeneral;
                    pSettingsPage->setId(i);
                    addItem(":/machine_32px.png", ":/machine_disabled_32px.png",
                            ":/machine_16px.png", ":/machine_disabled_16px.png",
                            i, "#general", pSettingsPage);
                    break;
                }
                /* Input page: */
                case GLSettingsPage_Input:
                {
                    UISettingsPage *pSettingsPage = new UIGlobalSettingsInput;
                    pSettingsPage->setId(i);
                    addItem(":/hostkey_32px.png", ":/hostkey_disabled_32px.png",
                            ":/hostkey_16px.png", ":/hostkey_disabled_16px.png",
                            i, "#input", pSettingsPage);
                    break;
                }
                /* Update page: */
                case GLSettingsPage_Update:
                {
                    UISettingsPage *pSettingsPage = new UIGlobalSettingsUpdate;
                    pSettingsPage->setId(i);
                    addItem(":/refresh_32px.png", ":/refresh_disabled_32px.png",
                            ":/refresh_16px.png", ":/refresh_disabled_16px.png",
                            i, "#update", pSettingsPage);
                    break;
                }
                /* Language page: */
                case GLSettingsPage_Language:
                {
                    UISettingsPage *pSettingsPage = new UIGlobalSettingsLanguage;
                    pSettingsPage->setId(i);
                    addItem(":/site_32px.png", ":/site_disabled_32px.png",
                            ":/site_16px.png", ":/site_disabled_16px.png",
                            i, "#language", pSettingsPage);
                    break;
                }
                /* USB page: */
                case GLSettingsPage_USB:
                {
                    UISettingsPage *pSettingsPage = new UIMachineSettingsUSB(UISettingsPageType_Global);
                    pSettingsPage->setId(i);
                    addItem(":/usb_32px.png", ":/usb_disabled_32px.png",
                            ":/usb_16px.png", ":/usb_disabled_16px.png",
                            i, "#usb", pSettingsPage);
                    break;
                }
                /* Network page: */
                case GLSettingsPage_Network:
                {
                    UISettingsPage *pSettingsPage = new UIGlobalSettingsNetwork;
                    pSettingsPage->setId(i);
                    addItem(":/nw_32px.png", ":/nw_disabled_32px.png",
                            ":/nw_16px.png", ":/nw_disabled_16px.png",
                            i, "#language", pSettingsPage);
                    break;
                }
                /* Extension page: */
                case GLSettingsPage_Extension:
                {
                    UISettingsPage *pSettingsPage = new UIGlobalSettingsExtension;
                    pSettingsPage->setId(i);
                    addItem(":/extension_pack_32px.png", ":/extension_pack_disabled_32px.png",
                            ":/extension_pack_16px.png", ":/extension_pack_disabled_16px.png",
                            i, "#extension", pSettingsPage);
                    break;
                }
                default:
                    break;
            }
        }
    }

    /* Retranslate UI: */
    retranslateUi();

    /* Choose first item by default: */
    m_pSelector->selectById(0);
}

void UIGLSettingsDlg::getFrom()
{
    /* Prepare global data: */
    qRegisterMetaType<UISettingsDataGlobal>();
    UISettingsDataGlobal data(vboxGlobal().virtualBox().GetSystemProperties(), vboxGlobal().settings());
    /* Create global settings loader,
     * it will load global settings & delete itself in the appropriate time: */
    UISettingsSerializer *pGlobalSettingsLoader = new UISettingsSerializer(this, QVariant::fromValue(data), UISettingsSerializeDirection_Load);
    /* Set pages to be loaded: */
    pGlobalSettingsLoader->setPageList(m_pSelector->settingPages());
    /* Start loader: */
    pGlobalSettingsLoader->start();
    /* Wait for just one (first) page to be loaded: */
    pGlobalSettingsLoader->waitForPageToBeProcessed(m_pSelector->currentId());
}

void UIGLSettingsDlg::putBackTo()
{
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
    /* Wait for all pages to be saved: */
    pGlobalSettingsSaver->waitForPagesToBeProcessed();

    /* Get updated properties & settings: */
    CSystemProperties newProperties = pGlobalSettingsSaver->data().value<UISettingsDataGlobal>().m_properties;
    VBoxGlobalSettings newSettings = pGlobalSettingsSaver->data().value<UISettingsDataGlobal>().m_settings;
    /* If properties are not OK => show the error: */
    if (!newProperties.isOk())
        vboxProblem().cannotSetSystemProperties(newProperties);
    /* Else save the new settings if they were changed: */
    else if (!(newSettings == settings))
        vboxGlobal().setSettings(newSettings);
}

void UIGLSettingsDlg::retranslateUi()
{
    /* Set dialog's name: */
    setWindowTitle(title());

    /* General page: */
    m_pSelector->setItemText(GLSettingsPage_General, tr("General"));

    /* Input page: */
    m_pSelector->setItemText(GLSettingsPage_Input, tr("Input"));

    /* Update page: */
    m_pSelector->setItemText(GLSettingsPage_Update, tr("Update"));

    /* Language page: */
    m_pSelector->setItemText(GLSettingsPage_Language, tr("Language"));

    /* USB page: */
    m_pSelector->setItemText(GLSettingsPage_USB, tr("USB"));

    /* Network page: */
    m_pSelector->setItemText(GLSettingsPage_Network, tr("Network"));

    /* Extension page: */
    m_pSelector->setItemText(GLSettingsPage_Extension, tr("Extensions"));

    /* Translate the selector: */
    m_pSelector->polish();

    /* Base-class UI translation: */
    UISettingsDialog::retranslateUi();
}

QString UIGLSettingsDlg::title() const
{
    return tr("VirtualBox - %1").arg(titleExtension());
}

bool UIGLSettingsDlg::isAvailable(int id)
{
    /* Show the host error message for particular group if present.
     * We don't use the generic cannotLoadGlobalConfig()
     * call here because we want this message to be suppressible: */
    switch (id)
    {
        case GLSettingsPage_USB:
        {
#ifdef ENABLE_GLOBAL_USB
            /* Get the host object: */
            CHost host = vboxGlobal().virtualBox().GetHost();
            /* Show the host error message if any: */
            if (!host.isReallyOk())
                vboxProblem().cannotAccessUSB(host);
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

UIVMSettingsDlg::UIVMSettingsDlg(QWidget *pParent,
                                 const CMachine &machine,
                                 const QString &strCategory,
                                 const QString &strControl)
    : UISettingsDialog(pParent)
    , m_machine(machine)
    , m_fAllowResetFirstRunFlag(false)
    , m_fResetFirstRunFlag(false)
{
    /* Window icon: */
#ifndef Q_WS_MAC
    setWindowIcon(QIcon(":/settings_16px.png"));
#endif /* Q_WS_MAC */

    /* Allow to reset first-run flag just when medium enumeration was finished: */
    connect(&vboxGlobal(), SIGNAL(mediumEnumFinished(const VBoxMediaList &)), this, SLOT(sltAllowResetFirstRunFlag()));

    /* Creating settings pages: */
    for (int i = VMSettingsPage_General; i < VMSettingsPage_MAX; ++i)
    {
        if (isAvailable(i))
        {
            switch (i)
            {
                /* General page: */
                case VMSettingsPage_General:
                {
                    UISettingsPage *pSettingsPage = new UIMachineSettingsGeneral;
                    pSettingsPage->setId(i);
                    addItem(":/machine_32px.png", ":/machine_disabled_32px.png",
                            ":/machine_16px.png", ":/machine_disabled_16px.png",
                            i, "#general", pSettingsPage);
                    break;
                }
                /* System page: */
                case VMSettingsPage_System:
                {
                    UISettingsPage *pSettingsPage = new UIMachineSettingsSystem;
                    pSettingsPage->setId(i);
                    connect(pSettingsPage, SIGNAL(tableChanged()), this, SLOT(sltResetFirstRunFlag()));
                    addItem(":/chipset_32px.png", ":/chipset_disabled_32px.png",
                            ":/chipset_16px.png", ":/chipset_disabled_16px.png",
                            i, "#system", pSettingsPage);
                    break;
                }
                /* Display page: */
                case VMSettingsPage_Display:
                {
                    UISettingsPage *pSettingsPage = new UIMachineSettingsDisplay;
                    pSettingsPage->setId(i);
                    addItem(":/vrdp_32px.png", ":/vrdp_disabled_32px.png",
                            ":/vrdp_16px.png", ":/vrdp_disabled_16px.png",
                            i, "#display", pSettingsPage);
                    break;
                }
                /* Storage page: */
                case VMSettingsPage_Storage:
                {
                    UISettingsPage *pSettingsPage = new UIMachineSettingsStorage;
                    pSettingsPage->setId(i);
                    connect(pSettingsPage, SIGNAL(storageChanged()), this, SLOT(sltResetFirstRunFlag()));
                    addItem(":/hd_32px.png", ":/hd_disabled_32px.png",
                            ":/attachment_16px.png", ":/attachment_disabled_16px.png",
                            i, "#storage", pSettingsPage);
                    break;
                }
                /* Audio page: */
                case VMSettingsPage_Audio:
                {
                    UISettingsPage *pSettingsPage = new UIMachineSettingsAudio;
                    pSettingsPage->setId(i);
                    addItem(":/sound_32px.png", ":/sound_disabled_32px.png",
                            ":/sound_16px.png", ":/sound_disabled_16px.png",
                            i, "#audio", pSettingsPage);
                    break;
                }
                /* Network page: */
                case VMSettingsPage_Network:
                {
                    UISettingsPage *pSettingsPage = new UIMachineSettingsNetworkPage;
                    pSettingsPage->setId(i);
                    addItem(":/nw_32px.png", ":/nw_disabled_32px.png",
                            ":/nw_16px.png", ":/nw_disabled_16px.png",
                            i, "#network", pSettingsPage);
                    break;
                }
                /* Ports page: */
                case VMSettingsPage_Ports:
                {
                    addItem(":/serial_port_32px.png", ":/serial_port_disabled_32px.png",
                            ":/serial_port_16px.png", ":/serial_port_disabled_16px.png",
                            i, "#ports");
                    break;
                }
                /* Serial page: */
                case VMSettingsPage_Serial:
                {
                    UISettingsPage *pSettingsPage = new UIMachineSettingsSerialPage;
                    pSettingsPage->setId(i);
                    addItem(":/serial_port_32px.png", ":/serial_port_disabled_32px.png",
                            ":/serial_port_16px.png", ":/serial_port_disabled_16px.png",
                            i, "#serialPorts", pSettingsPage, VMSettingsPage_Ports);
                    break;
                }
                /* Parallel page: */
                case VMSettingsPage_Parallel:
                {
                    UISettingsPage *pSettingsPage = new UIMachineSettingsParallelPage;
                    pSettingsPage->setId(i);
                    addItem(":/parallel_port_32px.png", ":/parallel_port_disabled_32px.png",
                            ":/parallel_port_16px.png", ":/parallel_port_disabled_16px.png",
                            i, "#parallelPorts", pSettingsPage, VMSettingsPage_Ports);
                    break;
                }
                /* USB page: */
                case VMSettingsPage_USB:
                {
                    UISettingsPage *pSettingsPage = new UIMachineSettingsUSB(UISettingsPageType_Machine);
                    pSettingsPage->setId(i);
                    addItem(":/usb_32px.png", ":/usb_disabled_32px.png",
                            ":/usb_16px.png", ":/usb_disabled_16px.png",
                            i, "#usb", pSettingsPage, VMSettingsPage_Ports);
                    break;
                }
                /* Shared Folders page: */
                case VMSettingsPage_SF:
                {
                    UISettingsPage *pSettingsPage = new UIMachineSettingsSF;
                    pSettingsPage->setId(i);
                    addItem(":/shared_folder_32px.png", ":/shared_folder_disabled_32px.png",
                            ":/shared_folder_16px.png", ":/shared_folder_disabled_16px.png",
                            i, "#sfolders", pSettingsPage);
                    break;
                }
                default:
                    break;
            }
        }
    }

    /* Retranslate UI: */
    retranslateUi();

    /* Setup Settings Dialog: */
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

void UIVMSettingsDlg::getFrom()
{
    /* Prepare machine data: */
    qRegisterMetaType<UISettingsDataMachine>();
    UISettingsDataMachine data(m_machine);
    /* Create machine settings loader,
     * it will load machine settings & delete itself in the appropriate time: */
    UISettingsSerializer *pMachineSettingsLoader = new UISettingsSerializer(this, QVariant::fromValue(data), UISettingsSerializeDirection_Load);
    connect(pMachineSettingsLoader, SIGNAL(sigNotifyAboutPagesProcessed()), this, SLOT(sltSetFirstRunFlag()));
    /* Set pages to be loaded: */
    pMachineSettingsLoader->setPageList(m_pSelector->settingPages());
    /* Ask to raise required page priority: */
    pMachineSettingsLoader->raisePriorityOfPage(m_pSelector->currentId());
    /* Start page loader: */
    pMachineSettingsLoader->start();
    /* Wait for just one (required) page to be loaded: */
    pMachineSettingsLoader->waitForPageToBeProcessed(m_pSelector->currentId());
}

void UIVMSettingsDlg::putBackTo()
{
    /* Prepare machine data: */
    qRegisterMetaType<UISettingsDataMachine>();
    UISettingsDataMachine data(m_machine);
    /* Create machine settings saver,
     * it will save machine settings & delete itself in the appropriate time: */
    UISettingsSerializer *pMachineSettingsSaver = new UISettingsSerializer(this, QVariant::fromValue(data), UISettingsSerializeDirection_Save);
    /* Set pages to be saved: */
    pMachineSettingsSaver->setPageList(m_pSelector->settingPages());
    /* Start saver: */
    pMachineSettingsSaver->start();
    /* Wait for all pages to be saved: */
    pMachineSettingsSaver->waitForPagesToBeProcessed();

    /* Get updated machine: */
    m_machine = pMachineSettingsSaver->data().value<UISettingsDataMachine>().m_machine;
    /* If machine is not OK => show the error: */
    if (!m_machine.isOk())
        vboxProblem().cannotSaveMachineSettings(m_machine);

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

#ifndef VBOX_OSE
    /* Enable OHCI controller if HID is enabled: */
    if (pSystemPage && pSystemPage->isHIDEnabled())
    {
        CUSBController controller = m_machine.GetUSBController();
        if (!controller.isNull())
            controller.SetEnabled(true);
    }
#endif /* !VBOX_OSE */

    /* Clear the "GUI_FirstRun" extra data key in case if
     * the boot order or disk configuration were changed: */
    if (m_fResetFirstRunFlag)
        m_machine.SetExtraData(VBoxDefs::GUI_FirstRun, QString::null);
}

void UIVMSettingsDlg::retranslateUi()
{
    /* Set dialog's name: */
    setWindowTitle(title());

    /* We have to make sure that the Serial & Network subpages are retranslated
     * before they are revalidated. Cause: They do string comparing within
     * vboxGlobal which is retranslated at that point already. */
    QEvent event(QEvent::LanguageChange);

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
    if (QWidget *pPage = m_pSelector->idToPage(VMSettingsPage_Network))
        qApp->sendEvent(pPage, &event);

    /* Ports page: */
    m_pSelector->setItemText(VMSettingsPage_Ports, tr("Ports"));

    /* Serial page: */
    m_pSelector->setItemText(VMSettingsPage_Serial, tr("Serial Ports"));
    if (QWidget *pPage = m_pSelector->idToPage(VMSettingsPage_Serial))
        qApp->sendEvent(pPage, &event);

    /* Parallel page: */
    m_pSelector->setItemText(VMSettingsPage_Parallel, tr("Parallel Ports"));
    if (QWidget *pPage = m_pSelector->idToPage(VMSettingsPage_Parallel))
        qApp->sendEvent(pPage, &event);

    /* USB page: */
    m_pSelector->setItemText(VMSettingsPage_USB, tr("USB"));

    /* SFolders page: */
    m_pSelector->setItemText(VMSettingsPage_SF, tr("Shared Folders"));

    /* Translate the selector: */
    m_pSelector->polish();

    /* Base-class UI translation: */
    UISettingsDialog::retranslateUi();

    /* Revalidate all pages to retranslate the warning messages also: */
    QList<QIWidgetValidator*> validators = findChildren<QIWidgetValidator*>();
    for (int i = 0; i < validators.size(); ++i)
    {
        QIWidgetValidator *pValidator = validators[i];
        if (!pValidator->isValid())
            sltRevalidate(pValidator);
    }
}

QString UIVMSettingsDlg::title() const
{
    QString strDialogTitle;
    if (!m_machine.isNull())
        strDialogTitle = tr("%1 - %2").arg(m_machine.GetName()).arg(titleExtension());
    return strDialogTitle;
}

bool UIVMSettingsDlg::recorrelate(QWidget *pPage, QString &strWarning)
{
    /* This method performs correlation option check
     * between different pages of VM Settings dialog: */

    if (pPage == m_pSelector->idToPage(VMSettingsPage_General))
    {
        /* Get General & System pages: */
        UIMachineSettingsGeneral *pGeneralPage =
            qobject_cast<UIMachineSettingsGeneral*>(m_pSelector->idToPage(VMSettingsPage_General));
        UIMachineSettingsSystem *pSystemPage =
            qobject_cast<UIMachineSettingsSystem*>(m_pSelector->idToPage(VMSettingsPage_System));

        /* Guest OS type & VT-x/AMD-V option correlation test: */
        if (pGeneralPage && pSystemPage &&
            pGeneralPage->is64BitOSTypeSelected() && !pSystemPage->isHWVirtExEnabled())
        {
            strWarning = tr(
                "you have selected a 64-bit guest OS type for this VM. As such guests "
                "require hardware virtualization (VT-x/AMD-V), this feature will be enabled "
                "automatically.");
            return true;
        }
    }

#if defined(VBOX_WITH_VIDEOHWACCEL) || defined(VBOX_WITH_CRHGSMI)
    /* 2D Video Acceleration is available for Windows guests only: */
    if (pPage == m_pSelector->idToPage(VMSettingsPage_Display))
    {
        /* Get General & Display pages: */
        UIMachineSettingsGeneral *pGeneralPage =
            qobject_cast<UIMachineSettingsGeneral*>(m_pSelector->idToPage(VMSettingsPage_General));
        UIMachineSettingsDisplay *pDisplayPage =
            qobject_cast<UIMachineSettingsDisplay*>(m_pSelector->idToPage(VMSettingsPage_Display));
#ifdef VBOX_WITH_CRHGSMI
        if (pGeneralPage && pDisplayPage)
        {
            bool bWddmSupported = pGeneralPage->isWddmSupportedForOSType();
            pDisplayPage->setWddmMode(bWddmSupported);
        }
#endif
#ifdef VBOX_WITH_VIDEOHWACCEL
        if (pGeneralPage && pDisplayPage &&
            pDisplayPage->isAcceleration2DVideoSelected() && !pGeneralPage->isWindowsOSTypeSelected())
        {
            strWarning = tr(
                "you have 2D Video Acceleration enabled. As 2D Video Acceleration "
                "is supported for Windows guests only, this feature will be disabled.");
            return true;
        }
#endif
    }
#endif /* VBOX_WITH_VIDEOHWACCEL */

#ifndef VBOX_OSE
    if (pPage == m_pSelector->idToPage(VMSettingsPage_System))
    {
        /* Get System & USB pages: */
        UIMachineSettingsSystem *pSystemPage =
            qobject_cast<UIMachineSettingsSystem*>(m_pSelector->idToPage(VMSettingsPage_System));
        UIMachineSettingsUSB *pUsbPage =
            qobject_cast<UIMachineSettingsUSB*>(m_pSelector->idToPage(VMSettingsPage_USB));
        if (pSystemPage && pUsbPage &&
            pSystemPage->isHIDEnabled() && !pUsbPage->isOHCIEnabled())
        {
            strWarning = tr(
                "you have enabled a USB HID (Human Interface Device). "
                "This will not work unless USB emulation is also enabled. "
                "This will be done automatically when you accept the VM Settings "
                "by pressing the OK button.");
            return true;
        }
    }
#endif /* !VBOX_OSE */

    if (pPage == m_pSelector->idToPage(VMSettingsPage_Storage))
    {
        /* Get System & Storage pages: */
        UIMachineSettingsSystem *pSystemPage =
            qobject_cast<UIMachineSettingsSystem*>(m_pSelector->idToPage(VMSettingsPage_System));
        UIMachineSettingsStorage *pStoragePage =
            qobject_cast<UIMachineSettingsStorage*>(m_pSelector->idToPage(VMSettingsPage_Storage));
        if (pSystemPage && pStoragePage)
        {
            /* Update chiset type for the Storage settings page: */
            if (pStoragePage->chipsetType() != pSystemPage->chipsetType())
                pStoragePage->setChipsetType(pSystemPage->chipsetType());
            /* Check for excessive controllers on Storage page controllers list: */
            QStringList excessiveList;
            QMap<KStorageBus, int> currentType = pStoragePage->currentControllerTypes();
            QMap<KStorageBus, int> maximumType = pStoragePage->maximumControllerTypes();
            for (int iStorageBusType = KStorageBus_IDE; iStorageBusType <= KStorageBus_SAS; ++iStorageBusType)
            {
                if (currentType[(KStorageBus)iStorageBusType] > maximumType[(KStorageBus)iStorageBusType])
                {
                    QString strExcessiveRecord = QString("%1 (%2)");
                    strExcessiveRecord = strExcessiveRecord.arg(QString("<b>%1</b>").arg(vboxGlobal().toString((KStorageBus)iStorageBusType)));
                    strExcessiveRecord = strExcessiveRecord.arg(maximumType[(KStorageBus)iStorageBusType] == 1 ?
                                                                tr("at most one supported") :
                                                                tr("up to %1 supported").arg(maximumType[(KStorageBus)iStorageBusType]));
                    excessiveList << strExcessiveRecord;
                }
            }
            if (!excessiveList.isEmpty())
            {
                strWarning = tr(
                    "you are currently using more storage controllers than a %1 chipset supports. "
                    "Please change the chipset type on the System settings page or reduce the number "
                    "of the following storage controllers on the Storage settings page: %2.")
                    .arg(vboxGlobal().toString(pStoragePage->chipsetType()))
                    .arg(excessiveList.join(", "));
                return false;
            }
        }
    }

    return true;
}

void UIVMSettingsDlg::sltCategoryChanged(int cId)
{
    if (UISettingsSerializer::instance())
        UISettingsSerializer::instance()->raisePriorityOfPage(cId);

    UISettingsDialog::sltCategoryChanged(cId);
}

void UIVMSettingsDlg::sltAllowResetFirstRunFlag()
{
    m_fAllowResetFirstRunFlag = true;
}

void UIVMSettingsDlg::sltSetFirstRunFlag()
{
    m_fResetFirstRunFlag = false;
}

void UIVMSettingsDlg::sltResetFirstRunFlag()
{
    if (m_fAllowResetFirstRunFlag)
        m_fResetFirstRunFlag = true;
}

bool UIVMSettingsDlg::isAvailable(int id)
{
    if (m_machine.isNull())
        return false;

    /* Show the machine error message for particular group if present.
     * We don't use the generic cannotLoadMachineSettings()
     * call here because we want this message to be suppressible. */
    switch (id)
    {
        case VMSettingsPage_Serial:
        {
            /* Depends on ports availability: */
            if (!isAvailable(VMSettingsPage_Ports))
                return false;
            break;
        }
        case VMSettingsPage_Parallel:
        {
            /* Depends on ports availability: */
            if (!isAvailable(VMSettingsPage_Ports))
                return false;
            /* But for now this page is always disabled: */
            return false;
        }
        case VMSettingsPage_USB:
        {
            /* Depends on ports availability: */
            if (!isAvailable(VMSettingsPage_Ports))
                return false;
            /* Get the USB controller object: */
            CUSBController controller = m_machine.GetUSBController();
            /* Show the machine error message if any: */
            if (!m_machine.isReallyOk())
                vboxProblem().cannotAccessUSB(m_machine);
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

# include "UISettingsDialogSpecific.moc"

