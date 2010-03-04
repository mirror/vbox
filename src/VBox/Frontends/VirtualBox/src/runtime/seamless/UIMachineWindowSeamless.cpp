/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowSeamless class implementation
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
#ifdef Q_WS_MAC
# include <QMenuBar>
#endif /* Q_WS_MAC */

/* Local includes */
#include "VBoxGlobal.h"

#ifdef Q_WS_MAC
# include "UISession.h"
#endif /* Q_WS_MAC */
#include "UIMachineLogic.h"
#include "UIMachineView.h"
#include "UIMachineWindowSeamless.h"

UIMachineWindowSeamless::UIMachineWindowSeamless(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : QIWithRetranslateUI2<QIMainDialog>(0, Qt::FramelessWindowHint)
    , UIMachineWindow(pMachineLogic, uScreenId)
{
    /* "This" is machine window: */
    m_pMachineWindow = this;

    /* Prepare window icon: */
    prepareWindowIcon();

    /* Prepare console connections: */
    prepareConsoleConnections();

    /* Prepare seamless window: */
    prepareSeamless();

#ifdef Q_WS_MAC
    /* Prepare menu: */
    prepareMenu();
#endif /* Q_WS_MAC */

    /* Retranslate seamless window finally: */
    retranslateUi();

    /* Prepare normal machine view container: */
    prepareMachineViewContainer();

    /* Prepare normal machine view: */
    prepareMachineView();

#ifdef Q_WS_MAC
    /* Load normal window settings: */
    loadWindowSettings();
#endif /* Q_WS_MAC */

    /* Update all the elements: */
    updateAppearanceOf(UIVisualElement_AllStuff);

    /* Show window: */
    show();
}

UIMachineWindowSeamless::~UIMachineWindowSeamless()
{
    /* Cleanup normal machine view: */
    cleanupMachineView();
}

void UIMachineWindowSeamless::sltMachineStateChanged()
{
    UIMachineWindow::sltMachineStateChanged();
}

void UIMachineWindowSeamless::sltTryClose()
{
    UIMachineWindow::sltTryClose();
}

void UIMachineWindowSeamless::retranslateUi()
{
    /* Translate parent class: */
    UIMachineWindow::retranslateUi();

#ifdef Q_WS_MAC
    // TODO_NEW_CORE
//    m_pDockSettingsMenu->setTitle(tr("Dock Icon"));
//    m_pDockDisablePreview->setText(tr("Show Application Icon"));
//    m_pDockEnablePreviewMonitor->setText(tr("Show Monitor Preview"));
#endif /* Q_WS_MAC */
}

#ifdef Q_WS_X11
bool UIMachineWindowSeamless::x11Event(XEvent *pEvent)
{
    /* Qt bug: when the console view grabs the keyboard, FocusIn, FocusOut,
     * WindowActivate and WindowDeactivate Qt events are not properly sent
     * on top level window (i.e. this) deactivation. The fix is to substiute
     * the mode in FocusOut X11 event structure to NotifyNormal to cause
     * Qt to process it as desired. */
    if (pEvent->type == FocusOut)
    {
        if (pEvent->xfocus.mode == NotifyWhileGrabbed  &&
            (pEvent->xfocus.detail == NotifyAncestor ||
             pEvent->xfocus.detail == NotifyInferior ||
             pEvent->xfocus.detail == NotifyNonlinear))
        {
             pEvent->xfocus.mode = NotifyNormal;
        }
    }
    return false;
}
#endif

void UIMachineWindowSeamless::closeEvent(QCloseEvent *pEvent)
{
    return UIMachineWindow::closeEvent(pEvent);
}

void UIMachineWindowSeamless::prepareSeamless()
{
    /* Move & resize seamless frameless window: */
    QRect geometry = QApplication::desktop()->availableGeometry();
#ifdef Q_WS_WIN
    m_prevRegion = geometry;
#endif
    move(geometry.topLeft());
    resize(geometry.size());
    /* Perform these events: */
    qApp->processEvents();
}

#ifdef Q_WS_MAC
void UIMachineWindowSeamless::prepareMenu()
{
    setMenuBar(uisession()->newMenuBar());
}
#endif

void UIMachineWindowSeamless::prepareMachineView()
{
#ifdef VBOX_WITH_VIDEOHWACCEL
    /* Need to force the QGL framebuffer in case 2D Video Acceleration is supported & enabled: */
    bool bAccelerate2DVideo = session().GetMachine().GetAccelerate2DVideoEnabled() && VBoxGlobal::isAcceleration2DVideoAvailable();
#endif

    /* Set central widget: */
    setCentralWidget(new QWidget);

    /* Set central widget layout: */
    centralWidget()->setLayout(m_pMachineViewContainer);

    m_pMachineView = UIMachineView::create(  this
                                           , vboxGlobal().vmRenderMode()
#ifdef VBOX_WITH_VIDEOHWACCEL
                                           , bAccelerate2DVideo
#endif
                                           , machineLogic()->visualStateType()
                                           , m_uScreenId);

    /* Add machine view into layout: */
    m_pMachineViewContainer->addWidget(m_pMachineView, 1, 1);
}

#ifdef Q_WS_MAC
void UIMachineWindowSeamless::loadWindowSettings()
{
    /* Load global settings: */
    {
        VBoxGlobalSettings settings = vboxGlobal().settings();
        menuBar()->setHidden(settings.isFeatureActive("noMenuBar"));
    }
}
#endif

void UIMachineWindowSeamless::cleanupMachineView()
{
    /* Do not cleanup machine view if it is not present: */
    if (!machineView())
        return;

    UIMachineView::destroy(m_pMachineView);
    m_pMachineView = 0;
}

void UIMachineWindowSeamless::setMask(const QRegion &constRegion)
{
    QRegion region = constRegion;

#if 0 // TODO: Is it really needed now?
    /* The global mask shift cause of toolbars and such things. */
    region.translate(mMaskShift.width(), mMaskShift.height());
#endif

#if 0 // TODO: Add mini-toolbar support!
    /* Including mini toolbar area */
    QRegion toolBarRegion(mMiniToolBar->mask());
    toolBarRegion.translate(mMiniToolBar->mapToGlobal (toolBarRegion.boundingRect().topLeft()) - QPoint (1, 0));
    region += toolBarRegion;
#endif

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
    RedrawWindow(mConsole->viewport()->winId(), 0, 0, RDW_INVALIDATE);

    m_prevRegion = region;
#elif defined (Q_WS_MAC)
# if defined (VBOX_GUI_USE_QUARTZ2D)
    if (vboxGlobal().vmRenderMode() == VBoxDefs::Quartz2DMode)
    {
        /* If we are using the Quartz2D backend we have to trigger
         * an repaint only. All the magic clipping stuff is done
         * in the paint engine. */
        ::darwinWindowInvalidateShape (mConsole->viewport());
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
        //  * mask is set. This leads into a missplaced drawing of the content. Currently
        //  * no alternative to this and also this is not 100% perfect. */
        // repaint();
        // qApp->processEvents();
        // /* Now force the reshaping of the window. This is definitly necessary. */
        // ReshapeCustomWindow (reinterpret_cast <WindowPtr> (winId()));
        QMainWindow::setMask (region);
        // HIWindowInvalidateShadow (::darwinToWindowRef (mConsole->viewport()));
    }
#else
    QMainWindow::setMask(region);
#endif
}

void UIMachineWindowSeamless::clearMask()
{
#ifdef Q_WS_WIN
    SetWindowRgn(winId(), 0, TRUE);
#else
    QMainWindow::clearMask();
#endif
}

