/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineLogicFullscreen class implementation
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
#include "VBoxProblemReporter.h"

#include "UIActionsPool.h"
#include "UIMachineLogicFullscreen.h"
#include "UIMachineWindow.h"
#include "UIMachineWindowFullscreen.h"
#include "UIMultiScreenLayout.h"
#include "UISession.h"


#ifdef Q_WS_MAC
# include "VBoxUtils.h"
# include <Carbon/Carbon.h>
#endif /* Q_WS_MAC */

UIMachineLogicFullscreen::UIMachineLogicFullscreen(QObject *pParent, UISession *pSession, UIActionsPool *pActionsPool)
    : UIMachineLogic(pParent, pSession, pActionsPool, UIVisualStateType_Fullscreen)
{
    m_pScreenLayout = new UIMultiScreenLayout(this);
}

UIMachineLogicFullscreen::~UIMachineLogicFullscreen()
{
    /* Cleanup machine window: */
    cleanupMachineWindows();

    /* Cleanup action related stuff */
    cleanupActionGroups();

    delete m_pScreenLayout;
}

bool UIMachineLogicFullscreen::checkAvailability()
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
        vboxProblem().cannotEnterFullscreenMode();
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
            int result = vboxProblem().cannotEnterFullscreenMode(0, 0, 0,
                                                                 (((usedBits + 7) / 8 + _1M - 1) / _1M) * _1M);
            if (result == QIMessageBox::Cancel)
                return false;
        }
    }

    /* Take the toggle hot key from the menu item. Since
     * VBoxGlobal::extractKeyFromActionText gets exactly the
     * linked key without the 'Host+' part we are adding it here. */
    QString hotKey = QString("Host+%1")
        .arg(VBoxGlobal::extractKeyFromActionText(actionsPool()->action(UIActionIndex_Toggle_Fullscreen)->text()));
    Assert(!hotKey.isEmpty());

    /* Show the info message. */
    if (!vboxProblem().confirmGoingFullscreen(hotKey))
        return false;

    return true;
}

void UIMachineLogicFullscreen::initialize()
{
    /* Prepare required features: */
    prepareRequiredFeatures();

#ifdef Q_WS_MAC
    /* Prepare common connections: */
    prepareCommonConnections();
#endif /* Q_WS_MAC */

    /* Prepare console connections: */
    prepareSessionConnections();

    /* Prepare action groups:
     * Note: This has to be done before prepareActionConnections
     * cause here actions/menus are recreated. */
    prepareActionGroups();

    /* Prepare action connections: */
    prepareActionConnections();

    /* Prepare machine window: */
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

int UIMachineLogicFullscreen::hostScreenForGuestScreen(int screenId) const
{
    return m_pScreenLayout->hostScreenForGuestScreen(screenId);
}

#ifdef Q_WS_MAC
void UIMachineLogicFullscreen::prepareCommonConnections()
{
    /* Presentation mode connection */
    connect (&vboxGlobal(), SIGNAL(presentationModeChanged(const VBoxChangePresentationModeEvent &)),
             this, SLOT(sltChangePresentationMode(const VBoxChangePresentationModeEvent &)));
}
#endif /* Q_WS_MAC */

void UIMachineLogicFullscreen::prepareActionGroups()
{
    /* Base class action groups: */
    UIMachineLogic::prepareActionGroups();

    /* Adjust-window action isn't allowed in fullscreen: */
    actionsPool()->action(UIActionIndex_Simple_AdjustWindow)->setVisible(false);

    /* Add the view menu: */
    QMenu *pMenu = actionsPool()->action(UIActionIndex_Menu_View)->menu();
    m_pScreenLayout->initialize(pMenu);
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
    for (int screenId = 0; screenId < m_pScreenLayout->guestScreenCount(); ++screenId)
        addMachineWindow(UIMachineWindow::create(this, visualStateType(), screenId));

    /* Connect screen-layout change handler: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        connect(m_pScreenLayout, SIGNAL(screenLayoutChanged()),
                static_cast<UIMachineWindowFullscreen*>(pMachineWindow), SLOT(sltPlaceOnScreen()));

#ifdef Q_WS_MAC
    /* Note: Presentation mode has to be set *after* the windows are created. */
    setPresentationModeEnabled(true);
#endif /* Q_WS_MAC */

    /* Remember what machine window(s) created: */
    setMachineWindowsCreated(true);
}

void UIMachineLogicFullscreen::cleanupMachineWindows()
{
    /* Do not cleanup machine window if it is not present: */
    if (!isMachineWindowsCreated())
        return;

    /* Base class cleanup: */
    UIMachineLogic::cleanupMachineWindows();

    /* Cleanup normal machine window: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        UIMachineWindow::destroy(pMachineWindow);

#ifdef Q_WS_MAC
    setPresentationModeEnabled(false);
#endif/* Q_WS_MAC */
}

void UIMachineLogicFullscreen::cleanupActionGroups()
{
    /* Reenable adjust-window action: */
    actionsPool()->action(UIActionIndex_Simple_AdjustWindow)->setVisible(true);
}

#ifdef Q_WS_MAC
void UIMachineLogicFullscreen::sltChangePresentationMode(const VBoxChangePresentationModeEvent & /* event */)
{
    setPresentationModeEnabled(true);
}

void UIMachineLogicFullscreen::setPresentationModeEnabled(bool fEnabled)
{
    if (fEnabled)
    {
        /* First check if we are on the primary screen, only than the
         * presentation mode have to be changed. */
        // TODO_NEW_CORE: we need some algorithm to decide which virtual screen
        // is on which physical host display. Than we can decide on the
        // presentation mode as well. */
//        QDesktopWidget* pDesktop = QApplication::desktop();
//        if (pDesktop->screenNumber(this) == pDesktop->primaryScreen())
        {
            QString testStr = vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_PresentationModeEnabled).toLower();
            /* Default to false if it is an empty value */
            if (testStr.isEmpty() || testStr == "false")
                SetSystemUIMode(kUIModeAllHidden, 0);
            else
                SetSystemUIMode(kUIModeAllSuppressed, 0);
        }
    }
    else
        SetSystemUIMode(kUIModeNormal, 0);
}
#endif /* Q_WS_MAC */

