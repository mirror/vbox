/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineLogicScale class implementation
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogicScale.h"
#include "UIMachineWindow.h"
#include "UIDownloaderAdditions.h"
#include "UIDownloaderExtensionPack.h"
#ifdef Q_WS_MAC
#include "VBoxUtils.h"
#endif /* Q_WS_MAC */

UIMachineLogicScale::UIMachineLogicScale(QObject *pParent, UISession *pSession)
    : UIMachineLogic(pParent, pSession, UIVisualStateType_Scale)
{
}

bool UIMachineLogicScale::checkAvailability()
{
    /* Take the toggle hot key from the menu item.
     * Since VBoxGlobal::extractKeyFromActionText gets exactly
     * the linked key without the 'Host+' part we are adding it here. */
    QString strHotKey = QString("Host+%1")
        .arg(VBoxGlobal::extractKeyFromActionText(gActionPool->action(UIActionIndexRuntime_Toggle_Scale)->text()));
    Assert(!strHotKey.isEmpty());

    /* Show the info message. */
    if (!msgCenter().confirmGoingScale(strHotKey))
        return false;

    return true;
}

void UIMachineLogicScale::prepareActionGroups()
{
    /* Call to base-class: */
    UIMachineLogic::prepareActionGroups();

    /* Guest auto-resize isn't allowed in scale-mode: */
    gActionPool->action(UIActionIndexRuntime_Toggle_GuestAutoresize)->setVisible(false);

    /* Adjust-window isn't allowed in scale-mode: */
    gActionPool->action(UIActionIndexRuntime_Simple_AdjustWindow)->setVisible(false);
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
        addMachineWindow(UIMachineWindow::create(this, uScreenId));
    /* Order machine window(s): */
    for (ulong uScreenId = uMonitorCount; uScreenId > 0; -- uScreenId)
        machineWindows()[uScreenId - 1]->raise();

    /* Remember what machine window(s) created: */
    setMachineWindowsCreated(true);
}

void UIMachineLogicScale::cleanupMachineWindows()
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
    /* Call to base-class: */
    UIMachineLogic::cleanupActionGroups();

    /* Reenable guest-autoresize action: */
    gActionPool->action(UIActionIndexRuntime_Toggle_GuestAutoresize)->setVisible(true);

    /* Reenable adjust-window action: */
    gActionPool->action(UIActionIndexRuntime_Simple_AdjustWindow)->setVisible(true);
}

