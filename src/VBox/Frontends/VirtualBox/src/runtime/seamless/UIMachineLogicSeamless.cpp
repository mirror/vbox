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

#include "UIActionsPool.h"
#include "UIMachineLogicSeamless.h"
#include "UIMachineWindow.h"
#include "UIMachineWindowSeamless.h"
#include "UIMultiScreenLayout.h"
#include "UISession.h"

#ifdef Q_WS_MAC
# include "VBoxUtils.h"
#endif /* Q_WS_MAC */

UIMachineLogicSeamless::UIMachineLogicSeamless(QObject *pParent, UISession *pSession, UIActionsPool *pActionsPool)
    : UIMachineLogic(pParent, pSession, pActionsPool, UIVisualStateType_Seamless)
{
    m_pScreenLayout = new UIMultiScreenLayout(this);
}

UIMachineLogicSeamless::~UIMachineLogicSeamless()
{
    /* Cleanup normal machine window: */
    cleanupMachineWindows();

    /* Cleanup actions groups: */
    cleanupActionGroups();

    delete m_pScreenLayout;
}

bool UIMachineLogicSeamless::checkAvailability()
{
    /* Base class availability: */
    if (!UIMachineLogic::checkAvailability())
        return false;

    /* Temporary get a machine object: */
    const CMachine &machine = uisession()->session().GetMachine();
    const CConsole &console = uisession()->session().GetConsole();

    int cHostScreens = m_pScreenLayout->hostScreenCount();
    int cGuestScreens = m_pScreenLayout->guestScreenCount();
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
    /* Prepare required features: */
    prepareRequiredFeatures();

    /* Prepare console connections: */
    prepareSessionConnections();

    /* Prepare action connections: */
    prepareActionConnections();

    /* Prepare action groups: */
    prepareActionGroups();

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

    /* Retranslate logic part: */
    retranslateUi();
}

int UIMachineLogicSeamless::hostScreenForGuestScreen(int screenId) const
{
    return m_pScreenLayout->hostScreenForGuestScreen(screenId);
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

    /* Add the view menu: */
    QMenu *pMenu = actionsPool()->action(UIActionIndex_Menu_View)->menu();
    m_pScreenLayout->initialize(pMenu);
    pMenu->setVisible(true);
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

    /* Update the multi screen layout */
    m_pScreenLayout->update();

    /* Create machine window(s): */
    for (int screenId = 0; screenId < m_pScreenLayout->guestScreenCount(); ++screenId)
        addMachineWindow(UIMachineWindow::create(this, visualStateType(), screenId));

    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        connect(m_pScreenLayout, SIGNAL(screenLayoutChanged()),
                static_cast<UIMachineWindowSeamless*>(pMachineWindow), SLOT(sltPlaceOnScreen()));

    /* Remember what machine window(s) created: */
    setMachineWindowsCreated(true);
}

void UIMachineLogicSeamless::cleanupMachineWindows()
{
    /* Do not cleanup machine window if it is not present: */
    if (!isMachineWindowsCreated())
        return;

    /* Cleanup normal machine window: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        UIMachineWindow::destroy(pMachineWindow);
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

