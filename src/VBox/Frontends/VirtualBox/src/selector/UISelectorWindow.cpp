/* $Id$ */
/** @file
 * VBox Qt GUI - UISelectorWindow class implementation.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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
# include <QDesktopServices>
# include <QMenuBar>
# include <QStatusBar>
# include <QResizeEvent>
# include <QStackedWidget>
# include <QTimer>

/* Local includes: */
# include "QISplitter.h"
# include "QIFileDialog.h"
# include "UIBar.h"
# ifdef VBOX_GUI_WITH_NETWORK_MANAGER
#  include "UINetworkManager.h"
#  include "UINetworkManagerIndicator.h"
#  include "UIUpdateManager.h"
#  include "UIDownloaderUserManual.h"
#  include "UIDownloaderExtensionPack.h"
# endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
# include "UIIconPool.h"
# include "UIWizardCloneVM.h"
# include "UIWizardExportApp.h"
# include "UIWizardImportApp.h"
# include "UIVMDesktop.h"
# include "UIVirtualBoxEventHandler.h"
# include "UIMediumManager.h"
# include "UIMedium.h"
# include "UIMessageCenter.h"
# include "UISelectorWindow.h"
# include "UISettingsDialogSpecific.h"
# include "UIToolBar.h"
# include "UIVMLogViewer.h"
# include "UIDesktopServices.h"
# include "UIGlobalSettingsExtension.h"
# include "UIActionPoolSelector.h"
# include "UIGChooser.h"
# include "UIGDetails.h"
# include "UIVMItem.h"
# include "UIExtraDataManager.h"
# include "VBoxGlobal.h"

# ifdef Q_WS_MAC
#  include "VBoxUtils.h"
#  include "UIWindowMenuManager.h"
#  include "UIImageTools.h"
# endif /* Q_WS_MAC */

/* Other VBox stuff: */
# include <iprt/buildconfig.h>
# include <VBox/version.h>
# ifdef Q_WS_X11
#  include <iprt/env.h>
# endif /* Q_WS_X11 */

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UISelectorWindow::UISelectorWindow(UISelectorWindow **ppSelf, QWidget *pParent,
                                   Qt::WindowFlags flags /* = Qt::Window */)
    : QIWithRetranslateUI2<QMainWindow>(pParent, flags)
    , m_fPolished(false)
    , m_fWarningAboutInaccessibleMediumShown(false)
    , m_pActionPool(0)
    , m_pSplitter(0)
#ifndef Q_WS_MAC
    , m_pBar(0)
#endif /* !Q_WS_MAC */
    , mVMToolBar(0)
    , m_pContainer(0)
    , m_pChooser(0)
    , m_pDetails(0)
    , m_pVMDesktop(0)
{
    /* Remember self: */
    if (ppSelf)
        *ppSelf = this;

#ifdef Q_WS_MAC
    /* We have to make sure that we are getting the front most process.
     * This is necessary for Qt versions > 4.3.3: */
    ::darwinSetFrontMostProcess();
#endif /* Q_WS_MAC */

    /* Prepare: */
    prepareIcon();
    prepareMenuBar();
    prepareStatusBar();
    prepareWidgets();
    prepareConnections();

    /* Load settings: */
    loadSettings();

    /* Translate UI: */
    retranslateUi();

#ifdef Q_WS_MAC
# if MAC_LEOPARD_STYLE
    /* Enable unified toolbars on Mac OS X. Available on Qt >= 4.3.
     * We do this after setting the window pos/size, cause Qt sometimes
     * includes the toolbar height in the content height. */
    mVMToolBar->enableMacToolbar();
# endif /* MAC_LEOPARD_STYLE */

    UIWindowMenuManager::instance()->addWindow(this);
    /* Beta label? */
    if (vboxGlobal().isBeta())
    {
        QPixmap betaLabel = ::betaLabelSleeve(QSize(107, 16));
        ::darwinLabelWindow(this, &betaLabel, false);
    }

    /* General event filter: */
    qApp->installEventFilter(this);
#endif /* Q_WS_MAC */
}

UISelectorWindow::~UISelectorWindow()
{
    /* Destroy event handlers: */
    UIVirtualBoxEventHandler::destroy();

#ifdef Q_WS_MAC
    UIWindowMenuManager::destroy();
#endif /* Q_WS_MAC */

    /* Save settings: */
    saveSettings();

    /* Cleanup: */
    cleanupConnections();
    cleanupMenuBar();
}

void UISelectorWindow::sltStateChanged(QString)
{
    /* Get current item: */
    UIVMItem *pItem = currentItem();

    /* Make sure current item present: */
    if (!pItem)
        return;

    /* Update actions: */
    updateActionsAppearance();
}

void UISelectorWindow::sltSnapshotChanged(QString strId)
{
    /* Get current item: */
    UIVMItem *pItem = currentItem();

    /* Make sure current item present: */
    if (!pItem)
        return;

    /* If signal is for the current item: */
    if (pItem->id() == strId)
        m_pVMDesktop->updateSnapshots(pItem, pItem->machine());
}

void UISelectorWindow::sltDetailsViewIndexChanged(int iWidgetIndex)
{
    if (iWidgetIndex)
        m_pContainer->setCurrentWidget(m_pVMDesktop);
    else
        m_pContainer->setCurrentWidget(m_pDetails);
}

void UISelectorWindow::sltHandleMediumEnumerationFinish()
{
    /* We try to warn about inaccessible mediums only once
     * (after media emumeration started from main() at startup),
     * to avoid annoying the user: */
    if (m_fWarningAboutInaccessibleMediumShown)
        return;
    m_fWarningAboutInaccessibleMediumShown = true;

    /* Make sure MM window is not opened: */
    if (UIMediumManager::instance())
        return;

    /* Look for at least one inaccessible medium: */
    bool fIsThereAnyInaccessibleMedium = false;
    foreach (const QString &strMediumID, vboxGlobal().mediumIDs())
    {
        if (vboxGlobal().medium(strMediumID).state() == KMediumState_Inaccessible)
        {
            fIsThereAnyInaccessibleMedium = true;
            break;
        }
    }

    /* Warn the user about inaccessible medium: */
    if (fIsThereAnyInaccessibleMedium && !msgCenter().warnAboutInaccessibleMedia())
    {
        /* Open the MM window (without refresh): */
        UIMediumManager::showModeless(this, false /* refresh? */);
    }
}

void UISelectorWindow::sltShowSelectorContextMenu(const QPoint &pos)
{
    /* Populate toolbar/statusbar acctions: */
    QList<QAction*> actions;
    QAction *pShowToolBar = new QAction(tr("Show Toolbar"), 0);
    pShowToolBar->setCheckable(true);
#ifdef Q_WS_MAC
    pShowToolBar->setChecked(mVMToolBar->isVisible());
#else /* Q_WS_MAC */
    pShowToolBar->setChecked(m_pBar->isVisible());
#endif /* !Q_WS_MAC */
    actions << pShowToolBar;
    QAction *pShowStatusBar = new QAction(tr("Show Statusbar"), 0);
    pShowStatusBar->setCheckable(true);
    pShowStatusBar->setChecked(statusBar()->isVisible());
    actions << pShowStatusBar;

    QPoint gpos = pos;
    QWidget *pSender = static_cast<QWidget*>(sender());
    if (pSender)
        gpos = pSender->mapToGlobal(pos);
    QAction *pResult = QMenu::exec(actions, gpos);
    if (pResult == pShowToolBar)
    {
        if (pResult->isChecked())
        {
#ifdef Q_WS_MAC
            mVMToolBar->show();
#else /* Q_WS_MAC */
            m_pBar->show();
#endif /* !Q_WS_MAC */
        }
        else
        {
#ifdef Q_WS_MAC
            mVMToolBar->hide();
#else /* Q_WS_MAC */
            m_pBar->hide();
#endif /* !Q_WS_MAC */
        }
    }
    else if (pResult == pShowStatusBar)
    {
        if (pResult->isChecked())
            statusBar()->show();
        else
            statusBar()->hide();
    }
}

void UISelectorWindow::sltShowMediumManager()
{
    /* Show modeless Virtual Medium Manager: */
    UIMediumManager::showModeless(this);
}

void UISelectorWindow::sltShowImportApplianceWizard(const QString &strFileName /* = QString() */)
{
    /* Show Import Appliance wizard: */
#ifdef Q_WS_MAC
    QString strTmpFile = ::darwinResolveAlias(strFileName);
#else /* Q_WS_MAC */
    QString strTmpFile = strFileName;
#endif /* !Q_WS_MAC */
    UISafePointerWizardImportApp pWizard = new UIWizardImportApp(this, strTmpFile);
    pWizard->prepare();
    if (strFileName.isEmpty() || pWizard->isValid())
        pWizard->exec();
    if (pWizard)
        delete pWizard;
}

void UISelectorWindow::sltShowExportApplianceWizard()
{
    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* Populate the list of VM names: */
    QStringList names;
    for (int i = 0; i < items.size(); ++i)
        names << items[i]->name();
    /* Show Export Appliance wizard: */
    UISafePointerWizard pWizard = new UIWizardExportApp(this, names);
    pWizard->prepare();
    pWizard->exec();
    if (pWizard)
        delete pWizard;
}

#ifdef DEBUG
void UISelectorWindow::sltOpenExtraDataManagerWindow()
{
    gEDataManager->openWindow(this);
}
#endif /* DEBUG */

void UISelectorWindow::sltShowPreferencesDialog()
{
#ifdef RT_OS_DARWIN
    /* Check that we do NOT handling that already: */
    if (actionPool()->action(UIActionIndex_M_Application_S_Preferences)->data().toBool())
        return;
    /* Remember that we handling that already: */
    actionPool()->action(UIActionIndex_M_Application_S_Preferences)->setData(true);
#else /* !RT_OS_DARWIN */
    /* Check that we do NOT handling that already: */
    if (actionPool()->action(UIActionIndex_Simple_Preferences)->data().toBool())
        return;
    /* Remember that we handling that already: */
    actionPool()->action(UIActionIndex_Simple_Preferences)->setData(true);
#endif /* !RT_OS_DARWIN */

    /* Don't show the inaccessible warning
     * if the user tries to open global settings: */
    m_fWarningAboutInaccessibleMediumShown = true;

    /* Create and execute global settings window: */
    UISettingsDialogGlobal dialog(this);
    dialog.execute();

#ifdef RT_OS_DARWIN
    /* Remember that we do NOT handling that already: */
    actionPool()->action(UIActionIndex_M_Application_S_Preferences)->setData(false);
#else /* !RT_OS_DARWIN */
    /* Remember that we do NOT handling that already: */
    actionPool()->action(UIActionIndex_Simple_Preferences)->setData(false);
#endif /* !RT_OS_DARWIN */
}

void UISelectorWindow::sltPerformExit()
{
    close();
}

void UISelectorWindow::sltShowAddMachineDialog(const QString &strFileName /* = QString() */)
{
    /* Initialize variables: */
#ifdef Q_WS_MAC
    QString strTmpFile = ::darwinResolveAlias(strFileName);
#else /* Q_WS_MAC */
    QString strTmpFile = strFileName;
#endif /* !Q_WS_MAC */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    if (strTmpFile.isEmpty())
    {
        QString strBaseFolder = vbox.GetSystemProperties().GetDefaultMachineFolder();
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
    CMachine newMachine = vbox.OpenMachine(strTmpFile);
    if (!vbox.isOk())
    {
        msgCenter().cannotOpenMachine(vbox, strTmpFile);
        return;
    }

    /* Make sure this machine was NOT registered already: */
    CMachine oldMachine = vbox.FindMachine(newMachine.GetId());
    if (!oldMachine.isNull())
    {
        msgCenter().cannotReregisterExistingMachine(strTmpFile, oldMachine.GetName());
        return;
    }

    /* Register that machine: */
    vbox.RegisterMachine(newMachine);
}

void UISelectorWindow::sltShowMachineSettingsDialog(const QString &strCategoryRef /* = QString() */,
                                                    const QString &strControlRef /* = QString() */,
                                                    const QString &strId /* = QString() */)
{
    /* Check that we do NOT handling that already: */
    if (actionPool()->action(UIActionIndexST_M_Machine_S_Settings)->data().toBool())
        return;
    /* Remember that we handling that already: */
    actionPool()->action(UIActionIndexST_M_Machine_S_Settings)->setData(true);

    /* Process href from VM details / description: */
    if (!strCategoryRef.isEmpty() && strCategoryRef[0] != '#')
    {
        vboxGlobal().openURL(strCategoryRef);
        return;
    }

    /* Get category and control: */
    QString strCategory = strCategoryRef;
    QString strControl = strControlRef;
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
    m_fWarningAboutInaccessibleMediumShown = true;

    /* Create and execute corresponding VM settings window: */
    UISettingsDialogMachine dialog(this,
                                   QUuid(strId).isNull() ? currentItem()->id() : strId,
                                   strCategory, strControl);
    dialog.execute();

    /* Remember that we do NOT handling that already: */
    actionPool()->action(UIActionIndexST_M_Machine_S_Settings)->setData(false);
}

void UISelectorWindow::sltShowCloneMachineWizard()
{
    /* Get current item: */
    UIVMItem *pItem = currentItem();
    AssertMsgReturnVoid(pItem, ("Current item should be selected!\n"));

    /* Show Clone VM wizard: */
    UISafePointerWizard pWizard = new UIWizardCloneVM(this, pItem->machine());
    pWizard->prepare();
    pWizard->exec();
    if (pWizard)
        delete pWizard;
}

void UISelectorWindow::sltPerformStartOrShowAction()
{
    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For every selected item: */
    foreach (UIVMItem *pItem, items)
    {
        /* Check if current item could be started/showed: */
        if (!isActionEnabled(UIActionIndexST_M_Group_P_StartOrShow, QList<UIVMItem*>() << pItem))
            continue;

        /* Launch/show current VM: */
        CMachine machine = pItem->machine();
        vboxGlobal().launchMachine(machine, qApp->keyboardModifiers() == Qt::ShiftModifier);
    }
}

void UISelectorWindow::sltPerformDiscardAction()
{
    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* Prepare the list of the machines to be discarded: */
    QStringList machineNames;
    QList<UIVMItem*> itemsToDiscard;
    foreach (UIVMItem *pItem, items)
        if (isActionEnabled(UIActionIndexST_M_Group_S_Discard, QList<UIVMItem*>() << pItem))
        {
            machineNames << pItem->name();
            itemsToDiscard << pItem;
        }
    AssertMsg(!machineNames.isEmpty(), ("This action should not be allowed!"));

    /* Confirm discarding saved VM state: */
    if (!msgCenter().confirmDiscardSavedState(machineNames.join(", ")))
        return;

    /* For every confirmed item: */
    foreach (UIVMItem *pItem, itemsToDiscard)
    {
        /* Open a session to modify VM: */
        CSession session = vboxGlobal().openSession(pItem->id());
        if (session.isNull())
            return;

        /* Get session console: */
        CConsole console = session.GetConsole();
        console.DiscardSavedState(true);
        if (!console.isOk())
            msgCenter().cannotDiscardSavedState(console);

        /* Unlock machine finally: */
        session.UnlockMachine();
    }
}

void UISelectorWindow::sltPerformPauseResumeAction(bool fPause)
{
    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For every selected item: */
    foreach (UIVMItem *pItem, items)
    {
        /* Get item state: */
        KMachineState state = pItem->machineState();

        /* Check if current item could be paused/resumed: */
        if (!isActionEnabled(UIActionIndexST_M_Group_T_Pause, QList<UIVMItem*>() << pItem))
            continue;

        /* Check if current item already paused: */
        if (fPause &&
            (state == KMachineState_Paused ||
             state == KMachineState_TeleportingPausedVM))
            continue;

        /* Check if current item already resumed: */
        if (!fPause &&
            (state == KMachineState_Running ||
             state == KMachineState_Teleporting ||
             state == KMachineState_LiveSnapshotting))
            continue;

        /* Open a session to modify VM state: */
        CSession session = vboxGlobal().openExistingSession(pItem->id());
        if (session.isNull())
            return;

        /* Get session console: */
        CConsole console = session.GetConsole();
        /* Pause/resume VM: */
        if (fPause)
            console.Pause();
        else
            console.Resume();
        bool ok = console.isOk();
        if (!ok)
        {
            if (fPause)
                msgCenter().cannotPauseMachine(console);
            else
                msgCenter().cannotResumeMachine(console);
        }

        /* Unlock machine finally: */
        session.UnlockMachine();
    }
}

void UISelectorWindow::sltPerformResetAction()
{
    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* Prepare the list of the machines to be reseted: */
    QStringList machineNames;
    QList<UIVMItem*> itemsToReset;
    foreach (UIVMItem *pItem, items)
        if (isActionEnabled(UIActionIndexST_M_Group_S_Reset, QList<UIVMItem*>() << pItem))
        {
            machineNames << pItem->name();
            itemsToReset << pItem;
        }
    AssertMsg(!machineNames.isEmpty(), ("This action should not be allowed!"));

    /* Confirm reseting VM: */
    if (!msgCenter().confirmResetMachine(machineNames.join(", ")))
        return;

    /* For each selected item: */
    foreach (UIVMItem *pItem, itemsToReset)
    {
        /* Open a session to modify VM state: */
        CSession session = vboxGlobal().openExistingSession(pItem->id());
        if (session.isNull())
            return;

        /* Get session console: */
        CConsole console = session.GetConsole();
        /* Reset VM: */
        console.Reset();

        /* Unlock machine finally: */
        session.UnlockMachine();
    }
}

void UISelectorWindow::sltPerformSaveAction()
{
    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    foreach (UIVMItem *pItem, items)
    {
        /* Check if current item could be saved: */
        if (!isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_SaveState, QList<UIVMItem*>() << pItem))
            continue;

        /* Open a session to modify VM state: */
        CSession session = vboxGlobal().openExistingSession(pItem->id());
        if (session.isNull())
            return;

        /* Get session console: */
        CConsole console = session.GetConsole();
        /* Pause VM first: */
        console.Pause();
        if (console.isOk())
        {
            /* Prepare machine state saving: */
            CProgress progress = console.SaveState();
            if (console.isOk())
            {
                /* Show machine state saving progress: */
                CMachine machine = session.GetMachine();
                msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_state_save_90px.png");
                if (!progress.isOk() || progress.GetResultCode() != 0)
                    msgCenter().cannotSaveMachineState(progress, machine.GetName());
            }
            else
                msgCenter().cannotSaveMachineState(console);
        }
        else
            msgCenter().cannotPauseMachine(console);

        /* Unlock machine finally: */
        session.UnlockMachine();
    }
}

void UISelectorWindow::sltPerformACPIShutdownAction()
{
    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* Prepare the list of the machines to be shutdowned: */
    QStringList machineNames;
    QList<UIVMItem*> itemsToShutdown;
    foreach (UIVMItem *pItem, items)
        if (isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_Shutdown, QList<UIVMItem*>() << pItem))
        {
            machineNames << pItem->name();
            itemsToShutdown << pItem;
        }
    AssertMsg(!machineNames.isEmpty(), ("This action should not be allowed!"));

    /* Confirm ACPI shutdown current VM: */
    if (!msgCenter().confirmACPIShutdownMachine(machineNames.join(", ")))
        return;

    /* For each selected item: */
    foreach (UIVMItem *pItem, itemsToShutdown)
    {
        /* Open a session to modify VM state: */
        CSession session = vboxGlobal().openExistingSession(pItem->id());
        if (session.isNull())
            return;

        /* Get session console: */
        CConsole console = session.GetConsole();
        /* ACPI Shutdown: */
        console.PowerButton();
        if (!console.isOk())
            msgCenter().cannotACPIShutdownMachine(console);

        /* Unlock machine finally: */
        session.UnlockMachine();
    }
}

void UISelectorWindow::sltPerformPowerOffAction()
{
    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* Prepare the list of the machines to be powered off: */
    QStringList machineNames;
    QList<UIVMItem*> itemsToPowerOff;
    foreach (UIVMItem *pItem, items)
        if (isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_PowerOff, QList<UIVMItem*>() << pItem))
        {
            machineNames << pItem->name();
            itemsToPowerOff << pItem;
        }
    AssertMsg(!machineNames.isEmpty(), ("This action should not be allowed!"));

    /* Confirm Power Off current VM: */
    if (!msgCenter().confirmPowerOffMachine(machineNames.join(", ")))
        return;

    /* For each selected item: */
    foreach (UIVMItem *pItem, itemsToPowerOff)
    {
        /* Open a session to modify VM state: */
        CSession session = vboxGlobal().openExistingSession(pItem->id());
        if (session.isNull())
            return;

        /* Get session console: */
        CConsole console = session.GetConsole();
        /* Prepare machine power down: */
        CProgress progress = console.PowerDown();
        if (console.isOk())
        {
            /* Show machine power down progress: */
            CMachine machine = session.GetMachine();
            msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_poweroff_90px.png");
            if (!progress.isOk() || progress.GetResultCode() != 0)
                msgCenter().cannotPowerDownMachine(progress, machine.GetName());
        }
        else
            msgCenter().cannotPowerDownMachine(console);

        /* Unlock machine finally: */
        session.UnlockMachine();
    }
}

void UISelectorWindow::sltShowLogDialog()
{
    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    foreach (UIVMItem *pItem, items)
    {
        /* Check if log could be show for the current item: */
        if (!isActionEnabled(UIActionIndexST_M_Group_S_ShowLogDialog, QList<UIVMItem*>() << pItem))
            continue;

        /* Show VM Log Viewer: */
        UIVMLogViewer::showLogViewerFor(this, pItem->machine());
    }
}

void UISelectorWindow::sltShowMachineInFileManager()
{
    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    foreach (UIVMItem *pItem, items)
    {
        /* Check if that item could be shown in file-browser: */
        if (!isActionEnabled(UIActionIndexST_M_Group_S_ShowInFileManager, QList<UIVMItem*>() << pItem))
            continue;

        /* Show VM in filebrowser: */
        UIDesktopServices::openInFileManager(pItem->machine().GetSettingsFilePath());
    }
}

void UISelectorWindow::sltPerformCreateShortcutAction()
{
    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    foreach (UIVMItem *pItem, items)
    {
        /* Check if shortcuts could be created for this item: */
        if (!isActionEnabled(UIActionIndexST_M_Group_S_CreateShortcut, QList<UIVMItem*>() << pItem))
            continue;

        /* Create shortcut for this VM: */
        const CMachine &machine = pItem->machine();
        UIDesktopServices::createMachineShortcut(machine.GetSettingsFilePath(),
                                                 QDesktopServices::storageLocation(QDesktopServices::DesktopLocation),
                                                 machine.GetName(), machine.GetId());
    }
}

void UISelectorWindow::sltGroupCloseMenuAboutToShow()
{
    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Shutdown)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close_S_Shutdown, items));
}

void UISelectorWindow::sltMachineCloseMenuAboutToShow()
{
    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Shutdown)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_Shutdown, items));
}

void UISelectorWindow::sltCurrentVMItemChanged(bool fRefreshDetails, bool fRefreshSnapshots, bool)
{
    /* Get current item: */
    UIVMItem *pItem = currentItem();

    /* Determine which menu to show: */
    m_pGroupMenuAction->setVisible(m_pChooser->isSingleGroupSelected());
    m_pMachineMenuAction->setVisible(!m_pChooser->isSingleGroupSelected());
    if (m_pGroupMenuAction->isVisible())
    {
        foreach (UIAction *pAction, m_machineActions)
            pAction->hideShortcut();
        foreach (UIAction *pAction, m_groupActions)
            pAction->showShortcut();
    }
    else if (m_pMachineMenuAction->isVisible())
    {
        foreach (UIAction *pAction, m_groupActions)
            pAction->hideShortcut();
        foreach (UIAction *pAction, m_machineActions)
            pAction->showShortcut();
    }

    /* Update action appearance: */
    updateActionsAppearance();

    /* If currently selected VM item is accessible: */
    if (pItem && pItem->accessible())
    {
        /* Make sure valid widget raised: */
        if (m_pVMDesktop->widgetIndex())
            m_pContainer->setCurrentWidget(m_pVMDesktop);
        else
            m_pContainer->setCurrentWidget(m_pDetails);

        if (fRefreshDetails)
            m_pDetails->setItems(currentItems());
        if (fRefreshSnapshots)
        {
            m_pVMDesktop->updateSnapshots(pItem, pItem->machine());
            /* Always hide snapshots-view if
             * single group or more than one machine is selected: */
            if (currentItems().size() > 1 || m_pChooser->isSingleGroupSelected())
                m_pVMDesktop->lockSnapshots();
        }
    }
    /* If currently selected VM item is NOT accessible: */
    else
    {
        /* Make sure valid widget raised: */
        m_pContainer->setCurrentWidget(m_pVMDesktop);

        /* Note that the machine becomes inaccessible (or if the last VM gets
         * deleted), we have to update all fields, ignoring input arguments. */
        if (pItem)
        {
            /* The VM is inaccessible: */
            m_pVMDesktop->updateDetailsError(UIMessageCenter::formatErrorInfo(pItem->accessError()));
        }
        else
        {
            /* Default HTML support in Qt is terrible so just try to get something really simple: */
            m_pVMDesktop->updateDetailsText(
                tr("<h3>Welcome to VirtualBox!</h3>"
                   "<p>The left part of this window is  "
                   "a list of all virtual machines on your computer. "
                   "The list is empty now because you haven't created any virtual "
                   "machines yet."
                   "<img src=:/welcome.png align=right/></p>"
                   "<p>In order to create a new virtual machine, press the "
                   "<b>New</b> button in the main tool bar located "
                   "at the top of the window.</p>"
                   "<p>You can press the <b>%1</b> key to get instant help, "
                   "or visit "
                   "<a href=http://www.virtualbox.org>www.virtualbox.org</a> "
                   "for the latest information and news.</p>")
                   .arg(QKeySequence(QKeySequence::HelpContents).toString(QKeySequence::NativeText)));
        }

        /* Empty and disable other tabs: */
        m_pVMDesktop->updateSnapshots(0, CMachine());
    }
}

void UISelectorWindow::sltOpenUrls(QList<QUrl> list /* = QList<QUrl>() */)
{
    /* Make sure any pending D&D events are consumed. */
    // TODO: What? So dangerous method for so cheap purpose?
    qApp->processEvents();

    if (list.isEmpty())
    {
        list = vboxGlobal().argUrlList();
        vboxGlobal().argUrlList().clear();
    }
    /* Check if we are can handle the dropped urls. */
    for (int i = 0; i < list.size(); ++i)
    {
#ifdef Q_WS_MAC
        QString strFile = ::darwinResolveAlias(list.at(i).toLocalFile());
#else /* Q_WS_MAC */
        QString strFile = list.at(i).toLocalFile();
#endif /* !Q_WS_MAC */
        if (!strFile.isEmpty() && QFile::exists(strFile))
        {
            if (VBoxGlobal::hasAllowedExtension(strFile, VBoxFileExts))
            {
                /* VBox config files. */
                CVirtualBox vbox = vboxGlobal().virtualBox();
                CMachine machine = vbox.FindMachine(strFile);
                if (!machine.isNull())
                {
                    CVirtualBox vbox = vboxGlobal().virtualBox();
                    CMachine machine = vbox.FindMachine(strFile);
                    if (!machine.isNull())
                        vboxGlobal().launchMachine(machine);
                }
                else
                    sltShowAddMachineDialog(strFile);
            }
            else if (VBoxGlobal::hasAllowedExtension(strFile, OVFFileExts))
            {
                /* OVF/OVA. Only one file at the time. */
                sltShowImportApplianceWizard(strFile);
                break;
            }
            else if (VBoxGlobal::hasAllowedExtension(strFile, VBoxExtPackFileExts))
            {
                UIGlobalSettingsExtension::doInstallation(strFile, QString(), this, NULL);
            }
        }
    }
}

void UISelectorWindow::sltGroupSavingUpdate()
{
    updateActionsAppearance();
}

void UISelectorWindow::retranslateUi()
{
    /* Set window title: */
    QString strTitle(VBOX_PRODUCT);
    strTitle += " " + tr("Manager", "Note: main window title which is pretended by the product name.");
#ifdef VBOX_BLEEDING_EDGE
    strTitle += QString(" EXPERIMENTAL build ")
             +  QString(RTBldCfgVersion())
             +  QString(" r")
             +  QString(RTBldCfgRevisionStr())
             +  QString(" - " VBOX_BLEEDING_EDGE);
#endif /* VBOX_BLEEDING_EDGE */
    setWindowTitle(strTitle);

    /* Ensure the details and screenshot view are updated: */
    sltCurrentVMItemChanged();

#ifdef QT_MAC_USE_COCOA
    /* There is a bug in Qt Cocoa which result in showing a "more arrow" when
       the necessary size of the toolbar is increased. Also for some languages
       the with doesn't match if the text increase. So manually adjust the size
       after changing the text. */
    mVMToolBar->updateLayout();
#endif /* QT_MAC_USE_COCOA */
}

bool UISelectorWindow::event(QEvent *pEvent)
{
    /* Which event do we have? */
    switch (pEvent->type())
    {
        /* Handle every Resize and Move we keep track of the geometry. */
        case QEvent::Resize:
        {
            if (isVisible() && (windowState() & (Qt::WindowMaximized | Qt::WindowMinimized | Qt::WindowFullScreen)) == 0)
            {
                QResizeEvent *pResizeEvent = static_cast<QResizeEvent*>(pEvent);
                m_geometry.setSize(pResizeEvent->size());
            }
            break;
        }
        case QEvent::Move:
        {
            if (isVisible() && (windowState() & (Qt::WindowMaximized | Qt::WindowMinimized | Qt::WindowFullScreen)) == 0)
            {
#ifdef Q_WS_MAC
                QMoveEvent *pMoveEvent = static_cast<QMoveEvent*>(pEvent);
                m_geometry.moveTo(pMoveEvent->pos());
#else /* Q_WS_MAC */
                m_geometry.moveTo(geometry().x(), geometry().y());
#endif /* !Q_WS_MAC */
            }
            break;
        }
        case QEvent::WindowDeactivate:
        {
            /* Make sure every status bar hint is cleared when the window lost focus. */
            statusBar()->clearMessage();
            break;
        }
#ifdef Q_WS_MAC
        case QEvent::ContextMenu:
        {
            /* This is the unified context menu event. Lets show the context menu. */
            QContextMenuEvent *pContextMenuEvent = static_cast<QContextMenuEvent*>(pEvent);
            sltShowSelectorContextMenu(pContextMenuEvent->globalPos());
            /* Accept it to interrupt the chain. */
            pContextMenuEvent->accept();
            return false;
            break;
        }
#endif /* Q_WS_MAC */
        default:
            break;
    }
    /* Call to base-class: */
    return QMainWindow::event(pEvent);
}

void UISelectorWindow::showEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    QMainWindow::showEvent(pEvent);

    /* Is polishing required? */
    if (!m_fPolished)
    {
        /* Pass the show-event to polish-event: */
        polishEvent(pEvent);
        /* Mark as polished: */
        m_fPolished = true;
    }
}

void UISelectorWindow::polishEvent(QShowEvent*)
{
    /* Make sure user warned about inaccessible medium(s)
     * even if enumeration had finished before selector window shown: */
    QTimer::singleShot(0, this, SLOT(sltHandleMediumEnumerationFinish()));
}

#ifdef Q_WS_MAC
bool UISelectorWindow::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* Ignore for non-active window: */
    if (!isActiveWindow())
        return QIWithRetranslateUI2<QMainWindow>::eventFilter(pObject, pEvent);

    /* Ignore for other objects: */
    if (qobject_cast<QWidget*>(pObject) &&
        qobject_cast<QWidget*>(pObject)->window() != this)
        return QIWithRetranslateUI2<QMainWindow>::eventFilter(pObject, pEvent);

    /* Which event do we have? */
    switch (pEvent->type())
    {
        case QEvent::FileOpen:
        {
            sltOpenUrls(QList<QUrl>() << static_cast<QFileOpenEvent*>(pEvent)->file());
            pEvent->accept();
            return true;
            break;
        }
        default:
            break;
    }
    /* Call to base-class: */
    return QIWithRetranslateUI2<QMainWindow>::eventFilter(pObject, pEvent);
}
#endif /* Q_WS_MAC */

void UISelectorWindow::prepareIcon()
{
    /* Prepare application icon: */
#if !(defined (Q_WS_WIN) || defined (Q_WS_MAC))
    /* On Win host it's built-in to the executable.
     * On Mac OS X the icon referenced in info.plist is used.
     * On X11 we will provide as much icons as we can.. */
    QIcon icon(":/VirtualBox.svg");
    icon.addFile(":/VirtualBox_48px.png");
    icon.addFile(":/VirtualBox_64px.png");
    setWindowIcon(icon);
#endif
}

void UISelectorWindow::prepareMenuBar()
{
    /* Create action-pool: */
    m_pActionPool = UIActionPool::create(UIActionPoolType_Selector);

    /* Prepare File-menu: */
    prepareMenuFile(actionPool()->action(UIActionIndexST_M_File)->menu());
    menuBar()->addMenu(actionPool()->action(UIActionIndexST_M_File)->menu());

    /* Prepare 'Group' / 'Close' menu: */
    prepareMenuGroupClose(actionPool()->action(UIActionIndexST_M_Group_M_Close)->menu());

    /* Prepare 'Machine' / 'Close' menu: */
    prepareMenuMachineClose(actionPool()->action(UIActionIndexST_M_Machine_M_Close)->menu());

    /* Prepare Group-menu: */
    prepareMenuGroup(actionPool()->action(UIActionIndexST_M_Group)->menu());
    m_pGroupMenuAction = menuBar()->addMenu(actionPool()->action(UIActionIndexST_M_Group)->menu());

    /* Prepare Machine-menu: */
    prepareMenuMachine(actionPool()->action(UIActionIndexST_M_Machine)->menu());
    m_pMachineMenuAction = menuBar()->addMenu(actionPool()->action(UIActionIndexST_M_Machine)->menu());

#ifdef Q_WS_MAC
    menuBar()->addMenu(UIWindowMenuManager::instance(this)->createMenu(this));
#endif /* Q_WS_MAC */

    /* Prepare Help-menu: */
    menuBar()->addMenu(actionPool()->action(UIActionIndex_Menu_Help)->menu());

    /* Setup menubar policy: */
    menuBar()->setContextMenuPolicy(Qt::CustomContextMenu);
}

void UISelectorWindow::prepareMenuFile(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

    /* Populate File-menu: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_File_S_ShowMediumManager));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_File_S_ImportAppliance));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_File_S_ExportAppliance));
#ifndef Q_WS_MAC
    pMenu->addSeparator();
#endif /* Q_WS_MAC */
#ifdef DEBUG
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_File_S_ShowExtraDataManager));
#endif /* DEBUG */
#ifdef RT_OS_DARWIN
    pMenu->addAction(actionPool()->action(UIActionIndex_M_Application_S_Preferences));
#else /* !RT_OS_DARWIN */
    pMenu->addAction(actionPool()->action(UIActionIndex_Simple_Preferences));
#endif /* !RT_OS_DARWIN */
#ifndef Q_WS_MAC
    pMenu->addSeparator();
#endif /* Q_WS_MAC */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_File_S_Close));
}

void UISelectorWindow::prepareMenuGroup(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

    /* Populate Machine-menu: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_New));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Add));
    pMenu->addSeparator();
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Rename));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Remove));
    pMenu->addSeparator();
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_P_StartOrShow));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_T_Pause));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Reset));
    pMenu->addMenu(actionPool()->action(UIActionIndexST_M_Group_M_Close)->menu());
    pMenu->addSeparator();
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Discard));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_ShowLogDialog));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Refresh));
    pMenu->addSeparator();
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_ShowInFileManager));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_CreateShortcut));
    pMenu->addSeparator();
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Sort));

    /* Remember action list: */
    m_groupActions << actionPool()->action(UIActionIndexST_M_Group_S_New)
                   << actionPool()->action(UIActionIndexST_M_Group_S_Add)
                   << actionPool()->action(UIActionIndexST_M_Group_S_Rename)
                   << actionPool()->action(UIActionIndexST_M_Group_S_Remove)
                   << actionPool()->action(UIActionIndexST_M_Group_P_StartOrShow)
                   << actionPool()->action(UIActionIndexST_M_Group_T_Pause)
                   << actionPool()->action(UIActionIndexST_M_Group_S_Reset)
                   << actionPool()->action(UIActionIndexST_M_Group_S_Discard)
                   << actionPool()->action(UIActionIndexST_M_Group_S_ShowLogDialog)
                   << actionPool()->action(UIActionIndexST_M_Group_S_Refresh)
                   << actionPool()->action(UIActionIndexST_M_Group_S_ShowInFileManager)
                   << actionPool()->action(UIActionIndexST_M_Group_S_CreateShortcut)
                   << actionPool()->action(UIActionIndexST_M_Group_S_Sort);
}

void UISelectorWindow::prepareMenuMachine(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

    /* Populate Machine-menu: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_New));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Add));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Settings));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Clone));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Remove));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_AddGroup));
    pMenu->addSeparator();
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_P_StartOrShow));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_T_Pause));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Reset));
    pMenu->addMenu(actionPool()->action(UIActionIndexST_M_Machine_M_Close)->menu());
    pMenu->addSeparator();
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Discard));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_ShowLogDialog));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Refresh));
    pMenu->addSeparator();
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_ShowInFileManager));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_CreateShortcut));
    pMenu->addSeparator();
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_SortParent));

    /* Remember action list: */
    m_machineActions << actionPool()->action(UIActionIndexST_M_Machine_S_New)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_Add)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_Settings)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_Clone)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_Remove)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_AddGroup)
                     << actionPool()->action(UIActionIndexST_M_Machine_P_StartOrShow)
                     << actionPool()->action(UIActionIndexST_M_Machine_T_Pause)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_Reset)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_Discard)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_ShowLogDialog)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_Refresh)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_ShowInFileManager)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_CreateShortcut)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_SortParent);
}

void UISelectorWindow::prepareMenuGroupClose(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

    /* Populate 'Group' / 'Close' menu: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_SaveState));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Shutdown));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_PowerOff));

    /* Remember action list: */
    m_groupActions << actionPool()->action(UIActionIndexST_M_Group_M_Close_S_SaveState)
                   << actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Shutdown)
                   << actionPool()->action(UIActionIndexST_M_Group_M_Close_S_PowerOff);
}

void UISelectorWindow::prepareMenuMachineClose(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

    /* Populate 'Machine' / 'Close' menu: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_SaveState));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Shutdown));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_PowerOff));

    /* Remember action list: */
    m_machineActions << actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_SaveState)
                     << actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Shutdown)
                     << actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_PowerOff);
}

void UISelectorWindow::prepareStatusBar()
{
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    /* Setup statusbar policy: */
    statusBar()->setContextMenuPolicy(Qt::CustomContextMenu);

    /* Add network-manager indicator: */
    UINetworkManagerIndicator *pIndicator = gNetworkManager->indicator();
    statusBar()->addPermanentWidget(pIndicator);
    pIndicator->updateAppearance();
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
}

void UISelectorWindow::prepareWidgets()
{
    /* Prepare splitter: */
    m_pSplitter = new QISplitter(this);
#ifdef Q_WS_X11
    m_pSplitter->setHandleType(QISplitter::Native);
#endif /* Q_WS_X11 */

    /* Prepare tool-bar: */
    mVMToolBar = new UIToolBar(this);
    mVMToolBar->setContextMenuPolicy(Qt::CustomContextMenu);
    mVMToolBar->setIconSize(QSize(32, 32));
    mVMToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    mVMToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_New));
    mVMToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Settings));
    mVMToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_P_StartOrShow));
    mVMToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Discard));

    /* Prepare graphics VM list: */
    m_pChooser = new UIGChooser(this);
    m_pChooser->setStatusBar(statusBar());

    /* Prepare graphics details: */
    m_pDetails = new UIGDetails(this);

    /* Configure splitter colors: */
    m_pSplitter->configureColors(m_pChooser->palette().color(QPalette::Active, QPalette::Window),
                                 m_pDetails->palette().color(QPalette::Active, QPalette::Window));

    /* Prepare details and snapshots tabs: */
    m_pVMDesktop = new UIVMDesktop(mVMToolBar, actionPool()->action(UIActionIndexST_M_Group_S_Refresh), this);

    /* Crate container: */
    m_pContainer = new QStackedWidget(this);
    m_pContainer->addWidget(m_pDetails);
    m_pContainer->addWidget(m_pVMDesktop);

    /* Layout all the widgets: */
#if MAC_LEOPARD_STYLE
    addToolBar(mVMToolBar);
    /* Central widget @ horizontal layout: */
    setCentralWidget(m_pSplitter);
    m_pSplitter->addWidget(m_pChooser);
#else /* MAC_LEOPARD_STYLE */
    QWidget *pCentralWidget = new QWidget(this);
    setCentralWidget(pCentralWidget);
    QVBoxLayout *pCentralLayout = new QVBoxLayout(pCentralWidget);
    pCentralLayout->setContentsMargins(0, 0, 0, 0);
    pCentralLayout->setSpacing(0);
    m_pBar = new UIMainBar(this);
    m_pBar->setContentWidget(mVMToolBar);
    pCentralLayout->addWidget(m_pBar);
    pCentralLayout->addWidget(m_pSplitter);
    m_pSplitter->addWidget(m_pChooser);
#endif /* !MAC_LEOPARD_STYLE */
    m_pSplitter->addWidget(m_pContainer);

    /* Set the initial distribution. The right site is bigger. */
    m_pSplitter->setStretchFactor(0, 2);
    m_pSplitter->setStretchFactor(1, 3);

    /* Bring the VM list to the focus: */
    m_pChooser->setFocus();
}

void UISelectorWindow::prepareConnections()
{
    /* Medium enumeration connections: */
    connect(&vboxGlobal(), SIGNAL(sigMediumEnumerationFinished()), this, SLOT(sltHandleMediumEnumerationFinish()));

    /* Menu-bar connections: */
    connect(menuBar(), SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(sltShowSelectorContextMenu(const QPoint&)));

    /* 'File' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_File_S_ShowMediumManager), SIGNAL(triggered()), this, SLOT(sltShowMediumManager()));
    connect(actionPool()->action(UIActionIndexST_M_File_S_ImportAppliance), SIGNAL(triggered()), this, SLOT(sltShowImportApplianceWizard()));
    connect(actionPool()->action(UIActionIndexST_M_File_S_ExportAppliance), SIGNAL(triggered()), this, SLOT(sltShowExportApplianceWizard()));
#ifdef DEBUG
    connect(actionPool()->action(UIActionIndexST_M_File_S_ShowExtraDataManager), SIGNAL(triggered()), this, SLOT(sltOpenExtraDataManagerWindow()));
#endif /* DEBUG */
#ifdef RT_OS_DARWIN
    connect(actionPool()->action(UIActionIndex_M_Application_S_Preferences), SIGNAL(triggered()), this, SLOT(sltShowPreferencesDialog()));
#else /* !RT_OS_DARWIN */
    connect(actionPool()->action(UIActionIndex_Simple_Preferences), SIGNAL(triggered()), this, SLOT(sltShowPreferencesDialog()));
#endif /* !RT_OS_DARWIN */
    connect(actionPool()->action(UIActionIndexST_M_File_S_Close), SIGNAL(triggered()), this, SLOT(sltPerformExit()));

    /* 'Group' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Group_S_Add), SIGNAL(triggered()), this, SLOT(sltShowAddMachineDialog()));
    connect(actionPool()->action(UIActionIndexST_M_Group_P_StartOrShow), SIGNAL(triggered()), this, SLOT(sltPerformStartOrShowAction()));
    connect(actionPool()->action(UIActionIndexST_M_Group_T_Pause), SIGNAL(toggled(bool)), this, SLOT(sltPerformPauseResumeAction(bool)));
    connect(actionPool()->action(UIActionIndexST_M_Group_S_Reset), SIGNAL(triggered()), this, SLOT(sltPerformResetAction()));
    connect(actionPool()->action(UIActionIndexST_M_Group_S_Discard), SIGNAL(triggered()), this, SLOT(sltPerformDiscardAction()));
    connect(actionPool()->action(UIActionIndexST_M_Group_S_ShowLogDialog), SIGNAL(triggered()), this, SLOT(sltShowLogDialog()));
    connect(actionPool()->action(UIActionIndexST_M_Group_S_ShowInFileManager), SIGNAL(triggered()), this, SLOT(sltShowMachineInFileManager()));
    connect(actionPool()->action(UIActionIndexST_M_Group_S_CreateShortcut), SIGNAL(triggered()), this, SLOT(sltPerformCreateShortcutAction()));

    /* 'Machine' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Add), SIGNAL(triggered()), this, SLOT(sltShowAddMachineDialog()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Settings), SIGNAL(triggered()), this, SLOT(sltShowMachineSettingsDialog()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Clone), SIGNAL(triggered()), this, SLOT(sltShowCloneMachineWizard()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_P_StartOrShow), SIGNAL(triggered()), this, SLOT(sltPerformStartOrShowAction()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_T_Pause), SIGNAL(toggled(bool)), this, SLOT(sltPerformPauseResumeAction(bool)));
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Reset), SIGNAL(triggered()), this, SLOT(sltPerformResetAction()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Discard), SIGNAL(triggered()), this, SLOT(sltPerformDiscardAction()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_ShowLogDialog), SIGNAL(triggered()), this, SLOT(sltShowLogDialog()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_ShowInFileManager), SIGNAL(triggered()), this, SLOT(sltShowMachineInFileManager()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_CreateShortcut), SIGNAL(triggered()), this, SLOT(sltPerformCreateShortcutAction()));

    /* 'Group/Close' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Group_M_Close)->menu(), SIGNAL(aboutToShow()), this, SLOT(sltGroupCloseMenuAboutToShow()));
    connect(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_SaveState), SIGNAL(triggered()), this, SLOT(sltPerformSaveAction()));
    connect(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Shutdown), SIGNAL(triggered()), this, SLOT(sltPerformACPIShutdownAction()));
    connect(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_PowerOff), SIGNAL(triggered()), this, SLOT(sltPerformPowerOffAction()));

    /* 'Machine/Close' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_Close)->menu(), SIGNAL(aboutToShow()), this, SLOT(sltMachineCloseMenuAboutToShow()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_SaveState), SIGNAL(triggered()), this, SLOT(sltPerformSaveAction()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Shutdown), SIGNAL(triggered()), this, SLOT(sltPerformACPIShutdownAction()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_PowerOff), SIGNAL(triggered()), this, SLOT(sltPerformPowerOffAction()));

    /* Status-bar connections: */
    connect(statusBar(), SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(sltShowSelectorContextMenu(const QPoint&)));

    /* Graphics VM chooser connections: */
    connect(m_pChooser, SIGNAL(sigSelectionChanged()), this, SLOT(sltCurrentVMItemChanged()), Qt::QueuedConnection);
    connect(m_pChooser, SIGNAL(sigSlidingStarted()), m_pDetails, SIGNAL(sigSlidingStarted()));
    connect(m_pChooser, SIGNAL(sigToggleStarted()), m_pDetails, SIGNAL(sigToggleStarted()));
    connect(m_pChooser, SIGNAL(sigToggleFinished()), m_pDetails, SIGNAL(sigToggleFinished()));
    connect(m_pChooser, SIGNAL(sigGroupSavingStateChanged()), this, SLOT(sltGroupSavingUpdate()));

    /* Tool-bar connections: */
#ifndef Q_WS_MAC
    connect(mVMToolBar, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(sltShowSelectorContextMenu(const QPoint&)));
#else /* !Q_WS_MAC */
    /* A simple connect doesn't work on the Mac, also we want receive right
     * click notifications on the title bar. So register our own handler. */
    ::darwinRegisterForUnifiedToolbarContextMenuEvents(this);
#endif /* Q_WS_MAC */

    /* VM desktop connections: */
    connect(m_pVMDesktop, SIGNAL(sigCurrentChanged(int)), this, SLOT(sltDetailsViewIndexChanged(int)));
    connect(m_pDetails, SIGNAL(sigLinkClicked(const QString&, const QString&, const QString&)),
            this, SLOT(sltShowMachineSettingsDialog(const QString&, const QString&, const QString&)));

    /* Global event handlers: */
    connect(gVBoxEvents, SIGNAL(sigMachineStateChange(QString, KMachineState)), this, SLOT(sltStateChanged(QString)));
    connect(gVBoxEvents, SIGNAL(sigSessionStateChange(QString, KSessionState)), this, SLOT(sltStateChanged(QString)));
    connect(gVBoxEvents, SIGNAL(sigSnapshotTake(QString, QString)), this, SLOT(sltSnapshotChanged(QString)));
    connect(gVBoxEvents, SIGNAL(sigSnapshotDelete(QString, QString)), this, SLOT(sltSnapshotChanged(QString)));
    connect(gVBoxEvents, SIGNAL(sigSnapshotChange(QString, QString)), this, SLOT(sltSnapshotChanged(QString)));
}

void UISelectorWindow::loadSettings()
{
    /* Restore window geometry: */
    {
        /* Load geometry: */
        m_geometry = gEDataManager->selectorWindowGeometry(this);
#ifdef Q_WS_MAC
        move(m_geometry.topLeft());
        resize(m_geometry.size());
#else /* Q_WS_MAC */
        setGeometry(m_geometry);
#endif /* !Q_WS_MAC */
        LogRel(("UISelectorWindow: Geometry loaded to: %dx%d @ %dx%d.\n",
                m_geometry.x(), m_geometry.y(), m_geometry.width(), m_geometry.height()));

        /* Maximize (if necessary): */
        if (gEDataManager->selectorWindowShouldBeMaximized())
            showMaximized();
    }

    /* Restore splitter handle position: */
    {
        /* Read splitter hints: */
        QList<int> sizes = gEDataManager->selectorWindowSplitterHints();
        /* If both hints are zero, we have the 'default' case: */
        if (sizes[0] == 0 && sizes[1] == 0)
        {
            /* Propose some 'default' based on current dialog width: */
            sizes[0] = (double)width()     / 3 * .9;
            sizes[1] = (double)width() * 2 / 3 * .9;
        }
        /* Pass hints to the splitter: */
        m_pSplitter->setSizes(sizes);
    }

    /* Restore toolbar and statusbar visibility: */
    {
#ifdef Q_WS_MAC
        mVMToolBar->setHidden(!gEDataManager->selectorWindowToolBarVisible());
#else /* Q_WS_MAC */
        m_pBar->setHidden(!gEDataManager->selectorWindowToolBarVisible());
#endif /* !Q_WS_MAC */
        statusBar()->setHidden(!gEDataManager->selectorWindowStatusBarVisible());
    }
}

void UISelectorWindow::saveSettings()
{
    /* Save toolbar and statusbar visibility: */
    {
#ifdef Q_WS_MAC
        gEDataManager->setSelectorWindowToolBarVisible(!mVMToolBar->isHidden());
#else /* Q_WS_MAC */
        gEDataManager->setSelectorWindowToolBarVisible(!m_pBar->isHidden());
#endif /* !Q_WS_MAC */
        gEDataManager->setSelectorWindowStatusBarVisible(!statusBar()->isHidden());
    }

    /* Save splitter handle position: */
    {
        gEDataManager->setSelectorWindowSplitterHints(m_pSplitter->sizes());
    }

    /* Save window geometry: */
    {
#ifdef Q_WS_MAC
        gEDataManager->setSelectorWindowGeometry(m_geometry, ::darwinIsWindowMaximized(this));
#else /* Q_WS_MAC */
        gEDataManager->setSelectorWindowGeometry(m_geometry, isMaximized());
#endif /* !Q_WS_MAC */
        LogRel(("UISelectorWindow: Geometry saved as: %dx%d @ %dx%d.\n",
                m_geometry.x(), m_geometry.y(), m_geometry.width(), m_geometry.height()));
    }
}

void UISelectorWindow::cleanupConnections()
{
#ifdef Q_WS_MAC
    /* Tool-bar connections: */
    ::darwinUnregisterForUnifiedToolbarContextMenuEvents(this);
#endif /* Q_WS_MAC */
}

void UISelectorWindow::cleanupMenuBar()
{
    /* Destroy action-pool: */
    UIActionPool::destroy(m_pActionPool);
}

UIVMItem* UISelectorWindow::currentItem() const
{
    return m_pChooser->currentItem();
}

QList<UIVMItem*> UISelectorWindow::currentItems() const
{
    return m_pChooser->currentItems();
}

void UISelectorWindow::updateActionsAppearance()
{
    /* Get current item(s): */
    UIVMItem *pItem = currentItem();
    QList<UIVMItem*> items = currentItems();

    /* Enable/disable group actions: */
    actionPool()->action(UIActionIndexST_M_Group_S_Rename)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Rename, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Remove)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Remove, items));
    actionPool()->action(UIActionIndexST_M_Group_P_StartOrShow)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_P_StartOrShow, items));
    actionPool()->action(UIActionIndexST_M_Group_T_Pause)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_T_Pause, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Reset)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Reset, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Discard)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Discard, items));
    actionPool()->action(UIActionIndexST_M_Group_S_ShowLogDialog)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_ShowLogDialog, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Refresh)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Refresh, items));
    actionPool()->action(UIActionIndexST_M_Group_S_ShowInFileManager)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_ShowInFileManager, items));
    actionPool()->action(UIActionIndexST_M_Group_S_CreateShortcut)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_CreateShortcut, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Sort)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Sort, items));

    /* Enable/disable machine actions: */
    actionPool()->action(UIActionIndexST_M_Machine_S_Settings)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Settings, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Clone)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Clone, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Remove)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Remove, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_AddGroup)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_AddGroup, items));
    actionPool()->action(UIActionIndexST_M_Machine_P_StartOrShow)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_P_StartOrShow, items));
    actionPool()->action(UIActionIndexST_M_Machine_T_Pause)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_T_Pause, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Reset)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Reset, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Discard)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Discard, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_ShowLogDialog)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_ShowLogDialog, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Refresh)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Refresh, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_ShowInFileManager)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_ShowInFileManager, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_CreateShortcut)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_CreateShortcut, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_SortParent)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_SortParent, items));

    /* Enable/disable group-close actions: */
    actionPool()->action(UIActionIndexST_M_Group_M_Close)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close, items));
    actionPool()->action(UIActionIndexST_M_Group_M_Close_S_SaveState)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close_S_SaveState, items));
    actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Shutdown)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close_S_Shutdown, items));
    actionPool()->action(UIActionIndexST_M_Group_M_Close_S_PowerOff)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close_S_PowerOff, items));

    /* Enable/disable machine-close actions: */
    actionPool()->action(UIActionIndexST_M_Machine_M_Close)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_SaveState)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_SaveState, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Shutdown)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_Shutdown, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_PowerOff)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_PowerOff, items));

    /* Start/Show action is deremined by 1st item: */
    if (pItem && pItem->accessible())
    {
        actionPool()->action(UIActionIndexST_M_Group_P_StartOrShow)->toActionPolymorphic()->setState(UIVMItem::isItemPoweredOff(pItem) ? 0 : 1);
        actionPool()->action(UIActionIndexST_M_Machine_P_StartOrShow)->toActionPolymorphic()->setState(UIVMItem::isItemPoweredOff(pItem) ? 0 : 1);
    }
    else
    {
        actionPool()->action(UIActionIndexST_M_Group_P_StartOrShow)->toActionPolymorphic()->setState(0);
        actionPool()->action(UIActionIndexST_M_Machine_P_StartOrShow)->toActionPolymorphic()->setState(0);
    }

    /* Pause/Resume action is deremined by 1st started item: */
    UIVMItem *pFirstStartedAction = 0;
    foreach (UIVMItem *pSelectedItem, items)
        if (UIVMItem::isItemStarted(pSelectedItem))
            pFirstStartedAction = pSelectedItem;
    /* Update the Pause/Resume action appearance: */
    actionPool()->action(UIActionIndexST_M_Group_T_Pause)->blockSignals(true);
    actionPool()->action(UIActionIndexST_M_Group_T_Pause)->setChecked(pFirstStartedAction && UIVMItem::isItemPaused(pFirstStartedAction));
    actionPool()->action(UIActionIndexST_M_Group_T_Pause)->retranslateUi();
    actionPool()->action(UIActionIndexST_M_Group_T_Pause)->blockSignals(false);

#ifdef QT_MAC_USE_COCOA
    /* There is a bug in Qt Cocoa which result in showing a "more arrow" when
       the necessary size of the toolbar is increased. Also for some languages
       the with doesn't match if the text increase. So manually adjust the size
       after changing the text. */
    mVMToolBar->updateLayout();
#endif /* QT_MAC_USE_COCOA */
}

bool UISelectorWindow::isActionEnabled(int iActionIndex, const QList<UIVMItem*> &items)
{
    /* No actions enabled for empty item list: */
    if (items.isEmpty())
        return false;

    /* Get first item: */
    UIVMItem *pItem = items.first();

    /* For known action types: */
    switch (iActionIndex)
    {
        case UIActionIndexST_M_Group_S_Rename:
        case UIActionIndexST_M_Group_S_Remove:
        {
            return !m_pChooser->isGroupSavingInProgress() &&
                   isItemsPoweredOff(items);
        }
        case UIActionIndexST_M_Group_S_Sort:
        {
            return !m_pChooser->isGroupSavingInProgress() &&
                   m_pChooser->isSingleGroupSelected();
        }
        case UIActionIndexST_M_Machine_S_Settings:
        {
            return !m_pChooser->isGroupSavingInProgress() &&
                   items.size() == 1 &&
                   pItem->configurationAccessLevel() != ConfigurationAccessLevel_Null;
        }
        case UIActionIndexST_M_Machine_S_Clone:
        {
            return !m_pChooser->isGroupSavingInProgress() &&
                   items.size() == 1 &&
                   UIVMItem::isItemEditable(pItem);
        }
        case UIActionIndexST_M_Machine_S_Remove:
        {
            return !m_pChooser->isGroupSavingInProgress() &&
                   isAtLeastOneItemRemovable(items);
        }
        case UIActionIndexST_M_Machine_S_AddGroup:
        {
            return !m_pChooser->isGroupSavingInProgress() &&
                   !m_pChooser->isAllItemsOfOneGroupSelected() &&
                   isItemsPoweredOff(items);
        }
        case UIActionIndexST_M_Group_P_StartOrShow:
        case UIActionIndexST_M_Machine_P_StartOrShow:
        {
            return !m_pChooser->isGroupSavingInProgress() &&
                   isAtLeastOneItemCanBeStartedOrShowed(items);
        }
        case UIActionIndexST_M_Group_S_Discard:
        case UIActionIndexST_M_Machine_S_Discard:
        {
            return !m_pChooser->isGroupSavingInProgress() &&
                   isAtLeastOneItemDiscardable(items);
        }
        case UIActionIndexST_M_Group_S_ShowLogDialog:
        case UIActionIndexST_M_Machine_S_ShowLogDialog:
        {
            return isAtLeastOneItemAccessible(items);
        }
        case UIActionIndexST_M_Group_T_Pause:
        case UIActionIndexST_M_Machine_T_Pause:
        {
            return isAtLeastOneItemStarted(items);
        }
        case UIActionIndexST_M_Group_S_Reset:
        case UIActionIndexST_M_Machine_S_Reset:
        {
            return isAtLeastOneItemRunning(items);
        }
        case UIActionIndexST_M_Group_S_Refresh:
        case UIActionIndexST_M_Machine_S_Refresh:
        {
            return isAtLeastOneItemInaccessible(items);
        }
        case UIActionIndexST_M_Group_S_ShowInFileManager:
        case UIActionIndexST_M_Machine_S_ShowInFileManager:
        {
            return isAtLeastOneItemAccessible(items);
        }
        case UIActionIndexST_M_Machine_S_SortParent:
        {
            return !m_pChooser->isGroupSavingInProgress();
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
        case UIActionIndexST_M_Group_M_Close_S_SaveState:
        case UIActionIndexST_M_Machine_M_Close_S_SaveState:
        {
            return isActionEnabled(UIActionIndexST_M_Machine_M_Close, items);
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
bool UISelectorWindow::isItemsPoweredOff(const QList<UIVMItem*> &items)
{
    foreach (UIVMItem *pItem, items)
        if (!UIVMItem::isItemPoweredOff(pItem))
            return false;
    return true;
}

/* static */
bool UISelectorWindow::isAtLeastOneItemAbleToShutdown(const QList<UIVMItem*> &items)
{
    /* Enumerate all the passed items: */
    foreach (UIVMItem *pItem, items)
    {
        /* Skip non-running machines: */
        if (!UIVMItem::isItemRunning(pItem))
            continue;
        /* Skip session failures: */
        CSession session = vboxGlobal().openExistingSession(pItem->id());
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
    /* False by default: */
    return false;
}

/* static */
bool UISelectorWindow::isAtLeastOneItemSupportsShortcuts(const QList<UIVMItem*> &items)
{
    foreach (UIVMItem *pItem, items)
        if (pItem->accessible()
#ifdef Q_WS_MAC
            /* On Mac OS X this are real alias files, which don't work with the old
             * legacy xml files. On the other OS's some kind of start up script is used. */
            && pItem->settingsFile().endsWith(".vbox", Qt::CaseInsensitive)
#endif /* Q_WS_MAC */
            )
            return true;
    return false;
}

/* static */
bool UISelectorWindow::isAtLeastOneItemAccessible(const QList<UIVMItem*> &items)
{
    foreach (UIVMItem *pItem, items)
        if (pItem->accessible())
            return true;
    return false;
}

/* static */
bool UISelectorWindow::isAtLeastOneItemInaccessible(const QList<UIVMItem*> &items)
{
    foreach (UIVMItem *pItem, items)
        if (!pItem->accessible())
            return true;
    return false;
}

/* static */
bool UISelectorWindow::isAtLeastOneItemRemovable(const QList<UIVMItem*> &items)
{
    foreach (UIVMItem *pItem, items)
        if (!pItem->accessible() || UIVMItem::isItemEditable(pItem))
            return true;
    return false;
}

/* static */
bool UISelectorWindow::isAtLeastOneItemCanBeStartedOrShowed(const QList<UIVMItem*> &items)
{
    foreach (UIVMItem *pItem, items)
    {
        if ((UIVMItem::isItemPoweredOff(pItem) && UIVMItem::isItemEditable(pItem)) ||
            (UIVMItem::isItemStarted(pItem) && pItem->canSwitchTo()))
            return true;
    }
    return false;
}

/* static */
bool UISelectorWindow::isAtLeastOneItemDiscardable(const QList<UIVMItem*> &items)
{
    foreach (UIVMItem *pItem, items)
        if (UIVMItem::isItemSaved(pItem) && UIVMItem::isItemEditable(pItem))
            return true;
    return false;
}

/* static */
bool UISelectorWindow::isAtLeastOneItemStarted(const QList<UIVMItem*> &items)
{
    foreach (UIVMItem *pItem, items)
        if (UIVMItem::isItemStarted(pItem))
            return true;
    return false;
}

/* static */
bool UISelectorWindow::isAtLeastOneItemRunning(const QList<UIVMItem*> &items)
{
    foreach (UIVMItem *pItem, items)
        if (UIVMItem::isItemRunning(pItem))
            return true;
    return false;
}

