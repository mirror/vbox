/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowFullscreen class implementation
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
#include <QMenuBar>
#include <QTimer>
#include <QContextMenuEvent>

/* Local includes */
#include "VBoxGlobal.h"

#include "UISession.h"
#include "UIActionsPool.h"
#include "UIMachineLogic.h"
#include "UIMachineView.h"
#include "UIMachineWindowFullscreen.h"

#ifdef Q_WS_MAC
# ifdef QT_MAC_USE_COCOA
#  include <Carbon/Carbon.h>
# endif /* QT_MAC_USE_COCOA */
#endif /* Q_WS_MAC */

UIMachineWindowFullscreen::UIMachineWindowFullscreen(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : QIWithRetranslateUI<QIMainDialog>(0)
    , UIMachineWindow(pMachineLogic, uScreenId)
{
    /* "This" is machine window: */
    m_pMachineWindow = this;

    /* Prepare window icon: */
    prepareWindowIcon();

    /* Prepare console connections: */
    prepareConsoleConnections();

    /* Prepare menu: */
    prepareMenu();

    /* Prepare connections: */
    prepareConnections();

    /* Retranslate normal window finally: */
    retranslateUi();

    /* Prepare machine view container: */
    prepareMachineViewContainer();

    /* Prepare normal machine view: */
    prepareMachineView();

    /* Load normal window settings: */
    loadWindowSettings();

    /* Update all the elements: */
    updateAppearanceOf(UIVisualElement_AllStuff);

    /* Show window: */
//    show();
}

UIMachineWindowFullscreen::~UIMachineWindowFullscreen()
{
    /* Save normal window settings: */
    saveWindowSettings();

    /* Cleanup normal machine view: */
    cleanupMachineView();
}

void UIMachineWindowFullscreen::sltMachineStateChanged()
{
    UIMachineWindow::sltMachineStateChanged();
}

void UIMachineWindowFullscreen::sltMediumChange(const CMediumAttachment &attachment)
{
    KDeviceType type = attachment.GetType();
    if (type == KDeviceType_HardDisk)
        updateAppearanceOf(UIVisualElement_HDStuff);
    if (type == KDeviceType_DVD)
        updateAppearanceOf(UIVisualElement_CDStuff);
    if (type == KDeviceType_Floppy)
        updateAppearanceOf(UIVisualElement_FDStuff);
}

void UIMachineWindowFullscreen::sltUSBControllerChange()
{
    updateAppearanceOf(UIVisualElement_USBStuff);
}

void UIMachineWindowFullscreen::sltUSBDeviceStateChange()
{
    updateAppearanceOf(UIVisualElement_USBStuff);
}

void UIMachineWindowFullscreen::sltNetworkAdapterChange()
{
    updateAppearanceOf(UIVisualElement_NetworkStuff);
}

void UIMachineWindowFullscreen::sltSharedFolderChange()
{
    updateAppearanceOf(UIVisualElement_SharedFolderStuff);
}

void UIMachineWindowFullscreen::sltTryClose()
{
    UIMachineWindow::sltTryClose();
}

void UIMachineWindowFullscreen::sltProcessGlobalSettingChange(const char * /* aPublicName */, const char * /* aName */)
{
}

void UIMachineWindowFullscreen::retranslateUi()
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

void UIMachineWindowFullscreen::updateAppearanceOf(int iElement)
{
    /* Update parent-class window: */
    UIMachineWindow::updateAppearanceOf(iElement);
}

bool UIMachineWindowFullscreen::event(QEvent *pEvent)
{
    return QIWithRetranslateUI<QIMainDialog>::event(pEvent);
    switch (pEvent->type())
    {
        case QEvent::Resize:
        {
            QResizeEvent *pResizeEvent = static_cast<QResizeEvent*>(pEvent);
            if (!isMaximized())
            {
                m_normalGeometry.setSize(pResizeEvent->size());
#ifdef VBOX_WITH_DEBUGGER_GUI
                // TODO: Update debugger window size!
                //dbgAdjustRelativePos();
#endif
            }
            break;
        }
        case QEvent::Move:
        {
            if (!isMaximized())
            {
                m_normalGeometry.moveTo(geometry().x(), geometry().y());
#ifdef VBOX_WITH_DEBUGGER_GUI
                // TODO: Update debugger window position!
                //dbgAdjustRelativePos();
#endif
            }
            break;
        }
#ifdef Q_WS_MAC
        case QEvent::Polish:
        {
            /* Fade back to the normal gamma */
//            CGDisplayFade (mFadeToken, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, false);
//            CGReleaseDisplayFadeReservation (mFadeToken);
            break;
        }
#endif /* Q_WS_MAC */
        default:
            break;
    }
    return QIWithRetranslateUI<QIMainDialog>::event(pEvent);
}

#ifdef Q_WS_X11
bool UIMachineWindowFullscreen::x11Event(XEvent *pEvent)
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

void UIMachineWindowFullscreen::closeEvent(QCloseEvent *pEvent)
{
    return UIMachineWindow::closeEvent(pEvent);
}

void UIMachineWindowFullscreen::prepareConsoleConnections()
{
    /* Base-class connections: */
    UIMachineWindow::prepareConsoleConnections();

    /* Medium change updater: */
    connect(machineLogic()->uisession(), SIGNAL(sigMediumChange(const CMediumAttachment &)),
            this, SLOT(sltMediumChange(const CMediumAttachment &)));

    /* USB controller change updater: */
    connect(machineLogic()->uisession(), SIGNAL(sigUSBControllerChange()),
            this, SLOT(sltUSBControllerChange()));

    /* USB device state-change updater: */
    connect(machineLogic()->uisession(), SIGNAL(sigUSBDeviceStateChange(const CUSBDevice &, bool, const CVirtualBoxErrorInfo &)),
            this, SLOT(sltUSBDeviceStateChange()));

    /* Network adapter change updater: */
    connect(machineLogic()->uisession(), SIGNAL(sigNetworkAdapterChange(const CNetworkAdapter &)),
            this, SLOT(sltNetworkAdapterChange()));

    /* Shared folder change updater: */
    connect(machineLogic()->uisession(), SIGNAL(sigSharedFolderChange()),
            this, SLOT(sltSharedFolderChange()));
}

void UIMachineWindowFullscreen::prepareMenu()
{
    setMenuBar(uisession()->newMenuBar());
    menuBar()->hide();
}

void UIMachineWindowFullscreen::prepareConnections()
{
    /* Setup global settings change updater: */
    connect(&vboxGlobal().settings(), SIGNAL(propertyChanged(const char *, const char *)),
            this, SLOT(sltProcessGlobalSettingChange(const char *, const char *)));
}

void UIMachineWindowFullscreen::prepareMachineView()
{
#ifdef VBOX_WITH_VIDEOHWACCEL
    /* Need to force the QGL framebuffer in case 2D Video Acceleration is supported & enabled: */
    bool bAccelerate2DVideo = session().GetMachine().GetAccelerate2DVideoEnabled() && VBoxGlobal::isAcceleration2DVideoAvailable();
#endif

    /* Set central widget: */
    setCentralWidget(new QWidget(this));

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
    m_pMachineViewContainer->addWidget(m_pMachineView, 1, 1, Qt::AlignVCenter | Qt::AlignHCenter);
}

void UIMachineWindowFullscreen::loadWindowSettings()
{
    /* Load normal window settings: */
    CMachine machine = session().GetMachine();

    /* Toggle console to manual resize mode. */
    m_pMachineView->setMachineWindowResizeIgnored(true);
    m_pMachineView->setFrameBufferResizeIgnored(true);

    /* The background has to go black */
    QPalette palette(centralWidget()->palette());
    palette.setColor(centralWidget()->backgroundRole(), Qt::black);
    centralWidget()->setPalette (palette);
    centralWidget()->setAutoFillBackground (true);
    setAutoFillBackground (true);

#ifdef Q_WS_MAC
    /* Fade to black */
//    CGAcquireDisplayFadeReservation (kCGMaxDisplayReservationInterval, &mFadeToken);
//    CGDisplayFade (mFadeToken, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0.0, 0.0, 0.0, true);
#endif


    /* We have to show the window early, or the position will be wrong on the
       Mac */
    show();

#ifdef Q_WS_MAC
# ifndef QT_MAC_USE_COCOA
    /* setWindowState removes the window group connection somehow. So save it
     * temporary. */
    WindowGroupRef g = GetWindowGroup(::darwinToNativeWindow(this));
# endif  /* !QT_MAC_USE_COCOA */
    /* Here we are going really fullscreen */
    setWindowState(windowState() ^ Qt::WindowFullScreen);
    setPresentationModeEnabled(true);

# ifndef QT_MAC_USE_COCOA
    /* Reassign the correct window group. */
    SetWindowGroup(::darwinToNativeWindow (this), g);
# endif /* !QT_MAC_USE_COCOA */
#else /* Q_WS_MAC */
    setWindowState(windowState() ^ Qt::WindowFullScreen);
#endif/* !Q_WS_MAC */

//    m_pMachineView->normalizeGeometry(true);
//    ((UIMachineViewFullscreen*)m_pMachineView)->sltPerformGuestResize(maximumSize());
//    show();

    QRect r = geometry();
    /* Load global settings: */
    {
//        VBoxGlobalSettings settings = vboxGlobal().settings();
//        menuBar()->setHidden(settings.isFeatureActive("noMenuBar"));
    }
}

void UIMachineWindowFullscreen::saveWindowSettings()
{
    CMachine machine = session().GetMachine();

    /* Save extra-data settings: */
    {
    }

#ifdef Q_WS_MAC
    setPresentationModeEnabled(false);
#endif/* Q_WS_MAC */
}

void UIMachineWindowFullscreen::cleanupMachineView()
{
    /* Do not cleanup machine view if it is not present: */
    if (!machineView())
        return;

    UIMachineView::destroy(m_pMachineView);
    m_pMachineView = 0;
}

#ifdef Q_WS_MAC
# ifdef QT_MAC_USE_COCOA
void UIMachineWindowFullscreen::setPresentationModeEnabled(bool fEnabled)
{
    if (fEnabled)
    {
        /* First check if we are on the primary screen, only than the presentation mode have to be changed. */
        QDesktopWidget* pDesktop = QApplication::desktop();
        if (pDesktop->screenNumber(this) == pDesktop->primaryScreen())
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
# endif /* QT_MAC_USE_COCOA */
#endif /* Q_WS_MAC */
