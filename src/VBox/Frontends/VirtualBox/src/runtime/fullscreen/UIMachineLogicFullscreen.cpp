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
    /* Temporary get a machine object */
    const CMachine &machine = uisession()->session().GetMachine();
    const CConsole &console = uisession()->session().GetConsole();

#if (QT_VERSION >= 0x040600)
    int cHostScreens = QApplication::desktop()->screenCount();
#else /* (QT_VERSION >= 0x040600) */
    int cHostScreens = QApplication::desktop()->numScreens();
#endif /* !(QT_VERSION >= 0x040600) */

    int cGuestScreens = machine.GetMonitorCount();
    /* Check that there are enough physical screens are connected */
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
    QString hotKey = QString ("Host+%1")
        .arg (VBoxGlobal::extractKeyFromActionText(actionsPool()->action(UIActionIndex_Toggle_Fullscreen)->text()));
    Assert (!hotKey.isEmpty());

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
        /* Prepare console connections: */
        prepareConsoleConnections();

        /* Prepare action groups: */
        prepareActionGroups();

        /* Prepare action connections: */
        prepareActionConnections();

        /* Prepare machine window: */
        prepareMachineWindows();

#ifdef Q_WS_MAC
        /* Prepare dock: */
        prepareDock();
#endif /* Q_WS_MAC */


        /* Initialization: */
        sltMachineStateChanged();
        sltAdditionsStateChanged();
        sltMouseCapabilityChanged();
    }
}

#ifdef Q_WS_MAC
void UIMachineLogicFullscreen::prepareConsoleConnections()
{
    /* Base class connections: */
    UIMachineLogic::prepareConsoleConnections();

    /* Presentation mode connection */
    connect (&vboxGlobal(), SIGNAL(presentationModeChanged(const VBoxChangePresentationModeEvent &)),
             this, SLOT(sltChangePresentationMode(const VBoxChangePresentationModeEvent &)));
}
#endif /* Q_WS_MAC */

void UIMachineLogicFullscreen::prepareActionGroups()
{
    /* Base class action groups: */
    UIMachineLogic::prepareActionGroups();

    /* Adjust window isn't allowed in fullscreen */
    actionsPool()->action(UIActionIndex_Simple_AdjustWindow)->setEnabled(false);
}

void UIMachineLogicFullscreen::prepareMachineWindows()
{
    /* Get monitor count: */
    ulong uMonitorCount = session().GetMachine().GetMonitorCount();

    /* Do not create window(s) if created already: */
    if ((ulong)machineWindows().size() == uMonitorCount)
        return;

    /* List should not be filled partially: */
    AssertMsg(machineWindows().size() == 0, ("Windows list is filled partially, something broken!\n"));

#ifdef Q_WS_MAC
    /* We have to make sure that we are getting the front most process.
     * This is necessary for Qt versions > 4.3.3: */
    ::darwinSetFrontMostProcess();
    setPresentationModeEnabled(true);
#endif /* Q_WS_MAC */

#if 0 // TODO: Add seamless multi-monitor support!
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

    /* Notify others about machine window(s) created: */
    setMachineWindowsCreated(true);

    /* If we are not started yet: */
    if (!uisession()->isRunning() && !uisession()->isPaused())
    {
        prepareConsolePowerUp();

        /* Get current machine/console: */
        CMachine machine = session().GetMachine();
        CConsole console = session().GetConsole();

        /* Start VM: */
        CProgress progress = vboxGlobal().isStartPausedEnabled() || vboxGlobal().isDebuggerAutoShowEnabled() ?
                             console.PowerUpPaused() : console.PowerUp();

#if 0 // TODO: Check immediate failure!
        /* Check for an immediate failure: */
        if (!console.isOk())
        {
            vboxProblem().cannotStartMachine(console);
            machineWindowWrapper()->machineWindow()->close();
            return;
        }

        /* Disable auto-closure because we want to have a chance to show the error dialog on startup failure: */
        setPreventAutoClose(true);
#endif

        /* Show "Starting/Restoring" progress dialog: */
        if (uisession()->isSaved())
            vboxProblem().showModalProgressDialog(progress, machine.GetName(), defaultMachineWindow()->machineWindow(), 0);
        else
            vboxProblem().showModalProgressDialog(progress, machine.GetName(), defaultMachineWindow()->machineWindow());

#if 0 // TODO: Check immediate failure!
        /* Check for an progress failure */
        if (progress.GetResultCode() != 0)
        {
            vboxProblem().cannotStartMachine(progress);
            machineWindowWrapper()->machineWindow()->close();
            return;
        }

        /* Enable auto-closure again: */
        setPreventAutoClose(false);

        /* Check if we missed a really quick termination after successful startup, and process it if we did: */
        if (uisession()->isTurnedOff())
        {
            machineWindowWrapper()->machineWindow()->close();
            return;
        }
#endif

#if 0 // TODO: Rework debugger logic!
# ifdef VBOX_WITH_DEBUGGER_GUI
        /* Open the debugger in "full screen" mode requested by the user. */
        else if (vboxGlobal().isDebuggerAutoShowEnabled())
        {
            /* console in upper left corner of the desktop. */
            QRect rct (0, 0, 0, 0);
            QDesktopWidget *desktop = QApplication::desktop();
            if (desktop)
                rct = desktop->availableGeometry(pos());
            move (QPoint (rct.x(), rct.y()));

            if (vboxGlobal().isDebuggerAutoShowStatisticsEnabled())
                sltShowDebugStatistics();
            if (vboxGlobal().isDebuggerAutoShowCommandLineEnabled())
                sltShowDebugCommandLine();

            if (!vboxGlobal().isStartPausedEnabled())
                machineWindowWrapper()->machineView()->pause (false);
        }
# endif
#endif

#ifdef VBOX_WITH_UPDATE_REQUEST
        /* Check for updates if necessary: */
        vboxGlobal().showUpdateDialog(false /* force request? */);
#endif
    }
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
    /* Reenable adjust window */
    actionsPool()->action(UIActionIndex_Simple_AdjustWindow)->setEnabled(true);
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
