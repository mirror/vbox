/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowSeamless class implementation
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

/* Global includes: */
#include <QDesktopWidget>
#include <QTimer>
#ifdef Q_WS_MAC
# include <QMenuBar>
#endif /* Q_WS_MAC */

/* Local includes: */
#include "VBoxGlobal.h"
#ifndef Q_WS_MAC
# include "VBoxMiniToolBar.h"
#endif /* !Q_WS_MAC */
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogicSeamless.h"
#include "UIMachineWindowSeamless.h"
#include "UIMachineViewSeamless.h"
#ifdef Q_WS_MAC
# include "VBoxUtils.h"
#endif /* Q_WS_MAC */

UIMachineWindowSeamless::UIMachineWindowSeamless(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : UIMachineWindow(pMachineLogic, uScreenId)
    , m_pMainMenu(0)
#ifndef Q_WS_MAC
    , m_pMiniToolBar(0)
#endif /* !Q_WS_MAC */
{
}

#ifndef Q_WS_MAC
void UIMachineWindowSeamless::sltMachineStateChanged()
{
    /* Call to base-class: */
    UIMachineWindow::sltMachineStateChanged();

    /* Update mini-toolbar: */
    updateAppearanceOf(UIVisualElement_MiniToolBar);
}
#endif /* !Q_WS_MAC */

void UIMachineWindowSeamless::sltPlaceOnScreen()
{
    /* Get corresponding screen: */
    int iScreen = qobject_cast<UIMachineLogicSeamless*>(machineLogic())->hostScreenForGuestScreen(m_uScreenId);
    /* Calculate working area: */
    QRect workingArea = vboxGlobal().availableGeometry(iScreen);
    /* Move to the appropriate position: */
    move(workingArea.topLeft());
    /* Resize to the appropriate size: */
    resize(workingArea.size());
    /* Process pending move & resize events: */
    qApp->processEvents();
}

void UIMachineWindowSeamless::sltPopupMainMenu()
{
    /* Popup main-menu if present: */
    if (m_pMainMenu && !m_pMainMenu->isEmpty())
    {
        m_pMainMenu->popup(geometry().center());
        QTimer::singleShot(0, m_pMainMenu, SLOT(sltSelectFirstAction()));
    }
}

#ifndef Q_WS_MAC
void UIMachineWindowSeamless::sltUpdateMiniToolBarMask()
{
    if (m_pMiniToolBar && machineView())
        setMask(qobject_cast<UIMachineViewSeamless*>(machineView())->lastVisibleRegion());
}
#endif /* !Q_WS_MAC */

void UIMachineWindowSeamless::prepareMenu()
{
    /* Call to base-class: */
    UIMachineWindow::prepareMenu();

    /* Prepare menu: */
#ifdef Q_WS_MAC
    setMenuBar(uisession()->newMenuBar());
#endif /* Q_WS_MAC */
    m_pMainMenu = uisession()->newMenu();
}

void UIMachineWindowSeamless::prepareVisualState()
{
    /* Call to base-class: */
    UIMachineWindow::prepareVisualState();

    /* This might be required to correctly mask: */
    centralWidget()->setAutoFillBackground(false);

#ifdef Q_WS_WIN
    /* Get corresponding screen: */
    int iScreen = qobject_cast<UIMachineLogicSeamless*>(machineLogic())->hostScreenForGuestScreen(m_uScreenId);
    /* Prepare previous region: */
    m_prevRegion = vboxGlobal().availableGeometry(iScreen);
#endif /* Q_WS_WIN */

#ifdef Q_WS_MAC
    /* Please note: All the stuff below has to be done after the window has
     * switched to fullscreen. Qt changes the winId on the fullscreen
     * switch and make this stuff useless with the old winId. So please be
     * careful on rearrangement of the method calls. */
    ::darwinSetShowsWindowTransparent(this, true);
#endif /* Q_WS_MAC */

#ifndef Q_WS_MAC
    /* Prepare mini-toolbar: */
    prepareMiniToolbar();
#endif /* !Q_WS_MAC */
}

#ifndef Q_WS_MAC
void UIMachineWindowSeamless::prepareMiniToolbar()
{
    /* Get machine: */
    CMachine m = machine();

    /* Make sure mini-toolbar is necessary: */
    bool fIsActive = m.GetExtraData(VBoxDefs::GUI_ShowMiniToolBar) != "no";
    if (!fIsActive)
        return;

    /* Get the mini-toolbar alignment: */
    bool fIsAtTop = m.GetExtraData(VBoxDefs::GUI_MiniToolBarAlignment) == "top";
    /* Get the mini-toolbar auto-hide feature availability: */
    bool fIsAutoHide = m.GetExtraData(VBoxDefs::GUI_MiniToolBarAutoHide) != "off";
    m_pMiniToolBar = new VBoxMiniToolBar(centralWidget(),
                                         fIsAtTop ? VBoxMiniToolBar::AlignTop : VBoxMiniToolBar::AlignBottom,
                                         true, fIsAutoHide);
    m_pMiniToolBar->setSeamlessMode(true);
    m_pMiniToolBar->updateDisplay(true, true);
    QList<QMenu*> menus;
    QList<QAction*> actions = uisession()->newMenu()->actions();
    for (int i=0; i < actions.size(); ++i)
        menus << actions.at(i)->menu();
    *m_pMiniToolBar << menus;
    connect(m_pMiniToolBar, SIGNAL(minimizeAction()), this, SLOT(showMinimized()));
    connect(m_pMiniToolBar, SIGNAL(exitAction()),
            gActionPool->action(UIActionIndexRuntime_Toggle_Seamless), SLOT(trigger()));
    connect(m_pMiniToolBar, SIGNAL(closeAction()),
            gActionPool->action(UIActionIndexRuntime_Simple_Close), SLOT(trigger()));
    connect(m_pMiniToolBar, SIGNAL(geometryUpdated()), this, SLOT(sltUpdateMiniToolBarMask()));
}
#endif /* !Q_WS_MAC */

#ifdef Q_WS_MAC
void UIMachineWindowSeamless::loadSettings()
{
    /* Call to base-class: */
    UIMachineWindow::loadSettings();

    /* Load global settings: */
    {
        VBoxGlobalSettings settings = vboxGlobal().settings();
        menuBar()->setHidden(settings.isFeatureActive("noMenuBar"));
    }
}
#endif /* Q_WS_MAC */

#ifndef Q_WS_MAC
void UIMachineWindowSeamless::cleanupMiniToolbar()
{
    /* Make sure mini-toolbar was created: */
    if (!m_pMiniToolBar)
        return;

    /* Save mini-toolbar settings: */
    machine().SetExtraData(VBoxDefs::GUI_MiniToolBarAutoHide, m_pMiniToolBar->isAutoHide() ? QString() : "off");
    /* Delete mini-toolbar: */
    delete m_pMiniToolBar;
    m_pMiniToolBar = 0;
}
#endif /* !Q_WS_MAC */

void UIMachineWindowSeamless::cleanupVisualState()
{
#ifndef Q_WS_MAC
    /* Cleeanup mini-toolbar: */
    cleanupMiniToolbar();
#endif /* !Q_WS_MAC */

    /* Call to base-class: */
    UIMachineWindow::cleanupVisualState();
}

void UIMachineWindowSeamless::cleanupMenu()
{
    /* Cleanup menu: */
    delete m_pMainMenu;
    m_pMainMenu = 0;

    /* Call to base-class: */
    UIMachineWindow::cleanupMenu();
}

void UIMachineWindowSeamless::showInNecessaryMode()
{
    /* Make sure we really have to show window: */
    BOOL fEnabled = true;
    ULONG guestOriginX = 0, guestOriginY = 0, guestWidth = 0, guestHeight = 0;
    machine().QuerySavedGuestScreenInfo(m_uScreenId, guestOriginX, guestOriginY, guestWidth, guestHeight, fEnabled);
    if (fEnabled)
    {
        /* Show manually maximized window: */
        sltPlaceOnScreen();

        /* Show normal window: */
        show();

#ifdef Q_WS_MAC
        /* Make sure it is really on the right place (especially on the Mac): */
        int iScreen = qobject_cast<UIMachineLogicSeamless*>(machineLogic())->hostScreenForGuestScreen(m_uScreenId);
        QRect r = vboxGlobal().availableGeometry(iScreen);
        move(r.topLeft());
#endif /* Q_WS_MAC */
    }
}

#ifndef Q_WS_MAC
void UIMachineWindowSeamless::updateAppearanceOf(int iElement)
{
    /* Call to base-class: */
    UIMachineWindow::updateAppearanceOf(iElement);

    /* Update mini-toolbar: */
    if (iElement & UIVisualElement_MiniToolBar)
    {
        if (m_pMiniToolBar)
        {
            /* Get machine: */
            const CMachine &m = machine();
            /* Get snapshot(s): */
            QString strSnapshotName;
            if (m.GetSnapshotCount() > 0)
            {
                CSnapshot snapshot = m.GetCurrentSnapshot();
                strSnapshotName = " (" + snapshot.GetName() + ")";
            }
            /* Update mini-toolbar text: */
            m_pMiniToolBar->setDisplayText(m.GetName() + strSnapshotName);
        }
    }
}
#endif /* !Q_WS_MAC */

#ifdef Q_WS_MAC
bool UIMachineWindowSeamless::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case QEvent::Paint:
        {
            /* Clear the background */
            CGContextClearRect(::darwinToCGContextRef(this), ::darwinToCGRect(frameGeometry()));
            break;
        }
        default:
            break;
    }
    return UIMachineWindow::event(pEvent);
}
#endif /* Q_WS_MAC */

void UIMachineWindowSeamless::setMask(const QRegion &constRegion)
{
    QRegion region = constRegion;

    /* Shift region if left spacer width is NOT zero or top spacer height is NOT zero: */
    if (m_pLeftSpacer->geometry().width() || m_pTopSpacer->geometry().height())
        region.translate(m_pLeftSpacer->geometry().width(), m_pTopSpacer->geometry().height());

#if 0 // TODO: Is it really needed now?
    /* The global mask shift cause of toolbars and such things. */
    region.translate(mMaskShift.width(), mMaskShift.height());
#endif

    /* Mini tool-bar: */
#ifndef Q_WS_MAC
    if (m_pMiniToolBar)
    {
        /* Get mini-toolbar mask: */
        QRegion toolBarRegion(m_pMiniToolBar->rect());

        /* Move mini-toolbar mask to mini-toolbar position: */
        toolBarRegion.translate(QPoint(m_pMiniToolBar->x(), m_pMiniToolBar->y()));

        /* Including mini-toolbar mask: */
        region += toolBarRegion;
    }
#endif /* !Q_WS_MAC */

#if 0 // TODO: Is it really needed now?
    /* Restrict the drawing to the available space on the screen.
     * (The &operator is better than the previous used -operator,
     * because this excludes space around the real screen also.
     * This is necessary for the mac.) */
    region &= mStrictedRegion;
#endif

#ifdef Q_WS_WIN
    QRegion difference = m_prevRegion.subtract(region);

    /* Region offset calculation */
    int fleft = 0, ftop = 0;

    /* Visible region calculation */
    HRGN newReg = CreateRectRgn(0, 0, 0, 0);
    CombineRgn(newReg, region.handle(), 0, RGN_COPY);
    OffsetRgn(newReg, fleft, ftop);

    /* Invisible region calculation */
    HRGN diffReg = CreateRectRgn(0, 0, 0, 0);
    CombineRgn(diffReg, difference.handle(), 0, RGN_COPY);
    OffsetRgn(diffReg, fleft, ftop);

    /* Set the current visible region and clean the previous */
    SetWindowRgn(winId(), newReg, FALSE);
    RedrawWindow(0, 0, diffReg, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    if (machineView())
        RedrawWindow(machineView()->viewport()->winId(), 0, 0, RDW_INVALIDATE);

    m_prevRegion = region;
#elif defined (Q_WS_MAC)
# if defined (VBOX_GUI_USE_QUARTZ2D)
    if (vboxGlobal().vmRenderMode() == VBoxDefs::Quartz2DMode)
    {
        /* If we are using the Quartz2D backend we have to trigger
         * an repaint only. All the magic clipping stuff is done
         * in the paint engine. */
        ::darwinWindowInvalidateShape(m_pMachineView->viewport());
    }
    else
# endif
    {
        /* This is necessary to avoid the flicker by an mask update.
         * See http://lists.apple.com/archives/Carbon-development/2001/Apr/msg01651.html
         * for the hint.
         * There *must* be a better solution. */
        if (!region.isEmpty())
            region |= QRect (0, 0, 1, 1);
        // /* Save the current region for later processing in the darwin event handler. */
        // mCurrRegion = region;
        // /* We repaint the screen before the ReshapeCustomWindow command. Unfortunately
        //  * this command flushes a copy of the backbuffer to the screen after the new
        //  * mask is set. This leads into a misplaced drawing of the content. Currently
        //  * no alternative to this and also this is not 100% perfect. */
        // repaint();
        // qApp->processEvents();
        // /* Now force the reshaping of the window. This is definitely necessary. */
        // ReshapeCustomWindow (reinterpret_cast <WindowPtr> (winId()));
        UIMachineWindow::setMask(region);
        // HIWindowInvalidateShadow (::darwinToWindowRef (mConsole->viewport()));
    }
#else
    UIMachineWindow::setMask(region);
#endif
}

