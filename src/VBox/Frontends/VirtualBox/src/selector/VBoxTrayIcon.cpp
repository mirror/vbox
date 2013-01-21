/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxTrayIcon class implementation
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

#ifdef VBOX_GUI_WITH_SYSTRAY

/* Local includes */
#include "VBoxTrayIcon.h"
#include "UISelectorWindow.h"
#include "UIIconPool.h"
#include "UIVMItem.h"
#include "UIVMListView.h"

/* Global includes */
#include <QMenu>
#include <QAction>

VBoxTrayIcon::VBoxTrayIcon (UISelectorWindow* aParent, UIVMItemModel* aVMModel)
{
    mParent = aParent;
    mVMModel = aVMModel;

    mShowSelectorAction = new QAction (this);
    Assert (mShowSelectorAction);
    mShowSelectorAction->setIcon(UIIconPool::iconSet(":/VirtualBox_16px.png"));

    mHideSystrayMenuAction = new QAction (this);
    Assert (mHideSystrayMenuAction);
    mHideSystrayMenuAction->setIcon(UIIconPool::iconSet(":/exit_16px.png"));

    /* reuse parent action data */

    mVmConfigAction = new QAction (this);
    Assert (mVmConfigAction);
    mVmConfigAction->setIcon (mParent->vmConfigAction()->icon());

    mVmDeleteAction = new QAction (this);
    Assert (mVmDeleteAction);
    mVmDeleteAction->setIcon (mParent->vmDeleteAction()->icon());

    mVmStartAction = new QAction (this);
    Assert (mVmStartAction);
    mVmStartAction->setIcon (mParent->vmStartAction()->icon());

    mVmDiscardAction = new QAction (this);
    Assert (mVmDiscardAction);
    mVmDiscardAction->setIcon (mParent->vmDiscardAction()->icon());

    mVmPauseAction = new QAction (this);
    Assert (mVmPauseAction);
    mVmPauseAction->setCheckable (true);
    mVmPauseAction->setIcon (mParent->vmPauseAction()->icon());

    mVmRefreshAction = new QAction (this);
    Assert (mVmRefreshAction);
    mVmRefreshAction->setIcon (mParent->vmRefreshAction()->icon());

    mVmShowLogsAction = new QAction (this);
    Assert (mVmConfigAction);
    mVmShowLogsAction->setIcon (mParent->vmShowLogsAction()->icon());

    mTrayIconMenu = new QMenu (aParent);
    Assert (mTrayIconMenu);

    setIcon (QIcon (":/VirtualBox_16px.png"));
    setContextMenu (mTrayIconMenu);

    connect (mShowSelectorAction, SIGNAL (triggered()), mParent, SLOT (showWindow()));
    connect (mHideSystrayMenuAction, SIGNAL (triggered()), this, SLOT (trayIconShow()));
}

VBoxTrayIcon::~VBoxTrayIcon ()
{
    /* Erase dialog handle in config file. */
    if (mActive)
    {
        vboxGlobal().virtualBox().SetExtraData(GUI_TrayIconWinID, QString::null);
        hide();
    }
}

void VBoxTrayIcon::retranslateUi ()
{
    if (!mActive)
        return;

    mShowSelectorAction->setText (tr ("Show Selector Window"));
    mShowSelectorAction->setStatusTip (tr (
        "Show the selector window assigned to this menu"));

    mHideSystrayMenuAction->setText (tr ("Hide Tray Icon"));
    mHideSystrayMenuAction->setStatusTip (tr (
        "Remove this icon from the system tray"));

    /* reuse parent action data */

    mVmConfigAction->setText (mParent->vmConfigAction()->text());
    mVmConfigAction->setStatusTip (mParent->vmConfigAction()->statusTip());

    mVmDeleteAction->setText (mParent->vmDeleteAction()->text());
    mVmDeleteAction->setStatusTip (mParent->vmDeleteAction()->statusTip());

    mVmPauseAction->setText (mParent->vmPauseAction()->text());
    mVmPauseAction->setStatusTip (mParent->vmPauseAction()->statusTip());

    mVmDiscardAction->setText (mParent->vmDiscardAction()->text());
    mVmDiscardAction->setStatusTip (mParent->vmDiscardAction()->statusTip());

    mVmShowLogsAction->setText (mParent->vmShowLogsAction()->text());
    mVmShowLogsAction->setStatusTip (mParent->vmShowLogsAction()->statusTip());
}

void VBoxTrayIcon::showSubMenu ()
{
    if (!mActive)
        return;

    UIVMItem* pItem = NULL;
    QMenu *pMenu = NULL;
    QVariant vID;

    if ((pMenu = qobject_cast<QMenu*>(sender())))
    {
        vID = pMenu->menuAction()->data();
        if (vID.canConvert<QString>() && mVMModel)
            pItem = mVMModel->itemById (qvariant_cast<QString>(vID));
    }

    mVmConfigAction->setData (vID);
    mVmDeleteAction->setData (vID);
    mVmDiscardAction->setData (vID);
    mVmStartAction->setData (vID);
    mVmPauseAction->setData (vID);
    mVmShowLogsAction->setData (vID);

    if (pItem && pItem->accessible())
    {
        /* look at vmListViewCurrentChanged() */
        CMachine m = pItem->machine();
        KMachineState s = pItem->machineState();
        bool running = pItem->sessionState() != KSessionState_Unlocked;
        bool modifyEnabled = !running && s != KMachineState_Saved;

        /* Settings */
        mVmConfigAction->setEnabled (modifyEnabled);

        /* Delete */
        mVmDeleteAction->setEnabled (!running);

        /* Discard */
        mVmDiscardAction->setEnabled (s == KMachineState_Saved && !running);

        /* Change the Start button text accordingly */
        if (   s == KMachineState_PoweredOff
            || s == KMachineState_Saved
            || s == KMachineState_Teleported
            || s == KMachineState_Aborted
           )
        {
            mVmStartAction->setText (UIVMListView::tr ("S&tart"));
            mVmStartAction->setStatusTip (
                  UIVMListView::tr ("Start the selected virtual machine"));
            mVmStartAction->setEnabled (!running);
        }
        else
        {
            mVmStartAction->setText (UIVMListView::tr ("S&how"));
            mVmStartAction->setStatusTip (
                  UIVMListView::tr ("Switch to the window of the selected virtual machine"));
            mVmStartAction->setEnabled (pItem->canSwitchTo());
        }

        /* Change the Pause/Resume button text accordingly */
        mVmPauseAction->setEnabled (   s == KMachineState_Running
                                    || s == KMachineState_Teleporting
                                    || s == KMachineState_LiveSnapshotting
                                    || s == KMachineState_Paused
                                    || s == KMachineState_TeleportingPausedVM
                                   );

        if (   s == KMachineState_Paused
            || s == KMachineState_TeleportingPausedVM /*?*/
           )
        {
            mVmPauseAction->setText (UIVMListView::tr ("R&esume"));
            mVmPauseAction->setStatusTip (
                  UIVMListView::tr ("Resume the execution of the virtual machine"));
            mVmPauseAction->blockSignals (true);
            mVmPauseAction->setChecked (true);
            mVmPauseAction->blockSignals (false);
        }
        else
        {
            mVmPauseAction->setText (UIVMListView::tr ("&Pause"));
            mVmPauseAction->setStatusTip (
                  UIVMListView::tr ("Suspend the execution of the virtual machine"));
            mVmPauseAction->blockSignals (true);
            mVmPauseAction->setChecked (false);
            mVmPauseAction->blockSignals (false);
        }

        mVmShowLogsAction->setEnabled (true);

        /* Disconnect old slot which maybe was connected from another selected sub menu. */
        disconnect (mVmConfigAction, SIGNAL (triggered()), this, SLOT (vmSettings()));
        disconnect (mVmDeleteAction, SIGNAL (triggered()), this, SLOT (vmDelete()));
        disconnect (mVmDiscardAction, SIGNAL (triggered()), this, SLOT (vmDiscard()));
        disconnect (mVmStartAction, SIGNAL (triggered()), this, SLOT (vmStart()));
        disconnect (mVmPauseAction, SIGNAL (toggled (bool)), this, SLOT (vmPause (bool)));
        disconnect (mVmShowLogsAction, SIGNAL (triggered()), this, SLOT (vmShowLogs()));

        /* Connect new sub menu with slots. */
        connect (mVmConfigAction, SIGNAL (triggered()), this, SLOT (vmSettings()));
        connect (mVmDeleteAction, SIGNAL (triggered()), this, SLOT (vmDelete()));
        connect (mVmDiscardAction, SIGNAL (triggered()), this, SLOT (vmDiscard()));
        connect (mVmStartAction, SIGNAL (triggered()), this, SLOT (vmStart()));
        connect (mVmPauseAction, SIGNAL (toggled (bool)), this, SLOT (vmPause (bool)));
        connect (mVmShowLogsAction, SIGNAL (triggered()), this, SLOT (vmShowLogs()));
    }
    else    /* Item is not accessible. */
    {
        mVmConfigAction->setEnabled (false);
        mVmDeleteAction->setEnabled (pItem != NULL);
        mVmDiscardAction->setEnabled (false);
        mVmPauseAction->setEnabled (false);

        /* Set the Start button text accordingly. */
        mVmStartAction->setText (UIVMListView::tr ("S&tart"));
        mVmStartAction->setStatusTip (
              UIVMListView::tr ("Start the selected virtual machine"));
        mVmStartAction->setEnabled (false);

        /* Disable the show log item for the selected vm. */
        mVmShowLogsAction->setEnabled (false);
    }

    /* Build sub menu entries (add rest of sub menu entries later here). */
    pMenu->addAction (mVmStartAction);
    pMenu->addAction (mVmPauseAction);
}

void VBoxTrayIcon::hideSubMenu ()
{
    if (!mActive)
        return;

    UIVMItem* pItem = NULL;
    QVariant vID;

    if (QMenu *pMenu = qobject_cast<QMenu*>(sender()))
    {
        vID = pMenu->menuAction()->data();
        if (vID.canConvert<QString>() && mVMModel)
            pItem = mVMModel->itemById (qvariant_cast<QString>(vID));
    }

    /* Nothing to do here yet. */

    Assert (pItem);
}

void VBoxTrayIcon::refresh ()
{
    if (!mActive)
        return;

    AssertReturnVoid (mVMModel);
    AssertReturnVoid (mTrayIconMenu);

    mTrayIconMenu->clear();

    UIVMItem* pItem = NULL;
    QMenu* pCurMenu = mTrayIconMenu;
    QMenu* pSubMenu = NULL;

    int iCurItemCount = 0;

    mTrayIconMenu->addAction (mShowSelectorAction);
    mTrayIconMenu->setDefaultAction (mShowSelectorAction);

    if (mVMModel->rowCount() > 0)
        mTrayIconMenu->addSeparator();

    for (int i = 0; i < mVMModel->rowCount(); i++, iCurItemCount++)
    {
        pItem = mVMModel->itemByRow(i);
        Assert(pItem);

        if (iCurItemCount > 10) /* 10 machines per sub menu. */
        {
            pSubMenu = new QMenu (tr ("&Other Machines...", "tray menu"));
            Assert (pSubMenu);
            pCurMenu->addMenu (pSubMenu);
            pCurMenu = pSubMenu;
            iCurItemCount = 0;
        }

        pSubMenu = new QMenu (QString ("&%1. %2")
                              .arg ((iCurItemCount + 1) % 100).arg (pItem->name()));
        Assert (pSubMenu);
        pSubMenu->setIcon (pItem->machineStateIcon());

        QAction *pAction = NULL;
        QVariant vID;
        vID.setValue (pItem->id());

        pSubMenu->menuAction()->setData (vID);
        connect (pSubMenu, SIGNAL (aboutToShow()), this, SLOT (showSubMenu()));
        connect (pSubMenu, SIGNAL (aboutToHide()), this, SLOT (hideSubMenu()));
        pCurMenu->addMenu (pSubMenu);
    }

    if (mVMModel->rowCount() > 0)
        mTrayIconMenu->addSeparator();

    mTrayIconMenu->addAction (mHideSystrayMenuAction);

    /* We're done constructing the menu, show it */
    setVisible (true);
}

UIVMItem* VBoxTrayIcon::GetItem (QObject* aObject)
{
    UIVMItem* pItem = NULL;
    if (QAction *pAction = qobject_cast<QAction*>(sender()))
    {
        QVariant v = pAction->data();
        if (v.canConvert<QString>() && mVMModel)
            pItem = mVMModel->itemById (qvariant_cast<QString>(v));
    }

    Assert (pItem);
    return pItem;
}

void VBoxTrayIcon::trayIconShow (bool aShow)
{
    if (!vboxGlobal().isTrayMenu())
        return;

    mActive = aShow;
    if (mActive)
    {
        refresh();
        retranslateUi();
    }
    setVisible (mActive);

    if (!mActive)
        mParent->fileExit();
}

void VBoxTrayIcon::vmSettings()
{
    UIVMItem* pItem = GetItem (sender());
    mParent->vmSettings (NULL, NULL, pItem->id());
}

void VBoxTrayIcon::vmDelete()
{
    UIVMItem* pItem = GetItem (sender());
    mParent->vmDelete (pItem->id());
}

void VBoxTrayIcon::vmStart()
{
    UIVMItem* pItem = GetItem (sender());
    mParent->vmStart (pItem->id());
}

void VBoxTrayIcon::vmDiscard()
{
    UIVMItem* pItem = GetItem (sender());
    mParent->vmDiscard (pItem->id());
}

void VBoxTrayIcon::vmPause(bool aPause)
{
    UIVMItem* pItem = GetItem (sender());
    mParent->vmPause (aPause, pItem->id());
}

void VBoxTrayIcon::vmRefresh()
{
    UIVMItem* pItem = GetItem (sender());
    mParent->vmRefresh (pItem->id());
}

void VBoxTrayIcon::vmShowLogs()
{
    UIVMItem* pItem = GetItem (sender());
    mParent->vmShowLogs (pItem->id());
}

#endif /* VBOX_GUI_WITH_SYSTRAY */

