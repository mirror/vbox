/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UISelectorWindow class implementation
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */
/* Global includes */
#include <QDesktopWidget>
#include <QMenuBar>
#include <QResizeEvent>
#include <QDesktopServices>

/* Local includes: */
#include "QISplitter.h"
#include "UIBar.h"
#include "UINetworkManager.h"
#include "UIUpdateManager.h"
#include "UIDownloaderUserManual.h"
#include "UIDownloaderExtensionPack.h"
#include "UIExportApplianceWzd.h"
#include "UIIconPool.h"
#include "UIImportApplianceWzd.h"
#include "UICloneVMWizard.h"
#include "UINewVMWzd.h"
#include "UIVMDesktop.h"
#include "UIVMListView.h"
#include "UIVirtualBoxEventHandler.h"
#include "VBoxGlobal.h"
#include "VBoxMediaManagerDlg.h"
#include "UIMessageCenter.h"
#include "UISelectorWindow.h"
#include "UISettingsDialogSpecific.h"
#include "UIToolBar.h"
#include "UIVMLogViewer.h"
#include "QIFileDialog.h"
#include "UISelectorShortcuts.h"
#include "UIDesktopServices.h"
#include "UIGlobalSettingsExtension.h"
#include "UIActionPoolSelector.h"

#ifdef VBOX_GUI_WITH_SYSTRAY
# include "VBoxTrayIcon.h"
# include "UIExtraDataEventHandler.h"
#endif /* VBOX_GUI_WITH_SYSTRAY */

#ifdef Q_WS_MAC
# include "VBoxUtils.h"
# include "UIWindowMenuManager.h"
# include "UIImageTools.h"
#endif /* Q_WS_MAC */

#ifdef Q_WS_X11
# include <iprt/env.h>
#endif /* Q_WS_X11 */

#include <iprt/buildconfig.h>
#include <VBox/version.h>
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UISelectorWindow::UISelectorWindow(UISelectorWindow **ppSelf, QWidget *pParent,
                                   Qt::WindowFlags flags /* = Qt::Window */)
    : QIWithRetranslateUI2<QMainWindow>(pParent, flags)
    , m_pSplitter(0)
#ifndef Q_WS_MAC
    , m_pBar(0)
#endif /* !Q_WS_MAC */
    , mVMToolBar(0)
    , m_pVMListView(0)
    , m_pVMModel(0)
    , m_pMachineContextMenu(0)
    , m_pVMDesktop(0)
    , m_fDoneInaccessibleWarningOnce(false)
{
    /* Remember self: */
    if (ppSelf)
        *ppSelf = this;

    /* Prepare everything: */
    prepareIcon();
    prepareMenuBar();
    prepareContextMenu();
    prepareStatusBar();
    prepareWidgets();
    prepareConnections();

    /* Load all settings: */
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
    /* Destroy our event handlers: */
    UIVirtualBoxEventHandler::destroy();

    /* Save all settings: */
    saveSettings();

#ifdef VBOX_GUI_WITH_SYSTRAY
    /* Delete systray menu object: */
    delete m_pTrayIcon;
    m_pTrayIcon = NULL;
#endif /* VBOX_GUI_WITH_SYSTRAY */
}

void UISelectorWindow::sltShowSelectorContextMenu(const QPoint &pos)
{
    /* Load toolbar/statusbar availability settings: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QString strToolbar = vbox.GetExtraData(VBoxDefs::GUI_Toolbar);
    QString strStatusbar = vbox.GetExtraData(VBoxDefs::GUI_Statusbar);
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
            vbox.SetExtraData(VBoxDefs::GUI_Toolbar, "true");
        }
        else
        {
#ifdef Q_WS_MAC
            mVMToolBar->hide();
#else /* Q_WS_MAC */
            m_pBar->hide();
#endif /* !Q_WS_MAC */
            vbox.SetExtraData(VBoxDefs::GUI_Toolbar, "false");
        }
    }
    else if (pResult == pShowStatusBar)
    {
        if (pResult->isChecked())
        {
            statusBar()->show();
            vbox.SetExtraData(VBoxDefs::GUI_Statusbar, "true");
        }
        else
        {
            statusBar()->hide();
            vbox.SetExtraData(VBoxDefs::GUI_Statusbar, "false");
        }
    }
}

void UISelectorWindow::sltShowMediumManager()
{
    /* Show modeless Virtual Medium Manager: */
    VBoxMediaManagerDlg::showModeless(this);
}

void UISelectorWindow::sltShowImportApplianceWizard(const QString &strFileName /* = QString() */)
{
    /* Show Import Appliance wizard: */
#ifdef Q_WS_MAC
    QString strTmpFile = ::darwinResolveAlias(strFileName);
#else /* Q_WS_MAC */
    QString strTmpFile = strFileName;
#endif /* !Q_WS_MAC */
    UIImportApplianceWzd wizard(strTmpFile, this);
    if (strFileName.isEmpty() || wizard.isValid())
        wizard.exec();
}

void UISelectorWindow::sltShowExportApplianceWizard()
{
    /* Get selected items: */
    QList<UIVMItem*> items = m_pVMListView->currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* Populate the list of VM names: */
    QStringList names;
    for (int i = 0; i < items.size(); ++i)
        names << items[i]->name();
    /* Show Export Appliance wizard: */
    UIExportApplianceWzd wizard(this, names);
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
    /* We have to check if there are any open windows beside this mainwindow
     * (e.g. VDM) and if so close them. Note that the default behavior is
     * different to Qt3 where a *mainWidget* exists & if this going to close
     * all other windows are closed automatically. We do the same below. */
    QWidgetList widgets = QApplication::topLevelWidgets();
    for(int i = 0; i < widgets.size(); ++i)
    {
        QWidget *pWidget = widgets[i];
        if (pWidget && pWidget->isVisible() && pWidget != this)
            pWidget->close();
    }
    /* We close this pWidget last: */
    close();
}

void UISelectorWindow::sltShowNewMachineWizard()
{
    /* Show New Machine wizard: */
    UINewVMWzd wizard(this);
    if (wizard.exec() == QDialog::Accepted)
    {
        /* Wait until the list is updated by OnMachineRegistered(): */
        const CMachine &machine = wizard.machine();
        QModelIndex index;
        while(!index.isValid())
        {
            qApp->processEvents();
            index = m_pVMModel->indexById(machine.GetId());
        }
        /* Choose newly created item: */
        m_pVMListView->setCurrentIndex(index);
    }
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
        for (int i = 0; i < VBoxDefs::VBoxFileExts.size(); ++i)
            extensions << QString("*.%1").arg(VBoxDefs::VBoxFileExts[i]);
        QString strFilter = tr("Virtual machine files (%1)").arg(extensions.join(" "));
        /* Create open file dialog: */
        QStringList fileNames = QIFileDialog::getOpenFileNames(strBaseFolder, strFilter, this, strTitle, 0, true, true);
        if (!fileNames.isEmpty())
            strTmpFile = fileNames.at(0);
    }

    /* Open corresponding machine: */
    if (!strTmpFile.isEmpty())
    {
        CMachine newMachine = vbox.OpenMachine(strTmpFile);
        /* First we should test what machine was opened: */
        if (vbox.isOk() && !newMachine.isNull())
        {
            /* Second we should check what such machine was NOT already registered.
             * Actually current Main implementation will even prevent such machine opening
             * but we will perform such a check anyway: */
            CMachine oldMachine = vbox.FindMachine(newMachine.GetId());
            if (oldMachine.isNull())
            {
                /* Register that machine: */
                vbox.RegisterMachine(newMachine);
                /* Wait until the list is updated by OnMachineRegistered(): */
                QModelIndex index;
                while(!index.isValid())
                {
                    qApp->processEvents();
                    index = m_pVMModel->indexById(newMachine.GetId());
                }
                /* And choose added item: */
                m_pVMListView->setCurrentIndex(index);
            }
            else
                msgCenter().cannotReregisterMachine(this, strTmpFile, oldMachine.GetName());
        }
        else
            msgCenter().cannotOpenMachine(this, strTmpFile, vbox);
    }
}

void UISelectorWindow::sltShowMachineSettingsDialog(const QString &strCategoryRef /* = QString() */,
                                                    const QString &strControlRef /* = QString() */,
                                                    const QString &strMachineId /* = QString() */)
{
    /* Check that we do NOT handling that already: */
    if (m_pSettingsDialogAction->data().toBool())
        return;
    /* Remember that we handling that already: */
    m_pSettingsDialogAction->setData(true);

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

    /* Get corresponding VM item: */
    UIVMItem *pItem = strMachineId.isNull() ? m_pVMListView->currentItem() : m_pVMModel->itemById(strMachineId);
    AssertMsgReturnVoid(pItem, ("Current item should be selected!\n"));

    /* Create and execute corresponding VM settings dialog: */
    UISettingsDialogMachine dialog(this, pItem->id(), strCategory, strControl);
    dialog.execute();

    /* Remember that we do NOT handling that already: */
    m_pSettingsDialogAction->setData(false);
}

void UISelectorWindow::sltShowCloneMachineWizard()
{
    /* Get current item: */
    UIVMItem *pItem = m_pVMListView->currentItem();
    AssertMsgReturnVoid(pItem, ("Current item should be selected!\n"));

    /* Show clone VM wizard: */
    UICloneVMWizard wizard(this, pItem->machine());
    wizard.exec();
}

void UISelectorWindow::sltShowRemoveMachineDialog()
{
    /* Get selected items: */
    QList<UIVMItem*> items = m_pVMListView->currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* Populate machine list: */
    QList<CMachine> machines;
    for (int i = 0; i < items.size(); ++i)
        machines << items[i]->machine();
    /* Show machine remove dialog: */
    int rc = msgCenter().confirmMachineDeletion(machines);
    if (rc != QIMessageBox::Cancel)
    {
        /* For every selected item: */
        for (int i = 0; i < machines.size(); ++i)
        {
            /* Check if current item could be removed: */
            if (!isActionEnabled(UIActionIndexSelector_Simple_Machine_RemoveDialog, items[i], items))
                continue;

            /* Get iterated VM: */
            CMachine machine = machines[i];
            if (rc == QIMessageBox::Yes)
            {
                /* Unregister and cleanup machine's data & hard-disks: */
                CMediumVector mediums = machine.Unregister(KCleanupMode_DetachAllReturnHardDisksOnly);
                if (machine.isOk())
                {
                    /* Delete machine hard-disks: */
                    CProgress progress = machine.Delete(mediums);
                    if (machine.isOk())
                    {
                        msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_delete_90px.png", 0, true);
                        if (progress.GetResultCode() != 0)
                            msgCenter().cannotDeleteMachine(machine, progress);
                    }
                }
                if (!machine.isOk())
                    msgCenter().cannotDeleteMachine(machine);
            }
            else
            {
                /* Just unregister machine: */
                machine.Unregister(KCleanupMode_DetachAllReturnNone);
                if (!machine.isOk())
                    msgCenter().cannotDeleteMachine(machine);
            }
        }
    }
}

void UISelectorWindow::sltPerformStartOrShowAction()
{
    /* We always get here when m_pVMListView emits the activated() signal,
     * so we must explicitly check if the action is enabled or not. */
    if (!m_pStartOrShowAction->isEnabled())
        return;

    /* Get selected items: */
    QList<UIVMItem*> items = m_pVMListView->currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For every selected item: */
    for (int i = 0; i < items.size(); ++i)
    {
        /* Check if current item could be started/showed: */
        if (!isActionEnabled(UIActionIndexSelector_State_Machine_StartOrShow, items[i], items))
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
    UIVMItem *pItem = m_pVMListView->currentItem();
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
    QList<UIVMItem*> items = m_pVMListView->currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For every selected item: */
    for (int i = 0; i < items.size(); ++i)
    {
        /* Get iterated item: */
        UIVMItem *pItem = items[i];
        /* Get item state: */
        KMachineState state = pItem->machineState();

        /* Check if current item could be paused/resumed: */
        if (!isActionEnabled(UIActionIndexSelector_Toggle_Machine_PauseAndResume, pItem, items))
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
    QList<UIVMItem*> items = m_pVMListView->currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    for (int i = 0; i < items.size(); ++i)
    {
        /* Get iterated item: */
        UIVMItem *pItem = items[i];

        /* Check if current item could be reseted: */
        if (!isActionEnabled(UIActionIndexSelector_Simple_Machine_Reset, pItem, items))
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
    QList<UIVMItem*> items = m_pVMListView->currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    for (int i = 0; i < items.size(); ++i)
    {
        /* Get iterated item: */
        UIVMItem *pItem = items[i];

        /* Check if current item could be shutdowned: */
        if (!isActionEnabled(UIActionIndexSelector_Simple_Machine_Close_ACPIShutdown, pItem, items))
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
    QList<UIVMItem*> items = m_pVMListView->currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    for (int i = 0; i < items.size(); ++i)
    {
        /* Get iterated item: */
        UIVMItem *pItem = items[i];

        /* Check if current item could be powered off: */
        if (!isActionEnabled(UIActionIndexSelector_Simple_Machine_Close_PowerOff, pItem, items))
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
    QList<UIVMItem*> items = m_pVMListView->currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    for (int i = 0; i < items.size(); ++i)
    {
        /* Get iterated item: */
        UIVMItem *pItem = items[i];

        /* Check if current item could be refreshed: */
        if (!isActionEnabled(UIActionIndexSelector_Simple_Machine_Refresh, pItem, items))
            continue;

        /* Refresh currently selected VM item: */
        sltRefreshVMItem(pItem->id(), true /* details */, true /* snapshot */, true /* description */);
    }
}

void UISelectorWindow::sltShowLogDialog()
{
    /* Get selected items: */
    QList<UIVMItem*> items = m_pVMListView->currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    for (int i = 0; i < items.size(); ++i)
    {
        /* Get iterated item: */
        UIVMItem *pItem = items[i];

        /* Check if log could be show for the current item: */
        if (!isActionEnabled(UIActionIndexSelector_Simple_Machine_LogDialog, pItem, items))
            continue;

        /* Show VM Log Viewer: */
        UIVMLogViewer::showLogViewerFor(this, pItem->machine());
    }
}

void UISelectorWindow::sltShowMachineInFileManager()
{
    /* Get selected items: */
    QList<UIVMItem*> items = m_pVMListView->currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    for (int i = 0; i < items.size(); ++i)
    {
        /* Get iterated item: */
        UIVMItem *pItem = items[i];

        /* Check if that item could be shown in file-browser: */
        if (!isActionEnabled(UIActionIndexSelector_Simple_Machine_ShowInFileManager, pItem, items))
            continue;

        /* Show VM in filebrowser: */
        UIDesktopServices::openInFileManager(pItem->machine().GetSettingsFilePath());
    }
}

void UISelectorWindow::sltPerformCreateShortcutAction()
{
    /* Get selected items: */
    QList<UIVMItem*> items = m_pVMListView->currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    for (int i = 0; i < items.size(); ++i)
    {
        /* Get iterated item: */
        UIVMItem *pItem = items[i];

        /* Check if shortcuts could be created for this item: */
        if (!isActionEnabled(UIActionIndexSelector_Simple_Machine_CreateShortcut, pItem, items))
            continue;

        /* Create shortcut for this VM: */
        const CMachine &machine = pItem->machine();
        UIDesktopServices::createMachineShortcut(machine.GetSettingsFilePath(),
                                                 QDesktopServices::storageLocation(QDesktopServices::DesktopLocation),
                                                 machine.GetName(), machine.GetId());
    }
}

void UISelectorWindow::sltPerformSortAction()
{
    /* Get current item: */
    UIVMItem *pItem = m_pVMListView->currentItem();
    AssertMsgReturnVoid(pItem, ("Current item should be selected!\n"));

    /* Sort VM list: */
    const QString strOldId = pItem->id();
    m_pVMModel->sortByIdList(QStringList(), qApp->keyboardModifiers() == Qt::ShiftModifier ? Qt::DescendingOrder : Qt::AscendingOrder);
    /* Select the VM which was selected before */
    m_pVMListView->selectItemById(strOldId);
}

void UISelectorWindow::sltMachineMenuAboutToShow()
{
    /* Get current item: */
    UIVMItem *pItem = m_pVMListView->currentItem();
    /* Get selected items: */
    QList<UIVMItem*> items = m_pVMListView->currentItems();
    AssertMsgReturnVoid(pItem, ("Current item should be selected!\n"));

    m_pMachineCloseMenuAction->setEnabled(isActionEnabled(UIActionIndexSelector_Menu_Machine_Close, pItem, items));
}

void UISelectorWindow::sltMachineCloseMenuAboutToShow()
{
    /* Get current item: */
    UIVMItem *pItem = m_pVMListView->currentItem();
    /* Get selected items: */
    QList<UIVMItem*> items = m_pVMListView->currentItems();
    AssertMsgReturnVoid(pItem, ("Current item should be selected!\n"));

    m_pACPIShutdownAction->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_Close_ACPIShutdown, pItem, items));
}

void UISelectorWindow::sltMachineContextMenuHovered(QAction *pAction)
{
    /* Update statusbar with howered action status text: */
    statusBar()->showMessage(pAction->statusTip());
}

void UISelectorWindow::sltRefreshVMList()
{
    /* Refresh VM list: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CMachineVector machines = vbox.GetMachines();
    for (CMachineVector::ConstIterator m = machines.begin(); m != machines.end(); ++m)
        m_pVMModel->addItem(*m);
    /* Apply the saved sort order. */
    m_pVMModel->sortByIdList(vbox.GetExtraDataStringList(VBoxDefs::GUI_SelectorVMPositions));
    /* Update details page. */
    sltCurrentVMItemChanged();
#ifdef VBOX_GUI_WITH_SYSTRAY
    if (vboxGlobal().isTrayMenu())
        m_pTrayIcon->refresh();
#endif /* VBOX_GUI_WITH_SYSTRAY */
}

void UISelectorWindow::sltRefreshVMItem(const QString &strMachineId, bool fDetails, bool fSnapshots, bool fDescription)
{
    /* Refresh VM item: */
    UIVMItem *pItem = m_pVMModel->itemById(strMachineId);
    if (pItem)
    {
        m_pVMModel->refreshItem(pItem);
        if (pItem && pItem->id() == strMachineId)
            sltCurrentVMItemChanged(fDetails, fSnapshots, fDescription);
    }
}

void UISelectorWindow::sltShowContextMenu(const QPoint &point)
{
    /* Show VM list context menu: */
    const QModelIndex &index = m_pVMListView->indexAt(point);
    if (index.isValid() && m_pVMListView->model()->data(index, UIVMItemModel::UIVMItemPtrRole).value<UIVMItem*>())
    {
        m_pMachineContextMenu->exec(m_pVMListView->mapToGlobal(point));
        /* Make sure every status bar hint from the context menu is cleared when the menu is closed: */
        statusBar()->clearMessage();
    }
}

void UISelectorWindow::sltCurrentVMItemChanged(bool fRefreshDetails, bool fRefreshSnapshots, bool fRefreshDescription)
{
    /* Get current item: */
    UIVMItem *pItem = m_pVMListView->currentItem();
    QList<UIVMItem*> items = m_pVMListView->currentItems();

    /* Enable/disable actions: */
    m_pSettingsDialogAction->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_SettingsDialog, pItem, items));
    m_pCloneWizardAction->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_CloneWizard, pItem, items));
    m_pRemoveDialogAction->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_RemoveDialog, pItem, items));
    m_pStartOrShowAction->setEnabled(isActionEnabled(UIActionIndexSelector_State_Machine_StartOrShow, pItem, items));
    m_pDiscardAction->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_Discard, pItem, items));
    m_pPauseAndResumeAction->setEnabled(isActionEnabled(UIActionIndexSelector_Toggle_Machine_PauseAndResume, pItem, items));
    m_pResetAction->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_Reset, pItem, items));
    m_pACPIShutdownAction->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_Close_ACPIShutdown, pItem, items));
    m_pPowerOffAction->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_Close_PowerOff, pItem, items));
    m_pRefreshAction->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_Refresh, pItem, items));
    m_pLogDialogAction->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_LogDialog, pItem, items));
    m_pShowInFileManagerAction->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_ShowInFileManager, pItem, items));
    m_pCreateShortcutAction->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_CreateShortcut, pItem, items));
    m_pSortAction->setEnabled(isActionEnabled(UIActionIndexSelector_Simple_Machine_Sort, pItem, items));

    /* If currently selected VM item is accessible: */
    if (pItem && pItem->accessible())
    {
        CMachine m = pItem->machine();
        QList<CMachine> machines;
        for (int i = 0; i < items.size(); ++i)
            machines << items[i]->machine();
        KMachineState state = pItem->machineState();

        if (fRefreshDetails || fRefreshDescription)
            m_pVMDesktop->updateDetails(pItem, machines);
        if (fRefreshSnapshots)
            m_pVMDesktop->updateSnapshots(pItem, m);

        /* Update the Start button action appearance: */
        if (state == KMachineState_PoweredOff ||
            state == KMachineState_Saved ||
            state == KMachineState_Teleported ||
            state == KMachineState_Aborted)
        {
            m_pStartOrShowAction->setState(1);
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
            m_pStartOrShowAction->setState(2);
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
            m_pPauseAndResumeAction->blockSignals(true);
            m_pPauseAndResumeAction->setChecked(true);
            m_pPauseAndResumeAction->blockSignals(false);
        }
        else
        {
            m_pPauseAndResumeAction->blockSignals(true);
            m_pPauseAndResumeAction->setChecked(false);
            m_pPauseAndResumeAction->blockSignals(false);
        }
        m_pPauseAndResumeAction->updateAppearance();
    }

    /* If currently selected VM item is NOT accessible: */
    else
    {
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
        m_pStartOrShowAction->setState(1);
    }
}

void UISelectorWindow::sltOpenUrls(QList<QUrl> list /* = QList<QUrl>() */)
{
    /* Make sure any pending D&D events are consumed. */
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
            if (VBoxGlobal::hasAllowedExtension(strFile, VBoxDefs::VBoxFileExts))
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
            else if (VBoxGlobal::hasAllowedExtension(strFile, VBoxDefs::OVFFileExts))
            {
                /* OVF/OVA. Only one file at the time. */
                sltShowImportApplianceWizard(strFile);
                break;
            }
            else if (VBoxGlobal::hasAllowedExtension(strFile, VBoxDefs::VBoxExtPackFileExts))
            {
                UIGlobalSettingsExtension::doInstallation(strFile, QString(), this, NULL);
            }
        }
    }
}

void UISelectorWindow::sltMachineRegistered(QString strMachineId, bool fRegistered)
{
    if (fRegistered)
    {
        CVirtualBox vbox = vboxGlobal().virtualBox();
        CMachine m = vbox.FindMachine(strMachineId);
        if (!m.isNull())
        {
            m_pVMModel->addItem(m);
            /* Make sure the description, ... pages are properly updated.
             * Actually we haven't call the next method, but unfortunately Qt
             * seems buggy if the new item is on the same position as the
             * previous one. So go on the safe side and call this by our self. */
            sltCurrentVMItemChanged();
        }
        /* m.isNull() is ok (theoretically, the machine could have been
         * already deregistered by some other client at this point) */
    }
    else
    {
        UIVMItem *pItem = m_pVMModel->itemById(strMachineId);
        if (pItem)
        {
            int iRow = m_pVMModel->rowById(pItem->id());
            m_pVMModel->removeItem(pItem);
            m_pVMListView->ensureSomeRowSelected(iRow);
        }
        /* item = 0 is ok (if we originated this event then the item
         * has been already removed) */
    }
}

void UISelectorWindow::sltMachineStateChanged(QString strMachineId, KMachineState /* state */)
{
#ifdef VBOX_GUI_WITH_SYSTRAY
    if (vboxGlobal().isTrayMenu())
    {
        /* Check if there are some machines alive - else quit, since
         * we're not needed as a systray menu anymore. */
        if (vboxGlobal().mainWindowCount() == 0)
        {
            sltPerformExit();
            return;
        }
    }
#endif /* VBOX_GUI_WITH_SYSTRAY */

    sltRefreshVMItem(strMachineId, false /* aDetails */, true /* aSnapshots */, false /* aDescription */);

    /* Simulate a state change signal: */
//    m_pVMDesktop->updateDescriptionState();
}

void UISelectorWindow::sltMachineDataChanged(QString strMachineId)
{
    sltRefreshVMItem(strMachineId, true  /* details */, false /* snapshots */, true  /* description */);
}

void UISelectorWindow::sltSessionStateChanged(QString strMachineId, KSessionState /* state */)
{
    sltRefreshVMItem(strMachineId, true  /* details */, false /* snapshots */, false /* description */);

//    /* Simulate a state change signal: */
//    m_pVMDesktop->updateDescriptionState();
}

void UISelectorWindow::sltSnapshotChanged(QString strMachineId, QString /* strSnapshotId */)
{
    sltRefreshVMItem(strMachineId, false /* details */, true  /* snapshot */, false /* description */);
}

#ifdef VBOX_GUI_WITH_SYSTRAY
void UISelectorWindow::sltMainWindowCountChanged(int iCount)
{
    if (vboxGlobal().isTrayMenu() && count <= 1)
        sltPerformExit();
}

void UISelectorWindow::sltTrayIconCanShow(bool fEnabled)
{
    emit sltTrayIconChanged(VBoxChangeTrayIconEvent(vboxGlobal().settings().trayIconEnabled()));
}

void UISelectorWindow::sltTrayIconShow(bool fEnabled)
{
    if (vboxGlobal().isTrayMenu() && m_pTrayIcon)
        m_pTrayIcon->sltTrayIconShow(fEnabled);
}

void UISelectorWindow::sltTrayIconChanged(bool fEnabled)
{
    /* Not used yet. */
}

void UISelectorWindow::sltTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason)
    {
        case QSystemTrayIcon::Context:
            m_pTrayIcon->refresh();
            break;

        case QSystemTrayIcon::Trigger:
            break;

        case QSystemTrayIcon::DoubleClick:
            vboxGlobal().trayIconShowSelector();
            break;

        case QSystemTrayIcon::MiddleClick:
            break;

        default:
            break;
    }
}

void UISelectorWindow::sltShowWindow()
{
    showNormal();
    raise();
    activateWindow();
}
#endif // VBOX_GUI_WITH_SYSTRAY

void UISelectorWindow::sltMediumEnumerationStarted()
{
    /* Refresh the current details to pick up hard disk sizes: */
    sltCurrentVMItemChanged(true /* fRefreshDetails */);
}

void UISelectorWindow::sltMediumEnumFinished(const VBoxMediaList &list)
{
    /* Refresh the current details to pick up hard disk sizes */
    sltCurrentVMItemChanged(true /* fRefreshDetails */);

    /* We warn about inaccessible media only once (after media emumeration
     * started from main() at startup), to avoid annoying the user */
    if (m_fDoneInaccessibleWarningOnce
#ifdef VBOX_GUI_WITH_SYSTRAY
        || vboxGlobal().isTrayMenu()
#endif /* VBOX_GUI_WITH_SYSTRAY */
       )
        return;

    m_fDoneInaccessibleWarningOnce = true;

    do
    {
        /* Ignore the signal if a modal widget is currently active (we won't be
         * able to properly show the modeless VDI manager window in this case): */
        if (QApplication::activeModalWidget())
            break;

        /* Ignore the signal if a VBoxMediaManagerDlg window is active: */
        if (qApp->activeWindow() &&
            !strcmp(qApp->activeWindow()->metaObject()->className(), "VBoxMediaManagerDlg"))
            break;

        /* Look for at least one inaccessible media: */
        VBoxMediaList::const_iterator it;
        for (it = list.begin(); it != list.end(); ++ it)
            if ((*it).state() == KMediumState_Inaccessible)
                break;

        if (it != list.end() && msgCenter().remindAboutInaccessibleMedia())
        {
            /* Show the VDM dialog but don't refresh once more after a just-finished refresh: */
            VBoxMediaManagerDlg::showModeless(this, false /* aRefresh */);
        }
    }
    while (0);
}

void UISelectorWindow::sltEmbedDownloader(UIDownloadType downloaderType)
{
    switch (downloaderType)
    {
        case UIDownloadType_UserManual:
        {
            if (UIDownloaderUserManual *pDl = UIDownloaderUserManual::current())
                statusBar()->addWidget(pDl->progressWidget(this), 0);
            break;
        }
        case UIDownloadType_ExtensionPack:
        {
            if (UIDownloaderExtensionPack *pDl = UIDownloaderExtensionPack::current())
                statusBar()->addWidget(pDl->progressWidget(this), 0);
            break;
        }
        default:
            break;
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
            vbox.SetExtraData(VBoxDefs::GUI_Toolbar, !::darwinIsToolbarVisible(mVMToolBar) ? "true" : "false");
            break;
        }
#endif /* Q_WS_MAC */
        default:
            break;
    }

    return QMainWindow::event(pEvent);
}

void UISelectorWindow::closeEvent(QCloseEvent *pEvent)
{
#ifdef VBOX_GUI_WITH_SYSTRAY
    /* Needed for breaking out of the while() loop in main(). */
    if (vboxGlobal().isTrayMenu())
        vboxGlobal().setTrayMenu(false);
#endif /* VBOX_GUI_WITH_SYSTRAY */

    emit closing();
    QMainWindow::closeEvent(pEvent);
}

#ifdef Q_WS_MAC
bool UISelectorWindow::eventFilter(QObject *pObject, QEvent *pEvent)
{
    if (!isActiveWindow())
        return QIWithRetranslateUI2<QMainWindow>::eventFilter(pObject, pEvent);

    if (qobject_cast<QWidget*>(pObject) &&
        qobject_cast<QWidget*>(pObject)->window() != this)
        return QIWithRetranslateUI2<QMainWindow>::eventFilter(pObject, pEvent);

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
    /* Prepare 'File' menu: */
    m_pFileMenu = gActionPool->action(UIActionIndexSelector_Menu_File)->menu();
    prepareMenuFile(m_pFileMenu);
    menuBar()->addMenu(m_pFileMenu);

    /* Prepare 'Machine' menu: */
    m_pMachineMenu = gActionPool->action(UIActionIndexSelector_Menu_Machine)->menu();
    prepareMenuMachine(m_pMachineMenu);
    menuBar()->addMenu(m_pMachineMenu);

#ifdef Q_WS_MAC
    menuBar()->addMenu(UIWindowMenuManager::instance(this)->createMenu(this));
#endif /* Q_WS_MAC */

    /* Prepare 'Help' menu: */
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

    /* Populate 'File' menu: */
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

void UISelectorWindow::prepareMenuMachine(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

    /* Populate 'Machine' menu: */
    m_pNewWizardAction = gActionPool->action(UIActionIndexSelector_Simple_Machine_NewWizard);
    pMenu->addAction(m_pNewWizardAction);
    m_pAddDialogAction = gActionPool->action(UIActionIndexSelector_Simple_Machine_AddDialog);
    pMenu->addAction(m_pAddDialogAction);
    m_pSettingsDialogAction = gActionPool->action(UIActionIndexSelector_Simple_Machine_SettingsDialog);
    pMenu->addAction(m_pSettingsDialogAction);
    m_pCloneWizardAction = gActionPool->action(UIActionIndexSelector_Simple_Machine_CloneWizard);
    pMenu->addAction(m_pCloneWizardAction);
    m_pRemoveDialogAction = gActionPool->action(UIActionIndexSelector_Simple_Machine_RemoveDialog);
    pMenu->addAction(m_pRemoveDialogAction);
    pMenu->addSeparator();
    m_pStartOrShowAction = gActionPool->action(UIActionIndexSelector_State_Machine_StartOrShow);
    pMenu->addAction(m_pStartOrShowAction);
    m_pDiscardAction = gActionPool->action(UIActionIndexSelector_Simple_Machine_Discard);
    pMenu->addAction(m_pDiscardAction);
    m_pPauseAndResumeAction = gActionPool->action(UIActionIndexSelector_Toggle_Machine_PauseAndResume);
    pMenu->addAction(m_pPauseAndResumeAction);
    m_pResetAction = gActionPool->action(UIActionIndexSelector_Simple_Machine_Reset);
    pMenu->addAction(m_pResetAction);
    /* Prepare 'machine/close' menu: */
    m_pMachineCloseMenuAction = gActionPool->action(UIActionIndexSelector_Menu_Machine_Close);
    m_pMachineCloseMenu = m_pMachineCloseMenuAction->menu();
    prepareMenuMachineClose(m_pMachineCloseMenu);
    pMenu->addMenu(m_pMachineCloseMenu);
    pMenu->addSeparator();
    m_pRefreshAction = gActionPool->action(UIActionIndexSelector_Simple_Machine_Refresh);
    pMenu->addAction(m_pRefreshAction);
    m_pLogDialogAction = gActionPool->action(UIActionIndexSelector_Simple_Machine_LogDialog);
    pMenu->addAction(m_pLogDialogAction);
    pMenu->addSeparator();
    m_pShowInFileManagerAction = gActionPool->action(UIActionIndexSelector_Simple_Machine_ShowInFileManager);
    pMenu->addAction(m_pShowInFileManagerAction);
    m_pCreateShortcutAction = gActionPool->action(UIActionIndexSelector_Simple_Machine_CreateShortcut);
    pMenu->addAction(m_pCreateShortcutAction);
    pMenu->addSeparator();
    m_pSortAction = gActionPool->action(UIActionIndexSelector_Simple_Machine_Sort);
    pMenu->addAction(m_pSortAction);
}

void UISelectorWindow::prepareMenuMachineClose(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

    /* Populate 'Machine/Close' menu: */
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

    /* Populate 'Help' menu: */
    m_pHelpAction = gActionPool->action(UIActionIndex_Simple_Help);
    pMenu->addAction(m_pHelpAction);
    m_pWebAction = gActionPool->action(UIActionIndex_Simple_Web);
    pMenu->addAction(m_pWebAction);
    pMenu->addSeparator();
    m_pResetWarningsAction = gActionPool->action(UIActionIndex_Simple_ResetWarnings);
    pMenu->addAction(m_pResetWarningsAction);
    pMenu->addSeparator();
#ifdef VBOX_WITH_REGISTRATION
    m_pRegisterAction = gActionPool->action(UIActionIndex_Simple_Register);
    pMenu->addAction(m_pRegisterAction);
#endif /* VBOX_WITH_REGISTRATION */
    m_pUpdateAction = gActionPool->action(UIActionIndex_Simple_Update);
    pMenu->addAction(m_pUpdateAction);
#ifndef Q_WS_MAC
    pMenu->addSeparator();
#endif /* !Q_WS_MAC */
    m_pAboutAction = gActionPool->action(UIActionIndex_Simple_About);
    pMenu->addAction(m_pAboutAction);
}

void UISelectorWindow::prepareContextMenu()
{
    m_pMachineContextMenu = new QMenu(this);
    m_pMachineContextMenu->addAction(m_pSettingsDialogAction);
    m_pMachineContextMenu->addAction(m_pCloneWizardAction);
    m_pMachineContextMenu->addAction(m_pRemoveDialogAction);
    m_pMachineContextMenu->addSeparator();
    m_pMachineContextMenu->addAction(m_pStartOrShowAction);
    m_pMachineContextMenu->addAction(m_pDiscardAction);
    m_pMachineContextMenu->addAction(m_pPauseAndResumeAction);
    m_pMachineContextMenu->addAction(m_pResetAction);
    m_pMachineContextMenu->addMenu(m_pMachineCloseMenu);
    m_pMachineContextMenu->addSeparator();
    m_pMachineContextMenu->addAction(m_pRefreshAction);
    m_pMachineContextMenu->addAction(m_pLogDialogAction);
    m_pMachineContextMenu->addSeparator();
    m_pMachineContextMenu->addAction(m_pShowInFileManagerAction);
    m_pMachineContextMenu->addAction(m_pCreateShortcutAction);
    m_pMachineContextMenu->addSeparator();
    m_pMachineContextMenu->addAction(m_pSortAction);
}

void UISelectorWindow::prepareStatusBar()
{
    /* Setup statusbar policy: */
    statusBar()->setContextMenuPolicy(Qt::CustomContextMenu);
}

void UISelectorWindow::prepareWidgets()
{
    /* Prepare splitter: */
    m_pSplitter = new QISplitter(this);
    m_pSplitter->setHandleType(QISplitter::Native);

    /* Prepare tool-bar: */
    mVMToolBar = new UIToolBar(this);
    mVMToolBar->setContextMenuPolicy(Qt::CustomContextMenu);
    mVMToolBar->setIconSize(QSize(32, 32));
    mVMToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    mVMToolBar->addAction(m_pNewWizardAction);
    mVMToolBar->addAction(m_pSettingsDialogAction);
    mVMToolBar->addAction(m_pStartOrShowAction);
    mVMToolBar->addAction(m_pDiscardAction);

    /* Prepare VM list: */
    m_pVMModel = new UIVMItemModel(this);
    m_pVMListView = new UIVMListView(m_pVMModel);
    m_pVMListView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    /* Make non-possible to activate list elements by single click,
     * this hack should disable the current possibility to do it if present: */
    if (m_pVMListView->style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick, 0, m_pVMListView))
        m_pVMListView->setStyleSheet("activate-on-singleclick : 0");

    /* Prepare details and snapshots tabs: */
    m_pVMDesktop = new UIVMDesktop(mVMToolBar, m_pRefreshAction, this);

    /* Layout all the widgets: */
#define BIG_TOOLBAR
#if MAC_LEOPARD_STYLE
    addToolBar(mVMToolBar);
    /* Central widget @ horizontal layout: */
    setCentralWidget(m_pSplitter);
    m_pSplitter->addWidget(m_pVMListView);
#else /* MAC_LEOPARD_STYLE */
    QWidget *pLeftWidget = new QWidget(this);
    QVBoxLayout *pLeftVLayout = new QVBoxLayout(pLeftWidget);
    pLeftVLayout->setContentsMargins(0, 0, 0, 0);
    pLeftVLayout->setSpacing(0);
# ifdef BIG_TOOLBAR
    m_pBar = new UIMainBar(this);
    m_pBar->setContentWidget(mVMToolBar);
    pLeftVLayout->addWidget(m_pBar);
    pLeftVLayout->addWidget(m_pSplitter);
    setCentralWidget(pLeftWidget);
    m_pSplitter->addWidget(m_pVMListView);
# else /* BIG_TOOLBAR */
    pLeftVLayout->addWidget(mVMToolBar);
    pLeftVLayout->addWidget(m_pVMListView);
    setCentralWidget(m_pSplitter);
    m_pSplitter->addWidget(pLeftWidget);
# endif /* !BIG_TOOLBAR */
#endif /* !MAC_LEOPARD_STYLE */
    m_pSplitter->addWidget(m_pVMDesktop);

    /* Set the initial distribution. The right site is bigger. */
    m_pSplitter->setStretchFactor(0, 2);
    m_pSplitter->setStretchFactor(1, 3);

    /* Refresh VM list: */
    sltRefreshVMList();
    /* Reset to the first item: */
    m_pVMListView->selectItemByRow(0);
    /* Bring the VM list to the focus: */
    m_pVMListView->setFocus();

#ifdef VBOX_GUI_WITH_SYSTRAY
    m_pTrayIcon = new VBoxTrayIcon(this, m_pVMModel);
    Assert(m_pTrayIcon);
#endif /* VBOX_GUI_WITH_SYSTRAY */
}

void UISelectorWindow::prepareConnections()
{
    /* VirtualBox event connections: */
    connect(gVBoxEvents, SIGNAL(sigMachineStateChange(QString, KMachineState)), this, SLOT(sltMachineStateChanged(QString, KMachineState)));
    connect(gVBoxEvents, SIGNAL(sigMachineDataChange(QString)), this, SLOT(sltMachineDataChanged(QString)));
    connect(gVBoxEvents, SIGNAL(sigMachineRegistered(QString, bool)), this, SLOT(sltMachineRegistered(QString, bool)));
    connect(gVBoxEvents, SIGNAL(sigSessionStateChange(QString, KSessionState)), this, SLOT(sltSessionStateChanged(QString, KSessionState)));
    connect(gVBoxEvents, SIGNAL(sigSnapshotChange(QString, QString)), this, SLOT(sltSnapshotChanged(QString, QString)));

    /* Medium enumeration connections: */
    connect(&vboxGlobal(), SIGNAL(mediumEnumStarted()), this, SLOT(sltMediumEnumerationStarted()));
    connect(&vboxGlobal(), SIGNAL(mediumEnumFinished(const VBoxMediaList &)), this, SLOT(sltMediumEnumFinished(const VBoxMediaList &)));

    /* Network manager connections: */
    connect(gNetworkManager, SIGNAL(sigDownloaderCreated(UIDownloadType)), this, SLOT(sltEmbedDownloader(UIDownloadType)));

    /* Menu-bar connections: */
    connect(menuBar(), SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(sltShowSelectorContextMenu(const QPoint&)));

    /* 'File' menu connections: */
    connect(m_pMediumManagerDialogAction, SIGNAL(triggered()), this, SLOT(sltShowMediumManager()));
    connect(m_pImportApplianceWizardAction, SIGNAL(triggered()), this, SLOT(sltShowImportApplianceWizard()));
    connect(m_pExportApplianceWizardAction, SIGNAL(triggered()), this, SLOT(sltShowExportApplianceWizard()));
    connect(m_pPreferencesDialogAction, SIGNAL(triggered()), this, SLOT(sltShowPreferencesDialog()));
    connect(m_pExitAction, SIGNAL(triggered()), this, SLOT(sltPerformExit()));

    /* 'Machine' menu connections: */
    connect(m_pMachineMenu, SIGNAL(aboutToShow()), this, SLOT(sltMachineMenuAboutToShow()));
    connect(m_pNewWizardAction, SIGNAL(triggered()), this, SLOT(sltShowNewMachineWizard()));
    connect(m_pAddDialogAction, SIGNAL(triggered()), this, SLOT(sltShowAddMachineDialog()));
    connect(m_pSettingsDialogAction, SIGNAL(triggered()), this, SLOT(sltShowMachineSettingsDialog()));
    connect(m_pCloneWizardAction, SIGNAL(triggered()), this, SLOT(sltShowCloneMachineWizard()));
    connect(m_pRemoveDialogAction, SIGNAL(triggered()), this, SLOT(sltShowRemoveMachineDialog()));
    connect(m_pStartOrShowAction, SIGNAL(triggered()), this, SLOT(sltPerformStartOrShowAction()));
    connect(m_pDiscardAction, SIGNAL(triggered()), this, SLOT(sltPerformDiscardAction()));
    connect(m_pPauseAndResumeAction, SIGNAL(toggled(bool)), this, SLOT(sltPerformPauseResumeAction(bool)));
    connect(m_pResetAction, SIGNAL(triggered()), this, SLOT(sltPerformResetAction()));
    connect(m_pRefreshAction, SIGNAL(triggered()), this, SLOT(sltPerformRefreshAction()));
    connect(m_pLogDialogAction, SIGNAL(triggered()), this, SLOT(sltShowLogDialog()));
    connect(m_pShowInFileManagerAction, SIGNAL(triggered()), this, SLOT(sltShowMachineInFileManager()));
    connect(m_pCreateShortcutAction, SIGNAL(triggered()), this, SLOT(sltPerformCreateShortcutAction()));
    connect(m_pSortAction, SIGNAL(triggered()), this, SLOT(sltPerformSortAction()));

    /* 'Machine/Close' menu connections: */
    connect(m_pMachineCloseMenu, SIGNAL(aboutToShow()), this, SLOT(sltMachineCloseMenuAboutToShow()));
    connect(m_pACPIShutdownAction, SIGNAL(triggered()), this, SLOT(sltPerformACPIShutdownAction()));
    connect(m_pPowerOffAction, SIGNAL(triggered()), this, SLOT(sltPerformPowerOffAction()));

    /* 'Help' menu connections: */
    connect(m_pHelpAction, SIGNAL(triggered()), &msgCenter(), SLOT(sltShowHelpHelpDialog()));
    connect(m_pWebAction, SIGNAL(triggered()), &msgCenter(), SLOT(sltShowHelpWebDialog()));
    connect(m_pResetWarningsAction, SIGNAL(triggered()), &msgCenter(), SLOT(sltResetSuppressedMessages()));
#ifdef VBOX_WITH_REGISTRATION
    connect(m_pRegisterAction, SIGNAL(triggered()), &vboxGlobal(), SLOT(showRegistrationDialog()));
    connect(gEDataEvents, SIGNAL(sigCanShowRegistrationDlg(bool)), m_pRegisterAction, SLOT(setEnabled(bool)));
#endif /* VBOX_WITH_REGISTRATION */
    connect(m_pUpdateAction, SIGNAL(triggered()), gUpdateManager, SLOT(sltForceCheck()));
    connect(m_pAboutAction, SIGNAL(triggered()), &msgCenter(), SLOT(sltShowHelpAboutDialog()));

    /* 'Machine' context menu connections: */
    connect(m_pMachineContextMenu, SIGNAL(aboutToShow()), this, SLOT(sltMachineMenuAboutToShow()));
    connect(m_pMachineContextMenu, SIGNAL(hovered(QAction*)), this, SLOT(sltMachineContextMenuHovered(QAction*)));

    /* Status-bar connections: */
    connect(statusBar(), SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(sltShowSelectorContextMenu(const QPoint&)));

    /* VM list-view connections: */
    connect(m_pVMListView, SIGNAL(sigUrlsDropped(QList<QUrl>)), this, SLOT(sltOpenUrls(QList<QUrl>)), Qt::QueuedConnection);
    connect(m_pVMListView, SIGNAL(currentChanged()), this, SLOT(sltCurrentVMItemChanged()));
    connect(m_pVMListView, SIGNAL(activated()), this, SLOT(sltPerformStartOrShowAction()));
    connect(m_pVMListView, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(sltShowContextMenu(const QPoint &)));

    /* Tool-bar connections: */
#ifndef Q_WS_MAC
    connect(mVMToolBar, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(sltShowSelectorContextMenu(const QPoint&)));
#else /* !Q_WS_MAC */
    /* A simple connect doesn't work on the Mac, also we want receive right
     * click notifications on the title bar. So register our own handler. */
    ::darwinRegisterForUnifiedToolbarContextMenuEvents(this);
#endif /* Q_WS_MAC */

    /* VM desktop connections: */
    connect(m_pVMDesktop, SIGNAL(linkClicked(const QString &)), this, SLOT(sltShowMachineSettingsDialog(const QString &)));

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
        QString strWinPos = vbox.GetExtraData(VBoxDefs::GUI_LastWindowPosition);

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
            max = strWinPos.section(',', 4, 4) == VBoxDefs::GUI_LastWindowState_Max;

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

    /* Restore selected VM(s): */
    {
        QString strPrevVMId = vbox.GetExtraData(VBoxDefs::GUI_LastVMSelected);

        m_pVMListView->selectItemById(strPrevVMId);
    }

    /* Restore splitter handle position: */
    {
        QList<int> sizes = vbox.GetExtraDataIntList(VBoxDefs::GUI_SplitterSizes);

        if (sizes.size() == 2)
            m_pSplitter->setSizes(sizes);
    }

    /* Restore toolbar and statusbar visibility: */
    {
        QString strToolbar = vbox.GetExtraData(VBoxDefs::GUI_Toolbar);
        QString strStatusbar = vbox.GetExtraData(VBoxDefs::GUI_Statusbar);

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
            strWinPos += QString(",%1").arg(VBoxDefs::GUI_LastWindowState_Max);

        vbox.SetExtraData(VBoxDefs::GUI_LastWindowPosition, strWinPos);
    }

    /* Save selected VM(s): */
    {
        UIVMItem *pItem = m_pVMListView->currentItem();
        QString strCurrentVMId = pItem ? QString(pItem->id()) : QString();
        vbox.SetExtraData(VBoxDefs::GUI_LastVMSelected, strCurrentVMId);
        vbox.SetExtraDataStringList(VBoxDefs::GUI_SelectorVMPositions, m_pVMModel->idList());
    }

    /* Save splitter handle position: */
    {
        vbox.SetExtraDataIntList(VBoxDefs::GUI_SplitterSizes, m_pSplitter->sizes());
    }
}

bool UISelectorWindow::isActionEnabled(int iActionIndex, UIVMItem *pItem, const QList<UIVMItem*> &items)
{
    switch (iActionIndex)
    {
        case UIActionIndexSelector_Simple_Machine_SettingsDialog:
        {
            /* Check that there is only one item, its accessible
             * and machine is not in 'stuck' or 'saved' state.
             * Modifying VM settings in 'saved' state will be available later. */
            return items.size() == 1 &&
                   pItem && pItem->accessible() &&
                   pItem->machineState() != KMachineState_Stuck &&
                   pItem->machineState() != KMachineState_Saved;
        }
        case UIActionIndexSelector_Simple_Machine_CloneWizard:
        {
            /* Check that there is only one item, its accessible
             * and session state is unlocked. */
            return items.size() == 1 &&
                   pItem && pItem->accessible() &&
                   pItem->sessionState() == KSessionState_Unlocked;
        }
        case UIActionIndexSelector_Simple_Machine_RemoveDialog:
        {
            /* Check that item is present and
             * machine is not accessible or session state is unlocked. */
            return pItem &&
                   (!pItem->accessible() || pItem->sessionState() == KSessionState_Unlocked);
        }
        case UIActionIndexSelector_State_Machine_StartOrShow:
        {
            /* Check that item present and accessible: */
            if (!pItem || !pItem->accessible())
                return false;

            /* Check if we are in powered off mode which unifies next possible states.
             * Then if session state is unlocked we can allow to start VM. */
            if (pItem->machineState() == KMachineState_PoweredOff ||
                pItem->machineState() == KMachineState_Saved ||
                pItem->machineState() == KMachineState_Teleported ||
                pItem->machineState() == KMachineState_Aborted)
                return pItem->sessionState() == KSessionState_Unlocked;

            /* Otherwise we are in running mode and
             * should allow to switch to VM if its possible: */
            return pItem->canSwitchTo();
        }
        case UIActionIndexSelector_Simple_Machine_Discard:
        {
            /* Check that there is only one item, its accessible
             * and machine is in 'saved' state and session state is unlocked. */
            return items.size() == 1 &&
                   pItem && pItem->accessible() &&
                   pItem->machineState() == KMachineState_Saved &&
                   pItem->sessionState() == KSessionState_Unlocked;
        }
        case UIActionIndexSelector_Toggle_Machine_PauseAndResume:
        {
            /* Check that item present and accessible
             * and machine is in 'running' or 'paused' mode which unifies next possible states. */
            return pItem && pItem->accessible() &&
                   (pItem->machineState() == KMachineState_Running ||
                    pItem->machineState() == KMachineState_Teleporting ||
                    pItem->machineState() == KMachineState_LiveSnapshotting ||
                    pItem->machineState() == KMachineState_Paused ||
                    pItem->machineState() == KMachineState_TeleportingPausedVM);
        }
        case UIActionIndexSelector_Simple_Machine_Reset:
        {
            /* Check that item present and accessible
             * and machine is in 'running' mode which unifies next possible states. */
            return pItem && pItem->accessible() &&
                   (pItem->machineState() == KMachineState_Running ||
                    pItem->machineState() == KMachineState_Teleporting ||
                    pItem->machineState() == KMachineState_LiveSnapshotting);
        }
        case UIActionIndexSelector_Menu_Machine_Close:
        {
            /* Check that item present and accessible
             * and machine is in 'running' or 'paused' state. */
            return pItem && pItem->accessible() &&
                   (pItem->machineState() == KMachineState_Running ||
                    pItem->machineState() == KMachineState_Paused);
        }
        case UIActionIndexSelector_Simple_Machine_Close_ACPIShutdown:
        {
            /* Check that 'Machine/Close' menu is enabled: */
            if (!isActionEnabled(UIActionIndexSelector_Menu_Machine_Close, pItem, items))
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
            return isActionEnabled(UIActionIndexSelector_Menu_Machine_Close, pItem, items);
        }
        case UIActionIndexSelector_Simple_Machine_Refresh:
        {
            /* Check if item present and NOT accessible: */
            return pItem && !pItem->accessible();
        }
        case UIActionIndexSelector_Simple_Machine_LogDialog:
        case UIActionIndexSelector_Simple_Machine_ShowInFileManager:
        case UIActionIndexSelector_Simple_Machine_Sort:
        {
            /* Check if item present and accessible: */
            return pItem && pItem->accessible();
        }
        case UIActionIndexSelector_Simple_Machine_CreateShortcut:
        {
#ifdef Q_WS_MAC
            /* On Mac OS X this are real alias files, which don't work with the old
             * legacy xml files. On the other OS's some kind of start up script is used. */
            return pItem && pItem->accessible() &&
                   pItem->settingsFile().endsWith(".vbox", Qt::CaseInsensitive);
#else /* Q_WS_MAC */
            return pItem && pItem->accessible();
#endif /* Q_WS_MAC */
        }
        default:
            break;
    }
    return false;
}

