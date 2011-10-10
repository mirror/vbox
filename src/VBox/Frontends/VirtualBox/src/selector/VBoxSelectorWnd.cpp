/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxSelectorWnd class implementation
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
#include "QISplitter.h"
#include "UIBar.h"
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
#include "VBoxSelectorWnd.h"
#include "UISettingsDialogSpecific.h"
#include "UIToolBar.h"
#include "UIVMLogViewer.h"
#include "QIFileDialog.h"
#include "UISelectorShortcuts.h"
#include "UIDesktopServices.h"
#include "UIGlobalSettingsExtension.h" /* extension pack installation */
#include "UIActionPoolSelector.h"

#ifdef VBOX_GUI_WITH_SYSTRAY
# include "VBoxTrayIcon.h"
# include "UIExtraDataEventHandler.h"
#endif /* VBOX_GUI_WITH_SYSTRAY */

#ifdef Q_WS_MAC
# include "VBoxUtils.h"
# include "UIWindowMenuManager.h"
# include "UIImageTools.h"
#endif

/* Global includes */
#include <QDesktopWidget>
#include <QMenuBar>
#include <QResizeEvent>
#include <QDesktopServices>

#include <iprt/buildconfig.h>
#include <VBox/version.h>
#ifdef Q_WS_X11
# include <iprt/env.h>
#endif
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

// VBoxSelectorWnd class
////////////////////////////////////////////////////////////////////////////////

/** \class VBoxSelectorWnd
 *
 *  The VBoxSelectorWnd class is a VM selector window, one of two main VBox
 *  GUI windows.
 *
 *  This window appears when the user starts the VirtualBox executable.
 *  It allows to view the list of configured VMs, their settings
 *  and the current state, create, reconfigure, delete and start VMs.
 */

/**
 *  Constructs the VM selector window.
 *
 *  @param aSelf pointer to a variable where to store |this| right after
 *               this object's constructor is called (necessary to avoid
 *               recursion in VBoxGlobal::selectorWnd())
 */
VBoxSelectorWnd::
VBoxSelectorWnd(VBoxSelectorWnd **aSelf, QWidget* aParent,
                Qt::WindowFlags aFlags /* = Qt::Window */)
    : QIWithRetranslateUI2<QMainWindow>(aParent, aFlags)
    , mDoneInaccessibleWarningOnce(false)
{
    /* Remember self: */
    if (aSelf)
        *aSelf = this;

    /* The application icon: */
#if !(defined (Q_WS_WIN) || defined (Q_WS_MAC))
    /* On Win32, it's built-in to the executable.
     * On Mac OS X the icon referenced in info.plist is used. */
    setWindowIcon(QIcon(":/VirtualBox_48px.png"));
#endif

    /* Prepare menu: */
    prepareMenuBar();
    prepareContextMenu();

    /* Prepare status bar: */
    prepareStatusBar();

    /* Prepare tool bar: */
    prepareWidgets();

    /* Prepare connections: */
    prepareConnections();

#ifdef VBOX_GUI_WITH_SYSTRAY
    mTrayIcon = new VBoxTrayIcon(this, mVMModel);
    Assert(mTrayIcon);
#endif

    retranslateUi();

    CVirtualBox vbox = vboxGlobal().virtualBox();
    /* Restore the position of the window */
    {
        QString winPos = vbox.GetExtraData(VBoxDefs::GUI_LastWindowPosition);

        bool ok = false, max = false;
        int x = 0, y = 0, w = 0, h = 0;
        x = winPos.section(',', 0, 0).toInt(&ok);
        if (ok)
            y = winPos.section(',', 1, 1).toInt(&ok);
        if (ok)
            w = winPos.section(',', 2, 2).toInt(&ok);
        if (ok)
            h = winPos.section(',', 3, 3).toInt(&ok);
        if (ok)
            max = winPos.section(',', 4, 4) == VBoxDefs::GUI_LastWindowState_Max;

        QRect ar = ok ? QApplication::desktop()->availableGeometry(QPoint(x, y)) :
                        QApplication::desktop()->availableGeometry(this);

        if (ok /* previous parameters were read correctly */
            && (y > 0) && (y < ar.bottom()) /* check vertical bounds */
            && (x + w > ar.left()) && (x < ar.right()) /* & horizontal bounds */)
        {
            mNormalGeo.moveTo(x, y);
            mNormalGeo.setSize(QSize(w, h)
                               .expandedTo(minimumSizeHint())
                               .boundedTo(ar.size()));
#if defined(Q_WS_MAC) && (QT_VERSION >= 0x040700)
            move(mNormalGeo.topLeft());
            resize(mNormalGeo.size());
            mNormalGeo = normalGeometry();
#else /* defined(Q_WS_MAC) && (QT_VERSION >= 0x040700) */
            setGeometry(mNormalGeo);
#endif /* !(defined(Q_WS_MAC) && (QT_VERSION >= 0x040700)) */
            if (max) /* maximize if needed */
                showMaximized();
        }
        else
        {
            mNormalGeo.setSize(QSize(770, 550).expandedTo(minimumSizeHint())
                                              .boundedTo(ar.size()));
            mNormalGeo.moveCenter(ar.center());
            setGeometry(mNormalGeo);
        }
    }
#if MAC_LEOPARD_STYLE
    /* Enable unified toolbars on Mac OS X. Available on Qt >= 4.3. We do this
     * after setting the window pos/size, cause Qt sometime includes the
     * toolbar height in the content height. */
    mVMToolBar->setMacToolbar();
#endif /* MAC_LEOPARD_STYLE */

    /* Update the list */
    refreshVMList();

    /* Reset to the first item */
    mVMListView->selectItemByRow(0);

    /* restore the position of vm selector */
    {
        QString prevVMId = vbox.GetExtraData(VBoxDefs::GUI_LastVMSelected);

        mVMListView->selectItemById(prevVMId);
    }

    /* Read the splitter handle position */
    {
        QList<int> sizes = vbox.GetExtraDataIntList(VBoxDefs::GUI_SplitterSizes);
        if (sizes.size() == 2)
            m_pSplitter->setSizes(sizes);
    }

    /* Restore toolbar and statusbar visibility */
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

    /* refresh the details et all (necessary for the case when the stored
     * selection is still the first list item) */
    vmListViewCurrentChanged();

    /* bring the VM list to the focus */
    mVMListView->setFocus();

#ifdef Q_WS_MAC
    UIWindowMenuManager::instance()->addWindow(this);
    /* Beta label? */
    if (vboxGlobal().isBeta())
    {
        QPixmap betaLabel = ::betaLabelSleeve(QSize(107, 16));
        ::darwinLabelWindow(this, &betaLabel, false);
    }
#endif /* Q_WS_MAC */

#ifdef Q_WS_MAC
    /* General event filter: */
    qApp->installEventFilter(this);
#endif /* Q_WS_MAC */
}

VBoxSelectorWnd::~VBoxSelectorWnd()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();

    /* Destroy our event handlers */
    UIVirtualBoxEventHandler::destroy();

    /* Save the position of the window */
    {
#if defined(Q_WS_MAC) && (QT_VERSION >= 0x040700)
        QRect frameGeo = frameGeometry();
        QRect save(frameGeo.x(), frameGeo.y(), mNormalGeo.width(), mNormalGeo.height());
#else /* defined(Q_WS_MAC) && (QT_VERSION >= 0x040700) */
        QRect save(mNormalGeo);
#endif /* !(defined(Q_WS_MAC) && (QT_VERSION >= 0x040700)) */
        QString winPos = QString("%1,%2,%3,%4")
            .arg(save.x()).arg(save.y())
            .arg(save.width()).arg(save.height());
#ifdef Q_WS_MAC
        UIWindowMenuManager::destroy();
        ::darwinUnregisterForUnifiedToolbarContextMenuEvents(this);
        if (::darwinIsWindowMaximized(this))
#else /* Q_WS_MAC */
        if (isMaximized())
#endif /* !Q_WS_MAC */
            winPos += QString(",%1").arg(VBoxDefs::GUI_LastWindowState_Max);

        vbox.SetExtraData(VBoxDefs::GUI_LastWindowPosition, winPos);
    }

    /* Save VM selector position */
    {
        UIVMItem *item = mVMListView->selectedItem();
        QString curVMId = item ?
            QString(item->id()) :
            QString::null;
        vbox.SetExtraData(VBoxDefs::GUI_LastVMSelected, curVMId);
        vbox.SetExtraDataStringList(VBoxDefs::GUI_SelectorVMPositions, mVMModel->idList());
    }

    /* Save the splitter handle position */
    {
        vbox.SetExtraDataIntList(VBoxDefs::GUI_SplitterSizes, m_pSplitter->sizes());
    }

#ifdef VBOX_GUI_WITH_SYSTRAY
    /* Delete systray menu object */
    delete mTrayIcon;
    mTrayIcon = NULL;
#endif

    /* Delete the items from our model */
    mVMModel->clear();
}

//
// Public slots
/////////////////////////////////////////////////////////////////////////////

void VBoxSelectorWnd::sltShowMediumManager()
{
    VBoxMediaManagerDlg::showModeless(this);
}

void VBoxSelectorWnd::sltShowImportApplianceWizard(const QString &strFile /* = "" */)
{
#ifdef Q_WS_MAC
    QString strTmpFile = ::darwinResolveAlias(strFile);
#else /* Q_WS_MAC */
    QString strTmpFile = strFile;
#endif /* !Q_WS_MAC */
    UIImportApplianceWzd wzd(strTmpFile, this);

    if (   strFile.isEmpty()
        || wzd.isValid())
        wzd.exec();
}

void VBoxSelectorWnd::sltShowExportApplianceWizard()
{
    QString name;

    UIVMItem *item = mVMListView->selectedItem();
    if (item)
        name = item->name();

    UIExportApplianceWzd wzd(this, name);

    wzd.exec();
}

void VBoxSelectorWnd::sltShowPreferencesDialog()
{
    /* Get corresponding action: */
    QAction *pShowPreferencesDialogAction = gActionPool->action(UIActionIndexSelector_Simple_File_PreferencesDialog);

    /* Check that we do NOT handling that already: */
    if (pShowPreferencesDialogAction->data().toBool())
        return;
    /* Remember that we handling that already: */
    pShowPreferencesDialogAction->setData(true);

    /* Create and execute global settings dialog: */
    UISettingsDialogGlobal dlg(this);
    dlg.execute();

    /* Remember that we do NOT handling that already: */
    pShowPreferencesDialogAction->setData(false);
}

void VBoxSelectorWnd::sltPerformExit()
{
    /* We have to check if there are any open windows beside this mainwindow
     * (e.g. VDM) and if so close them. Note that the default behavior is
     * different to Qt3 where a *mainWidget* exists & if this going to close
     * all other windows are closed automatically. We do the same below. */
    foreach(QWidget *widget, QApplication::topLevelWidgets())
    {
        if (widget->isVisible() &&
            widget != this)
            widget->close();
    }
    /* We close this widget last. */
    close();
}

void VBoxSelectorWnd::sltShowNewMachineWizard()
{
    UINewVMWzd wzd(this);
    if (wzd.exec() == QDialog::Accepted)
    {
        CMachine m = wzd.machine();

        /* wait until the list is updated by OnMachineRegistered() */
        QModelIndex index;
        while(!index.isValid())
        {
            qApp->processEvents();
            index = mVMModel->indexById(m.GetId());
        }
        mVMListView->setCurrentIndex(index);
    }
}

void VBoxSelectorWnd::sltShowAddMachineDialog(const QString &strFile /* = "" */)
{
#ifdef Q_WS_MAC
    QString strTmpFile = ::darwinResolveAlias(strFile);
#else /* Q_WS_MAC */
    QString strTmpFile = strFile;
#endif /* !Q_WS_MAC */
    /* Initialize variables: */
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
    if (!strTmpFile.isEmpty())
    {
        /* Open corresponding machine: */
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
                    index = mVMModel->indexById(newMachine.GetId());
                }
                /* And choose the new machine item: */
                mVMListView->setCurrentIndex(index);
            }
            else
                msgCenter().cannotReregisterMachine(this, strTmpFile, oldMachine.GetName());
        }
        else
            msgCenter().cannotOpenMachine(this, strTmpFile, vbox);
    }
}

/**
 *  Opens the VM settings dialog.
 */
void VBoxSelectorWnd::sltShowMachineSettingsDialog(const QString &strCategoryRef /* = QString::null */,
                                                   const QString &strControlRef /* = QString::null */,
                                                   const QString &strMachineId /* = QString::null */)
{
    /* Get corresponding action: */
    QAction *pShowSettingsDialogAction = gActionPool->action(UIActionIndexSelector_Simple_Machine_SettingsDialog);

    /* Check that we do NOT handling that already: */
    if (pShowSettingsDialogAction->data().toBool())
        return;
    /* Remember that we handling that already: */
    pShowSettingsDialogAction->setData(true);

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
    mDoneInaccessibleWarningOnce = true;

    /* Get corresponding VM item: */
    UIVMItem *pItem = strMachineId.isNull() ? mVMListView->selectedItem() : mVMModel->itemById(strMachineId);
    AssertMsgReturnVoid(pItem, ("Item must be always selected here!\n"));

    /* Create and execute corresponding VM settings dialog: */
    UISettingsDialogMachine dlg(this, pItem->id(), strCategory, strControl);
    dlg.execute();

    /* Remember that we do NOT handling that already: */
    pShowSettingsDialogAction->setData(false);
}

void VBoxSelectorWnd::sltShowCloneMachineDialog(const QString &aUuid /* = QString::null */)
{
    UIVMItem *item = aUuid.isNull() ? mVMListView->selectedItem() : mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    CMachine machine = item->machine();

    UICloneVMWizard wzd(this, machine);
    wzd.exec();
}

void VBoxSelectorWnd::sltShowRemoveMachineDialog(const QString &aUuid /* = QString::null */)
{
    UIVMItem *item = aUuid.isNull() ? mVMListView->selectedItem() : mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    CMachine machine = item->machine();
    int rc = msgCenter().confirmMachineDeletion(machine);
    if (rc != QIMessageBox::Cancel)
    {
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
                    msgCenter().showModalProgressDialog(progress, item->name(), ":/progress_delete_90px.png", 0, true);
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

void VBoxSelectorWnd::sltPerformStartOrShowAction(const QString &aUuid /* = QString::null */)
{
    QUuid uuid(aUuid);
    UIVMItem *item = uuid.isNull() ? mVMListView->selectedItem() :
                       mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    /* Are we called from the mVMListView's activated() signal? */
    if (uuid.isNull())
    {
        /* We always get here when mVMListView emits the activated() signal,
         * so we must explicitly check if the action is enabled or not. */
        if (!gActionPool->action(UIActionIndexSelector_State_Machine_StartOrShow)->isEnabled())
            return;
    }

    AssertMsg(!vboxGlobal().isVMConsoleProcess(),
              ("Must NOT be a VM console process"));

    CMachine machine = item->machine();
    vboxGlobal().launchMachine(machine, qApp->keyboardModifiers() == Qt::ShiftModifier);
}

void VBoxSelectorWnd::sltPerformDiscardAction(const QString &aUuid /* = QString::null */)
{
    UIVMItem *item = aUuid.isNull() ? mVMListView->selectedItem() :
                       mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    if (!msgCenter().confirmDiscardSavedState(item->machine()))
        return;

    /* open a session to modify VM settings */
    QString id = item->id();
    CSession session;
    CVirtualBox vbox = vboxGlobal().virtualBox();
    session.createInstance(CLSID_Session);
    if (session.isNull())
    {
        msgCenter().cannotOpenSession(session);
        return;
    }

    CMachine foundMachine = vbox.FindMachine(id);
    if (!foundMachine.isNull())
        foundMachine.LockMachine(session, KLockType_Write);
    if (!vbox.isOk())
    {
        msgCenter().cannotOpenSession(vbox, item->machine());
        return;
    }

    CConsole console = session.GetConsole();
    console.DiscardSavedState(true /* fDeleteFile */);
    if (!console.isOk())
        msgCenter().cannotDiscardSavedState(console);

    session.UnlockMachine();
}

void VBoxSelectorWnd::sltPerformPauseResumeAction(bool aPause, const QString &aUuid /* = QString::null */)
{
    UIVMItem *item = aUuid.isNull() ? mVMListView->selectedItem() :
                       mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    CSession session = vboxGlobal().openExistingSession(item->id());
    if (session.isNull())
        return;

    CConsole console = session.GetConsole();
    if (console.isNull())
        return;

    if (aPause)
        console.Pause();
    else
        console.Resume();

    bool ok = console.isOk();
    if (!ok)
    {
        if (aPause)
            msgCenter().cannotPauseMachine(console);
        else
            msgCenter().cannotResumeMachine(console);
    }

    session.UnlockMachine();
}

void VBoxSelectorWnd::sltPerformResetAction(const QString &aUuid /* = QString::null */)
{
    UIVMItem *item = aUuid.isNull() ? mVMListView->selectedItem() :
                       mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    /* Confirm/Reset current console: */
    if (msgCenter().confirmVMReset(this))
    {
        CSession session = vboxGlobal().openExistingSession(item->id());
        if (session.isNull())
            return;

        CConsole console = session.GetConsole();
        if (console.isNull())
            return;

        console.Reset();
    }
}

void VBoxSelectorWnd::sltPerformACPIShutdownAction(const QString &aUuid /* = QString::null */)
{
    UIVMItem *item = aUuid.isNull() ? mVMListView->selectedItem() :
                       mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    /* Confirm/Reset current console: */
    if (msgCenter().confirmVMReset(this))
    {
        CSession session = vboxGlobal().openExistingSession(item->id());
        if (session.isNull())
            return;

        CConsole console = session.GetConsole();
        if (console.isNull())
            return;

        console.PowerButton();

        session.UnlockMachine();
    }
}

void VBoxSelectorWnd::sltPerformPowerOffAction(const QString &aUuid /* = QString::null */)
{
    UIVMItem *item = aUuid.isNull() ? mVMListView->selectedItem() :
                       mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    /* Confirm/Reset current console: */
    if (msgCenter().confirmVMPowerOff(this))
    {
        CSession session = vboxGlobal().openExistingSession(item->id());
        if (session.isNull())
            return;

        CConsole console = session.GetConsole();
        if (console.isNull())
            return;

        console.PowerDown();

        session.UnlockMachine();
    }
}

void VBoxSelectorWnd::sltPerformRefreshAction(const QString &aUuid /* = QString::null */)
{
    UIVMItem *item = aUuid.isNull() ? mVMListView->selectedItem() :
                       mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    refreshVMItem(item->id(),
                   true /* aDetails */,
                   true /* aSnapshot */,
                   true /* aDescription */);
}

void VBoxSelectorWnd::sltShowLogDialog(const QString &aUuid /* = QString::null */)
{
    UIVMItem *item = aUuid.isNull() ? mVMListView->selectedItem() :
                       mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    CMachine machine = item->machine();
    UIVMLogViewer::showLogViewerFor(this, machine);
}

void VBoxSelectorWnd::sltShowMachineInFileManager(const QString &aUuid /* = QString::null */)
{
    UIVMItem *item = aUuid.isNull() ? mVMListView->selectedItem() :
                       mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    CMachine machine = item->machine();
    UIDesktopServices::openInFileManager(machine.GetSettingsFilePath());
}

void VBoxSelectorWnd::sltPerformCreateShortcutAction(const QString &aUuid /* = QString::null */)
{
    UIVMItem *item = aUuid.isNull() ? mVMListView->selectedItem() :
                       mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    CMachine machine = item->machine();
    UIDesktopServices::createMachineShortcut(machine.GetSettingsFilePath(), QDesktopServices::storageLocation(QDesktopServices::DesktopLocation), machine.GetName(), machine.GetId());
}

void VBoxSelectorWnd::sltPerformSortAction(const QString &aUuid /* = QString::null */)
{
    UIVMItem *item = aUuid.isNull() ? mVMListView->selectedItem() :
                       mVMModel->itemById(aUuid);
    const QString oldId = item->id();

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    mVMModel->sortByIdList(QStringList(),
                           qApp->keyboardModifiers() == Qt::ShiftModifier ? Qt::DescendingOrder : Qt::AscendingOrder);
    /* Select the VM which was selected before */
    mVMListView->selectItemById(oldId);
}

void VBoxSelectorWnd::sltMachineMenuAboutToShow()
{
    UIVMItem *pItem = mVMListView->selectedItem();
    AssertMsgReturnVoid(pItem, ("Item must be always selected here"));

    /* Check if we are in 'running' or 'paused' mode.
     * Only then it make sense to allow to close running VM. */
    bool fIsOnline = pItem->machineState() == KMachineState_Running ||
                     pItem->machineState() == KMachineState_Paused;
    gActionPool->action(UIActionIndexSelector_Menu_Machine_Close)->setEnabled(fIsOnline);
}

void VBoxSelectorWnd::sltMachineCloseMenuAboutToShow()
{
    UIVMItem *pItem = mVMListView->selectedItem();
    AssertMsgReturnVoid(pItem, ("Item must be always selected here"));

    /* Check if we are entered ACPI mode already.
     * Only then it make sense to send the ACPI shutdown sequence. */
    bool fHasACPIMode = false; /* Default is off */
    CSession session = vboxGlobal().openExistingSession(pItem->id());
    if (!session.isNull())
    {
        CConsole console = session.GetConsole();
        if (!console.isNull())
            fHasACPIMode = console.GetGuestEnteredACPIMode();

        session.UnlockMachine();
    }

    gActionPool->action(UIActionIndexSelector_Simple_Machine_Close_ACPIShutdown)->setEnabled(fHasACPIMode);
}

void VBoxSelectorWnd::sltMachineContextMenuHovered(QAction *pAction)
{
    statusBar()->showMessage(pAction->statusTip());
}

void VBoxSelectorWnd::refreshVMList()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CMachineVector vec = vbox.GetMachines();
    for (CMachineVector::ConstIterator m = vec.begin();
         m != vec.end(); ++m)
        mVMModel->addItem(*m);
    /* Apply the saved sort order. */
    mVMModel->sortByIdList(vbox.GetExtraDataStringList(VBoxDefs::GUI_SelectorVMPositions));
    /* Update details page. */
    vmListViewCurrentChanged();

#ifdef VBOX_GUI_WITH_SYSTRAY
    if (vboxGlobal().isTrayMenu())
        mTrayIcon->refresh();
#endif
}

void VBoxSelectorWnd::refreshVMItem(const QString &aID,
                                    bool aDetails,
                                    bool aSnapshots,
                                    bool aDescription)
{
    UIVMItem *item = mVMModel->itemById(aID);
    if (item)
    {
        mVMModel->refreshItem(item);
        if (item && item->id() == aID)
            vmListViewCurrentChanged(aDetails, aSnapshots, aDescription);
    }
}

void VBoxSelectorWnd::showContextMenu(const QPoint &aPoint)
{
    const QModelIndex &index = mVMListView->indexAt(aPoint);
    if (index.isValid() && mVMListView->model()->data(index, UIVMItemModel::UIVMItemPtrRole).value<UIVMItem*>())
    {
        /* Show 'Machine' menu as context one: */
        m_pMachineContextMenu->exec(mVMListView->mapToGlobal(aPoint));
        /* Make sure every status bar hint from the context menu is cleared when the menu is closed: */
        statusBar()->clearMessage();
    }
}

void VBoxSelectorWnd::sltOpenUrls(QList<QUrl> list /* = QList<QUrl>() */)
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
        if (   !strFile.isEmpty()
            && QFile::exists(strFile))
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
                }else
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
                UIGlobalSettingsExtension::doInstallation(strFile, this, NULL);
            }
        }
    }
}

#ifdef VBOX_GUI_WITH_SYSTRAY

void VBoxSelectorWnd::trayIconActivated(QSystemTrayIcon::ActivationReason aReason)
{
    switch (aReason)
    {
        case QSystemTrayIcon::Context:

            mTrayIcon->refresh();
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

void VBoxSelectorWnd::showWindow()
{
    showNormal();
    raise();
    activateWindow();
}

#endif // VBOX_GUI_WITH_SYSTRAY

// Protected members
/////////////////////////////////////////////////////////////////////////////

bool VBoxSelectorWnd::event(QEvent *e)
{
    switch (e->type())
    {
        /* By handling every Resize and Move we keep track of the normal
         * (non-minimized and non-maximized) window geometry. Shame on Qt
         * that it doesn't provide this geometry in its public APIs. */

        case QEvent::Resize:
        {
            QResizeEvent *re = (QResizeEvent *) e;
            if ((windowState() & (Qt::WindowMaximized | Qt::WindowMinimized |
                                  Qt::WindowFullScreen)) == 0)
                mNormalGeo.setSize(re->size());
            break;
        }
        case QEvent::Move:
        {
            if ((windowState() & (Qt::WindowMaximized | Qt::WindowMinimized |
                                  Qt::WindowFullScreen)) == 0)
                mNormalGeo.moveTo(geometry().x(), geometry().y());
            break;
        }
        case QEvent::WindowDeactivate:
        {
            /* Make sure every status bar hint is cleared when the window lost
             * focus. */
            statusBar()->clearMessage();
            break;
        }
#ifdef Q_WS_MAC
        case QEvent::ContextMenu:
        {
            /* This is the unified context menu event. Lets show the context
             * menu. */
            QContextMenuEvent *pCE = static_cast<QContextMenuEvent*>(e);
            showViewContextMenu(pCE->globalPos());
            /* Accept it to interrupt the chain. */
            pCE->accept();
            return false;
            break;
        }
        case QEvent::ToolBarChange:
        {
            CVirtualBox vbox = vboxGlobal().virtualBox();
            /* We have to invert the isVisible check one time, cause this event
             * is sent *before* the real toggle is done. Really intuitive
             * Trolls. */
            vbox.SetExtraData(VBoxDefs::GUI_Toolbar, !::darwinIsToolbarVisible(mVMToolBar) ? "true" : "false");
            break;
        }
#endif /* Q_WS_MAC */
        default:
            break;
    }

    return QMainWindow::event(e);
}

void VBoxSelectorWnd::closeEvent(QCloseEvent *aEvent)
{
#ifdef VBOX_GUI_WITH_SYSTRAY
    /* Needed for breaking out of the while() loop in main(). */
    if (vboxGlobal().isTrayMenu())
        vboxGlobal().setTrayMenu(false);
#endif

    emit closing();
    QMainWindow::closeEvent(aEvent);
}

#ifdef Q_WS_MAC
bool VBoxSelectorWnd::eventFilter(QObject *pObject, QEvent *pEvent)
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

/**
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void VBoxSelectorWnd::retranslateUi()
{
    QString title(VBOX_PRODUCT);
    title += " " + tr("Manager", "Note: main window title which is pretended by the product name.");

#ifdef VBOX_BLEEDING_EDGE
    title += QString(" EXPERIMENTAL build ")
          +  QString(RTBldCfgVersion())
          +  QString(" r")
          +  QString(RTBldCfgRevisionStr())
          +  QString(" - "VBOX_BLEEDING_EDGE);
#endif

    setWindowTitle(title);

    /* ensure the details and screenshot view are updated */
    vmListViewCurrentChanged();

#ifdef VBOX_GUI_WITH_SYSTRAY
    if (vboxGlobal().isTrayMenu())
    {
        mTrayIcon->retranslateUi();
        mTrayIcon->refresh();
    }
#endif

#ifdef QT_MAC_USE_COCOA
    /* There is a bug in Qt Cocoa which result in showing a "more arrow" when
       the necessary size of the toolbar is increased. Also for some languages
       the with doesn't match if the text increase. So manually adjust the size
       after changing the text. */
    mVMToolBar->updateLayout();
#endif /* QT_MAC_USE_COCOA */
}


// Private members
/////////////////////////////////////////////////////////////////////////////

//
// Private slots
/////////////////////////////////////////////////////////////////////////////

void VBoxSelectorWnd::vmListViewCurrentChanged(bool aRefreshDetails,
                                               bool aRefreshSnapshots,
                                               bool aRefreshDescription)
{
    UIVMItem *item = mVMListView->selectedItem();
    UIActionInterface *pStartOrShowAction = gActionPool->action(UIActionIndexSelector_State_Machine_StartOrShow);

    if (item && item->accessible())
    {
        CMachine m = item->machine();

        KMachineState state = item->machineState();
        bool fSessionLocked = item->sessionState() != KSessionState_Unlocked;
        bool fModifyEnabled = state != KMachineState_Stuck &&
                              state != KMachineState_Saved /* for now! */;
        bool fRunning       = state == KMachineState_Running ||
                              state == KMachineState_Teleporting ||
                              state == KMachineState_LiveSnapshotting;
        bool fPaused        = state == KMachineState_Paused ||
                              state == KMachineState_TeleportingPausedVM; /** @todo Live Migration: does this make sense? */

        if (   aRefreshDetails
            || aRefreshDescription)
            m_pVMDesktop->updateDetails(item, m);
        if (aRefreshSnapshots)
            m_pVMDesktop->updateSnapshots(item, m);
//        if (aRefreshDescription)
//            m_pVMDesktop->updateDescription(item, m);

        /* enable/disable modify actions */
        gActionPool->action(UIActionIndexSelector_Simple_Machine_SettingsDialog)->setEnabled(fModifyEnabled);
        gActionPool->action(UIActionIndexSelector_Simple_Machine_CloneWizard)->setEnabled(!fSessionLocked);
        gActionPool->action(UIActionIndexSelector_Simple_Machine_RemoveDialog)->setEnabled(!fSessionLocked);
        gActionPool->action(UIActionIndexSelector_Simple_Machine_Discard)->setEnabled(state == KMachineState_Saved && !fSessionLocked);
        gActionPool->action(UIActionIndexSelector_Toggle_Machine_PauseAndResume)->setEnabled(fRunning || fPaused);
        gActionPool->action(UIActionIndexSelector_Simple_Machine_Reset)->setEnabled(fRunning);
        gActionPool->action(UIActionIndexSelector_Simple_Machine_Close_ACPIShutdown)->setEnabled(fRunning);
        gActionPool->action(UIActionIndexSelector_Simple_Machine_Close_PowerOff)->setEnabled(fRunning || fPaused);

        /* change the Start button text accordingly */
        if (   state == KMachineState_PoweredOff
            || state == KMachineState_Saved
            || state == KMachineState_Teleported
            || state == KMachineState_Aborted
           )
        {
            pStartOrShowAction->setState(1);
            pStartOrShowAction->setEnabled(!fSessionLocked);
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
            pStartOrShowAction->setState(2);
            pStartOrShowAction->setEnabled(item->canSwitchTo());
#ifdef QT_MAC_USE_COCOA
            /* There is a bug in Qt Cocoa which result in showing a "more arrow" when
               the necessary size of the toolbar is increased. Also for some languages
               the with doesn't match if the text increase. So manually adjust the size
               after changing the text. */
            mVMToolBar->updateLayout();
#endif /* QT_MAC_USE_COCOA */
        }

        /* Update the Pause/Resume action appearance: */
        if (state == KMachineState_Paused || state == KMachineState_TeleportingPausedVM)
        {
            gActionPool->action(UIActionIndexSelector_Toggle_Machine_PauseAndResume)->blockSignals(true);
            gActionPool->action(UIActionIndexSelector_Toggle_Machine_PauseAndResume)->setChecked(true);
            gActionPool->action(UIActionIndexSelector_Toggle_Machine_PauseAndResume)->blockSignals(false);
        }
        else
        {
            gActionPool->action(UIActionIndexSelector_Toggle_Machine_PauseAndResume)->blockSignals(true);
            gActionPool->action(UIActionIndexSelector_Toggle_Machine_PauseAndResume)->setChecked(false);
            gActionPool->action(UIActionIndexSelector_Toggle_Machine_PauseAndResume)->blockSignals(false);
        }
        gActionPool->action(UIActionIndexSelector_Toggle_Machine_PauseAndResume)->updateAppearance();

        /* disable Refresh for accessible machines */
        gActionPool->action(UIActionIndexSelector_Simple_Machine_Refresh)->setEnabled(false);

        /* enable the show log item for the selected vm */
        gActionPool->action(UIActionIndexSelector_Simple_Machine_LogDialog)->setEnabled(true);
        /* Enable the shell interaction features. */
        gActionPool->action(UIActionIndexSelector_Simple_Machine_ShowInFileManager)->setEnabled(true);
#ifdef Q_WS_MAC
        /* On Mac OS X this are real alias files, which don't work with the old
         * legacy xml files. On the other OS's some kind of start up script is
         * used. */
        gActionPool->action(UIActionIndexSelector_Simple_Machine_CreateShortcut)->setEnabled(item->settingsFile().endsWith(".vbox", Qt::CaseInsensitive));
#else /* Q_WS_MAC */
        gActionPool->action(UIActionIndexSelector_Simple_Machine_CreateShortcut)->setEnabled(true);
#endif /* Q_WS_MAC */
        gActionPool->action(UIActionIndexSelector_Simple_Machine_Sort)->setEnabled(true);
    }
    else
    {
        /* Note that the machine becomes inaccessible (or if the last VM gets
         * deleted), we have to update all fields, ignoring input
         * arguments. */

        if (item)
        {
            /* the VM is inaccessible */
            m_pVMDesktop->updateDetailsErrorText(UIMessageCenter::formatErrorInfo(item->accessError()));
            gActionPool->action(UIActionIndexSelector_Simple_Machine_Refresh)->setEnabled(true);
        }
        else
        {
            /* default HTML support in Qt is terrible so just try to get
             * something really simple */
            m_pVMDesktop->updateDetailsText(
                tr("<h3>"
                   "Welcome to VirtualBox!</h3>"
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
                   "for the latest information and news.</p>").arg(QKeySequence(QKeySequence::HelpContents).toString(QKeySequence::NativeText)));
            gActionPool->action(UIActionIndexSelector_Simple_Machine_Refresh)->setEnabled(false);
        }

        /* empty and disable other tabs */
        m_pVMDesktop->updateSnapshots(0, CMachine());
//        m_pVMDesktop->updateDescription(0, CMachine());

        /* Disable modify actions: */
        gActionPool->action(UIActionIndexSelector_Simple_Machine_SettingsDialog)->setEnabled(false);
        gActionPool->action(UIActionIndexSelector_Simple_Machine_CloneWizard)->setEnabled(false);
        gActionPool->action(UIActionIndexSelector_Simple_Machine_RemoveDialog)->setEnabled(item != NULL);
        gActionPool->action(UIActionIndexSelector_Simple_Machine_Discard)->setEnabled(false);
        gActionPool->action(UIActionIndexSelector_Toggle_Machine_PauseAndResume)->setEnabled(false);
        gActionPool->action(UIActionIndexSelector_Simple_Machine_Reset)->setEnabled(false);
        gActionPool->action(UIActionIndexSelector_Simple_Machine_Close_ACPIShutdown)->setEnabled(false);
        gActionPool->action(UIActionIndexSelector_Simple_Machine_Close_PowerOff)->setEnabled(false);

        /* Change the Start button text accordingly: */
        pStartOrShowAction->setState(1);
        pStartOrShowAction->setEnabled(false);

        /* Disable the show log item for the selected vm: */
        gActionPool->action(UIActionIndexSelector_Simple_Machine_LogDialog)->setEnabled(false);
        /* Disable the shell interaction features: */
        gActionPool->action(UIActionIndexSelector_Simple_Machine_ShowInFileManager)->setEnabled(false);
        gActionPool->action(UIActionIndexSelector_Simple_Machine_CreateShortcut)->setEnabled(false);
        /* Disable sorting if there is nothing to sort: */
        gActionPool->action(UIActionIndexSelector_Simple_Machine_Sort)->setEnabled(false);
    }
}

void VBoxSelectorWnd::mediumEnumStarted()
{
    /* refresh the current details to pick up hard disk sizes */
    vmListViewCurrentChanged(true /* aRefreshDetails */);
}

void VBoxSelectorWnd::mediumEnumFinished(const VBoxMediaList &list)
{
    /* refresh the current details to pick up hard disk sizes */
    vmListViewCurrentChanged(true /* aRefreshDetails */);

    /* we warn about inaccessible media only once (after media emumeration
     * started from main() at startup), to avoid annoying the user */
    if (   mDoneInaccessibleWarningOnce
#ifdef VBOX_GUI_WITH_SYSTRAY
        || vboxGlobal().isTrayMenu()
#endif
       )
        return;

    mDoneInaccessibleWarningOnce = true;

    do
    {
        /* ignore the signal if a modal widget is currently active (we won't be
         * able to properly show the modeless VDI manager window in this case) */
        if (QApplication::activeModalWidget())
            break;

        /* ignore the signal if a VBoxMediaManagerDlg window is active */
        if (qApp->activeWindow() &&
            !strcmp(qApp->activeWindow()->metaObject()->className(), "VBoxMediaManagerDlg"))
            break;

        /* look for at least one inaccessible media */
        VBoxMediaList::const_iterator it;
        for (it = list.begin(); it != list.end(); ++ it)
            if ((*it).state() == KMediumState_Inaccessible)
                break;

        if (it != list.end() && msgCenter().remindAboutInaccessibleMedia())
        {
            /* Show the VDM dialog but don't refresh once more after a
             * just-finished refresh */
            VBoxMediaManagerDlg::showModeless(this, false /* aRefresh */);
        }
    }
    while (0);
}

void VBoxSelectorWnd::machineStateChanged(QString strId, KMachineState /* state */)
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
#endif

    refreshVMItem(strId,
                  false /* aDetails */,
                  false /* aSnapshots */,
                  false /* aDescription */);

    /* simulate a state change signal */
//    m_pVMDesktop->updateDescriptionState();
}

void VBoxSelectorWnd::machineDataChanged(QString strId)
{
    refreshVMItem(strId,
                  true  /* aDetails */,
                  false /* aSnapshots */,
                  true  /* aDescription */);
}

void VBoxSelectorWnd::machineRegistered(QString strId, bool fRegistered)
{
    if (fRegistered)
    {
        CVirtualBox vbox = vboxGlobal().virtualBox();
        CMachine m = vbox.FindMachine(strId);
        if (!m.isNull())
        {
            mVMModel->addItem(m);
            /* Make sure the description, ... pages are properly updated.
             * Actually we haven't call the next method, but unfortunately Qt
             * seems buggy if the new item is on the same position as the
             * previous one. So go on the safe side and call this by our self. */
            vmListViewCurrentChanged();
        }
        /* m.isNull() is ok (theoretically, the machine could have been
         * already deregistered by some other client at this point) */
    }
    else
    {
        UIVMItem *item = mVMModel->itemById(strId);
        if (item)
        {
            int row = mVMModel->rowById(item->id());
            mVMModel->removeItem(item);
            mVMListView->ensureSomeRowSelected(row);
        }

        /* item = 0 is ok (if we originated this event then the item
         * has been already removed) */
    }
}

void VBoxSelectorWnd::sessionStateChanged(QString strId, KSessionState /* state */)
{
    refreshVMItem(strId,
                  true  /* aDetails */,
                  false /* aSnapshots */,
                  false /* aDescription */);

    /* simulate a state change signal */
//    m_pVMDesktop->updateDescriptionState();
}

void VBoxSelectorWnd::snapshotChanged(QString strId, QString /* strSnapshotId */)
{
    refreshVMItem(strId,
                  false /* aDetails */,
                  true  /* aSnapshot */,
                  false /* aDescription */);
}

#ifdef VBOX_GUI_WITH_SYSTRAY

void VBoxSelectorWnd::mainWindowCountChanged(int count)
{
    if (vboxGlobal().isTrayMenu() && count <= 1)
        sltPerformExit();
}

void VBoxSelectorWnd::trayIconCanShow(bool fEnabled)
{
    emit trayIconChanged(VBoxChangeTrayIconEvent(vboxGlobal().settings().trayIconEnabled()));
}

void VBoxSelectorWnd::trayIconShow(bool fEnabled)
{
    if (vboxGlobal().isTrayMenu() && mTrayIcon)
        mTrayIcon->trayIconShow(fEnabled);
}

void VBoxSelectorWnd::trayIconChanged(bool fEnabled)
{
    /* Not used yet. */
}

#endif /* VBOX_GUI_WITH_SYSTRAY */

void VBoxSelectorWnd::sltEmbedDownloaderForUserManual()
{
    /* If there is User Manual downloader created => show the process bar: */
    if (UIDownloaderUserManual *pDl = UIDownloaderUserManual::current())
        statusBar()->addWidget(pDl->progressWidget(this), 0);
}

void VBoxSelectorWnd::sltEmbedDownloaderForExtensionPack()
{
    /* If there is Extension Pack downloader created => show the process bar: */
    if (UIDownloaderExtensionPack *pDl = UIDownloaderExtensionPack::current())
        statusBar()->addWidget(pDl->progressWidget(this), 0);
}

void VBoxSelectorWnd::showViewContextMenu(const QPoint &pos)
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QString strToolbar = vbox.GetExtraData(VBoxDefs::GUI_Toolbar);
    QString strStatusbar = vbox.GetExtraData(VBoxDefs::GUI_Statusbar);
    bool fToolbar = strToolbar.isEmpty() || strToolbar == "true";
    bool fStatusbar = strStatusbar.isEmpty() || strStatusbar == "true";

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
    }else if (pResult == pShowStatusBar)
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

void VBoxSelectorWnd::prepareMenuBar()
{
    /* Prepare 'file' menu: */
    QMenu *pFileMenu = gActionPool->action(UIActionIndexSelector_Menu_File)->menu();
    prepareMenuFile(pFileMenu);
    menuBar()->addMenu(pFileMenu);

    /* Prepare 'machine' menu: */
    QMenu *pMachineMenu = gActionPool->action(UIActionIndexSelector_Menu_Machine)->menu();
    prepareMenuMachine(pMachineMenu);
    menuBar()->addMenu(pMachineMenu);

#ifdef Q_WS_MAC
    menuBar()->addMenu(UIWindowMenuManager::instance(this)->createMenu(this));
#endif /* Q_WS_MAC */

    /* Prepare help menu: */
    QMenu *pHelpMenu = gActionPool->action(UIActionIndex_Menu_Help)->menu();
    prepareMenuHelp(pHelpMenu);
    menuBar()->addMenu(pHelpMenu);

    menuBar()->setContextMenuPolicy(Qt::CustomContextMenu);
}

void VBoxSelectorWnd::prepareMenuFile(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

    /* Populate 'file' menu: */
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_File_MediumManagerDialog));
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_File_ImportApplianceWizard));
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_File_ExportApplianceWizard));
#ifndef Q_WS_MAC
    pMenu->addSeparator();
#endif /* Q_WS_MAC */
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_File_PreferencesDialog));
#ifndef Q_WS_MAC
    pMenu->addSeparator();
#endif /* Q_WS_MAC */
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_File_Exit));
}

void VBoxSelectorWnd::prepareMenuMachine(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

    /* Populate 'machine' menu: */
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_NewWizard));
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_AddDialog));
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_SettingsDialog));
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_CloneWizard));
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_RemoveDialog));
    pMenu->addSeparator();
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_State_Machine_StartOrShow));
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_Discard));
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_Toggle_Machine_PauseAndResume));
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_Reset));
    /* Prepare 'machine/close' menu: */
    QMenu *pMachineCloseMenu = gActionPool->action(UIActionIndexSelector_Menu_Machine_Close)->menu();
    prepareMenuMachineClose(pMachineCloseMenu);
    pMenu->addMenu(pMachineCloseMenu);
    pMenu->addSeparator();
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_Refresh));
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_LogDialog));
    pMenu->addSeparator();
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_ShowInFileManager));
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_CreateShortcut));
    pMenu->addSeparator();
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_Sort));
}

void VBoxSelectorWnd::prepareMenuMachineClose(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

    /* Populate 'machine/close' menu: */
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_Close_ACPIShutdown));
    pMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_Close_PowerOff));
}

void VBoxSelectorWnd::prepareMenuHelp(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

    /* Help submenu: */
    pMenu->addAction(gActionPool->action(UIActionIndex_Simple_Help));
    pMenu->addAction(gActionPool->action(UIActionIndex_Simple_Web));

    pMenu->addSeparator();

    pMenu->addAction(gActionPool->action(UIActionIndex_Simple_ResetWarnings));

    pMenu->addSeparator();

#ifdef VBOX_WITH_REGISTRATION
    pMenu->addAction(gActionPool->action(UIActionIndex_Simple_Register));
#endif /* VBOX_WITH_REGISTRATION */

    pMenu->addAction(gActionPool->action(UIActionIndex_Simple_Update));

#ifndef Q_WS_MAC
    pMenu->addSeparator();
#endif /* !Q_WS_MAC */

    pMenu->addAction(gActionPool->action(UIActionIndex_Simple_About));
}

void VBoxSelectorWnd::prepareContextMenu()
{
    m_pMachineContextMenu = new QMenu(this);
    m_pMachineContextMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_SettingsDialog));
    m_pMachineContextMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_CloneWizard));
    m_pMachineContextMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_RemoveDialog));
    m_pMachineContextMenu->addSeparator();
    m_pMachineContextMenu->addAction(gActionPool->action(UIActionIndexSelector_State_Machine_StartOrShow));
    m_pMachineContextMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_Discard));
    m_pMachineContextMenu->addAction(gActionPool->action(UIActionIndexSelector_Toggle_Machine_PauseAndResume));
    m_pMachineContextMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_Reset));
    m_pMachineContextMenu->addMenu(gActionPool->action(UIActionIndexSelector_Menu_Machine_Close)->menu());
    m_pMachineContextMenu->addSeparator();
    m_pMachineContextMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_Refresh));
    m_pMachineContextMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_LogDialog));
    m_pMachineContextMenu->addSeparator();
    m_pMachineContextMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_ShowInFileManager));
    m_pMachineContextMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_CreateShortcut));
    m_pMachineContextMenu->addSeparator();
    m_pMachineContextMenu->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_Sort));
}

void VBoxSelectorWnd::prepareStatusBar()
{
    statusBar()->setContextMenuPolicy(Qt::CustomContextMenu);
}

void VBoxSelectorWnd::prepareWidgets()
{
    /* Prepare splitter: */
    m_pSplitter = new QISplitter(this);
    m_pSplitter->setHandleType(QISplitter::Native);

    /* Prepare tool-bar: */
    mVMToolBar = new UIToolBar(this);
    mVMToolBar->setContextMenuPolicy(Qt::CustomContextMenu);
    mVMToolBar->setIconSize(QSize(32, 32));
    mVMToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    mVMToolBar->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_NewWizard));
    mVMToolBar->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_SettingsDialog));
    mVMToolBar->addAction(gActionPool->action(UIActionIndexSelector_State_Machine_StartOrShow));
    mVMToolBar->addAction(gActionPool->action(UIActionIndexSelector_Simple_Machine_Discard));

    /* Prepare VM list: */
    mVMModel = new UIVMItemModel(this);
    mVMListView = new UIVMListView(mVMModel);
    mVMListView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    /* Make non-possible to activate list elements by single click,
     * this hack should disable the current possibility to do it if present: */
    if (mVMListView->style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick, 0, mVMListView))
        mVMListView->setStyleSheet("activate-on-singleclick : 0");

    /* Prepare details and snapshots tabs: */
    m_pVMDesktop = new UIVMDesktop(mVMToolBar, gActionPool->action(UIActionIndexSelector_Simple_Machine_Refresh), this);

    /* Layout all the widgets: */
#define BIG_TOOLBAR
#if MAC_LEOPARD_STYLE
    addToolBar(mVMToolBar);
    /* Central widget @ horizontal layout: */
    setCentralWidget(m_pSplitter);
    m_pSplitter->addWidget(mVMListView);
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
    m_pSplitter->addWidget(mVMListView);
# else /* BIG_TOOLBAR */
    pLeftVLayout->addWidget(mVMToolBar);
    pLeftVLayout->addWidget(mVMListView);
    setCentralWidget(m_pSplitter);
    m_pSplitter->addWidget(pLeftWidget);
# endif /* !BIG_TOOLBAR */
#endif /* !MAC_LEOPARD_STYLE */
    m_pSplitter->addWidget(m_pVMDesktop);

    /* Set the initial distribution. The right site is bigger. */
    m_pSplitter->setStretchFactor(0, 2);
    m_pSplitter->setStretchFactor(1, 3);
}

void VBoxSelectorWnd::prepareConnections()
{
    /* VirtualBox event connections: */
    connect(gVBoxEvents, SIGNAL(sigMachineStateChange(QString, KMachineState)), this, SLOT(machineStateChanged(QString, KMachineState)));
    connect(gVBoxEvents, SIGNAL(sigMachineDataChange(QString)), this, SLOT(machineDataChanged(QString)));
    connect(gVBoxEvents, SIGNAL(sigMachineRegistered(QString, bool)), this, SLOT(machineRegistered(QString, bool)));
    connect(gVBoxEvents, SIGNAL(sigSessionStateChange(QString, KSessionState)), this, SLOT(sessionStateChanged(QString, KSessionState)));
    connect(gVBoxEvents, SIGNAL(sigSnapshotChange(QString, QString)), this, SLOT(snapshotChanged(QString, QString)));

    /* Medium enumeration connections: */
    connect(&vboxGlobal(), SIGNAL(mediumEnumStarted()), this, SLOT(mediumEnumStarted()));
    connect(&vboxGlobal(), SIGNAL(mediumEnumFinished(const VBoxMediaList &)), this, SLOT(mediumEnumFinished(const VBoxMediaList &)));

    /* Downloader connections: */
    connect(&msgCenter(), SIGNAL(sigDownloaderUserManualCreated()), this, SLOT(sltEmbedDownloaderForUserManual()));
    connect(gUpdateManager, SIGNAL(sigDownloaderCreatedForExtensionPack()), this, SLOT(sltEmbedDownloaderForExtensionPack()));

    /* Menu-bar connections: */
    connect(menuBar(), SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showViewContextMenu(const QPoint&)));

    /* 'File' menu connections: */
    connect(gActionPool->action(UIActionIndexSelector_Simple_File_MediumManagerDialog),
            SIGNAL(triggered()), this, SLOT(sltShowMediumManager()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_File_ImportApplianceWizard),
            SIGNAL(triggered()), this, SLOT(sltShowImportApplianceWizard()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_File_ExportApplianceWizard),
            SIGNAL(triggered()), this, SLOT(sltShowExportApplianceWizard()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_File_PreferencesDialog),
            SIGNAL(triggered()), this, SLOT(sltShowPreferencesDialog()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_File_Exit),
            SIGNAL(triggered()), this, SLOT(sltPerformExit()));

    /* 'Machine' menu connections: */
    connect(gActionPool->action(UIActionIndexSelector_Menu_Machine)->menu(),
            SIGNAL(aboutToShow()), this, SLOT(sltMachineMenuAboutToShow()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_Machine_NewWizard),
            SIGNAL(triggered()), this, SLOT(sltShowNewMachineWizard()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_Machine_AddDialog),
            SIGNAL(triggered()), this, SLOT(sltShowAddMachineDialog()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_Machine_SettingsDialog),
            SIGNAL(triggered()), this, SLOT(sltShowMachineSettingsDialog()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_Machine_CloneWizard),
            SIGNAL(triggered()), this, SLOT(sltShowCloneMachineDialog()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_Machine_RemoveDialog),
            SIGNAL(triggered()), this, SLOT(sltShowRemoveMachineDialog()));
    connect(gActionPool->action(UIActionIndexSelector_State_Machine_StartOrShow),
            SIGNAL(triggered()), this, SLOT(sltPerformStartOrShowAction()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_Machine_Discard),
            SIGNAL(triggered()), this, SLOT(sltPerformDiscardAction()));
    connect(gActionPool->action(UIActionIndexSelector_Toggle_Machine_PauseAndResume),
            SIGNAL(toggled(bool)), this, SLOT(sltPerformPauseResumeAction(bool)));
    connect(gActionPool->action(UIActionIndexSelector_Simple_Machine_Reset),
            SIGNAL(triggered()), this, SLOT(sltPerformResetAction()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_Machine_Refresh),
            SIGNAL(triggered()), this, SLOT(sltPerformRefreshAction()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_Machine_LogDialog),
            SIGNAL(triggered()), this, SLOT(sltShowLogDialog()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_Machine_ShowInFileManager),
            SIGNAL(triggered()), this, SLOT(sltShowMachineInFileManager()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_Machine_CreateShortcut),
            SIGNAL(triggered()), this, SLOT(sltPerformCreateShortcutAction()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_Machine_Sort),
            SIGNAL(triggered()), this, SLOT(sltPerformSortAction()));

    /* 'Machine/Close' menu connections: */
    connect(gActionPool->action(UIActionIndexSelector_Menu_Machine_Close)->menu(),
            SIGNAL(aboutToShow()), this, SLOT(sltMachineCloseMenuAboutToShow()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_Machine_Close_ACPIShutdown),
            SIGNAL(triggered()), this, SLOT(sltPerformACPIShutdownAction()));
    connect(gActionPool->action(UIActionIndexSelector_Simple_Machine_Close_PowerOff),
            SIGNAL(triggered()), this, SLOT(sltPerformPowerOffAction()));

    /* 'Help' menu connections: */
    connect(gActionPool->action(UIActionIndex_Simple_Help), SIGNAL(triggered()),
            &msgCenter(), SLOT(sltShowHelpHelpDialog()));
    connect(gActionPool->action(UIActionIndex_Simple_Web), SIGNAL(triggered()),
            &msgCenter(), SLOT(sltShowHelpWebDialog()));
    connect(gActionPool->action(UIActionIndex_Simple_ResetWarnings), SIGNAL(triggered()),
            &msgCenter(), SLOT(sltResetSuppressedMessages()));
#ifdef VBOX_WITH_REGISTRATION
    connect(gActionPool->action(UIActionIndex_Simple_Register), SIGNAL(triggered()),
            &vboxGlobal(), SLOT(showRegistrationDialog()));
    connect(gEDataEvents, SIGNAL(sigCanShowRegistrationDlg(bool)),
            gActionPool->action(UIActionIndex_Simple_Register), SLOT(setEnabled(bool)));
#endif /* VBOX_WITH_REGISTRATION */
    connect(gActionPool->action(UIActionIndex_Simple_Update), SIGNAL(triggered()),
            gUpdateManager, SLOT(sltForceCheck()));
    connect(gActionPool->action(UIActionIndex_Simple_About), SIGNAL(triggered()),
            &msgCenter(), SLOT(sltShowHelpAboutDialog()));

    /* 'Machine' context menu connections: */
    connect(m_pMachineContextMenu, SIGNAL(aboutToShow()), this, SLOT(sltMachineMenuAboutToShow()));
    connect(m_pMachineContextMenu, SIGNAL(hovered(QAction*)), this, SLOT(sltMachineContextMenuHovered(QAction*)));

    /* Status-bar connections: */
    connect(statusBar(), SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(showViewContextMenu(const QPoint&)));

    /* VM list-view connections: */
    connect(mVMListView, SIGNAL(sigUrlsDropped(QList<QUrl>)), this, SLOT(sltOpenUrls(QList<QUrl>)), Qt::QueuedConnection);
    connect(mVMListView, SIGNAL(currentChanged()), this, SLOT(vmListViewCurrentChanged()));
    connect(mVMListView, SIGNAL(activated()), this, SLOT(sltPerformStartOrShowAction()));
    connect(mVMListView, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(showContextMenu(const QPoint &)));

    /* Tool-bar connections: */
#ifndef Q_WS_MAC
    connect(mVMToolBar, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showViewContextMenu(const QPoint&)));
#else /* !Q_WS_MAC */
    /* A simple connect doesn't work on the Mac, also we want receive right
     * click notifications on the title bar. So register our own handler. */
    ::darwinRegisterForUnifiedToolbarContextMenuEvents(this);
#endif /* Q_WS_MAC */

    /* VM desktop connections: */
    connect(m_pVMDesktop, SIGNAL(linkClicked(const QString &)), this, SLOT(sltShowMachineSettingsDialog(const QString &)));

#ifdef VBOX_GUI_WITH_SYSTRAY
    /* Tray icon connections: */
    connect(mTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
    connect(gEDataEvents, SIGNAL(sigMainWindowCountChange(int)), this, SLOT(mainWindowCountChanged(int)));
    connect(gEDataEvents, SIGNAL(sigCanShowTrayIcon(bool)), this, SLOT(trayIconCanShow(bool)));
    connect(gEDataEvents, SIGNAL(sigTrayIconChange(bool)), this, SLOT(trayIconChanged(bool)));
    connect(&vboxGlobal(), SIGNAL(sigTrayIconShow(bool)), this, SLOT(trayIconShow(bool)));
#endif /* VBOX_GUI_WITH_SYSTRAY */
}

