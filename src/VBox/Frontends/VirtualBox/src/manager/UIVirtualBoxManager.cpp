/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualBoxManager class implementation.
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
#include <QMenuBar>
#include <QStandardPaths>
#include <QStatusBar>
//# include <QToolButton>

/* GUI includes: */
#include "QIFileDialog.h"
#include "UIActionPoolManager.h"
#include "UICloudMachineSettingsDialog.h"
#include "UICloudNetworkingStuff.h"
#include "UICloudProfileManager.h"
#include "UIDesktopServices.h"
#include "UIExtraDataManager.h"
#include "UIHostNetworkManager.h"
#include "UIMedium.h"
#include "UIMediumManager.h"
#include "UIMessageCenter.h"
#include "UIModalWindowManager.h"
#include "UIQObjectStuff.h"
#include "UISettingsDialogSpecific.h"
#include "UIVirtualBoxManager.h"
#include "UIVirtualBoxManagerWidget.h"
#include "UIVirtualMachineItemCloud.h"
#include "UIVirtualMachineItemLocal.h"
#include "UIVMLogViewerDialog.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIWizardAddCloudVM.h"
#include "UIWizardCloneVM.h"
#include "UIWizardExportApp.h"
#include "UIWizardImportApp.h"
#include "UIWizardNewCloudVM.h"
#include "UIWizardNewVM.h"
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
# include "UIUpdateManager.h"
#endif
#ifdef VBOX_WS_MAC
# include "UIImageTools.h"
# include "UIWindowMenuManager.h"
# include "VBoxUtils.h"
#else
# include "UIMenuBar.h"
#endif
#ifdef VBOX_WS_X11
# include "UIDesktopWidgetWatchdog.h"
#endif

/* COM includes: */
#include "CSystemProperties.h"

/* Other VBox stuff: */
#include <iprt/buildconfig.h>
#include <VBox/version.h>
#ifdef VBOX_WS_X11
# include <iprt/env.h>
#endif /* VBOX_WS_X11 */


/* static */
UIVirtualBoxManager *UIVirtualBoxManager::s_pInstance = 0;

/* static */
void UIVirtualBoxManager::create()
{
    /* Make sure VirtualBox Manager isn't created: */
    AssertReturnVoid(s_pInstance == 0);

    /* Create VirtualBox Manager: */
    new UIVirtualBoxManager;
    /* Prepare VirtualBox Manager: */
    s_pInstance->prepare();
    /* Show VirtualBox Manager: */
    s_pInstance->show();
    /* Register in the modal window manager: */
    windowManager().setMainWindowShown(s_pInstance);
}

/* static */
void UIVirtualBoxManager::destroy()
{
    /* Make sure VirtualBox Manager is created: */
    AssertPtrReturnVoid(s_pInstance);

    /* Unregister in the modal window manager: */
    windowManager().setMainWindowShown(0);
    /* Cleanup VirtualBox Manager: */
    s_pInstance->cleanup();
    /* Destroy machine UI: */
    delete s_pInstance;
}

UIVirtualBoxManager::UIVirtualBoxManager()
    : m_fPolished(false)
    , m_fFirstMediumEnumerationHandled(false)
    , m_pActionPool(0)
    , m_pManagerVirtualMedia(0)
    , m_pManagerHostNetwork(0)
    , m_pManagerCloudProfile(0)
{
    s_pInstance = this;
}

UIVirtualBoxManager::~UIVirtualBoxManager()
{
    s_pInstance = 0;
}

bool UIVirtualBoxManager::shouldBeMaximized() const
{
    return gEDataManager->selectorWindowShouldBeMaximized();
}

#ifdef VBOX_WS_MAC
bool UIVirtualBoxManager::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* Ignore for non-active window except for FileOpen event which should be always processed: */
    if (!isActiveWindow() && pEvent->type() != QEvent::FileOpen)
        return QMainWindowWithRestorableGeometryAndRetranslateUi::eventFilter(pObject, pEvent);

    /* Ignore for other objects: */
    if (qobject_cast<QWidget*>(pObject) &&
        qobject_cast<QWidget*>(pObject)->window() != this)
        return QMainWindowWithRestorableGeometryAndRetranslateUi::eventFilter(pObject, pEvent);

    /* Which event do we have? */
    switch (pEvent->type())
    {
        case QEvent::FileOpen:
        {
            sltHandleOpenUrlCall(QList<QUrl>() << static_cast<QFileOpenEvent*>(pEvent)->url());
            pEvent->accept();
            return true;
            break;
        }
        default:
            break;
    }

    /* Call to base-class: */
    return QMainWindowWithRestorableGeometryAndRetranslateUi::eventFilter(pObject, pEvent);
}
#endif /* VBOX_WS_MAC */

void UIVirtualBoxManager::retranslateUi()
{
    /* Set window title: */
    QString strTitle(VBOX_PRODUCT);
    strTitle += " " + tr("Manager", "Note: main window title which is prepended by the product name.");
#ifdef VBOX_BLEEDING_EDGE
    strTitle += QString(" EXPERIMENTAL build ")
             +  QString(RTBldCfgVersion())
             +  QString(" r")
             +  QString(RTBldCfgRevisionStr())
             +  QString(" - " VBOX_BLEEDING_EDGE);
#endif /* VBOX_BLEEDING_EDGE */
    setWindowTitle(strTitle);
}

bool UIVirtualBoxManager::event(QEvent *pEvent)
{
    /* Which event do we have? */
    switch (pEvent->type())
    {
        /* Handle every ScreenChangeInternal event to notify listeners: */
        case QEvent::ScreenChangeInternal:
        {
            emit sigWindowRemapped();
            break;
        }
        default:
            break;
    }
    /* Call to base-class: */
    return QMainWindowWithRestorableGeometryAndRetranslateUi::event(pEvent);
}

void UIVirtualBoxManager::showEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    QMainWindowWithRestorableGeometryAndRetranslateUi::showEvent(pEvent);

    /* Is polishing required? */
    if (!m_fPolished)
    {
        /* Pass the show-event to polish-event: */
        polishEvent(pEvent);
        /* Mark as polished: */
        m_fPolished = true;
    }
}

void UIVirtualBoxManager::polishEvent(QShowEvent *)
{
    /* Make sure user warned about inaccessible media: */
    QMetaObject::invokeMethod(this, "sltHandleMediumEnumerationFinish", Qt::QueuedConnection);
}

void UIVirtualBoxManager::closeEvent(QCloseEvent *pEvent)
{
    /* Call to base-class: */
    QMainWindowWithRestorableGeometryAndRetranslateUi::closeEvent(pEvent);

    /* Quit application: */
    QApplication::quit();
}

#ifdef VBOX_WS_X11
void UIVirtualBoxManager::sltHandleHostScreenAvailableAreaChange()
{
    /* Prevent handling if fake screen detected: */
    if (gpDesktop->isFakeScreenDetected())
        return;

    /* Restore the geometry cached by the window: */
    const QRect geo = currentGeometry();
    resize(geo.size());
    move(geo.topLeft());
}
#endif /* VBOX_WS_X11 */

void UIVirtualBoxManager::sltHandleMediumEnumerationFinish()
{
#if 0 // ohh, come on!
    /* To avoid annoying the user, we check for inaccessible media just once, after
     * the first media emumeration [started from main() at startup] is complete. */
    if (m_fFirstMediumEnumerationHandled)
        return;
    m_fFirstMediumEnumerationHandled = true;

    /* Make sure MM window/tool is not opened,
     * otherwise user sees everything himself: */
    if (   m_pManagerVirtualMedia
        || m_pWidget->isGlobalToolOpened(UIToolType_Media))
        return;

    /* Look for at least one inaccessible medium: */
    bool fIsThereAnyInaccessibleMedium = false;
    foreach (const QUuid &uMediumID, uiCommon().mediumIDs())
    {
        if (uiCommon().medium(uMediumID).state() == KMediumState_Inaccessible)
        {
            fIsThereAnyInaccessibleMedium = true;
            break;
        }
    }
    /* Warn the user about inaccessible medium, propose to open MM window/tool: */
    if (fIsThereAnyInaccessibleMedium && msgCenter().warnAboutInaccessibleMedia())
    {
        /* Open the MM window: */
        sltOpenVirtualMediumManagerWindow();
    }
#endif
}

void UIVirtualBoxManager::sltHandleOpenUrlCall(QList<QUrl> list /* = QList<QUrl>() */)
{
    /* If passed list is empty, we take the one from UICommon: */
    if (list.isEmpty())
        list = uiCommon().takeArgumentUrls();

    /* Check if we are can handle the dropped urls: */
    for (int i = 0; i < list.size(); ++i)
    {
#ifdef VBOX_WS_MAC
        const QString strFile = ::darwinResolveAlias(list.at(i).toLocalFile());
#else
        const QString strFile = list.at(i).toLocalFile();
#endif
        /* If there is such file exists: */
        if (!strFile.isEmpty() && QFile::exists(strFile))
        {
            /* And has allowed VBox config file extension: */
            if (UICommon::hasAllowedExtension(strFile, VBoxFileExts))
            {
                /* Handle VBox config file: */
                CVirtualBox comVBox = uiCommon().virtualBox();
                CMachine comMachine = comVBox.FindMachine(strFile);
                if (comVBox.isOk() && comMachine.isNotNull())
                    uiCommon().launchMachine(comMachine);
                else
                    openAddMachineDialog(strFile);
            }
            /* And has allowed VBox OVF file extension: */
            else if (UICommon::hasAllowedExtension(strFile, OVFFileExts))
            {
                /* Allow only one file at the time: */
                sltOpenImportApplianceWizard(strFile);
                break;
            }
            /* And has allowed VBox extension pack file extension: */
            else if (UICommon::hasAllowedExtension(strFile, VBoxExtPackFileExts))
            {
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
                /* Prevent update manager from proposing us to update EP: */
                gUpdateManager->setEPInstallationRequested(true);
#endif
                /* Propose the user to install EP described by the arguments @a list. */
                uiCommon().doExtPackInstallation(strFile, QString(), this, NULL);
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
                /* Allow update manager to propose us to update EP: */
                gUpdateManager->setEPInstallationRequested(false);
#endif
            }
        }
    }
}

void UIVirtualBoxManager::sltHandleChooserPaneIndexChange()
{
    updateActionsVisibility();
    updateActionsAppearance();
}

void UIVirtualBoxManager::sltHandleGroupSavingProgressChange()
{
    updateActionsAppearance();
}

void UIVirtualBoxManager::sltHandleToolTypeChange()
{
    updateActionsVisibility();
    updateActionsAppearance();

    /* Make sure separate dialogs are closed when corresponding tools are opened: */
    switch (m_pWidget->toolsType())
    {
        case UIToolType_Media:   sltCloseVirtualMediumManagerWindow(); break;
        case UIToolType_Network: sltCloseHostNetworkManagerWindow(); break;
        case UIToolType_Cloud:   sltCloseCloudProfileManagerWindow(); break;
        case UIToolType_Logs:    sltCloseLogViewerWindow(); break;
        default: break;
    }
}

void UIVirtualBoxManager::sltCurrentSnapshotItemChange()
{
    updateActionsAppearance();
}

void UIVirtualBoxManager::sltHandleCloudMachineStateChange(const QUuid & /* uId */)
{
    updateActionsAppearance();
}

void UIVirtualBoxManager::sltHandleStateChange(const QUuid &)
{
    updateActionsAppearance();
}

void UIVirtualBoxManager::sltOpenVirtualMediumManagerWindow()
{
    /* First check if instance of widget opened the embedded way: */
    if (m_pWidget->isGlobalToolOpened(UIToolType_Media))
    {
        m_pWidget->setToolsType(UIToolType_Welcome);
        m_pWidget->closeGlobalTool(UIToolType_Media);
    }

    /* Create instance if not yet created: */
    if (!m_pManagerVirtualMedia)
    {
        UIMediumManagerFactory(m_pActionPool).prepare(m_pManagerVirtualMedia, this);
        connect(m_pManagerVirtualMedia, &QIManagerDialog::sigClose,
                this, &UIVirtualBoxManager::sltCloseVirtualMediumManagerWindow);
    }

    /* Show instance: */
    m_pManagerVirtualMedia->show();
    m_pManagerVirtualMedia->setWindowState(m_pManagerVirtualMedia->windowState() & ~Qt::WindowMinimized);
    m_pManagerVirtualMedia->activateWindow();
}

void UIVirtualBoxManager::sltCloseVirtualMediumManagerWindow()
{
    /* Destroy instance if still exists: */
    if (m_pManagerVirtualMedia)
        UIMediumManagerFactory().cleanup(m_pManagerVirtualMedia);
}

void UIVirtualBoxManager::sltOpenHostNetworkManagerWindow()
{
    /* First check if instance of widget opened the embedded way: */
    if (m_pWidget->isGlobalToolOpened(UIToolType_Network))
    {
        m_pWidget->setToolsType(UIToolType_Welcome);
        m_pWidget->closeGlobalTool(UIToolType_Network);
    }

    /* Create instance if not yet created: */
    if (!m_pManagerHostNetwork)
    {
        UIHostNetworkManagerFactory(m_pActionPool).prepare(m_pManagerHostNetwork, this);
        connect(m_pManagerHostNetwork, &QIManagerDialog::sigClose,
                this, &UIVirtualBoxManager::sltCloseHostNetworkManagerWindow);
    }

    /* Show instance: */
    m_pManagerHostNetwork->show();
    m_pManagerHostNetwork->setWindowState(m_pManagerHostNetwork->windowState() & ~Qt::WindowMinimized);
    m_pManagerHostNetwork->activateWindow();
}

void UIVirtualBoxManager::sltCloseHostNetworkManagerWindow()
{
    /* Destroy instance if still exists: */
    if (m_pManagerHostNetwork)
        UIHostNetworkManagerFactory().cleanup(m_pManagerHostNetwork);
}

void UIVirtualBoxManager::sltOpenCloudProfileManagerWindow()
{
    /* First check if instance of widget opened the embedded way: */
    if (m_pWidget->isGlobalToolOpened(UIToolType_Cloud))
    {
        m_pWidget->setToolsType(UIToolType_Welcome);
        m_pWidget->closeGlobalTool(UIToolType_Cloud);
    }

    /* Create instance if not yet created: */
    if (!m_pManagerCloudProfile)
    {
        UICloudProfileManagerFactory(m_pActionPool).prepare(m_pManagerCloudProfile, this);
        connect(m_pManagerCloudProfile, &QIManagerDialog::sigClose,
                this, &UIVirtualBoxManager::sltCloseCloudProfileManagerWindow);
        connect(m_pManagerCloudProfile, &QIManagerDialog::sigChange,
                this, &UIVirtualBoxManager::sigCloudProfileManagerChange);
    }

    /* Show instance: */
    m_pManagerCloudProfile->show();
    m_pManagerCloudProfile->setWindowState(m_pManagerCloudProfile->windowState() & ~Qt::WindowMinimized);
    m_pManagerCloudProfile->activateWindow();
}

void UIVirtualBoxManager::sltCloseCloudProfileManagerWindow()
{
    /* Destroy instance if still exists: */
    if (m_pManagerCloudProfile)
        UIHostNetworkManagerFactory().cleanup(m_pManagerCloudProfile);
}

void UIVirtualBoxManager::sltOpenImportApplianceWizard(const QString &strFileName /* = QString() */)
{
    /* Initialize variables: */
#ifdef VBOX_WS_MAC
    const QString strTmpFile = ::darwinResolveAlias(strFileName);
#else
    const QString strTmpFile = strFileName;
#endif

    /* Lock the action preventing cascade calls: */
    UIQObjectPropertySetter guardBlock(actionPool()->action(UIActionIndexST_M_File_S_ImportAppliance), "opened", true);
    connect(&guardBlock, &UIQObjectPropertySetter::sigAboutToBeDestroyed,
            this, &UIVirtualBoxManager::sltHandleUpdateActionAppearanceRequest);
    updateActionsAppearance();

    /* Use the "safe way" to open stack of Mac OS X Sheets: */
    QWidget *pWizardParent = windowManager().realParentWindow(this);
    UISafePointerWizardImportApp pWizard = new UIWizardImportApp(pWizardParent, false /* OCI by default? */, strTmpFile);
    windowManager().registerNewParent(pWizard, pWizardParent);
    pWizard->prepare();
    if (strFileName.isEmpty() || pWizard->isValid())
        pWizard->exec();
    delete pWizard;
}

void UIVirtualBoxManager::sltOpenExportApplianceWizard()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();

    /* Populate the list of VM names: */
    QStringList names;
    for (int i = 0; i < items.size(); ++i)
        names << items.at(i)->name();

    /* Lock the actions preventing cascade calls: */
    UIQObjectPropertySetter guardBlock(QList<QObject*>() << actionPool()->action(UIActionIndexST_M_File_S_ExportAppliance)
                                                         << actionPool()->action(UIActionIndexST_M_Machine_S_ExportToOCI),
                                       "opened", true);
    connect(&guardBlock, &UIQObjectPropertySetter::sigAboutToBeDestroyed,
            this, &UIVirtualBoxManager::sltHandleUpdateActionAppearanceRequest);
    updateActionsAppearance();

    /* Check what was the action invoked us: */
    UIAction *pAction = qobject_cast<UIAction*>(sender());

    /* Use the "safe way" to open stack of Mac OS X Sheets: */
    QWidget *pWizardParent = windowManager().realParentWindow(this);
    UISafePointerWizard pWizard = new UIWizardExportApp(pWizardParent, names,
                                                        pAction &&
                                                        pAction == actionPool()->action(UIActionIndexST_M_Machine_S_ExportToOCI));
    windowManager().registerNewParent(pWizard, pWizardParent);
    pWizard->prepare();
    pWizard->exec();
    delete pWizard;
}

void UIVirtualBoxManager::sltOpenNewCloudVMWizard()
{
    /* Lock the action preventing cascade calls: */
    UIQObjectPropertySetter guardBlock(actionPool()->action(UIActionIndexST_M_File_S_NewCloudVM), "opened", true);
    connect(&guardBlock, &UIQObjectPropertySetter::sigAboutToBeDestroyed,
            this, &UIVirtualBoxManager::sltHandleUpdateActionAppearanceRequest);
    updateActionsAppearance();

    /* Use the "safe way" to open stack of Mac OS X Sheets: */
    QWidget *pWizardParent = windowManager().realParentWindow(this);
    UISafePointerWizardNewCloudVM pWizard = new UIWizardNewCloudVM(pWizardParent);
    windowManager().registerNewParent(pWizard, pWizardParent);
    pWizard->prepare();
    pWizard->exec();
    delete pWizard;
}

#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
void UIVirtualBoxManager::sltOpenExtraDataManagerWindow()
{
    gEDataManager->openWindow(this);
}
#endif /* VBOX_GUI_WITH_EXTRADATA_MANAGER_UI */

void UIVirtualBoxManager::sltOpenPreferencesDialog()
{
    /* Don't show the inaccessible warning
     * if the user tries to open global settings: */
    m_fFirstMediumEnumerationHandled = true;

    /* Lock the action preventing cascade calls: */
    UIQObjectPropertySetter guardBlock(actionPool()->action(UIActionIndex_M_Application_S_Preferences), "opened", true);
    connect(&guardBlock, &UIQObjectPropertySetter::sigAboutToBeDestroyed,
            this, &UIVirtualBoxManager::sltHandleUpdateActionAppearanceRequest);
    updateActionsAppearance();

    /* Use the "safe way" to open stack of Mac OS X Sheets: */
    QWidget *pDialogParent = windowManager().realParentWindow(this);
    UISafePointerSettingsDialogGlobal pDialog = new UISettingsDialogGlobal(pDialogParent);
    windowManager().registerNewParent(pDialog, pDialogParent);

    /* Execute dialog: */
    pDialog->execute();
    delete pDialog;
}

void UIVirtualBoxManager::sltPerformExit()
{
    close();
}

void UIVirtualBoxManager::sltOpenNewMachineWizard()
{
    /* Lock the actions preventing cascade calls: */
    UIQObjectPropertySetter guardBlock(QList<QObject*>() << actionPool()->action(UIActionIndexST_M_Welcome_S_New)
                                                         << actionPool()->action(UIActionIndexST_M_Machine_S_New)
                                                         << actionPool()->action(UIActionIndexST_M_Group_S_New),
                                       "opened", true);
    connect(&guardBlock, &UIQObjectPropertySetter::sigAboutToBeDestroyed,
            this, &UIVirtualBoxManager::sltHandleUpdateActionAppearanceRequest);
    updateActionsAppearance();

    /* Get first selected item: */
    UIVirtualMachineItem *pItem = currentItem();

    /* For global item or local machine: */
    if (   !pItem
        || pItem->itemType() == UIVirtualMachineItemType_Local)
    {
        /* Use the "safe way" to open stack of Mac OS X Sheets: */
        QWidget *pWizardParent = windowManager().realParentWindow(this);
        UISafePointerWizardNewVM pWizard = new UIWizardNewVM(pWizardParent, m_pWidget->fullGroupName());
        windowManager().registerNewParent(pWizard, pWizardParent);
        pWizard->prepare();

        /* Execute wizard: */
        pWizard->exec();
        delete pWizard;
    }
    /* For cloud machine: */
    else
    {
        /* Use the "safe way" to open stack of Mac OS X Sheets: */
        QWidget *pWizardParent = windowManager().realParentWindow(this);
        UISafePointerWizardNewCloudVM pWizard = new UIWizardNewCloudVM(pWizardParent);
        windowManager().registerNewParent(pWizard, pWizardParent);
        pWizard->prepare();

        /* Execute wizard: */
        pWizard->exec();
        delete pWizard;
    }
}

void UIVirtualBoxManager::sltOpenAddMachineDialog()
{
    /* Lock the actions preventing cascade calls: */
    UIQObjectPropertySetter guardBlock(QList<QObject*>() << actionPool()->action(UIActionIndexST_M_Welcome_S_Add)
                                                         << actionPool()->action(UIActionIndexST_M_Machine_S_Add)
                                                         << actionPool()->action(UIActionIndexST_M_Group_S_Add),
                                       "opened", true);
    connect(&guardBlock, &UIQObjectPropertySetter::sigAboutToBeDestroyed,
            this, &UIVirtualBoxManager::sltHandleUpdateActionAppearanceRequest);
    updateActionsAppearance();

    /* Get first selected item: */
    UIVirtualMachineItem *pItem = currentItem();

    /* For global item or local machine: */
    if (   !pItem
        || pItem->itemType() == UIVirtualMachineItemType_Local)
    {
        /* Open add machine dialog: */
        openAddMachineDialog();
    }
    /* For cloud machine: */
    else
    {
        /* Use the "safe way" to open stack of Mac OS X Sheets: */
        QWidget *pWizardParent = windowManager().realParentWindow(this);
        UISafePointerWizardAddCloudVM pWizard = new UIWizardAddCloudVM(pWizardParent);
        windowManager().registerNewParent(pWizard, pWizardParent);
        pWizard->prepare();

        /* Execute wizard: */
        pWizard->exec();
        delete pWizard;
    }
}

void UIVirtualBoxManager::sltOpenGroupNameEditor()
{
    m_pWidget->openGroupNameEditor();
}

void UIVirtualBoxManager::sltDisbandGroup()
{
    m_pWidget->disbandGroup();
}

void UIVirtualBoxManager::sltOpenMachineSettingsDialog(QString strCategory /* = QString() */,
                                                       QString strControl /* = QString() */,
                                                       const QUuid &uID /* = QString() */)
{
    /* Lock the action preventing cascade calls: */
    UIQObjectPropertySetter guardBlock(actionPool()->action(UIActionIndexST_M_Machine_S_Settings), "opened", true);
    connect(&guardBlock, &UIQObjectPropertySetter::sigAboutToBeDestroyed,
            this, &UIVirtualBoxManager::sltHandleUpdateActionAppearanceRequest);
    updateActionsAppearance();

    /* Get current item: */
    UIVirtualMachineItem *pItem = currentItem();
    AssertMsgReturnVoid(pItem, ("Current item should be selected!\n"));

    /* For local machine: */
    if (pItem->itemType() == UIVirtualMachineItemType_Local)
    {
        /* Process href from VM details / description: */
        if (!strCategory.isEmpty() && strCategory[0] != '#')
        {
            uiCommon().openURL(strCategory);
        }
        else
        {
            /* Check if control is coded into the URL by %%: */
            if (strControl.isEmpty())
            {
                QStringList parts = strCategory.split("%%");
                if (parts.size() == 2)
                {
                    strCategory = parts.at(0);
                    strControl = parts.at(1);
                }
            }

            /* Don't show the inaccessible warning
             * if the user tries to open VM settings: */
            m_fFirstMediumEnumerationHandled = true;

            /* Use the "safe way" to open stack of Mac OS X Sheets: */
            QWidget *pDialogParent = windowManager().realParentWindow(this);
            UISafePointerSettingsDialogMachine pDialog = new UISettingsDialogMachine(pDialogParent,
                                                                                     uID.isNull() ? pItem->id() : uID,
                                                                                     strCategory, strControl);
            windowManager().registerNewParent(pDialog, pDialogParent);

            /* Execute dialog: */
            pDialog->execute();
            delete pDialog;
        }
    }
    /* For cloud machine: */
    else
    {
        /* Use the "safe way" to open stack of Mac OS X Sheets: */
        QWidget *pDialogParent = windowManager().realParentWindow(this);
        UISafePointerCloudMachineSettingsDialog pDialog = new UICloudMachineSettingsDialog(pDialogParent,
                                                                                           pItem->toCloud()->machine());
        windowManager().registerNewParent(pDialog, pDialogParent);

        /* Execute dialog: */
        pDialog->exec();
        delete pDialog;
    }
}

void UIVirtualBoxManager::sltOpenCloneMachineWizard()
{
    /* Get current item: */
    UIVirtualMachineItem *pItem = currentItem();
    AssertMsgReturnVoid(pItem, ("Current item should be selected!\n"));
    /* Make sure current item is local one: */
    UIVirtualMachineItemLocal *pItemLocal = pItem->toLocal();
    AssertMsgReturnVoid(pItemLocal, ("Current item should be local one!\n"));

    /* Use the "safe way" to open stack of Mac OS X Sheets: */
    QWidget *pWizardParent = windowManager().realParentWindow(this);
    const QStringList &machineGroupNames = pItemLocal->groups();
    const QString strGroup = !machineGroupNames.isEmpty() ? machineGroupNames.at(0) : QString();
    UISafePointerWizard pWizard = new UIWizardCloneVM(pWizardParent, pItemLocal->machine(), strGroup);
    windowManager().registerNewParent(pWizard, pWizardParent);
    pWizard->prepare();
    pWizard->exec();
    delete pWizard;
}

void UIVirtualBoxManager::sltPerformMachineMove()
{
    /* Get current item: */
    UIVirtualMachineItem *pItem = currentItem();
    AssertMsgReturnVoid(pItem, ("Current item should be selected!\n"));

    /* Open a session thru which we will modify the machine: */
    CSession comSession = uiCommon().openSession(pItem->id(), KLockType_Write);
    if (comSession.isNull())
        return;

    /* Get session machine: */
    CMachine comMachine = comSession.GetMachine();
    AssertMsgReturnVoid(comSession.isOk() && comMachine.isNotNull(), ("Unable to acquire machine!\n"));

    /* Open a file dialog for the user to select a destination folder. Start with the default machine folder: */
    CVirtualBox comVBox = uiCommon().virtualBox();
    QString strBaseFolder = comVBox.GetSystemProperties().GetDefaultMachineFolder();
    QString strTitle = tr("Select a destination folder to move the selected virtual machine");
    QString strDestinationFolder = QIFileDialog::getExistingDirectory(strBaseFolder, this, strTitle);
    if (!strDestinationFolder.isEmpty())
    {
        /* Prepare machine move progress: */
        CProgress comProgress = comMachine.MoveTo(strDestinationFolder, "basic");
        if (comMachine.isOk() && comProgress.isNotNull())
        {
            /* Show machine move progress: */
            msgCenter().showModalProgressDialog(comProgress, comMachine.GetName(), ":/progress_dnd_hg_90px.png");
            if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
                msgCenter().cannotMoveMachine(comProgress, comMachine.GetName());
        }
        else
            msgCenter().cannotMoveMachine(comMachine);
    }
    comSession.UnlockMachine();
}

void UIVirtualBoxManager::sltPerformMachineRemove()
{
    m_pWidget->removeMachine();
}

void UIVirtualBoxManager::sltPerformMachineMoveToNewGroup()
{
    m_pWidget->moveMachineToNewGroup();
}

void UIVirtualBoxManager::sltPerformStartOrShowMachine()
{
    /* Start selected VMs in corresponding mode: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));
    performStartOrShowVirtualMachines(items, UICommon::LaunchMode_Invalid);
}

void UIVirtualBoxManager::sltPerformStartMachineNormal()
{
    /* Start selected VMs in corresponding mode: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));
    performStartOrShowVirtualMachines(items, UICommon::LaunchMode_Default);
}

void UIVirtualBoxManager::sltPerformStartMachineHeadless()
{
    /* Start selected VMs in corresponding mode: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));
    performStartOrShowVirtualMachines(items, UICommon::LaunchMode_Headless);
}

void UIVirtualBoxManager::sltPerformStartMachineDetachable()
{
    /* Start selected VMs in corresponding mode: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));
    performStartOrShowVirtualMachines(items, UICommon::LaunchMode_Separate);
}

void UIVirtualBoxManager::sltPerformDiscardMachineState()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* Prepare the list of the machines to be discarded/terminated: */
    QStringList machinesToDiscard;
    QStringList machinesToTerminate;
    QList<UIVirtualMachineItem*> itemsToDiscard;
    QList<UIVirtualMachineItem*> itemsToTerminate;
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (isActionEnabled(UIActionIndexST_M_Group_S_Discard, QList<UIVirtualMachineItem*>() << pItem))
        {
            if (pItem->itemType() == UIVirtualMachineItemType_Local)
            {
                machinesToDiscard << pItem->name();
                itemsToDiscard << pItem;
            }
            else if (pItem->itemType() == UIVirtualMachineItemType_CloudReal)
            {
                machinesToTerminate << pItem->name();
                itemsToTerminate << pItem;
            }
        }
    }
    AssertMsg(!machinesToDiscard.isEmpty() || !machinesToTerminate.isEmpty(), ("This action should not be allowed!"));

    /* Confirm discarding/terminating: */
    if (   (machinesToDiscard.isEmpty() || !msgCenter().confirmDiscardSavedState(machinesToDiscard.join(", ")))
        && (machinesToTerminate.isEmpty() || !msgCenter().confirmTerminateCloudInstance(machinesToTerminate.join(", "))))
        return;

    /* For every confirmed item to discard: */
    foreach (UIVirtualMachineItem *pItem, itemsToDiscard)
    {
        /* Open a session to modify VM: */
        AssertPtrReturnVoid(pItem);
        CSession comSession = uiCommon().openSession(pItem->id());
        if (comSession.isNull())
            return;

        /* Get session machine: */
        CMachine comMachine = comSession.GetMachine();
        comMachine.DiscardSavedState(true);
        if (!comMachine.isOk())
            msgCenter().cannotDiscardSavedState(comMachine);

        /* Unlock machine finally: */
        comSession.UnlockMachine();
    }

    /* For every confirmed item to terminate: */
    foreach (UIVirtualMachineItem *pItem, itemsToTerminate)
    {
        /* Get cloud machine: */
        AssertPtrReturnVoid(pItem);
        UIVirtualMachineItemCloud *pCloudItem = pItem->toCloud();
        AssertPtrReturnVoid(pCloudItem);
        CCloudMachine comMachine = pCloudItem->machine();

        /* Acquire machine name: */
        QString strName;
        if (!cloudMachineName(comMachine, strName))
            continue;

        /* Prepare terminate cloud instance progress: */
        CProgress comProgress = comMachine.Terminate();
        if (!comMachine.isOk())
        {
            msgCenter().cannotTerminateCloudInstance(comMachine);
            continue;
        }

        /* Show terminate cloud instance progress: */
        msgCenter().showModalProgressDialog(comProgress, strName, ":/progress_media_delete_90px.png", 0, 0); /// @todo use proper icon
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            msgCenter().cannotTerminateCloudInstance(comProgress, strName);
    }
}

void UIVirtualBoxManager::sltPerformPauseOrResumeMachine(bool fPause)
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For every selected item: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* But for local machine items only: */
        AssertPtrReturnVoid(pItem);
        if (pItem->itemType() != UIVirtualMachineItemType_Local)
            continue;

        /* Get local machine item state: */
        UIVirtualMachineItemLocal *pLocalItem = pItem->toLocal();
        AssertPtrReturnVoid(pLocalItem);
        const KMachineState enmState = pLocalItem->machineState();

        /* Check if current item could be paused/resumed: */
        if (!isActionEnabled(UIActionIndexST_M_Group_T_Pause, QList<UIVirtualMachineItem*>() << pItem))
            continue;

        /* Check if current item already paused: */
        if (fPause &&
            (enmState == KMachineState_Paused ||
             enmState == KMachineState_TeleportingPausedVM))
            continue;

        /* Check if current item already resumed: */
        if (!fPause &&
            (enmState == KMachineState_Running ||
             enmState == KMachineState_Teleporting ||
             enmState == KMachineState_LiveSnapshotting))
            continue;

        /* Open a session to modify VM state: */
        CSession comSession = uiCommon().openExistingSession(pItem->id());
        if (comSession.isNull())
            return;

        /* Get session console: */
        CConsole comConsole = comSession.GetConsole();
        /* Pause/resume VM: */
        if (fPause)
            comConsole.Pause();
        else
            comConsole.Resume();
        if (!comConsole.isOk())
        {
            if (fPause)
                msgCenter().cannotPauseMachine(comConsole);
            else
                msgCenter().cannotResumeMachine(comConsole);
        }

        /* Unlock machine finally: */
        comSession.UnlockMachine();
    }
}

void UIVirtualBoxManager::sltPerformResetMachine()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* Prepare the list of the machines to be reseted: */
    QStringList machineNames;
    QList<UIVirtualMachineItem*> itemsToReset;
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (isActionEnabled(UIActionIndexST_M_Group_S_Reset, QList<UIVirtualMachineItem*>() << pItem))
        {
            machineNames << pItem->name();
            itemsToReset << pItem;
        }
    }
    AssertMsg(!machineNames.isEmpty(), ("This action should not be allowed!"));

    /* Confirm reseting VM: */
    if (!msgCenter().confirmResetMachine(machineNames.join(", ")))
        return;

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, itemsToReset)
    {
        /* Open a session to modify VM state: */
        CSession comSession = uiCommon().openExistingSession(pItem->id());
        if (comSession.isNull())
            return;

        /* Get session console: */
        CConsole comConsole = comSession.GetConsole();
        /* Reset VM: */
        comConsole.Reset();

        /* Unlock machine finally: */
        comSession.UnlockMachine();
    }
}

void UIVirtualBoxManager::sltPerformDetachMachineUI()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* Check if current item could be detached: */
        if (!isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_Detach, QList<UIVirtualMachineItem*>() << pItem))
            continue;

        /// @todo Detach separate UI process..
        AssertFailed();
    }
}

void UIVirtualBoxManager::sltPerformSaveMachineState()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* Check if current item could be saved: */
        AssertPtrReturnVoid(pItem);
        if (!isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_SaveState, QList<UIVirtualMachineItem*>() << pItem))
            continue;

        /* Open a session to modify VM state: */
        CSession comSession = uiCommon().openExistingSession(pItem->id());
        if (comSession.isNull())
            return;

        /* Get session console: */
        CConsole comConsole = comSession.GetConsole();
        /* Get session machine: */
        CMachine comMachine = comSession.GetMachine();

        /* Get local machine item state: */
        UIVirtualMachineItemLocal *pLocalItem = pItem->toLocal();
        AssertPtrReturnVoid(pLocalItem);
        const KMachineState enmState = pLocalItem->machineState();

        /* Pause VM first if necessary: */
        if (enmState != KMachineState_Paused)
            comConsole.Pause();
        if (comConsole.isOk())
        {
            /* Prepare machine state saving progress: */
            CProgress comProgress = comMachine.SaveState();
            if (comMachine.isOk())
            {
                /* Show machine state saving progress: */
                msgCenter().showModalProgressDialog(comProgress, comMachine.GetName(), ":/progress_state_save_90px.png");
                if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
                    msgCenter().cannotSaveMachineState(comProgress, comMachine.GetName());
            }
            else
                msgCenter().cannotSaveMachineState(comMachine);
        }
        else
            msgCenter().cannotPauseMachine(comConsole);

        /* Unlock machine finally: */
        comSession.UnlockMachine();
    }
}

void UIVirtualBoxManager::sltPerformShutdownMachine()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* Prepare the list of the machines to be shutdowned: */
    QStringList machineNames;
    QList<UIVirtualMachineItem*> itemsToShutdown;
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_Shutdown, QList<UIVirtualMachineItem*>() << pItem))
        {
            machineNames << pItem->name();
            itemsToShutdown << pItem;
        }
    }
    AssertMsg(!machineNames.isEmpty(), ("This action should not be allowed!"));

    /* Confirm ACPI shutdown current VM: */
    if (!msgCenter().confirmACPIShutdownMachine(machineNames.join(", ")))
        return;

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, itemsToShutdown)
    {
        /* For local machine: */
        if (pItem->itemType() == UIVirtualMachineItemType_Local)
        {
            /* Open a session to modify VM state: */
            CSession comSession = uiCommon().openExistingSession(pItem->id());
            if (comSession.isNull())
                return;

            /* Get session console: */
            CConsole comConsole = comSession.GetConsole();
            /* ACPI Shutdown: */
            comConsole.PowerButton();
            if (!comConsole.isOk())
                msgCenter().cannotACPIShutdownMachine(comConsole);

            /* Unlock machine finally: */
            comSession.UnlockMachine();
        }
        /* For real cloud machine: */
        else if (pItem->itemType() == UIVirtualMachineItemType_CloudReal)
        {
            /* Acquire cloud machine: */
            CCloudMachine comCloudMachine = pItem->toCloud()->machine();
            /* Prepare machine ACPI shutdown: */
            CProgress comProgress = comCloudMachine.Shutdown();
            if (!comCloudMachine.isOk())
                msgCenter().cannotACPIShutdownCloudMachine(comCloudMachine);
            else
            {
                /* Show machine ACPI shutdown progress: */
                msgCenter().showModalProgressDialog(comProgress, pItem->name(), ":/progress_poweroff_90px.png", 0, 0);
                if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
                    msgCenter().cannotACPIShutdownCloudMachine(comProgress, pItem->name());
                /* Update info in any case: */
                pItem->toCloud()->updateInfoAsync(false /* delayed? */);
            }
        }
    }
}

void UIVirtualBoxManager::sltPerformPowerOffMachine()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* Prepare the list of the machines to be powered off: */
    QStringList machineNames;
    QList<UIVirtualMachineItem*> itemsToPowerOff;
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_PowerOff, QList<UIVirtualMachineItem*>() << pItem))
        {
            machineNames << pItem->name();
            itemsToPowerOff << pItem;
        }
    }
    AssertMsg(!machineNames.isEmpty(), ("This action should not be allowed!"));

    /* Confirm Power Off current VM: */
    if (!msgCenter().confirmPowerOffMachine(machineNames.join(", ")))
        return;

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, itemsToPowerOff)
    {
        /* For local machine: */
        if (pItem->itemType() == UIVirtualMachineItemType_Local)
        {
            /* Open a session to modify VM state: */
            CSession comSession = uiCommon().openExistingSession(pItem->id());
            if (comSession.isNull())
                break;

            /* Get session console: */
            CConsole comConsole = comSession.GetConsole();
            /* Prepare machine power down: */
            CProgress comProgress = comConsole.PowerDown();
            if (!comConsole.isOk())
                msgCenter().cannotPowerDownMachine(comConsole);
            else
            {
                /* Show machine power down progress: */
                msgCenter().showModalProgressDialog(comProgress, pItem->name(), ":/progress_poweroff_90px.png");
                if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
                    msgCenter().cannotPowerDownMachine(comProgress, pItem->name());
            }

            /* Unlock machine finally: */
            comSession.UnlockMachine();
        }
        /* For real cloud machine: */
        else if (pItem->itemType() == UIVirtualMachineItemType_CloudReal)
        {
            /* Acquire cloud machine: */
            CCloudMachine comCloudMachine = pItem->toCloud()->machine();
            /* Prepare machine power down: */
            CProgress comProgress = comCloudMachine.PowerDown();
            if (!comCloudMachine.isOk())
                msgCenter().cannotPowerDownCloudMachine(comCloudMachine);
            else
            {
                /* Show machine power down progress: */
                msgCenter().showModalProgressDialog(comProgress, pItem->name(), ":/progress_poweroff_90px.png", 0, 0);
                if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
                    msgCenter().cannotPowerDownCloudMachine(comProgress, pItem->name());
                /* Update info in any case: */
                pItem->toCloud()->updateInfoAsync(false /* delayed? */);
            }
        }
    }
}

void UIVirtualBoxManager::sltPerformShowMachineTool(QAction *pAction)
{
    AssertPtrReturnVoid(pAction);
    AssertPtrReturnVoid(m_pWidget);
    m_pWidget->setToolsType(pAction->property("UIToolType").value<UIToolType>());
}

void UIVirtualBoxManager::sltOpenLogViewerWindow()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* First check if instance of widget opened the embedded way: */
    if (m_pWidget->isMachineToolOpened(UIToolType_Logs))
    {
        m_pWidget->setToolsType(UIToolType_Details);
        m_pWidget->closeMachineTool(UIToolType_Logs);
    }

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* Make sure current item is local one: */
        UIVirtualMachineItemLocal *pItemLocal = pItem->toLocal();
        if (!pItemLocal)
            continue;

        /* Check if log could be show for the current item: */
        if (!isActionEnabled(UIActionIndexST_M_Group_S_ShowLogDialog, QList<UIVirtualMachineItem*>() << pItem))
            continue;

        QIManagerDialog *pLogViewerDialog = 0;
        /* Create and Show VM Log Viewer: */
        if (!m_logViewers[pItemLocal->machine().GetHardwareUUID().toString()])
        {
            UIVMLogViewerDialogFactory dialogFactory(actionPool(), pItemLocal->machine());
            dialogFactory.prepare(pLogViewerDialog, this);
            if (pLogViewerDialog)
            {
                m_logViewers[pItemLocal->machine().GetHardwareUUID().toString()] = pLogViewerDialog;
                connect(pLogViewerDialog, &QIManagerDialog::sigClose,
                        this, &UIVirtualBoxManager::sltCloseLogViewerWindow);
            }
        }
        else
        {
            pLogViewerDialog = m_logViewers[pItemLocal->machine().GetHardwareUUID().toString()];
        }
        if (pLogViewerDialog)
        {
            /* Show instance: */
            pLogViewerDialog->show();
            pLogViewerDialog->setWindowState(pLogViewerDialog->windowState() & ~Qt::WindowMinimized);
            pLogViewerDialog->activateWindow();
        }
    }
}

void UIVirtualBoxManager::sltCloseLogViewerWindow()
{
    /* If there is a proper sender: */
    if (qobject_cast<QIManagerDialog*>(sender()))
    {
        /* Search for the sender of the signal within the m_logViewers map: */
        QMap<QString, QIManagerDialog*>::iterator sendersIterator = m_logViewers.begin();
        while (sendersIterator != m_logViewers.end() && sendersIterator.value() != sender())
            ++sendersIterator;
        /* Do nothing if we cannot find it with the map: */
        if (sendersIterator == m_logViewers.end())
            return;

        /* Check whether we have found the proper dialog: */
        QIManagerDialog *pDialog = qobject_cast<QIManagerDialog*>(sendersIterator.value());
        if (!pDialog)
            return;

        /* First remove this log-viewer dialog from the map.
         * This should be done before closing the dialog which will incur
         * a second call to this function and result in double delete!!! */
        m_logViewers.erase(sendersIterator);
        UIVMLogViewerDialogFactory().cleanup(pDialog);
    }
    /* Otherwise: */
    else
    {
        /* Just wipe out everything: */
        foreach (const QString &strKey, m_logViewers.keys())
        {
            /* First remove each log-viewer dialog from the map.
             * This should be done before closing the dialog which will incur
             * a second call to this function and result in double delete!!! */
            QIManagerDialog *pDialog = m_logViewers.value(strKey);
            m_logViewers.remove(strKey);
            UIVMLogViewerDialogFactory().cleanup(pDialog);
        }
    }
}

void UIVirtualBoxManager::sltPerformRefreshMachine()
{
    m_pWidget->refreshMachine();
}

void UIVirtualBoxManager::sltShowMachineInFileManager()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* Make sure current item is local one: */
        UIVirtualMachineItemLocal *pItemLocal = pItem->toLocal();
        if (!pItemLocal)
            continue;

        /* Check if that item could be shown in file-browser: */
        if (!isActionEnabled(UIActionIndexST_M_Group_S_ShowInFileManager, QList<UIVirtualMachineItem*>() << pItem))
            continue;

        /* Show VM in filebrowser: */
        UIDesktopServices::openInFileManager(pItemLocal->machine().GetSettingsFilePath());
    }
}

void UIVirtualBoxManager::sltPerformCreateMachineShortcut()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* Make sure current item is local one: */
        UIVirtualMachineItemLocal *pItemLocal = pItem->toLocal();
        if (!pItemLocal)
            continue;

        /* Check if shortcuts could be created for this item: */
        if (!isActionEnabled(UIActionIndexST_M_Group_S_CreateShortcut, QList<UIVirtualMachineItem*>() << pItem))
            continue;

        /* Create shortcut for this VM: */
        const CMachine &comMachine = pItemLocal->machine();
        UIDesktopServices::createMachineShortcut(comMachine.GetSettingsFilePath(),
                                                 QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
                                                 comMachine.GetName(), comMachine.GetId());
    }
}

void UIVirtualBoxManager::sltPerformGroupSorting()
{
    m_pWidget->sortGroup();
}

void UIVirtualBoxManager::sltPerformMachineSearchWidgetVisibilityToggling(bool fVisible)
{
    m_pWidget->setMachineSearchWidgetVisibility(fVisible);
}

void UIVirtualBoxManager::sltGroupCloseMenuAboutToShow()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Shutdown)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close_S_Shutdown, items));
}

void UIVirtualBoxManager::sltMachineCloseMenuAboutToShow()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Shutdown)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_Shutdown, items));
}

void UIVirtualBoxManager::prepare()
{
#ifdef VBOX_WS_X11
    /* Assign same name to both WM_CLASS name & class for now: */
    UICommon::setWMClass(this, "VirtualBox Manager", "VirtualBox Manager");
#endif

#ifdef VBOX_WS_MAC
    /* We have to make sure that we are getting the front most process: */
    ::darwinSetFrontMostProcess();
    /* Install global event-filter, since vmstarter.app can send us FileOpen events,
     * see UIVirtualBoxManager::eventFilter for handler implementation. */
    qApp->installEventFilter(this);
#endif

    /* Cache media data early if necessary: */
    if (uiCommon().agressiveCaching())
        uiCommon().enumerateMedia();

    /* Prepare: */
    prepareIcon();
    prepareMenuBar();
    prepareStatusBar();
    prepareWidgets();
    prepareConnections();

    /* Update actions initially: */
    updateActionsVisibility();
    updateActionsAppearance();

    /* Load settings: */
    loadSettings();

    /* Translate UI: */
    retranslateUi();

#ifdef VBOX_WS_MAC
    /* Beta label? */
    if (uiCommon().isBeta())
    {
        QPixmap betaLabel = ::betaLabel(QSize(100, 16));
        ::darwinLabelWindow(this, &betaLabel, true);
    }
#endif /* VBOX_WS_MAC */

    /* If there are unhandled URLs we should handle them after manager is shown: */
    if (uiCommon().argumentUrlsPresent())
        QMetaObject::invokeMethod(this, "sltHandleOpenUrlCall", Qt::QueuedConnection);
}

void UIVirtualBoxManager::prepareIcon()
{
    /* Prepare application icon.
     * On Win host it's built-in to the executable.
     * On Mac OS X the icon referenced in info.plist is used.
     * On X11 we will provide as much icons as we can. */
#if !defined(VBOX_WS_WIN) && !defined(VBOX_WS_MAC)
    QIcon icon(":/VirtualBox.svg");
    icon.addFile(":/VirtualBox_48px.png");
    icon.addFile(":/VirtualBox_64px.png");
    setWindowIcon(icon);
#endif /* !VBOX_WS_WIN && !VBOX_WS_MAC */
}

void UIVirtualBoxManager::prepareMenuBar()
{
#ifndef VBOX_WS_MAC
    /* Create menu-bar: */
    setMenuBar(new UIMenuBar);
    if (menuBar())
    {
        /* Make sure menu-bar fills own solid background: */
        menuBar()->setAutoFillBackground(true);
        QPalette pal = menuBar()->palette();
        const QColor color = pal.color(QPalette::Active, QPalette::Mid).lighter(160);
        pal.setColor(QPalette::Active, QPalette::Button, color);
        menuBar()->setPalette(pal);
    }
#endif

    /* Create action-pool: */
    m_pActionPool = UIActionPool::create(UIActionPoolType_Manager);

    /* Build menu-bar: */
    foreach (QMenu *pMenu, actionPool()->menus())
    {
#ifdef VBOX_WS_MAC
        /* Before 'Help' menu we should: */
        if (pMenu == actionPool()->action(UIActionIndex_Menu_Help)->menu())
        {
            /* Insert 'Window' menu: */
            UIWindowMenuManager::create();
            menuBar()->addMenu(gpWindowMenuManager->createMenu(this));
            gpWindowMenuManager->addWindow(this);
        }
#endif
        menuBar()->addMenu(pMenu);
    }

    /* Setup menu-bar policy: */
    menuBar()->setContextMenuPolicy(Qt::CustomContextMenu);
}

void UIVirtualBoxManager::prepareStatusBar()
{
    /* We are not using status-bar anymore: */
    statusBar()->setHidden(true);
}

void UIVirtualBoxManager::prepareWidgets()
{
    /* Prepare central-widget: */
    m_pWidget = new UIVirtualBoxManagerWidget(this);
    if (m_pWidget)
        setCentralWidget(m_pWidget);
}

void UIVirtualBoxManager::prepareConnections()
{
#ifdef VBOX_WS_X11
    /* Desktop event handlers: */
    connect(gpDesktop, &UIDesktopWidgetWatchdog::sigHostScreenWorkAreaResized,
            this, &UIVirtualBoxManager::sltHandleHostScreenAvailableAreaChange);
#endif

    /* Medium enumeration connections: */
    connect(&uiCommon(), &UICommon::sigMediumEnumerationFinished,
            this, &UIVirtualBoxManager::sltHandleMediumEnumerationFinish);

    /* Widget connections: */
    connect(m_pWidget, &UIVirtualBoxManagerWidget::sigChooserPaneIndexChange,
            this, &UIVirtualBoxManager::sltHandleChooserPaneIndexChange);
    connect(m_pWidget, &UIVirtualBoxManagerWidget::sigGroupSavingStateChanged,
            this, &UIVirtualBoxManager::sltHandleGroupSavingProgressChange);
    connect(m_pWidget, &UIVirtualBoxManagerWidget::sigStartOrShowRequest,
            this, &UIVirtualBoxManager::sltPerformStartOrShowMachine);
    connect(m_pWidget, &UIVirtualBoxManagerWidget::sigCloudMachineStateChange,
            this, &UIVirtualBoxManager::sltHandleCloudMachineStateChange);
    connect(m_pWidget, &UIVirtualBoxManagerWidget::sigToolTypeChange,
            this, &UIVirtualBoxManager::sltHandleToolTypeChange);
    connect(m_pWidget, &UIVirtualBoxManagerWidget::sigCloudProfileManagerChange,
            this, &UIVirtualBoxManager::sigCloudProfileManagerChange);
    connect(m_pWidget, &UIVirtualBoxManagerWidget::sigMachineSettingsLinkClicked,
            this, &UIVirtualBoxManager::sltOpenMachineSettingsDialog);
    connect(m_pWidget, &UIVirtualBoxManagerWidget::sigCurrentSnapshotItemChange,
            this, &UIVirtualBoxManager::sltCurrentSnapshotItemChange);
    connect(menuBar(), &QMenuBar::customContextMenuRequested,
            m_pWidget, &UIVirtualBoxManagerWidget::sltHandleContextMenuRequest);

    /* Global VBox event handlers: */
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineStateChange,
            this, &UIVirtualBoxManager::sltHandleStateChange);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigSessionStateChange,
            this, &UIVirtualBoxManager::sltHandleStateChange);

    /* 'File' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_File_S_ShowVirtualMediumManager), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltOpenVirtualMediumManagerWindow);
    connect(actionPool()->action(UIActionIndexST_M_File_S_ShowHostNetworkManager), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltOpenHostNetworkManagerWindow);
    connect(actionPool()->action(UIActionIndexST_M_File_S_ShowCloudProfileManager), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltOpenCloudProfileManagerWindow);
    connect(actionPool()->action(UIActionIndexST_M_File_S_ImportAppliance), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltOpenImportApplianceWizardDefault);
    connect(actionPool()->action(UIActionIndexST_M_File_S_ExportAppliance), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltOpenExportApplianceWizard);
    connect(actionPool()->action(UIActionIndexST_M_File_S_NewCloudVM), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltOpenNewCloudVMWizard);
#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
    connect(actionPool()->action(UIActionIndexST_M_File_S_ShowExtraDataManager), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltOpenExtraDataManagerWindow);
#endif /* VBOX_GUI_WITH_EXTRADATA_MANAGER_UI */
    connect(actionPool()->action(UIActionIndex_M_Application_S_Preferences), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltOpenPreferencesDialog);
    connect(actionPool()->action(UIActionIndexST_M_File_S_Close), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformExit);

    /* 'Welcome' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Welcome_S_New), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltOpenNewMachineWizard);
    connect(actionPool()->action(UIActionIndexST_M_Welcome_S_Add), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltOpenAddMachineDialog);

    /* 'Group' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Group_S_New), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltOpenNewMachineWizard);
    connect(actionPool()->action(UIActionIndexST_M_Group_S_Add), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltOpenAddMachineDialog);
    connect(actionPool()->action(UIActionIndexST_M_Group_S_Rename), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltOpenGroupNameEditor);
    connect(actionPool()->action(UIActionIndexST_M_Group_S_Remove), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltDisbandGroup);
    connect(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformStartOrShowMachine);
    connect(actionPool()->action(UIActionIndexST_M_Group_T_Pause), &UIAction::toggled,
            this, &UIVirtualBoxManager::sltPerformPauseOrResumeMachine);
    connect(actionPool()->action(UIActionIndexST_M_Group_S_Reset), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformResetMachine);
    connect(actionPool()->action(UIActionIndexST_M_Group_S_Discard), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformDiscardMachineState);
    connect(actionPool()->action(UIActionIndexST_M_Group_S_ShowLogDialog), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltOpenLogViewerWindow);
    connect(actionPool()->action(UIActionIndexST_M_Group_S_Refresh), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformRefreshMachine);
    connect(actionPool()->action(UIActionIndexST_M_Group_S_ShowInFileManager), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltShowMachineInFileManager);
    connect(actionPool()->action(UIActionIndexST_M_Group_S_CreateShortcut), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformCreateMachineShortcut);
    connect(actionPool()->action(UIActionIndexST_M_Group_S_Sort), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformGroupSorting);

    /* 'Machine' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_New), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltOpenNewMachineWizard);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Add), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltOpenAddMachineDialog);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Settings), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltOpenMachineSettingsDialogDefault);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Clone), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltOpenCloneMachineWizard);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Move), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformMachineMove);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_ExportToOCI), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltOpenExportApplianceWizard);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Remove), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformMachineRemove);
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_MoveToGroup_S_New), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformMachineMoveToNewGroup);
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformStartOrShowMachine);
    connect(actionPool()->action(UIActionIndexST_M_Machine_T_Pause), &UIAction::toggled,
            this, &UIVirtualBoxManager::sltPerformPauseOrResumeMachine);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Reset), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformResetMachine);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Discard), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformDiscardMachineState);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_ShowLogDialog), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltOpenLogViewerWindow);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Refresh), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformRefreshMachine);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_ShowInFileManager), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltShowMachineInFileManager);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_CreateShortcut), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformCreateMachineShortcut);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_SortParent), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformGroupSorting);
    connect(actionPool()->action(UIActionIndexST_M_Machine_T_Search), &UIAction::toggled,
            this, &UIVirtualBoxManager::sltPerformMachineSearchWidgetVisibilityToggling);
    connect(m_pWidget, &UIVirtualBoxManagerWidget::sigMachineSearchWidgetVisibilityChanged,
            actionPool()->action(UIActionIndexST_M_Machine_T_Search), &QAction::setChecked);

    /* 'Group/Start or Show' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartNormal), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformStartMachineNormal);
    connect(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartHeadless), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformStartMachineHeadless);
    connect(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartDetachable), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformStartMachineDetachable);

    /* 'Machine/Start or Show' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartNormal), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformStartMachineNormal);
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartHeadless), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformStartMachineHeadless);
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartDetachable), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformStartMachineDetachable);

    /* 'Group/Close' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Group_M_Close)->menu(), &UIMenu::aboutToShow,
            this, &UIVirtualBoxManager::sltGroupCloseMenuAboutToShow);
    connect(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Detach), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformDetachMachineUI);
    connect(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_SaveState), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformSaveMachineState);
    connect(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Shutdown), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformShutdownMachine);
    connect(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_PowerOff), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformPowerOffMachine);

    /* 'Machine/Close' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_Close)->menu(), &UIMenu::aboutToShow,
            this, &UIVirtualBoxManager::sltMachineCloseMenuAboutToShow);
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Detach), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformDetachMachineUI);
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_SaveState), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformSaveMachineState);
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Shutdown), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformShutdownMachine);
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_PowerOff), &UIAction::triggered,
            this, &UIVirtualBoxManager::sltPerformPowerOffMachine);

    /* 'Group/Tools' menu connections: */
    connect(actionPool()->actionGroup(UIActionIndexST_M_Group_M_Tools), &QActionGroup::triggered,
            this, &UIVirtualBoxManager::sltPerformShowMachineTool);

    /* 'Machine/Tools' menu connections: */
    connect(actionPool()->actionGroup(UIActionIndexST_M_Machine_M_Tools), &QActionGroup::triggered,
            this, &UIVirtualBoxManager::sltPerformShowMachineTool);
}

void UIVirtualBoxManager::loadSettings()
{
    /* Load window geometry: */
    {
        const QRect geo = gEDataManager->selectorWindowGeometry(this);
        LogRel2(("GUI: UIVirtualBoxManager: Restoring geometry to: Origin=%dx%d, Size=%dx%d\n",
                 geo.x(), geo.y(), geo.width(), geo.height()));
        restoreGeometry(geo);
    }
}

void UIVirtualBoxManager::saveSettings()
{
    /* Save window geometry: */
    {
        const QRect geo = currentGeometry();
        LogRel2(("GUI: UIVirtualBoxManager: Saving geometry as: Origin=%dx%d, Size=%dx%d\n",
                 geo.x(), geo.y(), geo.width(), geo.height()));
        gEDataManager->setSelectorWindowGeometry(geo, isCurrentlyMaximized());
    }
}

void UIVirtualBoxManager::cleanupConnections()
{
    /* Honestly we should disconnect everything here,
     * but for now it's enough to disconnect the most critical. */
    m_pWidget->disconnect(this);
}

void UIVirtualBoxManager::cleanupWidgets()
{
    /* Deconfigure central-widget: */
    setCentralWidget(0);
    /* Destroy central-widget: */
    delete m_pWidget;
    m_pWidget = 0;
}

void UIVirtualBoxManager::cleanupMenuBar()
{
#ifdef VBOX_WS_MAC
    /* Cleanup 'Window' menu: */
    UIWindowMenuManager::destroy();
#endif

    /* Destroy action-pool: */
    UIActionPool::destroy(m_pActionPool);
    m_pActionPool = 0;
}

void UIVirtualBoxManager::cleanup()
{
    /* Close the sub-dialogs first: */
    sltCloseVirtualMediumManagerWindow();
    sltCloseHostNetworkManagerWindow();
    sltCloseCloudProfileManagerWindow();

    /* Save settings: */
    saveSettings();

    /* Cleanup: */
    cleanupConnections();
    cleanupWidgets();
    cleanupMenuBar();
}

UIVirtualMachineItem *UIVirtualBoxManager::currentItem() const
{
    return m_pWidget->currentItem();
}

QList<UIVirtualMachineItem*> UIVirtualBoxManager::currentItems() const
{
    return m_pWidget->currentItems();
}

bool UIVirtualBoxManager::isGroupSavingInProgress() const
{
    return m_pWidget->isGroupSavingInProgress();
}

bool UIVirtualBoxManager::isAllItemsOfOneGroupSelected() const
{
    return m_pWidget->isAllItemsOfOneGroupSelected();
}

bool UIVirtualBoxManager::isSingleGroupSelected() const
{
    return m_pWidget->isSingleGroupSelected();
}

bool UIVirtualBoxManager::isSingleLocalGroupSelected() const
{
    return m_pWidget->isSingleLocalGroupSelected();
}

bool UIVirtualBoxManager::isSingleCloudProfileGroupSelected() const
{
    return m_pWidget->isSingleCloudProfileGroupSelected();
}

void UIVirtualBoxManager::openAddMachineDialog(const QString &strFileName /* = QString() */)
{
    /* Initialize variables: */
#ifdef VBOX_WS_MAC
    QString strTmpFile = ::darwinResolveAlias(strFileName);
#else
    QString strTmpFile = strFileName;
#endif
    CVirtualBox comVBox = uiCommon().virtualBox();

    /* No file specified: */
    if (strTmpFile.isEmpty())
    {
        QString strBaseFolder = comVBox.GetSystemProperties().GetDefaultMachineFolder();
        QString strTitle = tr("Select a virtual machine file");
        QStringList extensions;
        for (int i = 0; i < VBoxFileExts.size(); ++i)
            extensions << QString("*.%1").arg(VBoxFileExts[i]);
        QString strFilter = tr("Virtual machine files (%1)").arg(extensions.join(" "));
        /* Create open file dialog: */
        QStringList fileNames = QIFileDialog::getOpenFileNames(strBaseFolder, strFilter, this, strTitle, 0, true, true);
        if (!fileNames.isEmpty())
            strTmpFile = fileNames.at(0);
    }

    /* Nothing was chosen? */
    if (strTmpFile.isEmpty())
        return;

    /* Make sure this machine can be opened: */
    CMachine comMachineNew = comVBox.OpenMachine(strTmpFile);
    if (!comVBox.isOk())
    {
        msgCenter().cannotOpenMachine(comVBox, strTmpFile);
        return;
    }

    /* Make sure this machine was NOT registered already: */
    CMachine comMachineOld = comVBox.FindMachine(comMachineNew.GetId().toString());
    if (!comMachineOld.isNull())
    {
        msgCenter().cannotReregisterExistingMachine(strTmpFile, comMachineOld.GetName());
        return;
    }

    /* Register that machine: */
    comVBox.RegisterMachine(comMachineNew);
}

void UIVirtualBoxManager::performStartOrShowVirtualMachines(const QList<UIVirtualMachineItem*> &items, UICommon::LaunchMode enmLaunchMode)
{
    /* Do nothing while group saving is in progress: */
    if (isGroupSavingInProgress())
        return;

    /* Compose the list of startable items: */
    QStringList startableMachineNames;
    QList<UIVirtualMachineItem*> startableItems;
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (isAtLeastOneItemCanBeStarted(QList<UIVirtualMachineItem*>() << pItem))
        {
            startableItems << pItem;
            startableMachineNames << pItem->name();
        }
    }

    /* Initially we have start auto-confirmed: */
    bool fStartConfirmed = true;
    /* But if we have more than one item to start =>
     * We should still ask user for a confirmation: */
    if (startableItems.size() > 1)
        fStartConfirmed = msgCenter().confirmStartMultipleMachines(startableMachineNames.join(", "));

    /* For every item => check if it could be launched: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (   isAtLeastOneItemCanBeShown(QList<UIVirtualMachineItem*>() << pItem)
            || (   isAtLeastOneItemCanBeStarted(QList<UIVirtualMachineItem*>() << pItem)
                && fStartConfirmed))
        {
            /* For local machine: */
            if (pItem->itemType() == UIVirtualMachineItemType_Local)
            {
                /* Fetch item launch mode: */
                UICommon::LaunchMode enmItemLaunchMode = enmLaunchMode;
                if (enmItemLaunchMode == UICommon::LaunchMode_Invalid)
                    enmItemLaunchMode = pItem->isItemRunningHeadless()
                                      ? UICommon::LaunchMode_Separate
                                      : qApp->keyboardModifiers() == Qt::ShiftModifier
                                      ? UICommon::LaunchMode_Headless
                                      : UICommon::LaunchMode_Default;

                /* Launch current VM: */
                CMachine machine = pItem->toLocal()->machine();
                uiCommon().launchMachine(machine, enmItemLaunchMode);
            }
            /* For real cloud machine: */
            else if (pItem->itemType() == UIVirtualMachineItemType_CloudReal)
            {
                /* Acquire cloud machine: */
                CCloudMachine comCloudMachine = pItem->toCloud()->machine();
                /* Launch current VM: */
                uiCommon().launchMachine(comCloudMachine);
                /* Update info in any case: */
                pItem->toCloud()->updateInfoAsync(false /* delayed? */);
            }
        }
    }
}

void UIVirtualBoxManager::updateActionsVisibility()
{
    /* Determine whether Machine or Group menu should be shown at all: */
    const bool fGlobalMenuShown  = m_pWidget->isGlobalItemSelected();
    const bool fGroupMenuShown   = m_pWidget->isGroupItemSelected()   &&  isSingleGroupSelected();
    const bool fMachineMenuShown = m_pWidget->isMachineItemSelected() && !isSingleGroupSelected();
    actionPool()->action(UIActionIndexST_M_Welcome)->setVisible(fGlobalMenuShown);
    actionPool()->action(UIActionIndexST_M_Group)->setVisible(fGroupMenuShown);
    actionPool()->action(UIActionIndexST_M_Machine)->setVisible(fMachineMenuShown);

    /* Determine whether Media menu should be visible: */
    const bool fMediumMenuShown = fGlobalMenuShown && m_pWidget->currentGlobalTool() == UIToolType_Media;
    actionPool()->action(UIActionIndexST_M_Medium)->setVisible(fMediumMenuShown);
    /* Determine whether Network menu should be visible: */
    const bool fNetworkMenuShown = fGlobalMenuShown && m_pWidget->currentGlobalTool() == UIToolType_Network;
    actionPool()->action(UIActionIndexST_M_Network)->setVisible(fNetworkMenuShown);
    /* Determine whether Cloud menu should be visible: */
    const bool fCloudMenuShown = fGlobalMenuShown && m_pWidget->currentGlobalTool() == UIToolType_Cloud;
    actionPool()->action(UIActionIndexST_M_Cloud)->setVisible(fCloudMenuShown);

    /* Determine whether Resources menu should be visible: */
    const bool fResourcesMenuShown = fGlobalMenuShown && m_pWidget->currentGlobalTool() == UIToolType_VMResourceMonitor;
    actionPool()->action(UIActionIndexST_M_VMResourceMonitor)->setVisible(fResourcesMenuShown);

    /* Determine whether Snapshots menu should be visible: */
    const bool fSnapshotMenuShown = (fMachineMenuShown || fGroupMenuShown) &&
                                    m_pWidget->currentMachineTool() == UIToolType_Snapshots;
    actionPool()->action(UIActionIndexST_M_Snapshot)->setVisible(fSnapshotMenuShown);
    /* Determine whether Logs menu should be visible: */
    const bool fLogViewerMenuShown = (fMachineMenuShown || fGroupMenuShown) &&
                                     m_pWidget->currentMachineTool() == UIToolType_Logs;
    actionPool()->action(UIActionIndex_M_Log)->setVisible(fLogViewerMenuShown);

    /* Hide action shortcuts: */
    if (!fGlobalMenuShown)
        actionPool()->setShortcutsVisible(UIActionIndexST_M_Welcome, false);
    if (!fGroupMenuShown)
        actionPool()->setShortcutsVisible(UIActionIndexST_M_Group, false);
    if (!fMachineMenuShown)
        actionPool()->setShortcutsVisible(UIActionIndexST_M_Machine, false);

    /* Show action shortcuts: */
    if (fGlobalMenuShown)
        actionPool()->setShortcutsVisible(UIActionIndexST_M_Welcome, true);
    if (fGroupMenuShown)
        actionPool()->setShortcutsVisible(UIActionIndexST_M_Group, true);
    if (fMachineMenuShown)
        actionPool()->setShortcutsVisible(UIActionIndexST_M_Machine, true);
}

void UIVirtualBoxManager::updateActionsAppearance()
{
    /* Get current items: */
    QList<UIVirtualMachineItem*> items = currentItems();

    /* Enable/disable File/Application actions: */
    actionPool()->action(UIActionIndex_M_Application_S_Preferences)->setEnabled(isActionEnabled(UIActionIndex_M_Application_S_Preferences, items));
    actionPool()->action(UIActionIndexST_M_File_S_ExportAppliance)->setEnabled(isActionEnabled(UIActionIndexST_M_File_S_ExportAppliance, items));
    actionPool()->action(UIActionIndexST_M_File_S_ImportAppliance)->setEnabled(isActionEnabled(UIActionIndexST_M_File_S_ImportAppliance, items));
    actionPool()->action(UIActionIndexST_M_File_S_NewCloudVM)->setEnabled(isActionEnabled(UIActionIndexST_M_File_S_NewCloudVM, items));

    /* Enable/disable welcome actions: */
    actionPool()->action(UIActionIndexST_M_Welcome_S_New)->setEnabled(isActionEnabled(UIActionIndexST_M_Welcome_S_New, items));
    actionPool()->action(UIActionIndexST_M_Welcome_S_Add)->setEnabled(isActionEnabled(UIActionIndexST_M_Welcome_S_Add, items));

    /* Enable/disable group actions: */
    actionPool()->action(UIActionIndexST_M_Group_S_New)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_New, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Add)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Add, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Rename)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Rename, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Remove)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Remove, items));
    actionPool()->action(UIActionIndexST_M_Group_T_Pause)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_T_Pause, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Reset)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Reset, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Discard)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Discard, items));
    actionPool()->action(UIActionIndexST_M_Group_S_ShowLogDialog)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_ShowLogDialog, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Refresh)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Refresh, items));
    actionPool()->action(UIActionIndexST_M_Group_S_ShowInFileManager)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_ShowInFileManager, items));
    actionPool()->action(UIActionIndexST_M_Group_S_CreateShortcut)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_CreateShortcut, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Sort)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Sort, items));

    /* Enable/disable machine actions: */
    actionPool()->action(UIActionIndexST_M_Machine_S_New)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_New, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Add)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Add, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Settings)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Settings, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Clone)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Clone, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Move)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Move, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_ExportToOCI)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_ExportToOCI, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Remove)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Remove, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_MoveToGroup)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_MoveToGroup, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_MoveToGroup_S_New)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_MoveToGroup_S_New, items));
    actionPool()->action(UIActionIndexST_M_Machine_T_Pause)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_T_Pause, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Reset)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Reset, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Discard)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Discard, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_ShowLogDialog)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_ShowLogDialog, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Refresh)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Refresh, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_ShowInFileManager)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_ShowInFileManager, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_CreateShortcut)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_CreateShortcut, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_SortParent)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_SortParent, items));

    /* Enable/disable group-start-or-show actions: */
    actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_StartOrShow, items));
    actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartNormal)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_StartOrShow_S_StartNormal, items));
    actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartHeadless)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_StartOrShow_S_StartHeadless, items));
    actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartDetachable)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_StartOrShow_S_StartDetachable, items));

    /* Enable/disable machine-start-or-show actions: */
    actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_StartOrShow, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartNormal)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_StartOrShow_S_StartNormal, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartHeadless)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_StartOrShow_S_StartHeadless, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartDetachable)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_StartOrShow_S_StartDetachable, items));

    /* Enable/disable group-close actions: */
    actionPool()->action(UIActionIndexST_M_Group_M_Close)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close, items));
    actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Detach)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close_S_Detach, items));
    actionPool()->action(UIActionIndexST_M_Group_M_Close_S_SaveState)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close_S_SaveState, items));
    actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Shutdown)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close_S_Shutdown, items));
    actionPool()->action(UIActionIndexST_M_Group_M_Close_S_PowerOff)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close_S_PowerOff, items));

    /* Enable/disable machine-close actions: */
    actionPool()->action(UIActionIndexST_M_Machine_M_Close)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Detach)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_Detach, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_SaveState)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_SaveState, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Shutdown)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_Shutdown, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_PowerOff)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_PowerOff, items));

    /* Get current item: */
    UIVirtualMachineItem *pItem = currentItem();

    /* Discard/Terminate action is deremined by 1st item: */
    if (   pItem
        && (   pItem->itemType() == UIVirtualMachineItemType_CloudFake
            || pItem->itemType() == UIVirtualMachineItemType_CloudReal))
    {
        actionPool()->action(UIActionIndexST_M_Group_S_Discard)->toActionPolymorphicMenu()->setState(1);
        actionPool()->action(UIActionIndexST_M_Machine_S_Discard)->toActionPolymorphicMenu()->setState(1);
    }
    else
    {
        actionPool()->action(UIActionIndexST_M_Group_S_Discard)->toActionPolymorphicMenu()->setState(0);
        actionPool()->action(UIActionIndexST_M_Machine_S_Discard)->toActionPolymorphicMenu()->setState(0);
    }

    /* Start/Show action is deremined by 1st item: */
    if (pItem && pItem->accessible())
    {
        actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow)->toActionPolymorphicMenu()->setState(pItem->isItemPoweredOff() ? 0 : 1);
        actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow)->toActionPolymorphicMenu()->setState(pItem->isItemPoweredOff() ? 0 : 1);
        /// @todo Hmm, fix it?
//        QToolButton *pButton = qobject_cast<QToolButton*>(m_pToolBar->widgetForAction(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow)));
//        if (pButton)
//            pButton->setPopupMode(pItem->isItemPoweredOff() ? QToolButton::MenuButtonPopup : QToolButton::DelayedPopup);
    }
    else
    {
        actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow)->toActionPolymorphicMenu()->setState(0);
        actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow)->toActionPolymorphicMenu()->setState(0);
        /// @todo Hmm, fix it?
//        QToolButton *pButton = qobject_cast<QToolButton*>(m_pToolBar->widgetForAction(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow)));
//        if (pButton)
//            pButton->setPopupMode(pItem->isItemPoweredOff() ? QToolButton::MenuButtonPopup : QToolButton::DelayedPopup);
    }

    /* Pause/Resume action is deremined by 1st started item: */
    UIVirtualMachineItem *pFirstStartedAction = 0;
    foreach (UIVirtualMachineItem *pSelectedItem, items)
    {
        if (pSelectedItem->isItemStarted())
        {
            pFirstStartedAction = pSelectedItem;
            break;
        }
    }
    /* Update the group Pause/Resume action appearance: */
    actionPool()->action(UIActionIndexST_M_Group_T_Pause)->blockSignals(true);
    actionPool()->action(UIActionIndexST_M_Group_T_Pause)->setChecked(pFirstStartedAction && pFirstStartedAction->isItemPaused());
    actionPool()->action(UIActionIndexST_M_Group_T_Pause)->retranslateUi();
    actionPool()->action(UIActionIndexST_M_Group_T_Pause)->blockSignals(false);
    /* Update the machine Pause/Resume action appearance: */
    actionPool()->action(UIActionIndexST_M_Machine_T_Pause)->blockSignals(true);
    actionPool()->action(UIActionIndexST_M_Machine_T_Pause)->setChecked(pFirstStartedAction && pFirstStartedAction->isItemPaused());
    actionPool()->action(UIActionIndexST_M_Machine_T_Pause)->retranslateUi();
    actionPool()->action(UIActionIndexST_M_Machine_T_Pause)->blockSignals(false);

    /* Update action toggle states: */
    if (m_pWidget)
    {
        switch (m_pWidget->currentMachineTool())
        {
            case UIToolType_Details:
            {
                actionPool()->action(UIActionIndexST_M_Group_M_Tools_T_Details)->setChecked(true);
                actionPool()->action(UIActionIndexST_M_Machine_M_Tools_T_Details)->setChecked(true);
                break;
            }
            case UIToolType_Snapshots:
            {
                actionPool()->action(UIActionIndexST_M_Group_M_Tools_T_Snapshots)->setChecked(true);
                actionPool()->action(UIActionIndexST_M_Machine_M_Tools_T_Snapshots)->setChecked(true);
                break;
            }
            case UIToolType_Logs:
            {
                actionPool()->action(UIActionIndexST_M_Group_M_Tools_T_Logs)->setChecked(true);
                actionPool()->action(UIActionIndexST_M_Machine_M_Tools_T_Logs)->setChecked(true);
                break;
            }
            default:
                break;
        }
    }
}

bool UIVirtualBoxManager::isActionEnabled(int iActionIndex, const QList<UIVirtualMachineItem*> &items)
{
    /* Make sure action pool exists: */
    AssertPtrReturn(actionPool(), false);

    /* Any "opened" action is by definition disabled: */
    if (   actionPool()->action(iActionIndex)
        && actionPool()->action(iActionIndex)->property("opened").toBool())
        return false;

    /* For known *global* action types: */
    switch (iActionIndex)
    {
        case UIActionIndex_M_Application_S_Preferences:
        case UIActionIndexST_M_File_S_ExportAppliance:
        case UIActionIndexST_M_File_S_ImportAppliance:
        case UIActionIndexST_M_File_S_NewCloudVM:
        case UIActionIndexST_M_Welcome_S_New:
        case UIActionIndexST_M_Welcome_S_Add:
            return true;
        default:
            break;
    }

    /* No *machine* actions enabled for empty item list: */
    if (items.isEmpty())
        return false;

    /* Get first item: */
    UIVirtualMachineItem *pItem = items.first();

    /* For known *machine* action types: */
    switch (iActionIndex)
    {
        case UIActionIndexST_M_Group_S_New:
        case UIActionIndexST_M_Group_S_Add:
        {
            return !isGroupSavingInProgress() &&
                   (isSingleLocalGroupSelected() ||
                    isSingleCloudProfileGroupSelected());
        }
        case UIActionIndexST_M_Group_S_Sort:
        {
            return !isGroupSavingInProgress() &&
                   isSingleGroupSelected() &&
                   isItemsLocal(items);
        }
        case UIActionIndexST_M_Group_S_Rename:
        case UIActionIndexST_M_Group_S_Remove:
        {
            return !isGroupSavingInProgress() &&
                   isSingleGroupSelected() &&
                   isItemsLocal(items) &&
                   isItemsPoweredOff(items);
        }
        case UIActionIndexST_M_Machine_S_New:
        case UIActionIndexST_M_Machine_S_Add:
        {
            return !isGroupSavingInProgress();
        }
        case UIActionIndexST_M_Machine_S_Settings:
        {
            return !isGroupSavingInProgress() &&
                   items.size() == 1 &&
                   pItem->configurationAccessLevel() != ConfigurationAccessLevel_Null &&
                   (m_pWidget->currentMachineTool() != UIToolType_Snapshots ||
                    m_pWidget->isCurrentStateItemSelected());
        }
        case UIActionIndexST_M_Machine_S_Clone:
        case UIActionIndexST_M_Machine_S_Move:
        {
            return !isGroupSavingInProgress() &&
                   items.size() == 1 &&
                   pItem->toLocal() &&
                   pItem->isItemEditable();
        }
        case UIActionIndexST_M_Machine_S_ExportToOCI:
        {
            return items.size() == 1 &&
                   pItem->toLocal();
        }
        case UIActionIndexST_M_Machine_S_Remove:
        {
            return !isGroupSavingInProgress() &&
                   isAtLeastOneItemRemovable(items);
        }
        case UIActionIndexST_M_Machine_M_MoveToGroup:
        case UIActionIndexST_M_Machine_M_MoveToGroup_S_New:
        {
            return !isGroupSavingInProgress() &&
                   !isAllItemsOfOneGroupSelected() &&
                   isItemsLocal(items) &&
                   isItemsPoweredOff(items);
        }
        case UIActionIndexST_M_Group_M_StartOrShow:
        case UIActionIndexST_M_Group_M_StartOrShow_S_StartNormal:
        case UIActionIndexST_M_Machine_M_StartOrShow:
        case UIActionIndexST_M_Machine_M_StartOrShow_S_StartNormal:
        {
            return !isGroupSavingInProgress() &&
                   isAtLeastOneItemCanBeStartedOrShown(items) &&
                    (m_pWidget->currentMachineTool() != UIToolType_Snapshots ||
                     m_pWidget->isCurrentStateItemSelected());
        }
        case UIActionIndexST_M_Group_M_StartOrShow_S_StartHeadless:
        case UIActionIndexST_M_Group_M_StartOrShow_S_StartDetachable:
        case UIActionIndexST_M_Machine_M_StartOrShow_S_StartHeadless:
        case UIActionIndexST_M_Machine_M_StartOrShow_S_StartDetachable:
        {
            return !isGroupSavingInProgress() &&
                   isItemsLocal(items) &&
                   isAtLeastOneItemCanBeStartedOrShown(items) &&
                    (m_pWidget->currentMachineTool() != UIToolType_Snapshots ||
                     m_pWidget->isCurrentStateItemSelected());
        }
        case UIActionIndexST_M_Group_S_Discard:
        case UIActionIndexST_M_Machine_S_Discard:
        {
            return !isGroupSavingInProgress() &&
                   isAtLeastOneItemDiscardable(items) &&
                    (m_pWidget->currentMachineTool() != UIToolType_Snapshots ||
                     m_pWidget->isCurrentStateItemSelected());
        }
        case UIActionIndexST_M_Group_S_ShowLogDialog:
        case UIActionIndexST_M_Machine_S_ShowLogDialog:
        {
            return isItemsLocal(items) &&
                   isAtLeastOneItemAccessible(items);
        }
        case UIActionIndexST_M_Group_T_Pause:
        case UIActionIndexST_M_Machine_T_Pause:
        {
            return isItemsLocal(items) &&
                   isAtLeastOneItemStarted(items);
        }
        case UIActionIndexST_M_Group_S_Reset:
        case UIActionIndexST_M_Machine_S_Reset:
        {
            return isItemsLocal(items) &&
                   isAtLeastOneItemRunning(items);
        }
        case UIActionIndexST_M_Group_S_Refresh:
        case UIActionIndexST_M_Machine_S_Refresh:
        {
            return isAtLeastOneItemInaccessible(items);
        }
        case UIActionIndexST_M_Group_S_ShowInFileManager:
        case UIActionIndexST_M_Machine_S_ShowInFileManager:
        {
            return isItemsLocal(items) &&
                   isAtLeastOneItemAccessible(items);
        }
        case UIActionIndexST_M_Machine_S_SortParent:
        {
            return !isGroupSavingInProgress() &&
                   isItemsLocal(items);
        }
        case UIActionIndexST_M_Group_S_CreateShortcut:
        case UIActionIndexST_M_Machine_S_CreateShortcut:
        {
            return isAtLeastOneItemSupportsShortcuts(items);
        }
        case UIActionIndexST_M_Group_M_Close:
        case UIActionIndexST_M_Machine_M_Close:
        {
            return isAtLeastOneItemStarted(items);
        }
        case UIActionIndexST_M_Group_M_Close_S_Detach:
        case UIActionIndexST_M_Machine_M_Close_S_Detach:
        {
            return isItemsLocal(items) &&
                   isActionEnabled(UIActionIndexST_M_Machine_M_Close, items);
        }
        case UIActionIndexST_M_Group_M_Close_S_SaveState:
        case UIActionIndexST_M_Machine_M_Close_S_SaveState:
        {
            return isItemsLocal(items) &&
                   isActionEnabled(UIActionIndexST_M_Machine_M_Close, items);
        }
        case UIActionIndexST_M_Group_M_Close_S_Shutdown:
        case UIActionIndexST_M_Machine_M_Close_S_Shutdown:
        {
            return isActionEnabled(UIActionIndexST_M_Machine_M_Close, items) &&
                   isAtLeastOneItemAbleToShutdown(items);
        }
        case UIActionIndexST_M_Group_M_Close_S_PowerOff:
        case UIActionIndexST_M_Machine_M_Close_S_PowerOff:
        {
            return isActionEnabled(UIActionIndexST_M_Machine_M_Close, items);
        }
        default:
            break;
    }

    /* Unknown actions are disabled: */
    return false;
}

/* static */
bool UIVirtualBoxManager::isItemsLocal(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (!pItem->toLocal())
            return false;
    return true;
}

/* static */
bool UIVirtualBoxManager::isItemsPoweredOff(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (!pItem->isItemPoweredOff())
            return false;
    return true;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemAbleToShutdown(const QList<UIVirtualMachineItem*> &items)
{
    /* Enumerate all the passed items: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* Skip non-running machines: */
        if (!pItem->isItemRunning())
            continue;

        /* For local machine: */
        if (pItem->itemType() == UIVirtualMachineItemType_Local)
        {
            /* Skip session failures: */
            CSession session = uiCommon().openExistingSession(pItem->id());
            if (session.isNull())
                continue;
            /* Skip console failures: */
            CConsole console = session.GetConsole();
            if (console.isNull())
            {
                /* Do not forget to release machine: */
                session.UnlockMachine();
                continue;
            }
            /* Is the guest entered ACPI mode? */
            bool fGuestEnteredACPIMode = console.GetGuestEnteredACPIMode();
            /* Do not forget to release machine: */
            session.UnlockMachine();
            /* True if the guest entered ACPI mode: */
            if (fGuestEnteredACPIMode)
                return true;
        }
        /* For real cloud machine: */
        else if (pItem->itemType() == UIVirtualMachineItemType_CloudReal)
        {
            /* Running cloud VM has it by definition: */
            return true;
        }
    }
    /* False by default: */
    return false;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemSupportsShortcuts(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (   pItem->accessible()
            && pItem->toLocal()
#ifdef VBOX_WS_MAC
            /* On Mac OS X this are real alias files, which don't work with the old legacy xml files. */
            && pItem->toLocal()->settingsFile().endsWith(".vbox", Qt::CaseInsensitive)
#endif
            )
            return true;
    }
    return false;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemAccessible(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (pItem->accessible())
            return true;
    return false;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemInaccessible(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (!pItem->accessible())
            return true;
    return false;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemRemovable(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (pItem->isItemRemovable())
            return true;
    return false;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemCanBeStarted(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (pItem->isItemPoweredOff() && pItem->isItemEditable())
            return true;
    }
    return false;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemCanBeShown(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (   pItem->isItemStarted()
            && pItem->isItemCanBeSwitchedTo())
            return true;
    }
    return false;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemCanBeStartedOrShown(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (   (   pItem->isItemPoweredOff()
                && pItem->isItemEditable())
            || (   pItem->isItemStarted()
                && pItem->isItemCanBeSwitchedTo()))
            return true;
    }
    return false;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemDiscardable(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (   (   pItem->isItemSaved()
                || pItem->itemType() == UIVirtualMachineItemType_CloudReal)
            && pItem->isItemEditable())
            return true;
    return false;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemStarted(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (pItem->isItemStarted())
            return true;
    return false;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemRunning(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (pItem->isItemRunning())
            return true;
    return false;
}
