/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineLogicSeamless class implementation
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
#include <QDesktopWidget>

/* Local includes */
#include "COMDefs.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"

#include "UISession.h"
#include "UIActionsPool.h"
#include "UIMachineLogicSeamless.h"
#include "UIMachineWindowSeamless.h"
#include "UIMultiScreenLayout.h"

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
#ifdef Q_WS_MAC
    /* Cleanup the dock stuff before the machine window(s): */
    cleanupDock();
#endif /* Q_WS_MAC */

    /* Cleanup machine window(s): */
    cleanupMachineWindows();

    /* Cleanup handlers: */
    cleanupHandlers();

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

    int cHostScreens = m_pScreenLayout->hostScreenCount();
    int cGuestScreens = m_pScreenLayout->guestScreenCount();
    /* Check that there are enough physical screens are connected: */
    if (cHostScreens < cGuestScreens)
    {
        msgCenter().cannotEnterSeamlessMode();
        return false;
    }

    // TODO_NEW_CORE: this is how it looked in the old version
    // bool VBoxConsoleView::isAutoresizeGuestActive() { return mGuestSupportsGraphics && mAutoresizeGuest; }
//    if (uisession()->session().GetConsole().isAutoresizeGuestActive())
    if (uisession()->isGuestAdditionsActive())
    {
        quint64 availBits = machine.GetVRAMSize() /* VRAM */
                            * _1M /* MiB to bytes */
                            * 8; /* to bits */
        quint64 usedBits = m_pScreenLayout->memoryRequirements();
        if (availBits < usedBits)
        {
            msgCenter().cannotEnterSeamlessMode(0, 0, 0,
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
    if (!msgCenter().confirmGoingSeamless(hotKey))
        return false;

    return true;
}

void UIMachineLogicSeamless::initialize()
{
    /* Prepare required features: */
    prepareRequiredFeatures();

    /* Prepare console connections: */
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

    /* Update the multi screen layout: */
    m_pScreenLayout->update();

    /* Create machine window(s): */
    for (int cScreenId = 0; cScreenId < m_pScreenLayout->guestScreenCount(); ++cScreenId)
        addMachineWindow(UIMachineWindow::create(this, visualStateType(), cScreenId));

    /* Connect screen-layout change handler: */
    for (int i = 0; i < machineWindows().size(); ++i)
        connect(m_pScreenLayout, SIGNAL(screenLayoutChanged()),
                static_cast<UIMachineWindowSeamless*>(machineWindows()[i]), SLOT(sltPlaceOnScreen()));

    /* Remember what machine window(s) created: */
    setMachineWindowsCreated(true);
}

void UIMachineLogicSeamless::cleanupMachineWindows()
{
    /* Do not cleanup machine window(s) if not present: */
    if (!isMachineWindowsCreated())
        return;

    /* Cleanup machine window(s): */
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

