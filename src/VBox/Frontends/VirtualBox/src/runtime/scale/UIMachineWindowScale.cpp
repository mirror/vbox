/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowScale class implementation
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
#include <QContextMenuEvent>
#ifdef Q_WS_MAC
# include <QMenuBar>
#endif /* Q_WS_MAC */

/* Local includes: */
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "VBoxUtils.h"
#include "UISession.h"
#include "UIMachineLogic.h"
#include "UIMachineView.h"
#include "UIMachineWindowScale.h"
#ifdef Q_WS_MAC
# include "UIImageTools.h"
#endif /* Q_WS_MAC */

UIMachineWindowScale::UIMachineWindowScale(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : UIMachineWindow(pMachineLogic, uScreenId)
    , m_pMainMenu(0)
{
}

void UIMachineWindowScale::sltPopupMainMenu()
{
    /* Popup main-menu if present: */
    if (m_pMainMenu && !m_pMainMenu->isEmpty())
    {
        m_pMainMenu->popup(geometry().center());
        QTimer::singleShot(0, m_pMainMenu, SLOT(sltSelectFirstAction()));
    }
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

void UIMachineWindowScale::prepareMenu()
{
    /* Call to base-class: */
    UIMachineWindow::prepareMenu();

#ifdef Q_WS_MAC
    setMenuBar(uisession()->newMenuBar());
#endif /* Q_WS_MAC */
    m_pMainMenu = uisession()->newMenu();
}

#ifdef Q_WS_MAC
void UIMachineWindowScale::prepareVisualState()
{
    /* Call to base-class: */
    UIMachineWindow::prepareVisualState();

    /* Install the resize delegate for keeping the aspect ratio. */
    ::darwinInstallResizeDelegate(this);
    /* Beta label? */
    if (vboxGlobal().isBeta())
    {
        QPixmap betaLabel = ::betaLabel(QSize(100, 16));
        ::darwinLabelWindow(this, &betaLabel, true);
    }
}
#endif /* Q_WS_MAC */

void UIMachineWindowScale::loadSettings()
{
    /* Call to base-class: */
    UIMachineWindow::loadSettings();

    /* Load scale window settings: */
    CMachine m = machine();

    /* Load extra-data settings: */
    {
        QString strPositionAddress = m_uScreenId == 0 ? QString("%1").arg(VBoxDefs::GUI_LastScaleWindowPosition) :
                                     QString("%1%2").arg(VBoxDefs::GUI_LastScaleWindowPosition).arg(m_uScreenId);
        QStringList strPositionSettings = m.GetExtraDataStringList(strPositionAddress);

        bool ok = !strPositionSettings.isEmpty(), max = false;
        int x = 0, y = 0, w = 0, h = 0;

        if (ok && strPositionSettings.size() > 0)
            x = strPositionSettings[0].toInt(&ok);
        else ok = false;
        if (ok && strPositionSettings.size() > 1)
            y = strPositionSettings[1].toInt(&ok);
        else ok = false;
        if (ok && strPositionSettings.size() > 2)
            w = strPositionSettings[2].toInt(&ok);
        else ok = false;
        if (ok && strPositionSettings.size() > 3)
            h = strPositionSettings[3].toInt(&ok);
        else ok = false;
        if (ok && strPositionSettings.size() > 4)
            max = strPositionSettings[4] == VBoxDefs::GUI_LastWindowState_Max;

        QRect ar = ok ? QApplication::desktop()->availableGeometry(QPoint(x, y)) :
                        QApplication::desktop()->availableGeometry(this);

        /* If previous parameters were read correctly: */
        if (ok)
        {
            /* Restore window size and position: */
            m_normalGeometry = QRect(x, y, w, h);
            setGeometry(m_normalGeometry);
            /* Maximize if needed: */
            if (max)
                setWindowState(windowState() | Qt::WindowMaximized);
        }
        else
        {
            /* Resize to default size: */
            resize(640, 480);
            qApp->processEvents();
            /* Move newly created window to the screen center: */
            m_normalGeometry = geometry();
            m_normalGeometry.moveCenter(ar.center());
            setGeometry(m_normalGeometry);
        }
    }
}

void UIMachineWindowScale::saveSettings()
{
    /* Get machine: */
    CMachine m = machine();

    /* Save extra-data settings: */
    {
        QString strWindowPosition = QString("%1,%2,%3,%4")
                                    .arg(m_normalGeometry.x()).arg(m_normalGeometry.y())
                                    .arg(m_normalGeometry.width()).arg(m_normalGeometry.height());
        if (isMaximizedChecked())
            strWindowPosition += QString(",%1").arg(VBoxDefs::GUI_LastWindowState_Max);
        QString strPositionAddress = m_uScreenId == 0 ? QString("%1").arg(VBoxDefs::GUI_LastScaleWindowPosition) :
                                     QString("%1%2").arg(VBoxDefs::GUI_LastScaleWindowPosition).arg(m_uScreenId);
        m.SetExtraData(strPositionAddress, strWindowPosition);
    }

    /* Call to base-class: */
    UIMachineWindow::saveSettings();
}

#ifdef Q_WS_MAC
void UIMachineWindowScale::cleanupVisualState()
{
    /* Uninstall the resize delegate for keeping the aspect ratio. */
    ::darwinUninstallResizeDelegate(this);

    /* Call to base-class: */
    UIMachineWindow::cleanupVisualState();
}
#endif /* Q_WS_MAC */

void UIMachineWindowScale::cleanupMenu()
{
    /* Cleanup menu: */
    delete m_pMainMenu;
    m_pMainMenu = 0;

    /* Call to base-class: */
    UIMachineWindow::cleanupMenu();
}

void UIMachineWindowScale::showInNecessaryMode()
{
    /* Make sure we really have to show window: */
    BOOL fEnabled = true;
    ULONG guestOriginX = 0, guestOriginY = 0, guestWidth = 0, guestHeight = 0;
    machine().QuerySavedGuestScreenInfo(m_uScreenId, guestOriginX, guestOriginY, guestWidth, guestHeight, fEnabled);
    if (fEnabled)
        show();
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

#ifdef Q_WS_WIN
bool UIMachineWindowScale::winEvent(MSG *pMessage, long *pResult)
{
    /* Try to keep aspect ratio during window resize if:
     * 1. machine view exists and 2. event-type is WM_SIZING and 3. shift key is NOT pressed: */
    if (machineView() && pMessage->message == WM_SIZING && !(QApplication::keyboardModifiers() & Qt::ShiftModifier))
    {
        if (double dAspectRatio = machineView()->aspectRatio())
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
#endif /* Q_WS_WIN */

bool UIMachineWindowScale::isMaximizedChecked()
{
#ifdef Q_WS_MAC
    /* On the Mac the WindowStateChange signal doesn't seems to be delivered
     * when the user get out of the maximized state. So check this ourself. */
    return ::darwinIsWindowMaximized(this);
#else /* Q_WS_MAC */
    return isMaximized();
#endif /* !Q_WS_MAC */
}

