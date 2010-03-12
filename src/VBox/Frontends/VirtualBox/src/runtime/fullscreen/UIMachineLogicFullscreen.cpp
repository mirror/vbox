/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineLogicFullscreen class implementation
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
#include "UIMachineLogicFullscreen.h"
#include "UIMachineWindow.h"

#include "VBoxUtils.h"

#ifdef Q_WS_MAC
# ifdef QT_MAC_USE_COCOA
#  include <Carbon/Carbon.h>
# endif /* QT_MAC_USE_COCOA */
#endif /* Q_WS_MAC */

UIMachineLogicFullscreen::UIMachineLogicFullscreen(QObject *pParent, UISession *pSession, UIActionsPool *pActionsPool)
    : UIMachineLogic(pParent, pSession, pActionsPool, UIVisualStateType_Fullscreen)
{
}

UIMachineLogicFullscreen::~UIMachineLogicFullscreen()
{
    /* Cleanup machine window: */
    cleanupMachineWindows();

    /* Cleanup action related stuff */
    cleanupActionGroups();
}

bool UIMachineLogicFullscreen::checkAvailability()
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
        vboxProblem().cannotEnterFullscreenMode();
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
            QRect screen = QApplication::desktop()->screenGeometry(i);
            usedBits += screen.width() /* display width */
                      * screen.height() /* display height */
                      * guestBpp
                      + _1M * 8; /* current cache per screen - may be changed in future */
        }
        usedBits += 4096 * 8; /* adapter info */

        if (availBits < usedBits)
        {
//            int result = vboxProblem().cannotEnterFullscreenMode(screen.width(), screen.height(), guestBpp,
//                                                                 (((usedBits + 7) / 8 + _1M - 1) / _1M) * _1M);
            int result = vboxProblem().cannotEnterFullscreenMode(0, 0, guestBpp,
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
    /* Check the status of required features: */
    prepareRequiredFeatures();

    /* If required features are ready: */
    if (!isPreventAutoStart())
    {
#ifdef Q_WS_MAC
        /* Prepare common connections: */
        prepareCommonConnections();
#endif /* Q_WS_MAC */

        /* Prepare console connections: */
        prepareSessionConnections();

        /* Prepare action connections: */
        prepareActionConnections();

        /* Prepare action groups: */
        prepareActionGroups();

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
    setPresentationModeEnabled(true);
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
}

void UIMachineLogicFullscreen::cleanupMachineWindows()
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
