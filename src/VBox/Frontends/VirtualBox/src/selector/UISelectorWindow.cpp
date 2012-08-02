/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UISelectorWindow class implementation
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
#include <QDesktopWidget>
#include <QDesktopServices>
#include <QMenuBar>
#include <QResizeEvent>
#include <QStackedWidget>

/* Local includes: */
#include "QISplitter.h"
#include "QIFileDialog.h"
#include "UIBar.h"
#include "UINetworkManager.h"
#include "UINetworkManagerIndicator.h"
#include "UIUpdateManager.h"
#include "UIDownloaderUserManual.h"
#include "UIDownloaderExtensionPack.h"
#include "UIIconPool.h"
#include "UIWizardNewVM.h"
#include "UIWizardCloneVM.h"
#include "UIWizardExportApp.h"
#include "UIWizardImportApp.h"
#include "UIVMDesktop.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIMediumManager.h"
#include "UIMessageCenter.h"
#include "UISelectorWindow.h"
#include "UISettingsDialogSpecific.h"
#include "UIToolBar.h"
#include "UIVMLogViewer.h"
#include "UISelectorShortcuts.h"
#include "UIDesktopServices.h"
#include "UIGlobalSettingsExtension.h"
#include "UIActionPoolSelector.h"
#include "UIGChooser.h"
#include "UIGDetails.h"
#include "UIVMItem.h"
#include "VBoxGlobal.h"

#ifdef Q_WS_MAC
# include "VBoxUtils.h"
# include "UIWindowMenuManager.h"
# include "UIImageTools.h"
#endif /* Q_WS_MAC */

/* Other VBox stuff: */
#include <iprt/buildconfig.h>
#include <VBox/version.h>
#ifdef Q_WS_X11
# include <iprt/env.h>
#endif /* Q_WS_X11 */

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UISelectorWindow::UISelectorWindow(UISelectorWindow **ppSelf, QWidget *pParent,
                                   Qt::WindowFlags flags /* = Qt::Window */)
    : QIWithRetranslateUI2<QMainWindow>(pParent, flags)
    , m_pSplitter(0)
#ifndef Q_WS_MAC
    , m_pBar(0)
#endif /* !Q_WS_MAC */
    , mVMToolBar(0)
    , m_pContainer(0)
    , m_pChooser(0)
    , m_pDetails(0)
    , m_pVMDesktop(0)
    , m_fDoneInaccessibleWarningOnce(false)
{
    /* Remember self: */
    if (ppSelf)
        *ppSelf = this;

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
    mVMToolBar->setMacToolbar();
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

    /* Save settings: */
    saveSettings();
}

void UISelectorWindow::sltDetailsViewIndexChanged(int iWidgetIndex)
{
    if (iWidgetIndex)
        m_pContainer->setCurrentWidget(m_pVMDesktop);
    else
        m_pContainer->setCurrentWidget(m_pDetails);
}

void UISelectorWindow::sltMediumEnumFinished(const VBoxMediaList &list)
{
    /* We warn about inaccessible media only once
     * (after media emumeration started from main() at startup),
     * to avoid annoying the user: */
    if (m_fDoneInaccessibleWarningOnce)
        return;
    m_fDoneInaccessibleWarningOnce = true;

    /* Ignore the signal if a modal widget is currently active
     * (we won't be able to properly show the modeless VDI manager window in this case): */
    // TODO: Not sure that is required at all...
    if (QApplication::activeModalWidget())
        return;

    /* Ignore the signal if a UIMediumManager window is active: */
    // TODO: Thats a very dirty way, rework required!
    if (qApp->activeWindow() &&
        !strcmp(qApp->activeWindow()->metaObject()->className(), "UIMediumManager"))
        return;

    /* Look for at least one inaccessible media: */
    VBoxMediaList::const_iterator it;
    for (it = list.begin(); it != list.end(); ++it)
        if ((*it).state() == KMediumState_Inaccessible)
            break;
    /* Ask the user about: */
    if (it != list.end() && msgCenter().remindAboutInaccessibleMedia())
    {
        /* Show the VMM dialog without refresh: */
        UIMediumManager::showModeless(this, false /* refresh? */);
    }
}

void UISelectorWindow::sltShowSelectorContextMenu(const QPoint &pos)
{
    /* Load toolbar/statusbar availability settings: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QString strToolbar = vbox.GetExtraData(GUI_Toolbar);
    QString strStatusbar = vbox.GetExtraData(GUI_Statusbar);
    bool fToolbar = strToolbar.isEmpty() || strToolbar == "true";
    bool fStatusbar = strStatusbar.isEmpty() || strStatusbar == "true";

    /* Populate toolbar/statusbar acctions: */
    QList<QAction*> actions;
    QAction *pShowToolBar = new QAction(tr("Show Toolbar"), 0);
    pShowToolBar->setCheckable(true);
    pShowToolBar->setChecked(fToolbar);
    actions << pShowToolBar;
    QAction *pShowStatusBar = new QAction(tr("Show Statusbar"), 0);
    pShowStatusBar->setCheckable(true);
    pShowStatusBar->setChecked(fStatusbar);
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
            vbox.SetExtraData(GUI_Toolbar, "true");
        }
        else
        {
#ifdef Q_WS_MAC
            mVMToolBar->hide();
#else /* Q_WS_MAC */
            m_pBar->hide();
#endif /* !Q_WS_MAC */
            vbox.SetExtraData(GUI_Toolbar, "false");
        }
    }
    else if (pResult == pShowStatusBar)
    {
        if (pResult->isChecked())
        {
            statusBar()->show();
            vbox.SetExtraData(GUI_Statusbar, "true");
        }
        else
        {
            statusBar()->hide();
            vbox.SetExtraData(GUI_Statusbar, "false");
        }
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
    UIWizardImportApp wizard(this, strTmpFile);
    if (strFileName.isEmpty() || wizard.isValid())
        wizard.exec();
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
    UIWizardExportApp wizard(this, names);
    wizard.exec();
}

void UISelectorWindow::sltShowPreferencesDialog()
{
    /* Check that we do NOT handling that already: */
    if (m_pPreferencesDialogAction->data().toBool())
        return;
    /* Remember that we handling that already: */
    m_pPreferencesDialogAction->setData(true);

    /* Create and execute global settings dialog: */
    UISettingsDialogGlobal dialog(this);
    dialog.execute();

    /* Remember that we do NOT handling that already: */
    m_pPreferencesDialogAction->setData(false);
}

void UISelectorWindow::sltPerformExit()
{
    close();
}

void UISelectorWindow::sltShowCreateMachineWizard()
{
    UIWizardNewVM wizard(this);
    wizard.exec();
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
    if (!vbox.isOk() || newMachine.isNull())
    {
        msgCenter().cannotOpenMachine(this, strTmpFile, vbox);
        return;
    }

    /* Make sure this machine was NOT registered already: */
    CMachine oldMachine = vbox.FindMachine(newMachine.GetId());
    if (!oldMachine.isNull())
    {
        msgCenter().cannotReregisterMachine(this, strTmpFile, oldMachine.GetName());
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
    if (m_pActionMachineSettingsDialog->data().toBool())
        return;
    /* Remember that we handling that already: */
    m_pActionMachineSettingsDialog->setData(true);

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

    /* Don't show the inaccessible warning if the user tries to open VM settings: */
    m_fDoneInaccessibleWarningOnce = true;

    /* Create and execute corresponding VM settings dialog: */
    UISettingsDialogMachine dialog(this,
                                   strId.isEmpty() ? currentItem()->id() : strId,
                                   strCategory, strControl);
    dialog.execute();

    /* Remember that we do NOT handling that already: */
    m_pActionMachineSettingsDialog->setData(false);
}

void UISelectorWindow::sltShowCloneMachineWizard()
{
    /* Get current item: */
    UIVMItem *pItem = currentItem();
    AssertMsgReturnVoid(pItem, ("Current item should be selected!\n"));

    /* Show Clone VM wizard: */
    UIWizardCloneVM wizard(this, pItem->machine());
    wizard.exec();
}

void UISelectorWindow::sltPerformStartOrShowAction()
{
    /* Check if the action is enabled: */
    if (!m_pActionMachineStartOrShow->isEnabled())
        return;

    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For every selected item: */
    for (int i = 0; i < items.size(); ++i)
    {
        /* Check if current item could be started/showed: */
        if (!isActionEnabled(UIActionIndexSelector_State_Machine_StartOrShow, items))
            continue;

        /* Get iterated VM: */
        CMachine machine = items[i]->machine();
        /* Launch/show iterated VM: */
        vboxGlobal().launchMachine(machine, qApp->keyboardModifiers() == Qt::ShiftModifier);
    }
}

void UISelectorWindow::sltPerformDiscardAction()
{
    /* Get current item: */
    UIVMItem *pItem = currentItem();
    AssertMsgReturnVoid(pItem, ("Current item should be selected!\n"));

    /* Confirm discarding current VM saved state: */
    if (!msgCenter().confirmDiscardSavedState(pItem->machine()))
        return;

    /* Open a session to modify VM: */
    CSession session = vboxGlobal().openSession(pItem->id());
    if (session.isNull())
    {
        msgCenter().cannotOpenSession(session);
        return;
    }

    /* Get session console: */
    CConsole console = session.GetConsole();
    console.DiscardSavedState(true /* delete file */);
    if (!console.isOk())
        msgCenter().cannotDiscardSavedState(console);

    /* Unlock machine finally: */
    session.UnlockMachine();
}

void UISelectorWindow::sltPerformPauseResumeAction(bool fPause)
{
    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For every selected item: */
    for (int i = 0; i < items.size(); ++i)
    {
        /* Get iterated item: */
        UIVMItem *pItem = items[i];
        /* Get item state: */
        KMachineState state = pItem->machineState();

        /* Check if current item could be paused/resumed: */
        if (!isActionEnabled(UIActionIndexSelector_Toggle_Machine_PauseAndResume, items))
            continue;

        /* Check if current item already paused: */
        if (fPause &&
            (state == KMachineState_Paused || state == KMachineState_TeleportingPausedVM))
            continue;

        /* Check if current item already resumed: */
        if (!fPause &&
            (state == KMachineState_Running || state == KMachineState_Teleporting || state == KMachineState_LiveSnapshotting))
            continue;

        /* Open a session to modify VM state: */
        CSession session = vboxGlobal().openExistingSession(pItem->id());
        if (session.isNull())
        {
            msgCenter().cannotOpenSession(session);
            return;
        }

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
    /* Confirm reseting VM: */
    if (!msgCenter().confirmVMReset(this))
        return;

    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    for (int i = 0; i < items.size(); ++i)
    {
        /* Get iterated item: */
        UIVMItem *pItem = items[i];

        /* Check if current item could be reseted: */
        if (!isActionEnabled(UIActionIndexSelector_Simple_Machine_Reset, items))
            continue;

        /* Open a session to modify VM state: */
        CSession session = vboxGlobal().openExistingSession(pItem->id());
        if (session.isNull())
        {
            msgCenter().cannotOpenSession(session);
            return;
        }

        /* Get session console: */
        CConsole console = session.GetConsole();
        /* Reset VM: */
        console.Reset();

        /* Unlock machine finally: */
        session.UnlockMachine();
    }
}

void UISelectorWindow::sltPerformACPIShutdownAction()
{
    /* Confirm ACPI shutdown current VM: */
    if (!msgCenter().confirmVMACPIShutdown(this))
        return;

    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    for (int i = 0; i < items.size(); ++i)
    {
        /* Get iterated item: */
        UIVMItem *pItem = items[i];

        /* Check if current item could be shutdowned: */
        if (!isActionEnabled(UIActionIndexSelector_Simple_Machine_Close_ACPIShutdown, items))
            continue;

        /* Open a session to modify VM state: */
        CSession session = vboxGlobal().openExistingSession(pItem->id());
        if (session.isNull())
        {
            msgCenter().cannotOpenSession(session);
            return;
        }

        /* Get session console: */
        CConsole console = session.GetConsole();
        /* ACPI Shutdown: */
        console.PowerButton();

        /* Unlock machine finally: */
        session.UnlockMachine();
    }
}

void UISelectorWindow::sltPerformPowerOffAction()
{
    /* Confirm Power Off current VM: */
    if (!msgCenter().confirmVMPowerOff(this))
        return;

    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    for (int i = 0; i < items.size(); ++i)
    {
        /* Get iterated item: */
        UIVMItem *pItem = items[i];

        /* Check if current item could be powered off: */
        if (!isActionEnabled(UIActionIndexSelector_Simple_Machine_Close_PowerOff, items))
            continue;

        /* Open a session to modify VM state: */
        CSession session = vboxGlobal().openExistingSession(pItem->id());
        if (session.isNull())
        {
            msgCenter().cannotOpenSession(session);
            return;
        }

        /* Get session console: */
        CConsole console = session.GetConsole();
        /* Power Off: */
        console.PowerDown();

        /* Unlock machine finally: */
        session.UnlockMachine();
    }
}

void UISelectorWindow::sltPerformRefreshAction()
{
    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    // TODO: Notify selector-model to refresh selected items!

    /* Refresh details: */
    sltCurrentVMItemChanged();
}

void UISelectorWindow::sltShowLogDialog()
{
    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    for (int i = 0; i < items.size(); ++i)
    {
        /* Get iterated item: */
        UIVMItem *pItem = items[i];

        /* Check if log could be show for the current item: */
        if (!isActionEnabled(UIActionIndex_Simple_LogDialog, items))
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
    for (int i = 0; i < items.size(); ++i)
    {
        /* Get iterated item: */
        UIVMItem *pItem = items[i];

        /* Check if that item could be shown in file-browser: */
        if (!isActionEnabled(UIActionIndexSelector_Simple_Machine_ShowInFileManager, items))
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
    for (int i = 0; i < items.size(); ++i)
    {
        /* Get iterated item: */
        UIVMItem *pItem = items[i];

        /* Check if shortcuts could be created for this item: */
        if (!isActionEnabled(UIActionIndexSelector_Simple_Machine_CreateShortcut, items))
            continue;

        /* Create shortcut for this VM: */
        const CMachine &machine = pItem->machine();
        UIDesktopServices::createMachineShortcut(machine.GetSettingsFilePath(),
                                                 QDesktopServices::storageLocation(QDesktopServices::DesktopLocation),
                                                 machine.GetName(), machine.GetId());
    }
}

void UISelectorWindow::sltMachineMenuAboutToShow()
{
    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    /* Enable/disable 'Machine/Close' menu according logic: */
    m_pMachineCloseMenuAction->setEnabled(isActionEnabled(UIActionIndexSelector_Menu_Machine_Close, items));
}

void UISelectorWindow::sltMachineCloseMenuAboutToShow()
{
    /* Get current item: */
    UIVMItem *pItem = currentItem();
    /* Get selected items: */
    QList<UIVMItem*> items = currentItems();
    AssertMsgReturnVoid(pItem, ("Current item should be selected!\n"));

    m_pACPIShutdownAction->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_Close_ACPIShutdown, items));
}

void UISelectorWindow::sltCurrentVMItemChanged(bool fRefreshDetails, bool fRefreshSnapshots, bool fRefreshDescription)
{
    /* Get current item: */
    UIVMItem *pItem = currentItem();
    QList<UIVMItem*> items = currentItems();

    /* Determine which menu to show: */
    m_pGroupMenuAction->setVisible(m_pChooser->singleGroupSelected());
    m_pMachineMenuAction->setVisible(!m_pChooser->singleGroupSelected());
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

    /* Enable/disable group actions: */
    m_pActionGroupRenameDialog->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Group_RenameDialog, items));
    m_pActionGroupRemoveDialog->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Group_RemoveDialog, items));
    m_pActionGroupStartOrShow->setEnabled(isActionEnabled(UIActionIndexSelector_State_Group_StartOrShow, items));
    m_pActionGroupPauseAndResume->setEnabled(isActionEnabled(UIActionIndexSelector_Toggle_Group_PauseAndResume, items));
    m_pActionGroupReset->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Group_Reset, items));
    m_pActionGroupRefresh->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Group_Refresh, items));
    m_pActionGroupLogDialog->setEnabled(isActionEnabled(UIActionIndex_Simple_LogDialog, items));
    m_pActionGroupShowInFileManager->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Group_ShowInFileManager, items));
    m_pActionGroupCreateShortcut->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Group_CreateShortcut, items));
//    m_pActionGroupSort->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Group_Sort, items));

    /* Enable/disable machine actions: */
    m_pActionMachineSettingsDialog->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_SettingsDialog, items));
    m_pActionMachineCloneWizard->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_CloneWizard, items));
    m_pActionMachineAddGroupDialog->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_AddGroupDialog, items));
    m_pActionMachineRemoveDialog->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_RemoveDialog, items));
    m_pActionMachineStartOrShow->setEnabled(isActionEnabled(UIActionIndexSelector_State_Machine_StartOrShow, items));
    m_pActionMachineDiscard->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_Discard, items));
    m_pActionMachinePauseAndResume->setEnabled(isActionEnabled(UIActionIndexSelector_Toggle_Machine_PauseAndResume, items));
    m_pActionMachineReset->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_Reset, items));
    m_pActionMachineRefresh->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_Refresh, items));
    m_pActionMachineLogDialog->setEnabled(isActionEnabled(UIActionIndex_Simple_LogDialog, items));
    m_pActionMachineShowInFileManager->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_ShowInFileManager, items));
    m_pActionMachineCreateShortcut->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_CreateShortcut, items));
//    m_pActionMachineSort->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_Sort, items));

    /* Enable/disable machine-close actions: */
    m_pACPIShutdownAction->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_Close_ACPIShutdown, items));
    m_pPowerOffAction->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_Close_PowerOff, items));

    /* If currently selected VM item is accessible: */
    if (pItem && pItem->accessible())
    {
        /* Make sure valid widget raised: */
        if (m_pVMDesktop->widgetIndex())
            m_pContainer->setCurrentWidget(m_pVMDesktop);
        else
            m_pContainer->setCurrentWidget(m_pDetails);

        CMachine m = pItem->machine();
        QList<CMachine> machines;
        for (int i = 0; i < items.size(); ++i)
            machines << items[i]->machine();
        KMachineState state = pItem->machineState();

        if (fRefreshDetails || fRefreshDescription)
            m_pDetails->setItems(currentItems());
        if (fRefreshSnapshots)
            m_pVMDesktop->updateSnapshots(pItem, m);

        /* Update the Start button action appearance: */
        if (state == KMachineState_PoweredOff ||
            state == KMachineState_Saved ||
            state == KMachineState_Teleported ||
            state == KMachineState_Aborted)
        {
            m_pActionGroupStartOrShow->setState(1);
            m_pActionMachineStartOrShow->setState(1);
#ifdef QT_MAC_USE_COCOA
            /* There is a bug in Qt Cocoa which result in showing a "more arrow" when
               the necessary size of the toolbar is increased. Also for some languages
               the with doesn't match if the text increase. So manually adjust the size
               after changing the text. */
            mVMToolBar->updateLayout();
#endif /* QT_MAC_USE_COCOA */
        }
        else
        {
            m_pActionGroupStartOrShow->setState(2);
            m_pActionMachineStartOrShow->setState(2);
#ifdef QT_MAC_USE_COCOA
            /* There is a bug in Qt Cocoa which result in showing a "more arrow" when
               the necessary size of the toolbar is increased. Also for some languages
               the with doesn't match if the text increase. So manually adjust the size
               after changing the text. */
            mVMToolBar->updateLayout();
#endif /* QT_MAC_USE_COCOA */
        }

        /* Update the Pause/Resume action appearance: */
        if (state == KMachineState_Paused ||
            state == KMachineState_TeleportingPausedVM)
        {
            m_pActionGroupPauseAndResume->blockSignals(true);
            m_pActionGroupPauseAndResume->setChecked(true);
            m_pActionGroupPauseAndResume->blockSignals(false);
            m_pActionMachinePauseAndResume->blockSignals(true);
            m_pActionMachinePauseAndResume->setChecked(true);
            m_pActionMachinePauseAndResume->blockSignals(false);
        }
        else
        {
            m_pActionGroupPauseAndResume->blockSignals(true);
            m_pActionGroupPauseAndResume->setChecked(false);
            m_pActionGroupPauseAndResume->blockSignals(false);
            m_pActionMachinePauseAndResume->blockSignals(true);
            m_pActionMachinePauseAndResume->setChecked(false);
            m_pActionMachinePauseAndResume->blockSignals(false);
        }
        m_pActionGroupPauseAndResume->updateAppearance();
        m_pActionMachinePauseAndResume->updateAppearance();
    }

    /* If currently selected VM item is NOT accessible: */
    else
    {
        /* Show m_pVMDesktop: */
        m_pContainer->setCurrentWidget(m_pVMDesktop);

        /* Note that the machine becomes inaccessible (or if the last VM gets
         * deleted), we have to update all fields, ignoring input arguments. */
        if (pItem)
        {
            /* The VM is inaccessible: */
            m_pVMDesktop->updateDetailsErrorText(UIMessageCenter::formatErrorInfo(pItem->accessError()));
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

        /* Change the Start button text accordingly: */
        m_pActionGroupStartOrShow->setState(1);
        m_pActionMachineStartOrShow->setState(1);
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
             +  QString(" - "VBOX_BLEEDING_EDGE);
#endif /* VBOX_BLEEDING_EDGE */
    setWindowTitle(strTitle);

    /* Ensure the details and screenshot view are updated: */
    sltCurrentVMItemChanged();

#ifdef VBOX_GUI_WITH_SYSTRAY
    if (vboxGlobal().isTrayMenu())
    {
        m_pTrayIcon->retranslateUi();
        m_pTrayIcon->refresh();
    }
#endif /* VBOX_GUI_WITH_SYSTRAY */

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
        /* By handling every Resize and Move we keep track of the normal
         * (non-minimized and non-maximized) window geometry. Shame on Qt
         * that it doesn't provide this geometry in its public APIs. */
        case QEvent::Resize:
        {
            QResizeEvent *pResizeEvent = (QResizeEvent*) pEvent;
            if ((windowState() & (Qt::WindowMaximized | Qt::WindowMinimized | Qt::WindowFullScreen)) == 0)
                m_normalGeo.setSize(pResizeEvent->size());
            break;
        }
        case QEvent::Move:
        {
            if ((windowState() & (Qt::WindowMaximized | Qt::WindowMinimized | Qt::WindowFullScreen)) == 0)
                m_normalGeo.moveTo(geometry().x(), geometry().y());
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
        case QEvent::ToolBarChange:
        {
            CVirtualBox vbox = vboxGlobal().virtualBox();
            /* We have to invert the isVisible check one time, cause this event
             * is sent *before* the real toggle is done. Really intuitive Trolls. */
            vbox.SetExtraData(GUI_Toolbar, !::darwinIsToolbarVisible(mVMToolBar) ? "true" : "false");
            break;
        }
#endif /* Q_WS_MAC */
        default:
            break;
    }
    /* Call to base-class: */
    return QMainWindow::event(pEvent);
}

void UISelectorWindow::closeEvent(QCloseEvent *pEvent)
{
    // TODO: Such things are obsolete, rework required!
    emit closing();
    QMainWindow::closeEvent(pEvent);
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
# if (QT_VERSION < 0x040402)
        case QEvent::KeyPress:
        {
            /* Bug in Qt below 4.4.2. The key events are send to the current
             * window even if a menu is shown & has the focus. See
             * http://trolltech.com/developer/task-tracker/index_html?method=entry&id=214681. */
            if (::darwinIsMenuOpen())
                return true;
            break;
        }
# endif
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
    /* On Win32, it's built-in to the executable.
     * On Mac OS X the icon referenced in info.plist is used. */
    setWindowIcon(QIcon(":/VirtualBox_48px.png"));
#endif
}

void UISelectorWindow::prepareMenuBar()
{
    /* Prepare File-menu: */
    m_pFileMenu = gActionPool->action(UIActionIndexSelector_Menu_File)->menu();
    prepareMenuFile(m_pFileMenu);
    menuBar()->addMenu(m_pFileMenu);

    /* Prepare 'Close' menu: */
    m_pMachineCloseMenuAction = gActionPool->action(UIActionIndexSelector_Menu_Machine_Close);
    m_pMachineCloseMenu = m_pMachineCloseMenuAction->menu();
    prepareMenuMachineClose(m_pMachineCloseMenu);

    /* Prepare Group-menu: */
    m_pGroupMenu = gActionPool->action(UIActionIndexSelector_Menu_Group)->menu();
    prepareMenuGroup(m_pGroupMenu);
    m_pGroupMenuAction = menuBar()->addMenu(m_pGroupMenu);

    /* Prepare Machine-menu: */
    m_pMachineMenu = gActionPool->action(UIActionIndexSelector_Menu_Machine)->menu();
    prepareMenuMachine(m_pMachineMenu);
    m_pMachineMenuAction = menuBar()->addMenu(m_pMachineMenu);

#ifdef Q_WS_MAC
    menuBar()->addMenu(UIWindowMenuManager::instance(this)->createMenu(this));
#endif /* Q_WS_MAC */

    /* Prepare Help-menu: */
    m_pHelpMenu = gActionPool->action(UIActionIndex_Menu_Help)->menu();
    prepareMenuHelp(m_pHelpMenu);
    menuBar()->addMenu(m_pHelpMenu);

    /* Setup menubar policy: */
    menuBar()->setContextMenuPolicy(Qt::CustomContextMenu);
}

void UISelectorWindow::prepareMenuFile(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

    /* Populate File-menu: */
    m_pMediumManagerDialogAction = gActionPool->action(UIActionIndexSelector_Simple_File_MediumManagerDialog);
    pMenu->addAction(m_pMediumManagerDialogAction);
    m_pImportApplianceWizardAction = gActionPool->action(UIActionIndexSelector_Simple_File_ImportApplianceWizard);
    pMenu->addAction(m_pImportApplianceWizardAction);
    m_pExportApplianceWizardAction = gActionPool->action(UIActionIndexSelector_Simple_File_ExportApplianceWizard);
    pMenu->addAction(m_pExportApplianceWizardAction);
#ifndef Q_WS_MAC
    pMenu->addSeparator();
#endif /* Q_WS_MAC */
    m_pPreferencesDialogAction = gActionPool->action(UIActionIndexSelector_Simple_File_PreferencesDialog);
    pMenu->addAction(m_pPreferencesDialogAction);
#ifndef Q_WS_MAC
    pMenu->addSeparator();
#endif /* Q_WS_MAC */
    m_pExitAction = gActionPool->action(UIActionIndexSelector_Simple_File_Exit);
    pMenu->addAction(m_pExitAction);
}

void UISelectorWindow::prepareMenuGroup(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

    /* Populate Machine-menu: */
    m_pActionGroupNewWizard = gActionPool->action(UIActionIndexSelector_Simple_Group_NewWizard);
    pMenu->addAction(m_pActionGroupNewWizard);
    m_pActionGroupAddDialog = gActionPool->action(UIActionIndexSelector_Simple_Group_AddDialog);
    pMenu->addAction(m_pActionGroupAddDialog);
    m_pActionGroupRenameDialog = gActionPool->action(UIActionIndexSelector_Simple_Group_RenameDialog);
    pMenu->addAction(m_pActionGroupRenameDialog);
    m_pActionGroupRemoveDialog = gActionPool->action(UIActionIndexSelector_Simple_Group_RemoveDialog);
    pMenu->addAction(m_pActionGroupRemoveDialog);
    pMenu->addSeparator();
    m_pActionGroupStartOrShow = gActionPool->action(UIActionIndexSelector_State_Group_StartOrShow);
    pMenu->addAction(m_pActionGroupStartOrShow);
    m_pActionGroupPauseAndResume = gActionPool->action(UIActionIndexSelector_Toggle_Group_PauseAndResume);
    pMenu->addAction(m_pActionGroupPauseAndResume);
    m_pActionGroupReset = gActionPool->action(UIActionIndexSelector_Simple_Group_Reset);
    pMenu->addAction(m_pActionGroupReset);
    pMenu->addMenu(m_pMachineCloseMenu);
    pMenu->addSeparator();
    m_pActionGroupRefresh = gActionPool->action(UIActionIndexSelector_Simple_Group_Refresh);
    pMenu->addAction(m_pActionGroupRefresh);
    m_pActionGroupLogDialog = gActionPool->action(UIActionIndex_Simple_LogDialog);
    pMenu->addAction(m_pActionGroupLogDialog);
    pMenu->addSeparator();
    m_pActionGroupShowInFileManager = gActionPool->action(UIActionIndexSelector_Simple_Group_ShowInFileManager);
    pMenu->addAction(m_pActionGroupShowInFileManager);
    m_pActionGroupCreateShortcut = gActionPool->action(UIActionIndexSelector_Simple_Group_CreateShortcut);
    pMenu->addAction(m_pActionGroupCreateShortcut);
    pMenu->addSeparator();
//    m_pActionGroupSort = gActionPool->action(UIActionIndexSelector_Simple_Group_Sort);
//    pMenu->addAction(m_pActionGroupSort);

    /* Remember action list: */
    m_groupActions << m_pActionGroupNewWizard << m_pActionGroupAddDialog
                   << m_pActionGroupRenameDialog << m_pActionGroupRemoveDialog
                   << m_pActionGroupStartOrShow << m_pActionGroupPauseAndResume
                   << m_pActionGroupReset << m_pActionGroupRefresh
                   << m_pActionGroupLogDialog << m_pActionGroupShowInFileManager
                   << m_pActionGroupCreateShortcut /* << m_pActionGroupSort */;
}

void UISelectorWindow::prepareMenuMachine(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

    /* Populate Machine-menu: */
    m_pActionMachineNewWizard = gActionPool->action(UIActionIndexSelector_Simple_Machine_NewWizard);
    pMenu->addAction(m_pActionMachineNewWizard);
    m_pActionMachineAddDialog = gActionPool->action(UIActionIndexSelector_Simple_Machine_AddDialog);
    pMenu->addAction(m_pActionMachineAddDialog);
    m_pActionMachineSettingsDialog = gActionPool->action(UIActionIndexSelector_Simple_Machine_SettingsDialog);
    pMenu->addAction(m_pActionMachineSettingsDialog);
    m_pActionMachineCloneWizard = gActionPool->action(UIActionIndexSelector_Simple_Machine_CloneWizard);
    pMenu->addAction(m_pActionMachineCloneWizard);
    m_pActionMachineAddGroupDialog = gActionPool->action(UIActionIndexSelector_Simple_Machine_AddGroupDialog);
    pMenu->addAction(m_pActionMachineAddGroupDialog);
    m_pActionMachineRemoveDialog = gActionPool->action(UIActionIndexSelector_Simple_Machine_RemoveDialog);
    pMenu->addAction(m_pActionMachineRemoveDialog);
    pMenu->addSeparator();
    m_pActionMachineStartOrShow = gActionPool->action(UIActionIndexSelector_State_Machine_StartOrShow);
    pMenu->addAction(m_pActionMachineStartOrShow);
    m_pActionMachineDiscard = gActionPool->action(UIActionIndexSelector_Simple_Machine_Discard);
    pMenu->addAction(m_pActionMachineDiscard);
    m_pActionMachinePauseAndResume = gActionPool->action(UIActionIndexSelector_Toggle_Machine_PauseAndResume);
    pMenu->addAction(m_pActionMachinePauseAndResume);
    m_pActionMachineReset = gActionPool->action(UIActionIndexSelector_Simple_Machine_Reset);
    pMenu->addAction(m_pActionMachineReset);
    pMenu->addMenu(m_pMachineCloseMenu);
    pMenu->addSeparator();
    m_pActionMachineRefresh = gActionPool->action(UIActionIndexSelector_Simple_Machine_Refresh);
    pMenu->addAction(m_pActionMachineRefresh);
    m_pActionMachineLogDialog = gActionPool->action(UIActionIndex_Simple_LogDialog);
    pMenu->addAction(m_pActionMachineLogDialog);
    pMenu->addSeparator();
    m_pActionMachineShowInFileManager = gActionPool->action(UIActionIndexSelector_Simple_Machine_ShowInFileManager);
    pMenu->addAction(m_pActionMachineShowInFileManager);
    m_pActionMachineCreateShortcut = gActionPool->action(UIActionIndexSelector_Simple_Machine_CreateShortcut);
    pMenu->addAction(m_pActionMachineCreateShortcut);
    pMenu->addSeparator();
//    m_pActionMachineSort = gActionPool->action(UIActionIndexSelector_Simple_Machine_Sort);
//    pMenu->addAction(m_pActionMachineSort);

    /* Remember action list: */
    m_machineActions << m_pActionMachineNewWizard << m_pActionMachineAddDialog
                     << m_pActionMachineSettingsDialog << m_pActionMachineCloneWizard
                     << m_pActionMachineAddGroupDialog
                     << m_pActionMachineRemoveDialog << m_pActionMachineStartOrShow
                     << m_pActionMachineDiscard << m_pActionMachinePauseAndResume
                     << m_pActionMachineReset << m_pActionMachineRefresh
                     << m_pActionMachineLogDialog << m_pActionMachineShowInFileManager
                     << m_pActionMachineCreateShortcut /* << m_pActionMachineSort */;
}

void UISelectorWindow::prepareMenuMachineClose(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

    /* Populate Machine/Close-menu: */
    m_pACPIShutdownAction = gActionPool->action(UIActionIndexSelector_Simple_Machine_Close_ACPIShutdown);
    pMenu->addAction(m_pACPIShutdownAction);
    m_pPowerOffAction = gActionPool->action(UIActionIndexSelector_Simple_Machine_Close_PowerOff);
    pMenu->addAction(m_pPowerOffAction);
}

void UISelectorWindow::prepareMenuHelp(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

    /* Populate Help-menu: */
    m_pHelpAction = gActionPool->action(UIActionIndex_Simple_Contents);
    pMenu->addAction(m_pHelpAction);
    m_pWebAction = gActionPool->action(UIActionIndex_Simple_WebSite);
    pMenu->addAction(m_pWebAction);
    pMenu->addSeparator();
    m_pResetWarningsAction = gActionPool->action(UIActionIndex_Simple_ResetWarnings);
    pMenu->addAction(m_pResetWarningsAction);
    pMenu->addSeparator();
    m_pNetworkAccessManager = gActionPool->action(UIActionIndex_Simple_NetworkAccessManager);
    pMenu->addAction(m_pNetworkAccessManager);
#ifdef VBOX_WITH_REGISTRATION
    m_pRegisterAction = gActionPool->action(UIActionIndex_Simple_Register);
    pMenu->addAction(m_pRegisterAction);
#endif /* VBOX_WITH_REGISTRATION */
    m_pUpdateAction = gActionPool->action(UIActionIndex_Simple_CheckForUpdates);
    pMenu->addAction(m_pUpdateAction);
#ifndef Q_WS_MAC
    pMenu->addSeparator();
#endif /* !Q_WS_MAC */
    m_pAboutAction = gActionPool->action(UIActionIndex_Simple_About);
    pMenu->addAction(m_pAboutAction);
}

void UISelectorWindow::prepareStatusBar()
{
    /* Setup statusbar policy: */
    statusBar()->setContextMenuPolicy(Qt::CustomContextMenu);

    /* Add network-manager indicator: */
    QIStateIndicator *pIndicator = gNetworkManager->indicator();
    statusBar()->addPermanentWidget(pIndicator);
    pIndicator->updateAppearance();
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
    mVMToolBar->addAction(m_pActionMachineNewWizard);
    mVMToolBar->addAction(m_pActionMachineSettingsDialog);
    mVMToolBar->addAction(m_pActionMachineStartOrShow);
    mVMToolBar->addAction(m_pActionMachineDiscard);

    /* Prepare graphics VM list: */
    m_pChooser = new UIGChooser(this);
    m_pChooser->setStatusBar(statusBar());

    /* Prepare graphics details: */
    m_pDetails = new UIGDetails(this);

    /* Prepare details and snapshots tabs: */
    m_pVMDesktop = new UIVMDesktop(mVMToolBar, m_pActionMachineRefresh, this);

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
    connect(&vboxGlobal(), SIGNAL(mediumEnumFinished(const VBoxMediaList &)), this, SLOT(sltMediumEnumFinished(const VBoxMediaList &)));

    /* Menu-bar connections: */
    connect(menuBar(), SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(sltShowSelectorContextMenu(const QPoint&)));

    /* 'File' menu connections: */
    connect(m_pMediumManagerDialogAction, SIGNAL(triggered()), this, SLOT(sltShowMediumManager()));
    connect(m_pImportApplianceWizardAction, SIGNAL(triggered()), this, SLOT(sltShowImportApplianceWizard()));
    connect(m_pExportApplianceWizardAction, SIGNAL(triggered()), this, SLOT(sltShowExportApplianceWizard()));
    connect(m_pPreferencesDialogAction, SIGNAL(triggered()), this, SLOT(sltShowPreferencesDialog()));
    connect(m_pExitAction, SIGNAL(triggered()), this, SLOT(sltPerformExit()));

    /* 'Group' menu connections: */
    connect(m_pGroupMenu, SIGNAL(aboutToShow()), this, SLOT(sltMachineMenuAboutToShow()));
    connect(m_pActionGroupNewWizard, SIGNAL(triggered()), this, SLOT(sltShowCreateMachineWizard()));
    connect(m_pActionGroupAddDialog, SIGNAL(triggered()), this, SLOT(sltShowAddMachineDialog()));
    connect(m_pActionGroupStartOrShow, SIGNAL(triggered()), this, SLOT(sltPerformStartOrShowAction()));
    connect(m_pActionGroupPauseAndResume, SIGNAL(toggled(bool)), this, SLOT(sltPerformPauseResumeAction(bool)));
    connect(m_pActionGroupReset, SIGNAL(triggered()), this, SLOT(sltPerformResetAction()));
    connect(m_pActionGroupRefresh, SIGNAL(triggered()), this, SLOT(sltPerformRefreshAction()));
    connect(m_pActionGroupLogDialog, SIGNAL(triggered()), this, SLOT(sltShowLogDialog()));
    connect(m_pActionGroupShowInFileManager, SIGNAL(triggered()), this, SLOT(sltShowMachineInFileManager()));
    connect(m_pActionGroupCreateShortcut, SIGNAL(triggered()), this, SLOT(sltPerformCreateShortcutAction()));

    /* 'Machine' menu connections: */
    connect(m_pMachineMenu, SIGNAL(aboutToShow()), this, SLOT(sltMachineMenuAboutToShow()));
    connect(m_pActionMachineNewWizard, SIGNAL(triggered()), this, SLOT(sltShowCreateMachineWizard()));
    connect(m_pActionMachineAddDialog, SIGNAL(triggered()), this, SLOT(sltShowAddMachineDialog()));
    connect(m_pActionMachineSettingsDialog, SIGNAL(triggered()), this, SLOT(sltShowMachineSettingsDialog()));
    connect(m_pActionMachineCloneWizard, SIGNAL(triggered()), this, SLOT(sltShowCloneMachineWizard()));
    connect(m_pActionMachineStartOrShow, SIGNAL(triggered()), this, SLOT(sltPerformStartOrShowAction()));
    connect(m_pActionMachineDiscard, SIGNAL(triggered()), this, SLOT(sltPerformDiscardAction()));
    connect(m_pActionMachinePauseAndResume, SIGNAL(toggled(bool)), this, SLOT(sltPerformPauseResumeAction(bool)));
    connect(m_pActionMachineReset, SIGNAL(triggered()), this, SLOT(sltPerformResetAction()));
    connect(m_pActionMachineRefresh, SIGNAL(triggered()), this, SLOT(sltPerformRefreshAction()));
    connect(m_pActionMachineLogDialog, SIGNAL(triggered()), this, SLOT(sltShowLogDialog()));
    connect(m_pActionMachineShowInFileManager, SIGNAL(triggered()), this, SLOT(sltShowMachineInFileManager()));
    connect(m_pActionMachineCreateShortcut, SIGNAL(triggered()), this, SLOT(sltPerformCreateShortcutAction()));

    /* 'Machine/Close' menu connections: */
    connect(m_pMachineCloseMenu, SIGNAL(aboutToShow()), this, SLOT(sltMachineCloseMenuAboutToShow()));
    connect(m_pACPIShutdownAction, SIGNAL(triggered()), this, SLOT(sltPerformACPIShutdownAction()));
    connect(m_pPowerOffAction, SIGNAL(triggered()), this, SLOT(sltPerformPowerOffAction()));

    /* 'Help' menu connections: */
    connect(m_pHelpAction, SIGNAL(triggered()), &msgCenter(), SLOT(sltShowHelpHelpDialog()));
    connect(m_pWebAction, SIGNAL(triggered()), &msgCenter(), SLOT(sltShowHelpWebDialog()));
    connect(m_pResetWarningsAction, SIGNAL(triggered()), &msgCenter(), SLOT(sltResetSuppressedMessages()));
    connect(m_pNetworkAccessManager, SIGNAL(triggered()), gNetworkManager, SLOT(show()));
#ifdef VBOX_WITH_REGISTRATION
    connect(m_pRegisterAction, SIGNAL(triggered()), &vboxGlobal(), SLOT(showRegistrationDialog()));
    connect(gEDataEvents, SIGNAL(sigCanShowRegistrationDlg(bool)), m_pRegisterAction, SLOT(setEnabled(bool)));
#endif /* VBOX_WITH_REGISTRATION */
    connect(m_pUpdateAction, SIGNAL(triggered()), gUpdateManager, SLOT(sltForceCheck()));
    connect(m_pAboutAction, SIGNAL(triggered()), &msgCenter(), SLOT(sltShowHelpAboutDialog()));

    /* Status-bar connections: */
    connect(statusBar(), SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(sltShowSelectorContextMenu(const QPoint&)));

    /* Graphics VM chooser connections: */
    connect(m_pChooser, SIGNAL(sigSelectionChanged()), this, SLOT(sltCurrentVMItemChanged()), Qt::QueuedConnection);

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

#ifdef VBOX_GUI_WITH_SYSTRAY
    /* Tray icon connections: */
    connect(m_pTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(sltTrayIconActivated(QSystemTrayIcon::ActivationReason)));
    connect(gEDataEvents, SIGNAL(sigMainWindowCountChange(int)), this, SLOT(sltMainWindowCountChanged(int)));
    connect(gEDataEvents, SIGNAL(sigCanShowTrayIcon(bool)), this, SLOT(sltTrayIconCanShow(bool)));
    connect(gEDataEvents, SIGNAL(sigTrayIconChange(bool)), this, SLOT(sltTrayIconChanged(bool)));
    connect(&vboxGlobal(), SIGNAL(sigTrayIconShow(bool)), this, SLOT(sltTrayIconShow(bool)));
#endif /* VBOX_GUI_WITH_SYSTRAY */
}

void UISelectorWindow::loadSettings()
{
    /* Get VBox object: */
    CVirtualBox vbox = vboxGlobal().virtualBox();

    /* Restore window position: */
    {
        QString strWinPos = vbox.GetExtraData(GUI_LastSelectorWindowPosition);

        bool ok = false, max = false;
        int x = 0, y = 0, w = 0, h = 0;
        x = strWinPos.section(',', 0, 0).toInt(&ok);
        if (ok)
            y = strWinPos.section(',', 1, 1).toInt(&ok);
        if (ok)
            w = strWinPos.section(',', 2, 2).toInt(&ok);
        if (ok)
            h = strWinPos.section(',', 3, 3).toInt(&ok);
        if (ok)
            max = strWinPos.section(',', 4, 4) == GUI_LastWindowState_Max;

        QRect ar = ok ? QApplication::desktop()->availableGeometry(QPoint(x, y)) :
                        QApplication::desktop()->availableGeometry(this);

        if (ok /* previous parameters were read correctly */
            && (y > 0) && (y < ar.bottom()) /* check vertical bounds */
            && (x + w > ar.left()) && (x < ar.right()) /* & horizontal bounds */)
        {
            m_normalGeo.moveTo(x, y);
            m_normalGeo.setSize(QSize(w, h).expandedTo(minimumSizeHint()).boundedTo(ar.size()));
#if defined(Q_WS_MAC) && (QT_VERSION >= 0x040700)
            move(m_normalGeo.topLeft());
            resize(m_normalGeo.size());
            m_normalGeo = normalGeometry();
#else /* defined(Q_WS_MAC) && (QT_VERSION >= 0x040700) */
            setGeometry(m_normalGeo);
#endif /* !(defined(Q_WS_MAC) && (QT_VERSION >= 0x040700)) */
            if (max) /* maximize if needed */
                showMaximized();
        }
        else
        {
            m_normalGeo.setSize(QSize(770, 550).expandedTo(minimumSizeHint()).boundedTo(ar.size()));
            m_normalGeo.moveCenter(ar.center());
            setGeometry(m_normalGeo);
        }
    }

    /* Restore splitter handle position: */
    {
        QList<int> sizes = vbox.GetExtraDataIntList(GUI_SplitterSizes);

        if (sizes.size() == 2)
            m_pSplitter->setSizes(sizes);
    }

    /* Restore toolbar and statusbar visibility: */
    {
        QString strToolbar = vbox.GetExtraData(GUI_Toolbar);
        QString strStatusbar = vbox.GetExtraData(GUI_Statusbar);

#ifdef Q_WS_MAC
        mVMToolBar->setVisible(strToolbar.isEmpty() || strToolbar == "true");
#else /* Q_WS_MAC */
        m_pBar->setVisible(strToolbar.isEmpty() || strToolbar == "true");
#endif /* !Q_WS_MAC */
        statusBar()->setVisible(strStatusbar.isEmpty() || strStatusbar == "true");
    }
}

void UISelectorWindow::saveSettings()
{
    /* Get VBox object: */
    CVirtualBox vbox = vboxGlobal().virtualBox();

    /* Save window position: */
    {
#if defined(Q_WS_MAC) && (QT_VERSION >= 0x040700)
        QRect frameGeo = frameGeometry();
        QRect save(frameGeo.x(), frameGeo.y(), m_normalGeo.width(), m_normalGeo.height());
#else /* defined(Q_WS_MAC) && (QT_VERSION >= 0x040700) */
        QRect save(m_normalGeo);
#endif /* !(defined(Q_WS_MAC) && (QT_VERSION >= 0x040700)) */
        QString strWinPos = QString("%1,%2,%3,%4").arg(save.x()).arg(save.y()).arg(save.width()).arg(save.height());
#ifdef Q_WS_MAC
        UIWindowMenuManager::destroy();
        ::darwinUnregisterForUnifiedToolbarContextMenuEvents(this);
        if (::darwinIsWindowMaximized(this))
#else /* Q_WS_MAC */
        if (isMaximized())
#endif /* !Q_WS_MAC */
            strWinPos += QString(",%1").arg(GUI_LastWindowState_Max);

        vbox.SetExtraData(GUI_LastSelectorWindowPosition, strWinPos);
    }

    /* Save splitter handle position: */
    {
        vbox.SetExtraDataIntList(GUI_SplitterSizes, m_pSplitter->sizes());
    }
}

UIVMItem* UISelectorWindow::currentItem() const
{
    return m_pChooser->currentItem();
}

QList<UIVMItem*> UISelectorWindow::currentItems() const
{
    return m_pChooser->currentItems();
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
        case UIActionIndexSelector_Simple_Machine_SettingsDialog:
        {
            /* Check that there is only one item, its accessible
             * and machine is not in 'stuck' state. */
            return items.size() == 1 &&
                   pItem->accessible() &&
                   pItem->machineState() != KMachineState_Stuck;
        }
        case UIActionIndexSelector_Simple_Machine_CloneWizard:
        {
            /* Check that there is only one item, its accessible
             * and session state is unlocked. */
            return items.size() == 1 &&
                   pItem->accessible() &&
                   pItem->sessionState() == KSessionState_Unlocked;
        }
        case UIActionIndexSelector_Simple_Machine_AddGroupDialog:
        {
            /* Check that there is more than one item,
             * all items are accessible and in 'powered off' states: */
            return items.size() > 1 &&
                   isItemsAccessible(items) &&
                   isItemsPoweredOff(items);
        }
        case UIActionIndexSelector_Simple_Group_RenameDialog:
        case UIActionIndexSelector_Simple_Group_RemoveDialog:
        {
            /* Group can be always removed/renamed: */
            return true;
        }
        case UIActionIndexSelector_Simple_Machine_RemoveDialog:
        {
            /* Check that all machines are NOT accessible
             * or session states of all machines are unlocked. */
            return isItemsInaccessible(items) || isItemsHasUnlockedSession(items);
        }
        case UIActionIndexSelector_State_Group_StartOrShow:
        case UIActionIndexSelector_State_Machine_StartOrShow:
        {
            /* Make sure all items are accessible: */
            if (!isItemsAccessible(items))
                return false;

            /* If machine is in one of 'powered-off' states:*/
            if (pItem->machineState() == KMachineState_PoweredOff ||
                pItem->machineState() == KMachineState_Saved ||
                pItem->machineState() == KMachineState_Teleported ||
                pItem->machineState() == KMachineState_Aborted)
            {
                /* It depends on session state: */
                return pItem->sessionState() == KSessionState_Unlocked;
            }

            /* Otherwise we are in running mode and
             * should allow to switch to VM if its possible: */
            return pItem->canSwitchTo();
        }
        case UIActionIndexSelector_Simple_Machine_Discard:
        {
            /* Check that there is only one item, its accessible
             * and machine is in 'saved' state and session state is unlocked. */
            return items.size() == 1 &&
                   pItem->accessible() &&
                   pItem->machineState() == KMachineState_Saved &&
                   pItem->sessionState() == KSessionState_Unlocked;
        }
        case UIActionIndexSelector_Toggle_Group_PauseAndResume:
        case UIActionIndexSelector_Toggle_Machine_PauseAndResume:
        {
            /* Make sure all items are accessible: */
            if (!isItemsAccessible(items))
                return false;

            /* True if machine is in one of next 'running' or 'paused' states: */
            return pItem->machineState() == KMachineState_Running ||
                   pItem->machineState() == KMachineState_Teleporting ||
                   pItem->machineState() == KMachineState_LiveSnapshotting ||
                   pItem->machineState() == KMachineState_Paused ||
                   pItem->machineState() == KMachineState_TeleportingPausedVM;
        }
        case UIActionIndexSelector_Simple_Group_Reset:
        case UIActionIndexSelector_Simple_Machine_Reset:
        {
            /* Make sure all items are accessible: */
            if (!isItemsAccessible(items))
                return false;

            /* True if machine is in one of 'running' states: */
            return pItem->machineState() == KMachineState_Running ||
                   pItem->machineState() == KMachineState_Teleporting ||
                   pItem->machineState() == KMachineState_LiveSnapshotting;
        }
        case UIActionIndexSelector_Menu_Machine_Close:
        {
            /* Make sure all items are accessible: */
            if (!isItemsAccessible(items))
                return false;

            /* True if machine is in one of next 'running' or 'paused' states: */
            return pItem->machineState() == KMachineState_Running ||
                   pItem->machineState() == KMachineState_Paused;
        }
        case UIActionIndexSelector_Simple_Machine_Close_ACPIShutdown:
        {
            /* Check that 'Machine/Close' menu is enabled: */
            if (!isActionEnabled(UIActionIndexSelector_Menu_Machine_Close, items))
                return false;

            /* Check if we are entered ACPI mode already.
             * Only then it make sense to send the ACPI shutdown sequence: */
            bool fEnteredACPIMode = false;
            CSession session = vboxGlobal().openExistingSession(pItem->id());
            if (!session.isNull())
            {
                CConsole console = session.GetConsole();
                if (!console.isNull())
                    fEnteredACPIMode = console.GetGuestEnteredACPIMode();
                session.UnlockMachine();
            }
            else
                msgCenter().cannotOpenSession(session);
            return fEnteredACPIMode;
        }
        case UIActionIndexSelector_Simple_Machine_Close_PowerOff:
        {
            /* The same as 'Machine/Close' menu is enabled: */
            return isActionEnabled(UIActionIndexSelector_Menu_Machine_Close, items);
        }
        case UIActionIndexSelector_Simple_Group_Refresh:
        case UIActionIndexSelector_Simple_Machine_Refresh:
        {
            /* Make sure all items are NOT accessible: */
            return isItemsInaccessible(items);
        }
        case UIActionIndex_Simple_LogDialog:
        case UIActionIndexSelector_Simple_Group_ShowInFileManager:
        case UIActionIndexSelector_Simple_Machine_ShowInFileManager:
//        case UIActionIndexSelector_Simple_Group_Sort:
//        case UIActionIndexSelector_Simple_Machine_Sort:
//        {
//            /* Make sure all items are accessible: */
//            return isItemsAccessible(items);
//        }
        case UIActionIndexSelector_Simple_Group_CreateShortcut:
        case UIActionIndexSelector_Simple_Machine_CreateShortcut:
        {
            /* Make sure all items supports shortcuts: */
            return isItemsSupportsShortcuts(items);
        }
        default:
            break;
    }

    /* Unknown actions are disabled: */
    return false;
}

/* static */
bool UISelectorWindow::isItemsAccessible(const QList<UIVMItem*> &items)
{
    foreach (UIVMItem *pItem, items)
        if (!pItem->accessible())
            return false;
    return true;
}

/* static */
bool UISelectorWindow::isItemsInaccessible(const QList<UIVMItem*> &items)
{
    foreach (UIVMItem *pItem, items)
        if (pItem->accessible())
            return false;
    return true;
}

/* static */
bool UISelectorWindow::isItemsHasUnlockedSession(const QList<UIVMItem*> &items)
{
    foreach (UIVMItem *pItem, items)
        if (pItem->sessionState() != KSessionState_Unlocked)
            return false;
    return true;
}

/* static */
bool UISelectorWindow::isItemsSupportsShortcuts(const QList<UIVMItem*> &items)
{
    /* Make sure all items are accessible: */
    if (!isItemsAccessible(items))
        return false;

#ifdef Q_WS_MAC
    /* On Mac OS X this are real alias files, which don't work with the old
     * legacy xml files. On the other OS's some kind of start up script is used. */
    foreach (UIVMItem *pItem, items)
        if (!pItem->settingsFile().endsWith(".vbox", Qt::CaseInsensitive))
            return false;
#endif /* Q_WS_MAC */

    /* True by default: */
    return true;
}

/* static */
bool UISelectorWindow::isItemsPoweredOff(const QList<UIVMItem*> &items)
{
    foreach (UIVMItem *pItem, items)
        if (!(pItem->machineState() == KMachineState_PoweredOff ||
              pItem->machineState() == KMachineState_Saved ||
              pItem->machineState() == KMachineState_Teleported ||
              pItem->machineState() == KMachineState_Aborted))
        return false;
    return true;
}

