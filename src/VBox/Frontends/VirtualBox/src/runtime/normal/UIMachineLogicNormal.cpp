/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineLogicNormal class implementation
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

/* Global includes */
#include <QMenu>

/* Local includes */
#include "COMDefs.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"

#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogicNormal.h"
#include "UIMachineWindow.h"
#include "UIDownloaderAdditions.h"
#include "UIDownloaderUserManual.h"
#include "UIDownloaderExtensionPack.h"

#ifdef Q_WS_MAC
#include "VBoxUtils.h"
#endif /* Q_WS_MAC */

UIMachineLogicNormal::UIMachineLogicNormal(QObject *pParent, UISession *pSession)
    : UIMachineLogic(pParent, pSession, UIVisualStateType_Normal)
{
}

UIMachineLogicNormal::~UIMachineLogicNormal()
{
#ifdef Q_WS_MAC
    /* Cleanup the dock stuff before the machine window(s): */
    cleanupDock();
#endif /* Q_WS_MAC */

    /* Cleanup machine window(s): */
    cleanupMachineWindow();

    /* Cleanup handlers: */
    cleanupHandlers();
}

void UIMachineLogicNormal::initialize()
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

    /* Prepare normal machine window: */
    prepareMachineWindows();

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

void UIMachineLogicNormal::sltPrepareNetworkAdaptersMenu()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    AssertMsg(menu, ("This slot should be called only on Network Adapters menu show!\n"));
    menu->clear();
    menu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_NetworkAdaptersDialog));
}

void UIMachineLogicNormal::sltPrepareSharedFoldersMenu()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    AssertMsg(menu, ("This slot should be called only on Shared Folders menu show!\n"));
    menu->clear();
    menu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_SharedFoldersDialog));
}

void UIMachineLogicNormal::sltPrepareMouseIntegrationMenu()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    AssertMsg(menu, ("This slot should be called only on Mouse Integration Menu show!\n"));
    menu->clear();
    menu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_MouseIntegration));
}

void UIMachineLogicNormal::prepareActionConnections()
{
    /* Base class connections: */
    UIMachineLogic::prepareActionConnections();

    /* This class connections: */
    connect(gActionPool->action(UIActionIndexRuntime_Menu_NetworkAdapters)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareNetworkAdaptersMenu()));
    connect(gActionPool->action(UIActionIndexRuntime_Menu_SharedFolders)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareSharedFoldersMenu()));
    connect(gActionPool->action(UIActionIndexRuntime_Menu_MouseIntegration)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareMouseIntegrationMenu()));
}

void UIMachineLogicNormal::prepareMachineWindows()
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

void UIMachineLogicNormal::cleanupMachineWindow()
{
    /* Do not cleanup machine window(s) if not present: */
    if (!isMachineWindowsCreated())
        return;

    /* Cleanup machine window(s): */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        UIMachineWindow::destroy(pMachineWindow);
}

