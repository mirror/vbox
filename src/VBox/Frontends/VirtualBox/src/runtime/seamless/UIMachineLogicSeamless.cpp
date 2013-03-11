/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineLogicSeamless class implementation
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QDesktopWidget>

/* GUI includes: */
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogicSeamless.h"
#include "UIMachineWindowSeamless.h"
#include "UIMultiScreenLayout.h"
#ifdef Q_WS_MAC
# include "VBoxUtils.h"
#endif /* Q_WS_MAC */

UIMachineLogicSeamless::UIMachineLogicSeamless(QObject *pParent, UISession *pSession)
    : UIMachineLogic(pParent, pSession, UIVisualStateType_Seamless)
{
    /* Create multiscreen layout: */
    m_pScreenLayout = new UIMultiScreenLayout(this);
}

UIMachineLogicSeamless::~UIMachineLogicSeamless()
{
    /* Delete multiscreen layout: */
    delete m_pScreenLayout;
}

bool UIMachineLogicSeamless::checkAvailability()
{
    /* Temporary get a machine object: */
    const CMachine &machine = uisession()->session().GetMachine();

#if 0
    /* Check that there are enough physical screens are connected: */
    int cHostScreens = m_pScreenLayout->hostScreenCount();
    int cGuestScreens = m_pScreenLayout->guestScreenCount();
    if (cHostScreens < cGuestScreens)
    {
        msgCenter().cannotEnterSeamlessMode();
        return false;
    }
#endif

    /* Check if there is enough physical memory to enter seamless: */
    if (uisession()->isGuestAdditionsActive())
    {
        quint64 availBits = machine.GetVRAMSize() /* VRAM */ * _1M /* MiB to bytes */ * 8 /* to bits */;
        quint64 usedBits = m_pScreenLayout->memoryRequirements();
        if (availBits < usedBits)
        {
            msgCenter().cannotEnterSeamlessMode(0, 0, 0,
                                                (((usedBits + 7) / 8 + _1M - 1) / _1M) * _1M);
            return false;
        }
    }

    /* Take the toggle hot key from the menu item.
     * Since VBoxGlobal::extractKeyFromActionText gets exactly
     * the linked key without the 'Host+' part we are adding it here. */
    QString hotKey = QString("Host+%1")
        .arg(VBoxGlobal::extractKeyFromActionText(gActionPool->action(UIActionIndexRuntime_Toggle_Seamless)->text()));
    Assert(!hotKey.isEmpty());

    /* Show the info message. */
    if (!msgCenter().confirmGoingSeamless(hotKey))
        return false;

    return true;
}

int UIMachineLogicSeamless::hostScreenForGuestScreen(int iScreenId) const
{
    return m_pScreenLayout->hostScreenForGuestScreen(iScreenId);
}

bool UIMachineLogicSeamless::hasHostScreenForGuestScreen(int iScreenId) const
{
    return m_pScreenLayout->hasHostScreenForGuestScreen(iScreenId);
}

void UIMachineLogicSeamless::sltGuestMonitorChange(KGuestMonitorChangedEventType changeType, ulong uScreenId, QRect screenGeo)
{
    /* Update multi-screen layout before any window update: */
    if (changeType == KGuestMonitorChangedEventType_Enabled ||
        changeType == KGuestMonitorChangedEventType_Disabled)
        m_pScreenLayout->rebuild();

    /* Call to base-class: */
    UIMachineLogic::sltGuestMonitorChange(changeType, uScreenId, screenGeo);
}

void UIMachineLogicSeamless::sltHostScreenCountChanged(int cScreenCount)
{
    /* Update multi-screen layout before any window update: */
    m_pScreenLayout->rebuild();

    /* Call to base-class: */
    UIMachineLogic::sltHostScreenCountChanged(cScreenCount);
}

void UIMachineLogicSeamless::prepareActionGroups()
{
    /* Call to base-class: */
    UIMachineLogic::prepareActionGroups();

    /* Guest auto-resize isn't allowed in seamless: */
    gActionPool->action(UIActionIndexRuntime_Toggle_GuestAutoresize)->setVisible(false);

    /* Adjust-window isn't allowed in seamless: */
    gActionPool->action(UIActionIndexRuntime_Simple_AdjustWindow)->setVisible(false);

    /* Disable mouse-integration isn't allowed in seamless: */
    gActionPool->action(UIActionIndexRuntime_Toggle_MouseIntegration)->setVisible(false);
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
    for (uint cScreenId = 0; cScreenId < session().GetMachine().GetMonitorCount(); ++cScreenId)
        addMachineWindow(UIMachineWindow::create(this, cScreenId));

    /* Connect screen-layout change handler: */
    for (int i = 0; i < machineWindows().size(); ++i)
        connect(m_pScreenLayout, SIGNAL(sigScreenLayoutChanged()),
                static_cast<UIMachineWindowSeamless*>(machineWindows()[i]), SLOT(sltShowInNecessaryMode()));

    /* Remember what machine window(s) created: */
    setMachineWindowsCreated(true);
}

void UIMachineLogicSeamless::prepareMenu()
{
    /* Call to base-class: */
    UIMachineLogic::prepareMenu();

    /* Finally update view-menu: */
    m_pScreenLayout->setViewMenu(gActionPool->action(UIActionIndexRuntime_Menu_View)->menu());
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
    /* Call to base-class: */
    UIMachineLogic::cleanupActionGroups();

    /* Reenable guest-autoresize action: */
    gActionPool->action(UIActionIndexRuntime_Toggle_GuestAutoresize)->setVisible(true);

    /* Reenable adjust-window action: */
    gActionPool->action(UIActionIndexRuntime_Simple_AdjustWindow)->setVisible(true);

    /* Reenable mouse-integration action: */
    gActionPool->action(UIActionIndexRuntime_Toggle_MouseIntegration)->setVisible(true);
}

