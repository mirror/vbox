/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineLogicScale class implementation
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Local includes */
#include "COMDefs.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"

#include "UISession.h"
#include "UIActionsPool.h"
#include "UIMachineLogicScale.h"
#include "UIMachineWindow.h"
#include "UIDownloaderAdditions.h"

#ifdef Q_WS_MAC
#include "VBoxUtils.h"
#endif /* Q_WS_MAC */

UIMachineLogicScale::UIMachineLogicScale(QObject *pParent, UISession *pSession, UIActionsPool *pActionsPool)
    : UIMachineLogic(pParent, pSession, pActionsPool, UIVisualStateType_Scale)
{
}

UIMachineLogicScale::~UIMachineLogicScale()
{
#ifdef Q_WS_MAC
    /* Cleanup the dock stuff before the machine window(s): */
    cleanupDock();
#endif /* Q_WS_MAC */

    /* Cleanup machine window(s): */
    cleanupMachineWindow();

    /* Cleanup handlers: */
    cleanupHandlers();

    /* Cleanup actions groups: */
    cleanupActionGroups();
}

bool UIMachineLogicScale::checkAvailability()
{
    /* Base class availability: */
    if (!UIMachineLogic::checkAvailability())
        return false;

    /* Take the toggle hot key from the menu item. Since
     * VBoxGlobal::extractKeyFromActionText gets exactly the
     * linked key without the 'Host+' part we are adding it here. */
    QString strHotKey = QString("Host+%1")
        .arg(VBoxGlobal::extractKeyFromActionText(actionsPool()->action(UIActionIndex_Toggle_Scale)->text()));
    Assert(!strHotKey.isEmpty());

    /* Show the info message. */
    if (!msgCenter().confirmGoingScale(strHotKey))
        return false;

    return true;
}

void UIMachineLogicScale::initialize()
{
    /* Prepare required features: */
    prepareRequiredFeatures();

    /* Prepare session connections: */
    prepareSessionConnections();

    /* Prepare action groups:
     * Note: This has to be done before prepareActionConnections
     * cause here actions/menus are recreated. */
    prepareActionGroups();

    /* Prepare action connections: */
    prepareActionConnections();

    /* Prepare handlers: */
    prepareHandlers();

    /* Prepare scale machine window: */
    prepareMachineWindows();

    /* If there is an Additions download running, update the parent window
     * information. */
    if (UIDownloaderAdditions *pDl = UIDownloaderAdditions::current())
        pDl->setParentWidget(mainMachineWindow()->machineWindow());

#ifdef Q_WS_MAC
    /* Prepare dock: */
    prepareDock();
#endif /* Q_WS_MAC */

    /* Power up machine: */
    uisession()->powerUp();

    /* Initialization: */
    sltMachineStateChanged();
    sltAdditionsStateChanged();
    sltMouseCapabilityChanged();

#ifdef VBOX_WITH_DEBUGGER_GUI
    prepareDebugger();
#endif /* VBOX_WITH_DEBUGGER_GUI */

    /* Retranslate logic part: */
    retranslateUi();
}

void UIMachineLogicScale::prepareActionGroups()
{
    /* Base class action groups: */
    UIMachineLogic::prepareActionGroups();

    /* Guest auto-resize isn't allowed in scale-mode: */
    actionsPool()->action(UIActionIndex_Toggle_GuestAutoresize)->setVisible(false);

    /* Adjust-window isn't allowed in scale-mode: */
    actionsPool()->action(UIActionIndex_Simple_AdjustWindow)->setVisible(false);
}

void UIMachineLogicScale::prepareMachineWindows()
{
    /* Do not create window(s) if they created already: */
    if (isMachineWindowsCreated())
        return;

#ifdef Q_WS_MAC // TODO: Is that really need here?
    /* We have to make sure that we are getting the front most process.
     * This is necessary for Qt versions > 4.3.3: */
    ::darwinSetFrontMostProcess();
#endif /* Q_WS_MAC */

    /* Get monitors count: */
    ulong uMonitorCount = session().GetMachine().GetMonitorCount();
    /* Create machine window(s): */
    for (ulong uScreenId = 0; uScreenId < uMonitorCount; ++ uScreenId)
        addMachineWindow(UIMachineWindow::create(this, visualStateType(), uScreenId));
    /* Order machine window(s): */
    for (ulong uScreenId = uMonitorCount; uScreenId > 0; -- uScreenId)
        machineWindows()[uScreenId - 1]->machineWindow()->raise();

    /* Remember what machine window(s) created: */
    setMachineWindowsCreated(true);
}

void UIMachineLogicScale::cleanupMachineWindow()
{
    /* Do not cleanup machine window(s) if not present: */
    if (!isMachineWindowsCreated())
        return;

    /* Cleanup machine window(s): */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        UIMachineWindow::destroy(pMachineWindow);
}

void UIMachineLogicScale::cleanupActionGroups()
{
    /* Reenable guest-autoresize action: */
    actionsPool()->action(UIActionIndex_Toggle_GuestAutoresize)->setVisible(true);

    /* Reenable adjust-window action: */
    actionsPool()->action(UIActionIndex_Simple_AdjustWindow)->setVisible(true);
}

