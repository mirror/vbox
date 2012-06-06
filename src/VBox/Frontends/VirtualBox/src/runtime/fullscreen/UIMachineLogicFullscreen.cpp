/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineLogicFullscreen class implementation
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

/* Qt includes: */
#include <QDesktopWidget>

/* GUI includes: */
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogicFullscreen.h"
#include "UIMachineWindowFullscreen.h"
#include "UIMultiScreenLayout.h"
#ifdef Q_WS_MAC
# include "UIExtraDataEventHandler.h"
# include "VBoxUtils.h"
# include <Carbon/Carbon.h>
#endif /* Q_WS_MAC */

UIMachineLogicFullscreen::UIMachineLogicFullscreen(QObject *pParent, UISession *pSession)
    : UIMachineLogic(pParent, pSession, UIVisualStateType_Fullscreen)
{
    /* Create multiscreen layout: */
    m_pScreenLayout = new UIMultiScreenLayout(this);
}

UIMachineLogicFullscreen::~UIMachineLogicFullscreen()
{
    /* Delete multiscreen layout: */
    delete m_pScreenLayout;
}

bool UIMachineLogicFullscreen::checkAvailability()
{
    /* Temporary get a machine object: */
    const CMachine &machine = uisession()->session().GetMachine();

    /* Check that there are enough physical screens are connected: */
    int cHostScreens = m_pScreenLayout->hostScreenCount();
    int cGuestScreens = m_pScreenLayout->guestScreenCount();
    if (cHostScreens < cGuestScreens)
    {
        msgCenter().cannotEnterFullscreenMode();
        return false;
    }

    /* Check if there is enough physical memory to enter fullscreen: */
    if (uisession()->isGuestAdditionsActive())
    {
        quint64 availBits = machine.GetVRAMSize() /* VRAM */ * _1M /* MiB to bytes */ * 8 /* to bits */;
        quint64 usedBits = m_pScreenLayout->memoryRequirements();
        if (availBits < usedBits)
        {
            int result = msgCenter().cannotEnterFullscreenMode(0, 0, 0,
                                                               (((usedBits + 7) / 8 + _1M - 1) / _1M) * _1M);
            if (result == QIMessageBox::Cancel)
                return false;
        }
    }

    /* Take the toggle hot key from the menu item.
     * Since VBoxGlobal::extractKeyFromActionText gets exactly
     * the linked key without the 'Host+' part we are adding it here. */
    QString hotKey = QString("Host+%1")
        .arg(VBoxGlobal::extractKeyFromActionText(gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen)->text()));
    Assert(!hotKey.isEmpty());

    /* Show the info message. */
    if (!msgCenter().confirmGoingFullscreen(hotKey))
        return false;

    return true;
}

void UIMachineLogicFullscreen::prepare()
{
    /* Call to base-class: */
    UIMachineLogic::prepare();

#ifdef Q_WS_MAC
    /* Prepare fullscreen connections: */
    prepareFullscreenConnections();
#endif /* Q_WS_MAC */
}

int UIMachineLogicFullscreen::hostScreenForGuestScreen(int screenId) const
{
    return m_pScreenLayout->hostScreenForGuestScreen(screenId);
}

#ifdef Q_WS_MAC
void UIMachineLogicFullscreen::prepareFullscreenConnections()
{
    /* Presentation mode connection: */
    connect(gEDataEvents, SIGNAL(sigPresentationModeChange(bool)),
            this, SLOT(sltChangePresentationMode(bool)));
}
#endif /* Q_WS_MAC */

void UIMachineLogicFullscreen::prepareActionGroups()
{
    /* Call to base-class: */
    UIMachineLogic::prepareActionGroups();

    /* Adjust-window action isn't allowed in fullscreen: */
    gActionPool->action(UIActionIndexRuntime_Simple_AdjustWindow)->setVisible(false);

    /* Add the view menu: */
    QMenu *pMenu = gActionPool->action(UIActionIndexRuntime_Menu_View)->menu();
    m_pScreenLayout->initialize(pMenu);
    pMenu->setVisible(true);
}

void UIMachineLogicFullscreen::prepareMachineWindows()
{
    /* Do not create window(s) if they created already: */
    if (isMachineWindowsCreated())
        return;

#ifdef Q_WS_MAC // TODO: Is that "darwinSetFrontMostProcess" really need here?
    /* We have to make sure that we are getting the front most process.
     * This is necessary for Qt versions > 4.3.3: */
    ::darwinSetFrontMostProcess();
#endif /* Q_WS_MAC */

    /* Update the multi screen layout: */
    m_pScreenLayout->update();

    /* Create machine window(s): */
    for (int cScreenId = 0; cScreenId < m_pScreenLayout->guestScreenCount(); ++cScreenId)
        addMachineWindow(UIMachineWindow::create(this, cScreenId));

    /* Connect screen-layout change handler: */
    for (int i = 0; i < machineWindows().size(); ++i)
        connect(m_pScreenLayout, SIGNAL(screenLayoutChanged()),
                static_cast<UIMachineWindowFullscreen*>(machineWindows()[i]), SLOT(sltPlaceOnScreen()));

#ifdef Q_WS_MAC
    /* If the user change the screen, we have to decide again if the
     * presentation mode should be changed. */
    connect(m_pScreenLayout, SIGNAL(screenLayoutChanged()),
            this, SLOT(sltScreenLayoutChanged()));
    /* Note: Presentation mode has to be set *after* the windows are created. */
    setPresentationModeEnabled(true);
#endif /* Q_WS_MAC */

    /* Remember what machine window(s) created: */
    setMachineWindowsCreated(true);
}

void UIMachineLogicFullscreen::cleanupMachineWindows()
{
    /* Do not cleanup machine window(s) if not present: */
    if (!isMachineWindowsCreated())
        return;

    /* Cleanup machine window(s): */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        UIMachineWindow::destroy(pMachineWindow);

#ifdef Q_WS_MAC
    setPresentationModeEnabled(false);
#endif/* Q_WS_MAC */
}

void UIMachineLogicFullscreen::cleanupActionGroups()
{
    /* Call to base-class: */
    UIMachineLogic::cleanupActionGroups();

    /* Reenable adjust-window action: */
    gActionPool->action(UIActionIndexRuntime_Simple_AdjustWindow)->setVisible(true);
}

#ifdef Q_WS_MAC
void UIMachineLogicFullscreen::sltChangePresentationMode(bool /* fEnabled */)
{
    setPresentationModeEnabled(true);
}

void UIMachineLogicFullscreen::sltScreenLayoutChanged()
{
    setPresentationModeEnabled(true);
}

void UIMachineLogicFullscreen::setPresentationModeEnabled(bool fEnabled)
{
    /* First check if we are on a screen which contains the Dock or the
     * Menubar (which hasn't to be the same), only than the
     * presentation mode have to be changed. */
    if (   fEnabled
        && m_pScreenLayout->isHostTaskbarCovert())
    {
        QString testStr = vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_PresentationModeEnabled).toLower();
        /* Default to false if it is an empty value */
        if (testStr.isEmpty() || testStr == "false")
            SetSystemUIMode(kUIModeAllHidden, 0);
        else
            SetSystemUIMode(kUIModeAllSuppressed, 0);
    }
    else
        SetSystemUIMode(kUIModeNormal, 0);
}
#endif /* Q_WS_MAC */

