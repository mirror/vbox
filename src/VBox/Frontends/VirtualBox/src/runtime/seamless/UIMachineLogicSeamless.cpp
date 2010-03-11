/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineLogicSeamless class implementation
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/* Global includes */
#include <QDesktopWidget>

/* Local includes */
#include "COMDefs.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"

#include "UISession.h"
#include "UIActionsPool.h"
#include "UIMachineLogicSeamless.h"
#include "UIMachineWindow.h"

#include "VBoxUtils.h"

UIMachineLogicSeamless::UIMachineLogicSeamless(QObject *pParent, UISession *pSession, UIActionsPool *pActionsPool)
    : UIMachineLogic(pParent, pSession, pActionsPool, UIVisualStateType_Seamless)
{
}

UIMachineLogicSeamless::~UIMachineLogicSeamless()
{
    /* Cleanup normal machine window: */
    cleanupMachineWindows();

    /* Cleanup actions groups: */
    cleanupActionGroups();
}

bool UIMachineLogicSeamless::checkAvailability()
{
    /* Temporary get a machine object: */
    const CMachine &machine = uisession()->session().GetMachine();
    const CConsole &console = uisession()->session().GetConsole();

#if (QT_VERSION >= 0x040600)
    int cHostScreens = QApplication::desktop()->screenCount();
#else /* (QT_VERSION >= 0x040600) */
    int cHostScreens = QApplication::desktop()->numScreens();
#endif /* !(QT_VERSION >= 0x040600) */

    int cGuestScreens = machine.GetMonitorCount();
    /* Check that there are enough physical screens are connected: */
    if (cHostScreens < cGuestScreens)
    {
        vboxProblem().cannotEnterSeamlessMode();
        return false;
    }

    // TODO_NEW_CORE: this is how it looked in the old version
    // bool VBoxConsoleView::isAutoresizeGuestActive() { return mGuestSupportsGraphics && mAutoresizeGuest; }
//    if (uisession()->session().GetConsole().isAutoresizeGuestActive())
    if (uisession()->isGuestAdditionsActive())
    {
        ULONG64 availBits = machine.GetVRAMSize() /* VRAM */
                          * _1M /* MB to bytes */
                          * 8; /* to bits */
        ULONG guestBpp = console.GetDisplay().GetBitsPerPixel();
        ULONG64 usedBits = 0;
        for (int i = 0; i < cGuestScreens; ++ i)
        {
            // TODO_NEW_CORE: really take the screen geometry into account the
            // different fb will be displayed. */
            QRect screen = QApplication::desktop()->availableGeometry(i);
            usedBits += screen.width() /* display width */
                      * screen.height() /* display height */
                      * guestBpp
                      + _1M * 8; /* current cache per screen - may be changed in future */
        }
        usedBits += 4096 * 8; /* adapter info */

        if (availBits < usedBits)
        {
//          vboxProblem().cannotEnterSeamlessMode(screen.width(), screen.height(), guestBpp,
//                                                (((usedBits + 7) / 8 + _1M - 1) / _1M) * _1M);
            vboxProblem().cannotEnterSeamlessMode(0, 0, guestBpp,
                                                  (((usedBits + 7) / 8 + _1M - 1) / _1M) * _1M);
            return false;
        }
    }

    /* Take the toggle hot key from the menu item. Since
     * VBoxGlobal::extractKeyFromActionText gets exactly the
     * linked key without the 'Host+' part we are adding it here. */
    QString hotKey = QString("Host+%1")
        .arg(VBoxGlobal::extractKeyFromActionText(actionsPool()->action(UIActionIndex_Toggle_Seamless)->text()));
    Assert(!hotKey.isEmpty());

    /* Show the info message. */
    if (!vboxProblem().confirmGoingSeamless(hotKey))
        return false;

    return true;
}

void UIMachineLogicSeamless::initialize()
{
    /* Check the status of required features: */
    prepareRequiredFeatures();

    /* If required features are ready: */
    if (!isPreventAutoStart())
    {
        /* Prepare console connections: */
        prepareConsoleConnections();

        /* Prepare action groups: */
        prepareActionGroups();

        /* Prepare action connections: */
        prepareActionConnections();

        /* Prepare normal machine window: */
        prepareMachineWindows();

#ifdef Q_WS_MAC
        /* Prepare dock: */
        prepareDock();
#endif /* Q_WS_MAC */

        /* Initialization: */
        sltMachineStateChanged();
        sltAdditionsStateChanged();
        sltMouseCapabilityChanged();

        /* Retranslate logic part: */
        retranslateUi();
    }
}

void UIMachineLogicSeamless::prepareActionGroups()
{
    /* Base class action groups: */
    UIMachineLogic::prepareActionGroups();

    /* Guest auto-resize isn't allowed in seamless: */
    actionsPool()->action(UIActionIndex_Toggle_GuestAutoresize)->setVisible(false);

    /* Adjust-window isn't allowed in seamless: */
    actionsPool()->action(UIActionIndex_Simple_AdjustWindow)->setVisible(false);

    /* Disable mouse-integration isn't allowed in seamless: */
    actionsPool()->action(UIActionIndex_Toggle_MouseIntegration)->setVisible(false);
}

void UIMachineLogicSeamless::prepareMachineWindows()
{
    /* Do not create window(s) if they created already: */
    if (isMachineWindowsCreated())
        return;

#ifdef Q_WS_MAC // TODO: Is that really need here?
    /* We have to make sure that we are getting the front most process.
     * This is necessary for Qt versions > 4.3.3: */
    ::darwinSetFrontMostProcess();
#endif /* Q_WS_MAC */

#if 0 // TODO: Add seamless multi-monitor support!
    /* Get monitors count: */
    ulong uMonitorCount = session().GetMachine().GetMonitorCount();
    /* Create machine window(s): */
    for (ulong uScreenId = 0; uScreenId < uMonitorCount; ++ uScreenId)
        addMachineWindow(UIMachineWindow::create(this, visualStateType(), uScreenId));
    /* Order machine window(s): */
    for (ulong uScreenId = uMonitorCount; uScreenId > 0; -- uScreenId)
        machineWindows()[uScreenId - 1]->machineWindow()->raise();
#else
    /* Create primary machine window: */
    addMachineWindow(UIMachineWindow::create(this, visualStateType(), 0 /* primary only */));
#endif

    /* Remember what machine window(s) created: */
    setMachineWindowsCreated(true);

    /* Check if we need to start VM: */
    tryToStartMachine();
}

void UIMachineLogicSeamless::cleanupMachineWindows()
{
    /* Do not cleanup machine window if it is not present: */
    if (!isMachineWindowsCreated())
        return;

#if 0 // TODO: Add seamless multi-monitor support!
    /* Cleanup normal machine window: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        UIMachineWindow::destroy(pMachineWindow);
#else
    /* Create machine window(s): */
    UIMachineWindow::destroy(machineWindows()[0] /* primary only */);
#endif
}

void UIMachineLogicSeamless::cleanupActionGroups()
{
    /* Reenable guest-autoresize action: */
    actionsPool()->action(UIActionIndex_Toggle_GuestAutoresize)->setVisible(true);

    /* Reenable adjust-window action: */
    actionsPool()->action(UIActionIndex_Simple_AdjustWindow)->setVisible(true);

    /* Reenable mouse-integration action: */
    actionsPool()->action(UIActionIndex_Toggle_MouseIntegration)->setVisible(true);
}

