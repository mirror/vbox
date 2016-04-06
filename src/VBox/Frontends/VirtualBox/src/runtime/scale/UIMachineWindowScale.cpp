/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineWindowScale class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QMenu>
# include <QTimer>
# include <QSpacerItem>
# include <QResizeEvent>

/* GUI includes: */
# include "VBoxGlobal.h"
# include "UIExtraDataManager.h"
# include "UISession.h"
# include "UIMachineLogic.h"
# include "UIMachineWindowScale.h"
# include "UIMachineView.h"
# ifdef VBOX_WS_MAC
#  include "VBoxUtils.h"
#  include "UIImageTools.h"
#  include "UICocoaApplication.h"
# endif /* VBOX_WS_MAC */

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIMachineWindowScale::UIMachineWindowScale(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : UIMachineWindow(pMachineLogic, uScreenId)
{
}

void UIMachineWindowScale::prepareMainLayout()
{
    /* Call to base-class: */
    UIMachineWindow::prepareMainLayout();

    /* Strict spacers to hide them, they are not necessary for scale-mode: */
    m_pTopSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_pBottomSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_pLeftSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_pRightSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
}

#ifdef VBOX_WS_MAC
void UIMachineWindowScale::prepareVisualState()
{
    /* Call to base-class: */
    UIMachineWindow::prepareVisualState();

    /* Beta label? */
    if (vboxGlobal().isBeta())
    {
        QPixmap betaLabel = ::betaLabel(QSize(100, 16));
        ::darwinLabelWindow(this, &betaLabel, true);
    }

    /* For 'Yosemite' and above: */
    if (vboxGlobal().osRelease() >= MacOSXRelease_Yosemite)
    {
        /* Enable fullscreen support for every screen which requires it: */
        if (darwinScreensHaveSeparateSpaces() || m_uScreenId == 0)
            darwinEnableFullscreenSupport(this);
        /* Register 'Zoom' button to use our full-screen: */
        UICocoaApplication::instance()->registerCallbackForStandardWindowButton(this, StandardWindowButtonType_Zoom,
                                                                                UIMachineWindow::handleStandardWindowButtonCallback);
    }
}
#endif /* VBOX_WS_MAC */

void UIMachineWindowScale::loadSettings()
{
    /* Call to base-class: */
    UIMachineWindow::loadSettings();

    /* Load extra-data settings: */
    {
        /* Load extra-data: */
        QRect geo = gEDataManager->machineWindowGeometry(machineLogic()->visualStateType(),
                                                         m_uScreenId, vboxGlobal().managedVMUuid());

        /* If we do have proper geometry: */
        if (!geo.isNull())
        {
            /* Restore window geometry: */
            m_normalGeometry = geo;
            setGeometry(m_normalGeometry);

            /* Maximize (if necessary): */
            if (gEDataManager->machineWindowShouldBeMaximized(machineLogic()->visualStateType(),
                                                              m_uScreenId, vboxGlobal().managedVMUuid()))
                setWindowState(windowState() | Qt::WindowMaximized);
        }
        /* If we do NOT have proper geometry: */
        else
        {
            /* Get available geometry, for screen with (x,y) coords if possible: */
            QRect availableGeo = !geo.isNull() ? vboxGlobal().availableGeometry(QPoint(geo.x(), geo.y())) :
                                                 vboxGlobal().availableGeometry(this);

            /* Resize to default size: */
            resize(640, 480);
            /* Move newly created window to the screen-center: */
            m_normalGeometry = geometry();
            m_normalGeometry.moveCenter(availableGeo.center());
            setGeometry(m_normalGeometry);
        }
    }
}

void UIMachineWindowScale::saveSettings()
{
    /* Save window geometry: */
    {
        gEDataManager->setMachineWindowGeometry(machineLogic()->visualStateType(),
                                                m_uScreenId, m_normalGeometry,
                                                isMaximizedChecked(), vboxGlobal().managedVMUuid());
    }

    /* Call to base-class: */
    UIMachineWindow::saveSettings();
}

#ifdef VBOX_WS_MAC
void UIMachineWindowScale::cleanupVisualState()
{
    /* Unregister 'Zoom' button from using our full-screen since Yosemite: */
    if (vboxGlobal().osRelease() >= MacOSXRelease_Yosemite)
        UICocoaApplication::instance()->unregisterCallbackForStandardWindowButton(this, StandardWindowButtonType_Zoom);
}
#endif /* VBOX_WS_MAC */

void UIMachineWindowScale::showInNecessaryMode()
{
    /* Make sure this window should be shown at all: */
    if (!uisession()->isScreenVisible(m_uScreenId))
        return hide();

    /* Make sure this window is not minimized: */
    if (isMinimized())
        return;

    /* Show in normal mode: */
    show();

    /* Make sure machine-view have focus: */
    m_pMachineView->setFocus();
}

bool UIMachineWindowScale::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case QEvent::Resize:
        {
            QResizeEvent *pResizeEvent = static_cast<QResizeEvent*>(pEvent);
            if (!isMaximizedChecked())
            {
                m_normalGeometry.setSize(pResizeEvent->size());
#ifdef VBOX_WITH_DEBUGGER_GUI
                /* Update debugger window position: */
                updateDbgWindows();
#endif /* VBOX_WITH_DEBUGGER_GUI */
            }
            break;
        }
        case QEvent::Move:
        {
            if (!isMaximizedChecked())
            {
                m_normalGeometry.moveTo(geometry().x(), geometry().y());
#ifdef VBOX_WITH_DEBUGGER_GUI
                /* Update debugger window position: */
                updateDbgWindows();
#endif /* VBOX_WITH_DEBUGGER_GUI */
            }
            break;
        }
        default:
            break;
    }
    return UIMachineWindow::event(pEvent);
}

#ifdef VBOX_WS_WIN
# if QT_VERSION < 0x050000
bool UIMachineWindowScale::winEvent(MSG *pMessage, long *pResult)
{
    /* Try to keep aspect ratio during window resize if:
     * 1. machine view exists and 2. event-type is WM_SIZING and 3. shift key is NOT pressed: */
    if (machineView() && pMessage->message == WM_SIZING && !(QApplication::keyboardModifiers() & Qt::ShiftModifier))
    {
        double dAspectRatio = machineView()->aspectRatio();
        if (dAspectRatio)
        {
            RECT *pRect = reinterpret_cast<RECT*>(pMessage->lParam);
            switch (pMessage->wParam)
            {
                case WMSZ_LEFT:
                case WMSZ_RIGHT:
                {
                    pRect->bottom = pRect->top + (double)(pRect->right - pRect->left) / dAspectRatio;
                    break;
                }
                case WMSZ_TOP:
                case WMSZ_BOTTOM:
                {
                    pRect->right = pRect->left + (double)(pRect->bottom - pRect->top) * dAspectRatio;
                    break;
                }
                case WMSZ_BOTTOMLEFT:
                case WMSZ_BOTTOMRIGHT:
                {
                    pRect->bottom = pRect->top + (double)(pRect->right - pRect->left) / dAspectRatio;
                    break;
                }
                case WMSZ_TOPLEFT:
                case WMSZ_TOPRIGHT:
                {
                    pRect->top = pRect->bottom - (double)(pRect->right - pRect->left) / dAspectRatio;
                    break;
                }
                default:
                    break;
            }
        }
    }
    /* Call to base-class: */
    return UIMachineWindow::winEvent(pMessage, pResult);
}
# endif /* QT_VERSION < 0x050000 */
#endif /* VBOX_WS_WIN */

bool UIMachineWindowScale::isMaximizedChecked()
{
#ifdef VBOX_WS_MAC
    /* On the Mac the WindowStateChange signal doesn't seems to be delivered
     * when the user get out of the maximized state. So check this ourself. */
    return ::darwinIsWindowMaximized(this);
#else /* VBOX_WS_MAC */
    return isMaximized();
#endif /* !VBOX_WS_MAC */
}

